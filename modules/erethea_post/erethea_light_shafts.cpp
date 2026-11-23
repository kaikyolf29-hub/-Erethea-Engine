/**************************************************************************/
/*  erethea_light_shafts.cpp                                              */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#include "erethea_light_shafts.h"

#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "servers/rendering/rendering_server.h"

#ifdef RD_ENABLED
#include "erethea_light_shafts.glsl.gen.h"

#include "servers/rendering/renderer_compositor.h"
#include "servers/rendering/renderer_rd/effects/copy_effects.h"
#include "servers/rendering/renderer_rd/storage_rd/light_storage.h"
#include "servers/rendering/renderer_rd/storage_rd/material_storage.h"
#include "servers/rendering/renderer_rd/storage_rd/render_data_rd.h"
#include "servers/rendering/renderer_rd/storage_rd/render_scene_buffers_rd.h"
#include "servers/rendering/renderer_rd/storage_rd/render_scene_data_rd.h"
#include "servers/rendering/renderer_rd/uniform_set_cache_rd.h"
#include "servers/rendering/rendering_server_globals.h"

namespace {

struct PushConstant {
	int32_t size[2];
	float sun_uv[2];

	float tint[3];
	float intensity;

	float threshold;
	float density;
	float decay;
	int32_t samples;

	uint32_t frame;
	uint32_t pad[3];
};

// Shared by all instances. Only touched on the rendering thread.
struct RenderResources {
	EretheaLightShaftsShaderRD shader;
	RID version;
	RID pipeline;
};

RenderResources *render_resources = nullptr;

const StringName &context_name() {
	static const StringName name = "erethea_light_shafts";
	return name;
}

const StringName &source_texture_name() {
	static const StringName name = "source";
	return name;
}

} // namespace
#endif // RD_ENABLED

void EretheaLightShafts::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_intensity", "intensity"), &EretheaLightShafts::set_intensity);
	ClassDB::bind_method(D_METHOD("get_intensity"), &EretheaLightShafts::get_intensity);
	ClassDB::bind_method(D_METHOD("set_samples", "samples"), &EretheaLightShafts::set_samples);
	ClassDB::bind_method(D_METHOD("get_samples"), &EretheaLightShafts::get_samples);
	ClassDB::bind_method(D_METHOD("set_density", "density"), &EretheaLightShafts::set_density);
	ClassDB::bind_method(D_METHOD("get_density"), &EretheaLightShafts::get_density);
	ClassDB::bind_method(D_METHOD("set_decay", "decay"), &EretheaLightShafts::set_decay);
	ClassDB::bind_method(D_METHOD("get_decay"), &EretheaLightShafts::get_decay);
	ClassDB::bind_method(D_METHOD("set_tint", "tint"), &EretheaLightShafts::set_tint);
	ClassDB::bind_method(D_METHOD("get_tint"), &EretheaLightShafts::get_tint);
	ClassDB::bind_method(D_METHOD("set_threshold", "threshold"), &EretheaLightShafts::set_threshold);
	ClassDB::bind_method(D_METHOD("get_threshold"), &EretheaLightShafts::get_threshold);
	ClassDB::bind_method(D_METHOD("set_custom_direction", "direction"), &EretheaLightShafts::set_custom_direction);
	ClassDB::bind_method(D_METHOD("get_custom_direction"), &EretheaLightShafts::get_custom_direction);

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "intensity", PROPERTY_HINT_RANGE, "0,2,0.01,or_greater"), "set_intensity", "get_intensity");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "samples", PROPERTY_HINT_RANGE, "8,64,1"), "set_samples", "get_samples");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "density", PROPERTY_HINT_RANGE, "0.1,1,0.01"), "set_density", "get_density");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "decay", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_decay", "get_decay");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "tint", PROPERTY_HINT_COLOR_NO_ALPHA), "set_tint", "get_tint");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "threshold", PROPERTY_HINT_RANGE, "0,16,0.01,or_greater"), "set_threshold", "get_threshold");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "custom_direction"), "set_custom_direction", "get_custom_direction");
}

void EretheaLightShafts::_validate_property(PropertyInfo &p_property) const {
	// The effect always runs after the transparent pass with the resolved
	// depth, so these CompositorEffect settings don't apply.
	if (p_property.name == "effect_callback_type" || p_property.name == "access_resolved_color" || p_property.name == "access_resolved_depth" || p_property.name == "needs_motion_vectors" || p_property.name == "needs_normal_roughness" || p_property.name == "needs_separate_specular") {
		p_property.usage = PROPERTY_USAGE_NONE;
	}
}

void EretheaLightShafts::set_intensity(float p_intensity) {
	intensity = MAX(p_intensity, 0.0f);
}

float EretheaLightShafts::get_intensity() const {
	return intensity;
}

void EretheaLightShafts::set_samples(int p_samples) {
	samples = CLAMP(p_samples, 8, 64);
}

int EretheaLightShafts::get_samples() const {
	return samples;
}

void EretheaLightShafts::set_density(float p_density) {
	density = CLAMP(p_density, 0.1f, 1.0f);
}

float EretheaLightShafts::get_density() const {
	return density;
}

void EretheaLightShafts::set_decay(float p_decay) {
	decay = CLAMP(p_decay, 0.0f, 1.0f);
}

float EretheaLightShafts::get_decay() const {
	return decay;
}

void EretheaLightShafts::set_tint(const Color &p_tint) {
	tint = Color(p_tint.r, p_tint.g, p_tint.b);
}

Color EretheaLightShafts::get_tint() const {
	return tint;
}

void EretheaLightShafts::set_threshold(float p_threshold) {
	threshold = MAX(p_threshold, 0.0f);
}

float EretheaLightShafts::get_threshold() const {
	return threshold;
}

void EretheaLightShafts::set_custom_direction(const Vector3 &p_direction) {
	custom_direction = p_direction;
}

Vector3 EretheaLightShafts::get_custom_direction() const {
	return custom_direction;
}

float EretheaLightShafts::project_sun(const Projection &p_projection, const Transform3D &p_camera_transform, const Vector3 &p_sun_direction, bool p_flip_y, Vector2 &r_uv) {
	// The sun is infinitely far away, so project it as a direction (w = 0).
	const Vector3 view_direction = p_camera_transform.basis.orthonormalized().xform_inv(p_sun_direction.normalized());
	const Vector4 clip = p_projection.xform(Vector4(view_direction.x, view_direction.y, view_direction.z, 0.0f));
	// For a perspective projection, w is the distance in front of the camera.
	const float w = float(clip.w);
	if (w <= float(CMP_EPSILON)) {
		return 0.0f;
	}
	Vector2 ndc = Vector2(float(clip.x), float(clip.y)) / w;
	if (p_flip_y) {
		ndc.y = -ndc.y;
	}
	r_uv = ndc * 0.5f + Vector2(0.5f, 0.5f);

	// Fade out as the sun leaves the screen, and as it turns to the side of
	// the camera, before it goes behind it.
	const float edge = MAX(Math::abs(ndc.x), Math::abs(ndc.y));
	const float edge_fade = 1.0f - Math::smoothstep(1.0f, 1.6f, edge);
	const float front_fade = Math::smoothstep(0.0f, 0.2f, w / float(view_direction.length()));
	return edge_fade * front_fade;
}

void EretheaLightShafts::_render(int p_effect_callback_type, const RenderData *p_render_data) {
#ifdef RD_ENABLED
	if (p_effect_callback_type != RSE::COMPOSITOR_EFFECT_CALLBACK_TYPE_POST_TRANSPARENT || intensity <= 0.0f) {
		return;
	}

	const RenderDataRD *render_data = Object::cast_to<RenderDataRD>(p_render_data);
	ERR_FAIL_NULL(render_data);
	Ref<RenderSceneBuffersRD> rb = render_data->render_buffers;
	if (rb.is_null() || !rb->has_internal_texture()) {
		return;
	}

	// The Mobile renderer can't write to its color buffer from a compute shader.
	if (!rb->get_can_be_storage() || rb->get_base_data_format() != RD::DATA_FORMAT_R16G16B16A16_SFLOAT) {
		WARN_PRINT_ONCE("EretheaLightShafts needs the Forward+ renderer. It does nothing with the current renderer.");
		return;
	}

	const RenderSceneDataRD *scene_data = render_data->scene_data;
	if (scene_data == nullptr) {
		return;
	}

	// The direction to the sun: the custom one, or the first visible directional light.
	Vector3 sun_direction = custom_direction;
	if (sun_direction.is_zero_approx()) {
		RendererRD::LightStorage *light_storage = RendererRD::LightStorage::get_singleton();
		ERR_FAIL_NULL(light_storage);
		if (render_data->lights == nullptr) {
			return;
		}
		bool found = false;
		for (uint32_t i = 0; i < render_data->lights->size(); i++) {
			const RID light_instance = (*render_data->lights)[i];
			const RID light = light_storage->light_instance_get_base_light(light_instance);
			if (light_storage->light_get_type(light) != RSE::LIGHT_DIRECTIONAL || light_storage->light_get_param(light, RSE::LIGHT_PARAM_ENERGY) <= 0.0f) {
				continue;
			}
			// A light shines along its -Z axis, so +Z points to the light.
			sun_direction = light_storage->light_instance_get_base_transform(light_instance).basis.xform(Vector3(0, 0, 1));
			found = true;
			break;
		}
		if (!found) {
			return;
		}
	}

	const Size2i size = rb->get_internal_size();
	const uint32_t view_count = rb->get_view_count();

	// Project the sun for each view, and skip the work when it can't be seen.
	Vector2 sun_uvs[RendererSceneRender::MAX_RENDER_VIEWS];
	float visibilities[RendererSceneRender::MAX_RENDER_VIEWS];
	bool any_visible = false;
	for (uint32_t view = 0; view < view_count; view++) {
		Projection correction;
		correction.set_depth_correction(scene_data->flip_y);
		const Projection projection = view_count == 1 ? scene_data->cam_projection : scene_data->view_projection[view];
		visibilities[view] = project_sun(correction * projection, scene_data->cam_transform, sun_direction, false, sun_uvs[view]);
		any_visible = any_visible || visibilities[view] > 0.0f;
	}
	if (!any_visible) {
		return;
	}

	RD *rd = RD::get_singleton();
	if (render_resources == nullptr) {
		render_resources = memnew(RenderResources);
		Vector<String> modes;
		modes.push_back("");
		render_resources->shader.initialize(modes);
		render_resources->version = render_resources->shader.version_create();
		render_resources->pipeline = rd->compute_pipeline_create(render_resources->shader.version_get_shader(render_resources->version, 0));
	}

	if (!rb->has_texture(context_name(), source_texture_name())) {
		rb->create_texture(context_name(), source_texture_name(), RD::DATA_FORMAT_R16G16B16A16_SFLOAT, RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_STORAGE_BIT, RD::TEXTURE_SAMPLES_1, size, view_count);
	}

	PushConstant push_constant = {};
	push_constant.size[0] = size.width;
	push_constant.size[1] = size.height;
	push_constant.tint[0] = tint.r;
	push_constant.tint[1] = tint.g;
	push_constant.tint[2] = tint.b;
	push_constant.threshold = threshold;
	push_constant.density = density;
	push_constant.decay = decay;
	push_constant.samples = samples;
	push_constant.frame = uint32_t(RSG::rasterizer->get_frame_number());

	RendererRD::CopyEffects *copy_effects = RendererRD::CopyEffects::get_singleton();
	UniformSetCacheRD *uniform_set_cache = UniformSetCacheRD::get_singleton();
	RendererRD::MaterialStorage *material_storage = RendererRD::MaterialStorage::get_singleton();
	ERR_FAIL_NULL(copy_effects);
	ERR_FAIL_NULL(uniform_set_cache);
	ERR_FAIL_NULL(material_storage);

	const RID linear_sampler = material_storage->sampler_rd_get_default(RSE::CANVAS_ITEM_TEXTURE_FILTER_LINEAR, RSE::CANVAS_ITEM_TEXTURE_REPEAT_DISABLED);
	const RID nearest_sampler = material_storage->sampler_rd_get_default(RSE::CANVAS_ITEM_TEXTURE_FILTER_NEAREST, RSE::CANVAS_ITEM_TEXTURE_REPEAT_DISABLED);
	const RID shader = render_resources->shader.version_get_shader(render_resources->version, 0);

	rd->draw_command_begin_label("Erethea Light Shafts");
	for (uint32_t view = 0; view < view_count; view++) {
		if (visibilities[view] <= 0.0f) {
			continue;
		}
		push_constant.sun_uv[0] = sun_uvs[view].x;
		push_constant.sun_uv[1] = sun_uvs[view].y;
		push_constant.intensity = intensity * visibilities[view];

		const RID color = rb->get_internal_texture(view);
		const RID source = rb->get_texture_slice(context_name(), source_texture_name(), view, 0);
		copy_effects->copy_to_rect(color, source, Rect2i(Point2i(), size));

		RD::Uniform u_source(RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE, 0, Vector<RID>({ linear_sampler, source }));
		RD::Uniform u_depth(RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE, 1, Vector<RID>({ nearest_sampler, rb->get_depth_texture(view) }));
		RD::Uniform u_dest(RD::UNIFORM_TYPE_IMAGE, 0, Vector<RID>({ color }));

		RD::ComputeListID compute_list = rd->compute_list_begin();
		rd->compute_list_bind_compute_pipeline(compute_list, render_resources->pipeline);
		rd->compute_list_bind_uniform_set(compute_list, uniform_set_cache->get_cache(shader, 0, u_source, u_depth), 0);
		rd->compute_list_bind_uniform_set(compute_list, uniform_set_cache->get_cache(shader, 1, u_dest), 1);
		rd->compute_list_set_push_constant(compute_list, &push_constant, sizeof(PushConstant));
		rd->compute_list_dispatch_threads(compute_list, size.width, size.height, 1);
		rd->compute_list_end();
	}
	rd->draw_command_end_label();
#endif // RD_ENABLED
}

void EretheaLightShafts::_free_render_resources() {
#ifdef RD_ENABLED
	if (render_resources != nullptr) {
		render_resources->shader.version_free(render_resources->version);
		memdelete(render_resources);
		render_resources = nullptr;
	}
#endif // RD_ENABLED
}

void EretheaLightShafts::finish() {
	RenderingServer *rs = RenderingServer::get_singleton();
	if (rs != nullptr) {
		rs->call_on_render_thread(callable_mp_static(&EretheaLightShafts::_free_render_resources));
	}
}

EretheaLightShafts::EretheaLightShafts() {
	set_access_resolved_depth(true);

	RenderingServer *rs = RenderingServer::get_singleton();
	if (rs != nullptr && get_rid().is_valid()) {
		// CompositorEffect calls the _render_callback() virtual method, which
		// only scripts and extensions can override, so point the effect at
		// this class's own callback instead.
		rs->compositor_effect_set_callback(get_rid(), RSE::COMPOSITOR_EFFECT_CALLBACK_TYPE_POST_TRANSPARENT, callable_mp(this, &EretheaLightShafts::_render));
	}
}
