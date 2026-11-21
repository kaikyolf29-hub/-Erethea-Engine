/**************************************************************************/
/*  erethea_upscale_inputs.cpp                                            */
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

#include "erethea_upscale_inputs.h"

#include "servers/rendering/renderer_rd/storage_rd/material_storage.h"
#include "servers/rendering/renderer_rd/storage_rd/render_data_rd.h"
#include "servers/rendering/renderer_rd/storage_rd/render_scene_buffers_rd.h"
#include "servers/rendering/renderer_rd/storage_rd/render_scene_data_rd.h"
#include "servers/rendering/renderer_rd/uniform_set_cache_rd.h"
#include "servers/rendering/rendering_device.h"

namespace {

const StringName &depth_name() {
	static const StringName name = "depth";
	return name;
}

const StringName &velocity_name() {
	static const StringName name = "velocity";
	return name;
}

const StringName &reactive_name() {
	static const StringName name = "reactive";
	return name;
}

struct PushConstant {
	float reprojection[16];
	int32_t size[2];
	float pad[2];
};

} // namespace

EretheaUpscaleInputs::Textures EretheaUpscaleInputs::prepare(const Ref<RenderSceneBuffersRD> &p_render_buffers, RenderDataRD *p_render_data, const StringName &p_context) {
	Ref<RenderSceneBuffersRD> rb = p_render_buffers;
	RenderingDevice *rd = RenderingDevice::get_singleton();

	if (shader_version.is_null()) {
		shader.initialize(Vector<String>({ "" }));
		shader_version = shader.version_create();
		pipeline = rd->compute_pipeline_create(shader.version_get_shader(shader_version, 0));
	}

	const Size2i internal_size = rb->get_internal_size();
	const uint32_t usage = RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_STORAGE_BIT;
	if (!rb->has_texture(p_context, depth_name())) {
		rb->create_texture(p_context, depth_name(), RD::DATA_FORMAT_R32_SFLOAT, usage, RD::TEXTURE_SAMPLES_1, internal_size, 1);
		rb->create_texture(p_context, velocity_name(), RD::DATA_FORMAT_R16G16_SFLOAT, usage, RD::TEXTURE_SAMPLES_1, internal_size, 1);
		rb->create_texture(p_context, reactive_name(), RD::DATA_FORMAT_R8_UNORM, usage, RD::TEXTURE_SAMPLES_1, internal_size, 1);
	}

	Textures textures;
	textures.depth = rb->get_texture(p_context, depth_name());
	textures.velocity = rb->get_texture(p_context, velocity_name());
	textures.reactive = rb->get_texture(p_context, reactive_name());

	const RenderSceneDataRD *scene_data = p_render_data->scene_data;

	PushConstant push_constant = {};
	push_constant.size[0] = internal_size.width;
	push_constant.size[1] = internal_size.height;
	{
		// The same reprojection as Godot's FSR 2.2.
		Projection correction;
		correction.set_depth_correction(true, true, false);
		const Projection reprojection = (correction * scene_data->prev_cam_projection) * scene_data->prev_cam_transform.affine_inverse() * scene_data->cam_transform * (correction * scene_data->cam_projection).inverse();
		RendererRD::MaterialStorage::store_camera(reprojection, push_constant.reprojection);
	}

	UniformSetCacheRD *uniform_set_cache = UniformSetCacheRD::get_singleton();
	RendererRD::MaterialStorage *material_storage = RendererRD::MaterialStorage::get_singleton();
	const RID nearest_sampler = material_storage->sampler_rd_get_default(RSE::CANVAS_ITEM_TEXTURE_FILTER_NEAREST, RSE::CANVAS_ITEM_TEXTURE_REPEAT_DISABLED);
	const RID shader_rid = shader.version_get_shader(shader_version, 0);

	RD::Uniform u_color(RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE, 0, Vector<RID>({ nearest_sampler, rb->get_internal_texture(0) }));
	RD::Uniform u_depth(RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE, 1, Vector<RID>({ nearest_sampler, rb->get_depth_texture(0) }));
	RD::Uniform u_velocity(RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE, 2, Vector<RID>({ nearest_sampler, rb->get_velocity_buffer(false, 0) }));
	RD::Uniform u_dest_depth(RD::UNIFORM_TYPE_IMAGE, 0, Vector<RID>({ textures.depth }));
	RD::Uniform u_dest_velocity(RD::UNIFORM_TYPE_IMAGE, 1, Vector<RID>({ textures.velocity }));
	RD::Uniform u_dest_reactive(RD::UNIFORM_TYPE_IMAGE, 2, Vector<RID>({ textures.reactive }));

	RD::ComputeListID compute_list = rd->compute_list_begin();
	rd->compute_list_bind_compute_pipeline(compute_list, pipeline);
	rd->compute_list_bind_uniform_set(compute_list, uniform_set_cache->get_cache(shader_rid, 0, u_color, u_depth, u_velocity), 0);
	rd->compute_list_bind_uniform_set(compute_list, uniform_set_cache->get_cache(shader_rid, 1, u_dest_depth, u_dest_velocity, u_dest_reactive), 1);
	rd->compute_list_set_push_constant(compute_list, &push_constant, sizeof(PushConstant));
	rd->compute_list_dispatch_threads(compute_list, internal_size.width, internal_size.height, 1);
	rd->compute_list_end();

	return textures;
}

void EretheaUpscaleInputs::finish() {
	if (shader_version.is_valid()) {
		shader.version_free(shader_version);
		shader_version = RID();
		pipeline = RID();
	}
}

EretheaUpscaleInputs::~EretheaUpscaleInputs() {
	finish();
}
