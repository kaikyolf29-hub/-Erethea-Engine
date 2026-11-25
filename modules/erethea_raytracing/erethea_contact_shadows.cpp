/**************************************************************************/
/*  erethea_contact_shadows.cpp                                           */
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

#include "erethea_contact_shadows.h"

#include "erethea_scene_effects.h"

#include "core/config/project_settings.h"
#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "servers/rendering/rendering_server.h"

#ifdef RD_ENABLED
#include "erethea_contact_shadows.glsl.gen.h"

#include "servers/rendering/renderer_compositor.h"
#include "servers/rendering/renderer_rd/renderer_scene_render_rd.h"
#include "servers/rendering/renderer_rd/storage_rd/light_storage.h"
#include "servers/rendering/renderer_rd/storage_rd/material_storage.h"
#include "servers/rendering/renderer_rd/storage_rd/render_data_rd.h"
#include "servers/rendering/renderer_rd/storage_rd/render_scene_buffers_rd.h"
#include "servers/rendering/renderer_rd/storage_rd/render_scene_data_rd.h"
#include "servers/rendering/renderer_rd/uniform_set_cache_rd.h"
#include "servers/rendering/rendering_server_globals.h"

namespace {

// The std140 layout of Params in erethea_contact_shadows.glsl.
struct Params {
	float view_projection[16];
	float inv_view_projection[16];

	float camera_position[3];
	float ray_length;

	float light_direction[3];
	float thickness;

	int32_t size[2];
	uint32_t steps;
	uint32_t frame;

	float normal_bias;
	float pad[3];
};
static_assert(sizeof(Params) == 192, "Params must match the std140 layout of the shader.");

// Shared by all the effects. Only touched on the rendering thread.
struct RenderResources {
	EretheaContactShadowsShaderRD shader;
	RID version;
	RID pipeline;
	RID params_buffer;
};

RenderResources *render_resources = nullptr;

// The mask that the Forward+ renderer reads, shared with
// EretheaRayTracedShadows (render_forward_clustered.cpp, "erethea_rt" and
// "shadow_mask").
const StringName &context_name() {
	static const StringName name = "erethea_rt";
	return name;
}

const StringName &mask_texture_name() {
	static const StringName name = "shadow_mask";
	return name;
}

} // namespace
#endif // RD_ENABLED

void EretheaContactShadows::register_project_settings() {
	GLOBAL_DEF(PropertyInfo(Variant::INT, SUN_SETTING, PROPERTY_HINT_ENUM, "Disabled,Automatic,Enabled"), SUN_AUTOMATIC);
}

bool EretheaContactShadows::is_added_to_scenes(int p_setting, bool p_forward_plus, int p_automatic_quality_preset) {
	if (!p_forward_plus) {
		return false;
	}
	switch (p_setting) {
		case SUN_ENABLED:
			return true;
		case SUN_AUTOMATIC:
			// Not on integrated graphics ("Low"), where it would cost too much.
			return p_automatic_quality_preset == EretheaSceneEffects::QUALITY_BALANCED || p_automatic_quality_preset == EretheaSceneEffects::QUALITY_CINEMATIC;
		default:
			return false;
	}
}

void EretheaContactShadows::add_to_running_scenes() {
	const int setting = GLOBAL_GET(SUN_SETTING);
	if (setting == SUN_DISABLED) {
		return;
	}
	RenderingServer *rs = RenderingServer::get_singleton();
	const bool forward_plus = rs != nullptr && rs->get_current_rendering_method() == "forward_plus";
	if (!is_added_to_scenes(setting, forward_plus, EretheaSceneEffects::get_automatic_quality_preset())) {
		return;
	}
	print_verbose("Erethea: contact shadows of the sun added to the scenes (rendering/erethea/contact_shadows/sun).");
	EretheaSceneEffects::add_to_running_scenes(get_class_static());
}

void EretheaContactShadows::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_length", "length"), &EretheaContactShadows::set_length);
	ClassDB::bind_method(D_METHOD("get_length"), &EretheaContactShadows::get_length);
	ClassDB::bind_method(D_METHOD("set_thickness", "thickness"), &EretheaContactShadows::set_thickness);
	ClassDB::bind_method(D_METHOD("get_thickness"), &EretheaContactShadows::get_thickness);
	ClassDB::bind_method(D_METHOD("set_steps", "steps"), &EretheaContactShadows::set_steps);
	ClassDB::bind_method(D_METHOD("get_steps"), &EretheaContactShadows::get_steps);
	ClassDB::bind_method(D_METHOD("set_normal_bias", "bias"), &EretheaContactShadows::set_normal_bias);
	ClassDB::bind_method(D_METHOD("get_normal_bias"), &EretheaContactShadows::get_normal_bias);

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "length", PROPERTY_HINT_RANGE, "0.01,5,0.01,or_greater,suffix:m"), "set_length", "get_length");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "thickness", PROPERTY_HINT_RANGE, "0.01,2,0.01,or_greater,suffix:m"), "set_thickness", "get_thickness");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "steps", PROPERTY_HINT_RANGE, "4,64,1"), "set_steps", "get_steps");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "normal_bias", PROPERTY_HINT_RANGE, "0,0.5,0.001,suffix:m"), "set_normal_bias", "get_normal_bias");
}

void EretheaContactShadows::_validate_property(PropertyInfo &p_property) const {
	// The effect always runs after the depth pre-pass with the resolved depth,
	// so these CompositorEffect settings don't apply.
	if (p_property.name == "effect_callback_type" || p_property.name == "access_resolved_color" || p_property.name == "access_resolved_depth" || p_property.name == "needs_motion_vectors" || p_property.name == "needs_normal_roughness" || p_property.name == "needs_separate_specular") {
		p_property.usage = PROPERTY_USAGE_NONE;
	}
}

void EretheaContactShadows::set_length(float p_length) {
	length = MAX(p_length, 0.01f);
}

float EretheaContactShadows::get_length() const {
	return length;
}

void EretheaContactShadows::set_thickness(float p_thickness) {
	thickness = MAX(p_thickness, 0.01f);
}

float EretheaContactShadows::get_thickness() const {
	return thickness;
}

void EretheaContactShadows::set_steps(int p_steps) {
	steps = CLAMP(p_steps, 4, MAX_STEPS);
}

int EretheaContactShadows::get_steps() const {
	return steps;
}

void EretheaContactShadows::set_normal_bias(float p_bias) {
	normal_bias = CLAMP(p_bias, 0.0f, 0.5f);
}

float EretheaContactShadows::get_normal_bias() const {
	return normal_bias;
}

void EretheaContactShadows::_render(int p_effect_callback_type, const RenderData *p_render_data) {
#ifdef RD_ENABLED
	if (p_effect_callback_type != RSE::COMPOSITOR_EFFECT_CALLBACK_TYPE_PRE_OPAQUE) {
		return;
	}
	const RenderDataRD *render_data = Object::cast_to<RenderDataRD>(p_render_data);
	ERR_FAIL_NULL(render_data);
	Ref<RenderSceneBuffersRD> rb = render_data->render_buffers;
	const RenderSceneDataRD *scene_data = render_data->scene_data;
	if (rb.is_null() || scene_data == nullptr || rb->get_view_count() != 1 || render_data->reflection_probe.is_valid()) {
		return;
	}
	// The Mobile renderer doesn't read the mask.
	if (!rb->get_can_be_storage()) {
		WARN_PRINT_ONCE("EretheaContactShadows needs the Forward+ renderer. It does nothing with the current renderer.");
		return;
	}

	RendererSceneRenderRD *scene_render = RendererSceneRenderRD::get_singleton();
	ERR_FAIL_NULL(scene_render);
	// The ray traced shadows already have the contact shadows, more precisely.
	if (scene_render->erethea_is_ray_tracing_requested()) {
		return;
	}

	// The direction to the sun: the first directional light with energy.
	RendererRD::LightStorage *light_storage = RendererRD::LightStorage::get_singleton();
	ERR_FAIL_NULL(light_storage);
	Vector3 light_direction;
	if (render_data->lights != nullptr) {
		for (uint32_t i = 0; i < render_data->lights->size(); i++) {
			const RID light_instance = (*render_data->lights)[i];
			const RID light = light_storage->light_instance_get_base_light(light_instance);
			if (light_storage->light_get_type(light) != RSE::LIGHT_DIRECTIONAL || light_storage->light_get_param(light, RSE::LIGHT_PARAM_ENERGY) <= 0.0f) {
				continue;
			}
			// A light shines along its -Z axis, so +Z points to the light.
			light_direction = light_storage->light_instance_get_base_transform(light_instance).basis.xform(Vector3(0, 0, 1)).normalized();
			break;
		}
	}
	if (light_direction.is_zero_approx()) {
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
		render_resources->params_buffer = rd->uniform_buffer_create(sizeof(Params));
	}

	if (render_resources->pipeline.is_null()) {
		return;
	}
	// The renderer reads the mask while this effect keeps asking, so only ask
	// when the mask is written.
	scene_render->erethea_request_shadow_mask();
	const Size2i size = rb->get_internal_size();
	if (!rb->has_texture(context_name(), mask_texture_name())) {
		rb->create_texture(context_name(), mask_texture_name(), RD::DATA_FORMAT_R8_UNORM, RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_STORAGE_BIT | RD::TEXTURE_USAGE_CAN_COPY_TO_BIT, RD::TEXTURE_SAMPLES_1, size);
	}
	const RID mask = rb->get_texture(context_name(), mask_texture_name());

	// From clip space, with Godot's depth correction, to world space, and back.
	Projection correction;
	correction.set_depth_correction(scene_data->flip_y);
	const Projection view_projection = correction * scene_data->cam_projection * Projection(scene_data->cam_transform.affine_inverse());

	Params params = {};
	RendererRD::MaterialStorage::store_camera(view_projection, params.view_projection);
	RendererRD::MaterialStorage::store_camera(view_projection.inverse(), params.inv_view_projection);
	const Vector3 camera_position = scene_data->cam_transform.origin;
	params.camera_position[0] = camera_position.x;
	params.camera_position[1] = camera_position.y;
	params.camera_position[2] = camera_position.z;
	params.ray_length = length;
	params.light_direction[0] = light_direction.x;
	params.light_direction[1] = light_direction.y;
	params.light_direction[2] = light_direction.z;
	params.thickness = thickness;
	params.size[0] = size.width;
	params.size[1] = size.height;
	params.steps = uint32_t(steps);
	params.frame = uint32_t(RSG::rasterizer->get_frame_number());
	params.normal_bias = normal_bias;
	rd->buffer_update(render_resources->params_buffer, 0, sizeof(Params), &params);

	UniformSetCacheRD *uniform_set_cache = UniformSetCacheRD::get_singleton();
	RendererRD::MaterialStorage *material_storage = RendererRD::MaterialStorage::get_singleton();
	ERR_FAIL_NULL(uniform_set_cache);
	ERR_FAIL_NULL(material_storage);
	const RID nearest_sampler = material_storage->sampler_rd_get_default(RSE::CANVAS_ITEM_TEXTURE_FILTER_NEAREST, RSE::CANVAS_ITEM_TEXTURE_REPEAT_DISABLED);
	const RID shader = render_resources->shader.version_get_shader(render_resources->version, 0);

	RD::Uniform u_depth(RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE, 0, Vector<RID>({ nearest_sampler, rb->get_depth_texture() }));
	RD::Uniform u_mask(RD::UNIFORM_TYPE_IMAGE, 0, mask);
	RD::Uniform u_params(RD::UNIFORM_TYPE_UNIFORM_BUFFER, 0, render_resources->params_buffer);

	rd->draw_command_begin_label("Erethea Contact Shadows");
	RD::ComputeListID compute_list = rd->compute_list_begin();
	rd->compute_list_bind_compute_pipeline(compute_list, render_resources->pipeline);
	rd->compute_list_bind_uniform_set(compute_list, uniform_set_cache->get_cache(shader, 0, u_depth), 0);
	rd->compute_list_bind_uniform_set(compute_list, uniform_set_cache->get_cache(shader, 1, u_mask), 1);
	rd->compute_list_bind_uniform_set(compute_list, uniform_set_cache->get_cache(shader, 2, u_params), 2);
	rd->compute_list_dispatch_threads(compute_list, size.width, size.height, 1);
	rd->compute_list_end();
	rd->draw_command_end_label();
#endif // RD_ENABLED
}

void EretheaContactShadows::_free_render_resources() {
#ifdef RD_ENABLED
	if (render_resources != nullptr) {
		RD *rd = RD::get_singleton();
		if (render_resources->params_buffer.is_valid()) {
			rd->free_rid(render_resources->params_buffer);
		}
		render_resources->shader.version_free(render_resources->version);
		memdelete(render_resources);
		render_resources = nullptr;
	}
#endif // RD_ENABLED
}

void EretheaContactShadows::finish() {
	RenderingServer *rs = RenderingServer::get_singleton();
	if (rs != nullptr) {
		rs->call_on_render_thread(callable_mp_static(&EretheaContactShadows::_free_render_resources));
	}
}

EretheaContactShadows::EretheaContactShadows() {
	set_access_resolved_depth(true);

	RenderingServer *rs = RenderingServer::get_singleton();
	if (rs != nullptr && get_rid().is_valid()) {
		// CompositorEffect calls the _render_callback() virtual method, which
		// only scripts and extensions can override, so point the effect at
		// this class's own callback instead.
		rs->compositor_effect_set_callback(get_rid(), RSE::COMPOSITOR_EFFECT_CALLBACK_TYPE_PRE_OPAQUE, callable_mp(this, &EretheaContactShadows::_render));
	}
}
