/**************************************************************************/
/*  erethea_motion_blur.cpp                                               */
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

#include "erethea_motion_blur.h"

#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "servers/rendering/rendering_server.h"

#ifdef RD_ENABLED
#include "erethea_motion_blur.glsl.gen.h"

#include "servers/rendering/renderer_compositor.h"
#include "servers/rendering/renderer_rd/effects/copy_effects.h"
#include "servers/rendering/renderer_rd/storage_rd/material_storage.h"
#include "servers/rendering/renderer_rd/storage_rd/render_data_rd.h"
#include "servers/rendering/renderer_rd/storage_rd/render_scene_buffers_rd.h"
#include "servers/rendering/renderer_rd/storage_rd/render_scene_data_rd.h"
#include "servers/rendering/renderer_rd/uniform_set_cache_rd.h"
#include "servers/rendering/rendering_server_globals.h"

namespace {

struct PushConstant {
	float reprojection[16];

	int32_t size[2];
	float intensity;
	float max_length;

	int32_t samples;
	uint32_t frame;
	uint32_t use_reprojection;
	uint32_t pad;
};

// Shared by all instances. Only touched on the rendering thread.
struct RenderResources {
	EretheaMotionBlurShaderRD shader;
	RID version;
	RID pipeline;
};

RenderResources *render_resources = nullptr;

const StringName &context_name() {
	static const StringName name = "erethea_motion_blur";
	return name;
}

const StringName &source_texture_name() {
	static const StringName name = "source";
	return name;
}

} // namespace
#endif // RD_ENABLED

void EretheaMotionBlur::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_intensity", "intensity"), &EretheaMotionBlur::set_intensity);
	ClassDB::bind_method(D_METHOD("get_intensity"), &EretheaMotionBlur::get_intensity);
	ClassDB::bind_method(D_METHOD("set_samples", "samples"), &EretheaMotionBlur::set_samples);
	ClassDB::bind_method(D_METHOD("get_samples"), &EretheaMotionBlur::get_samples);
	ClassDB::bind_method(D_METHOD("set_max_length", "max_length"), &EretheaMotionBlur::set_max_length);
	ClassDB::bind_method(D_METHOD("get_max_length"), &EretheaMotionBlur::get_max_length);

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "intensity", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_intensity", "get_intensity");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "samples", PROPERTY_HINT_RANGE, "4,32,1"), "set_samples", "get_samples");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "max_length", PROPERTY_HINT_RANGE, "0.01,0.2,0.005"), "set_max_length", "get_max_length");
}

void EretheaMotionBlur::_validate_property(PropertyInfo &p_property) const {
	// The effect always runs after the transparent pass with the motion
	// vectors and the resolved depth, so these CompositorEffect settings
	// don't apply.
	if (p_property.name == "effect_callback_type" || p_property.name == "access_resolved_color" || p_property.name == "access_resolved_depth" || p_property.name == "needs_motion_vectors" || p_property.name == "needs_normal_roughness" || p_property.name == "needs_separate_specular") {
		p_property.usage = PROPERTY_USAGE_NONE;
	}
}

void EretheaMotionBlur::set_intensity(float p_intensity) {
	intensity = CLAMP(p_intensity, 0.0f, 1.0f);
}

float EretheaMotionBlur::get_intensity() const {
	return intensity;
}

void EretheaMotionBlur::set_samples(int p_samples) {
	samples = CLAMP(p_samples, 4, 32);
}

int EretheaMotionBlur::get_samples() const {
	return samples;
}

void EretheaMotionBlur::set_max_length(float p_max_length) {
	max_length = CLAMP(p_max_length, 0.01f, 0.2f);
}

float EretheaMotionBlur::get_max_length() const {
	return max_length;
}

void EretheaMotionBlur::_render(int p_effect_callback_type, const RenderData *p_render_data) {
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

	// The Mobile renderer has no motion vectors and can't write to its color
	// buffer from a compute shader.
	if (!rb->get_can_be_storage() || rb->get_base_data_format() != RD::DATA_FORMAT_R16G16B16A16_SFLOAT || !rb->has_velocity_buffer(false)) {
		WARN_PRINT_ONCE("EretheaMotionBlur needs the Forward+ renderer. It does nothing with the current renderer.");
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

	const Size2i size = rb->get_internal_size();
	const uint32_t view_count = rb->get_view_count();
	if (!rb->has_texture(context_name(), source_texture_name())) {
		rb->create_texture(context_name(), source_texture_name(), RD::DATA_FORMAT_R16G16B16A16_SFLOAT, RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_STORAGE_BIT, RD::TEXTURE_SAMPLES_1, size, view_count);
	}

	PushConstant push_constant = {};
	push_constant.size[0] = size.width;
	push_constant.size[1] = size.height;
	push_constant.intensity = intensity;
	push_constant.max_length = max_length;
	push_constant.samples = samples;
	push_constant.frame = uint32_t(RSG::rasterizer->get_frame_number());

	// The sky has no motion vectors: reproject it with the camera's motion,
	// using the same projection as the scene shaders (without the jitter,
	// which the motion vectors leave out too). Only for single view.
	const RenderSceneDataRD *scene_data = render_data->scene_data;
	if (scene_data != nullptr && view_count == 1) {
		Projection correction;
		correction.set_depth_correction(true);
		const Projection current = correction * scene_data->cam_projection;
		const Projection previous = correction * scene_data->prev_cam_projection;
		const Projection reprojection = previous * Projection(scene_data->prev_cam_transform.affine_inverse() * scene_data->cam_transform) * current.inverse();
		RendererRD::MaterialStorage::store_camera(reprojection, push_constant.reprojection);
		push_constant.use_reprojection = 1;
	} else {
		RendererRD::MaterialStorage::store_camera(Projection(), push_constant.reprojection);
	}

	RendererRD::CopyEffects *copy_effects = RendererRD::CopyEffects::get_singleton();
	UniformSetCacheRD *uniform_set_cache = UniformSetCacheRD::get_singleton();
	RendererRD::MaterialStorage *material_storage = RendererRD::MaterialStorage::get_singleton();
	ERR_FAIL_NULL(copy_effects);
	ERR_FAIL_NULL(uniform_set_cache);
	ERR_FAIL_NULL(material_storage);

	const RID linear_sampler = material_storage->sampler_rd_get_default(RSE::CANVAS_ITEM_TEXTURE_FILTER_LINEAR, RSE::CANVAS_ITEM_TEXTURE_REPEAT_DISABLED);
	const RID nearest_sampler = material_storage->sampler_rd_get_default(RSE::CANVAS_ITEM_TEXTURE_FILTER_NEAREST, RSE::CANVAS_ITEM_TEXTURE_REPEAT_DISABLED);
	const RID shader = render_resources->shader.version_get_shader(render_resources->version, 0);

	rd->draw_command_begin_label("Erethea Motion Blur");
	for (uint32_t view = 0; view < view_count; view++) {
		const RID color = rb->get_internal_texture(view);
		const RID source = rb->get_texture_slice(context_name(), source_texture_name(), view, 0);
		copy_effects->copy_to_rect(color, source, Rect2i(Point2i(), size));

		RD::Uniform u_source(RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE, 0, Vector<RID>({ linear_sampler, source }));
		RD::Uniform u_velocity(RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE, 1, Vector<RID>({ nearest_sampler, rb->get_velocity_buffer(false, view) }));
		RD::Uniform u_depth(RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE, 2, Vector<RID>({ nearest_sampler, rb->get_depth_texture(view) }));
		RD::Uniform u_dest(RD::UNIFORM_TYPE_IMAGE, 0, Vector<RID>({ color }));

		RD::ComputeListID compute_list = rd->compute_list_begin();
		rd->compute_list_bind_compute_pipeline(compute_list, render_resources->pipeline);
		rd->compute_list_bind_uniform_set(compute_list, uniform_set_cache->get_cache(shader, 0, u_source, u_velocity, u_depth), 0);
		rd->compute_list_bind_uniform_set(compute_list, uniform_set_cache->get_cache(shader, 1, u_dest), 1);
		rd->compute_list_set_push_constant(compute_list, &push_constant, sizeof(PushConstant));
		rd->compute_list_dispatch_threads(compute_list, size.width, size.height, 1);
		rd->compute_list_end();
	}
	rd->draw_command_end_label();
#endif // RD_ENABLED
}

void EretheaMotionBlur::_free_render_resources() {
#ifdef RD_ENABLED
	if (render_resources != nullptr) {
		render_resources->shader.version_free(render_resources->version);
		memdelete(render_resources);
		render_resources = nullptr;
	}
#endif // RD_ENABLED
}

void EretheaMotionBlur::finish() {
	RenderingServer *rs = RenderingServer::get_singleton();
	if (rs != nullptr) {
		rs->call_on_render_thread(callable_mp_static(&EretheaMotionBlur::_free_render_resources));
	}
}

EretheaMotionBlur::EretheaMotionBlur() {
	set_needs_motion_vectors(true);
	set_access_resolved_depth(true);

	RenderingServer *rs = RenderingServer::get_singleton();
	if (rs != nullptr && get_rid().is_valid()) {
		// CompositorEffect calls the _render_callback() virtual method, which
		// only scripts and extensions can override, so point the effect at
		// this class's own callback instead.
		rs->compositor_effect_set_callback(get_rid(), RSE::COMPOSITOR_EFFECT_CALLBACK_TYPE_POST_TRANSPARENT, callable_mp(this, &EretheaMotionBlur::_render));
	}
}
