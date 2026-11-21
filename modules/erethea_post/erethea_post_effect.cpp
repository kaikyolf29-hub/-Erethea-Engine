/**************************************************************************/
/*  erethea_post_effect.cpp                                               */
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

#include "erethea_post_effect.h"

#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "servers/rendering/rendering_server.h"

#ifdef RD_ENABLED
#include "erethea_post_effect.glsl.gen.h"

#include "servers/rendering/renderer_compositor.h"
#include "servers/rendering/renderer_rd/effects/copy_effects.h"
#include "servers/rendering/renderer_rd/storage_rd/material_storage.h"
#include "servers/rendering/renderer_rd/storage_rd/render_data_rd.h"
#include "servers/rendering/renderer_rd/storage_rd/render_scene_buffers_rd.h"
#include "servers/rendering/renderer_rd/uniform_set_cache_rd.h"
#include "servers/rendering/rendering_server_globals.h"

namespace {

struct PushConstant {
	int32_t size[2];
	float vignette_intensity;
	float vignette_smoothness;

	float chromatic_aberration;
	float grain_intensity;
	float grain_size;
	uint32_t frame;
};

// Shared by all instances. Only touched on the rendering thread.
struct RenderResources {
	EretheaPostEffectShaderRD shader;
	RID version;
	RID pipeline;
};

RenderResources *render_resources = nullptr;

const StringName &context_name() {
	static const StringName name = "erethea_post";
	return name;
}

const StringName &source_texture_name() {
	static const StringName name = "source";
	return name;
}

} // namespace
#endif // RD_ENABLED

void EretheaPostEffect::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_vignette_intensity", "intensity"), &EretheaPostEffect::set_vignette_intensity);
	ClassDB::bind_method(D_METHOD("get_vignette_intensity"), &EretheaPostEffect::get_vignette_intensity);
	ClassDB::bind_method(D_METHOD("set_vignette_smoothness", "smoothness"), &EretheaPostEffect::set_vignette_smoothness);
	ClassDB::bind_method(D_METHOD("get_vignette_smoothness"), &EretheaPostEffect::get_vignette_smoothness);
	ClassDB::bind_method(D_METHOD("set_chromatic_aberration", "amount"), &EretheaPostEffect::set_chromatic_aberration);
	ClassDB::bind_method(D_METHOD("get_chromatic_aberration"), &EretheaPostEffect::get_chromatic_aberration);
	ClassDB::bind_method(D_METHOD("set_film_grain_intensity", "intensity"), &EretheaPostEffect::set_film_grain_intensity);
	ClassDB::bind_method(D_METHOD("get_film_grain_intensity"), &EretheaPostEffect::get_film_grain_intensity);
	ClassDB::bind_method(D_METHOD("set_film_grain_size", "size"), &EretheaPostEffect::set_film_grain_size);
	ClassDB::bind_method(D_METHOD("get_film_grain_size"), &EretheaPostEffect::get_film_grain_size);
	ClassDB::bind_method(D_METHOD("is_active"), &EretheaPostEffect::is_active);

	ADD_GROUP("Vignette", "vignette_");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "vignette_intensity", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_vignette_intensity", "get_vignette_intensity");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "vignette_smoothness", PROPERTY_HINT_RANGE, "0.05,1,0.01"), "set_vignette_smoothness", "get_vignette_smoothness");

	ADD_GROUP("Chromatic Aberration", "");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "chromatic_aberration", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_chromatic_aberration", "get_chromatic_aberration");

	ADD_GROUP("Film Grain", "film_grain_");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "film_grain_intensity", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_film_grain_intensity", "get_film_grain_intensity");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "film_grain_size", PROPERTY_HINT_RANGE, "1,4,0.1,suffix:px"), "set_film_grain_size", "get_film_grain_size");
}

void EretheaPostEffect::_validate_property(PropertyInfo &p_property) const {
	// The effect always runs after the transparent pass and needs none of
	// the extra buffers, so these CompositorEffect settings don't apply.
	if (p_property.name == "effect_callback_type" || p_property.name == "access_resolved_color" || p_property.name == "access_resolved_depth" || p_property.name == "needs_motion_vectors" || p_property.name == "needs_normal_roughness" || p_property.name == "needs_separate_specular") {
		p_property.usage = PROPERTY_USAGE_NONE;
	}
}

void EretheaPostEffect::set_vignette_intensity(float p_intensity) {
	vignette_intensity = CLAMP(p_intensity, 0.0f, 1.0f);
}

float EretheaPostEffect::get_vignette_intensity() const {
	return vignette_intensity;
}

void EretheaPostEffect::set_vignette_smoothness(float p_smoothness) {
	vignette_smoothness = CLAMP(p_smoothness, 0.05f, 1.0f);
}

float EretheaPostEffect::get_vignette_smoothness() const {
	return vignette_smoothness;
}

void EretheaPostEffect::set_chromatic_aberration(float p_amount) {
	chromatic_aberration = CLAMP(p_amount, 0.0f, 1.0f);
}

float EretheaPostEffect::get_chromatic_aberration() const {
	return chromatic_aberration;
}

void EretheaPostEffect::set_film_grain_intensity(float p_intensity) {
	film_grain_intensity = CLAMP(p_intensity, 0.0f, 1.0f);
}

float EretheaPostEffect::get_film_grain_intensity() const {
	return film_grain_intensity;
}

void EretheaPostEffect::set_film_grain_size(float p_size) {
	film_grain_size = CLAMP(p_size, 1.0f, 4.0f);
}

float EretheaPostEffect::get_film_grain_size() const {
	return film_grain_size;
}

bool EretheaPostEffect::is_active() const {
	return vignette_intensity > 0.0f || chromatic_aberration > 0.0f || film_grain_intensity > 0.0f;
}

void EretheaPostEffect::_render(int p_effect_callback_type, const RenderData *p_render_data) {
#ifdef RD_ENABLED
	if (p_effect_callback_type != RSE::COMPOSITOR_EFFECT_CALLBACK_TYPE_POST_TRANSPARENT || !is_active()) {
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
		WARN_PRINT_ONCE("EretheaPostEffect needs the Forward+ renderer. It does nothing with the current renderer.");
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
	push_constant.vignette_intensity = vignette_intensity;
	push_constant.vignette_smoothness = vignette_smoothness;
	// At 1.0, red and blue are 1% of the screen width apart in the corners.
	push_constant.chromatic_aberration = chromatic_aberration * 0.02f;
	push_constant.grain_intensity = film_grain_intensity * 0.5f;
	push_constant.grain_size = film_grain_size;
	push_constant.frame = uint32_t(RSG::rasterizer->get_frame_number());

	RendererRD::CopyEffects *copy_effects = RendererRD::CopyEffects::get_singleton();
	UniformSetCacheRD *uniform_set_cache = UniformSetCacheRD::get_singleton();
	RendererRD::MaterialStorage *material_storage = RendererRD::MaterialStorage::get_singleton();
	ERR_FAIL_NULL(copy_effects);
	ERR_FAIL_NULL(uniform_set_cache);
	ERR_FAIL_NULL(material_storage);

	const RID sampler = material_storage->sampler_rd_get_default(RSE::CANVAS_ITEM_TEXTURE_FILTER_LINEAR, RSE::CANVAS_ITEM_TEXTURE_REPEAT_DISABLED);
	const RID shader = render_resources->shader.version_get_shader(render_resources->version, 0);

	rd->draw_command_begin_label("Erethea Post Effect");
	for (uint32_t view = 0; view < view_count; view++) {
		const RID color = rb->get_internal_texture(view);
		const RID source = rb->get_texture_slice(context_name(), source_texture_name(), view, 0);
		copy_effects->copy_to_rect(color, source, Rect2i(Point2i(), size));

		RD::Uniform u_source(RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE, 0, Vector<RID>({ sampler, source }));
		RD::Uniform u_dest(RD::UNIFORM_TYPE_IMAGE, 0, Vector<RID>({ color }));

		RD::ComputeListID compute_list = rd->compute_list_begin();
		rd->compute_list_bind_compute_pipeline(compute_list, render_resources->pipeline);
		rd->compute_list_bind_uniform_set(compute_list, uniform_set_cache->get_cache(shader, 0, u_source), 0);
		rd->compute_list_bind_uniform_set(compute_list, uniform_set_cache->get_cache(shader, 1, u_dest), 1);
		rd->compute_list_set_push_constant(compute_list, &push_constant, sizeof(PushConstant));
		rd->compute_list_dispatch_threads(compute_list, size.width, size.height, 1);
		rd->compute_list_end();
	}
	rd->draw_command_end_label();
#endif // RD_ENABLED
}

void EretheaPostEffect::_free_render_resources() {
#ifdef RD_ENABLED
	if (render_resources != nullptr) {
		render_resources->shader.version_free(render_resources->version);
		memdelete(render_resources);
		render_resources = nullptr;
	}
#endif // RD_ENABLED
}

void EretheaPostEffect::finish() {
	RenderingServer *rs = RenderingServer::get_singleton();
	if (rs != nullptr) {
		rs->call_on_render_thread(callable_mp_static(&EretheaPostEffect::_free_render_resources));
	}
}

EretheaPostEffect::EretheaPostEffect() {
	RenderingServer *rs = RenderingServer::get_singleton();
	if (rs != nullptr && get_rid().is_valid()) {
		// CompositorEffect calls the _render_callback() virtual method, which
		// only scripts and extensions can override, so point the effect at
		// this class's own callback instead.
		rs->compositor_effect_set_callback(get_rid(), RSE::COMPOSITOR_EFFECT_CALLBACK_TYPE_POST_TRANSPARENT, callable_mp(this, &EretheaPostEffect::_render));
	}
}
