/**************************************************************************/
/*  erethea_hair_strands.cpp                                              */
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


#include "erethea_hair_strands.h"

#include "erethea_hair_builder.h"

#include "core/object/callable_mp.h"
#include "scene/resources/mesh.h"

Ref<Shader> EretheaHairStrands::shader;

// The ribbons are opened in view space, so they face the camera whatever the
// pose of the skeleton (the skinning has already moved VERTEX and TANGENT).
// Transparency is alpha hash, which needs no sorting.
String EretheaHairStrands::get_shader_code() {
	return R"(shader_type spatial;
render_mode skip_vertex_transform, cull_disabled, depth_draw_opaque;

uniform float strand_width = 0.0015;
uniform float tip_width = 0.25;
uniform vec3 color_root : source_color = vec3(0.075, 0.045, 0.03);
uniform vec3 color_tip : source_color = vec3(0.2, 0.12, 0.08);
uniform float roughness_value : hint_range(0.0, 1.0) = 0.45;
uniform float specular_primary : hint_range(0.0, 1.0) = 0.06;
uniform float specular_secondary : hint_range(0.0, 1.0) = 0.1;
uniform float specular_shift_primary = -0.08;
uniform float specular_shift_secondary = 0.12;
uniform float backlight_amount : hint_range(0.0, 1.0) = 0.4;

varying vec3 strand_dir;
varying float side_v;
varying float root_to_tip;
varying float strand_rand;

void vertex() {
	vec3 pos = (MODELVIEW_MATRIX * vec4(VERTEX, 1.0)).xyz;
	vec3 dir = normalize((MODELVIEW_MATRIX * vec4(TANGENT, 0.0)).xyz);
	vec3 view_dir = PROJECTION_MATRIX[3][3] == 1.0 ? vec3(0.0, 0.0, -1.0) : normalize(pos);
	vec3 side = cross(dir, view_dir);
	float side_len = length(side);
	// A strand that points at the camera has no side: any perpendicular will do.
	side = side_len > 0.0001 ? side / side_len : normalize(cross(dir, vec3(0.0, 1.0, 0.0)) + vec3(0.0001, 0.0, 0.0));
	float half_width = 0.5 * strand_width * mix(1.0, tip_width, UV.y);
	VERTEX = pos + side * (UV.x * half_width);

	// The normal of the hair's shell, turned a little towards the viewer so a
	// ribbon seen from the side doesn't go dark.
	vec3 shell = normalize(MODELVIEW_NORMAL_MATRIX * NORMAL);
	NORMAL = normalize(shell - view_dir * 0.3);
	TANGENT = dir;
	BINORMAL = normalize(cross(NORMAL, dir));

	strand_dir = dir;
	side_v = UV.x;
	root_to_tip = UV.y;
	strand_rand = UV2.x;
}

void fragment() {
	ALBEDO = mix(color_root, color_tip, root_to_tip) * (0.85 + 0.3 * strand_rand);
	// Soft edges across the ribbon, dissolved by the alpha hash.
	ALPHA = clamp((1.0 - abs(side_v)) * 2.5, 0.0, 1.0);
	ALPHA_HASH_SCALE = 1.0;
	// The roots sit under the strands above them.
	AO = mix(0.5, 1.0, root_to_tip);
	AO_LIGHT_AFFECT = 0.5;
	ROUGHNESS = roughness_value;
	SPECULAR = 0.0;
	BACKLIGHT = ALBEDO * backlight_amount;
}

float kajiya(vec3 t, vec3 h, float exponent) {
	float th = dot(t, h);
	float fade = smoothstep(-1.0, 0.0, th);
	return fade * pow(sqrt(max(0.0, 1.0 - th * th)), exponent);
}

void light() {
	vec3 h = normalize(LIGHT + VIEW);
	float nl = dot(NORMAL, LIGHT);
	// Wrapped diffuse: hair is a volume, the light goes around it.
	DIFFUSE_LIGHT += max((nl + 0.45) / 1.45, 0.0) * ATTENUATION * LIGHT_COLOR / PI;
	// Each strand shifts the highlights by a different amount.
	float shift = strand_rand - 0.5;
	vec3 t1 = normalize(strand_dir + NORMAL * (specular_shift_primary + 0.3 * shift));
	vec3 t2 = normalize(strand_dir + NORMAL * (specular_shift_secondary + 0.3 * shift));
	// Highlights only where the light hits, weaker at the roots.
	float vis = smoothstep(0.0, 0.35, nl) * ATTENUATION * mix(0.25, 1.0, root_to_tip) * (0.55 + 0.9 * strand_rand);
	SPECULAR_LIGHT += (kajiya(t1, h, 280.0) * specular_primary + kajiya(t2, h, 60.0) * specular_secondary * ALBEDO) * vis * LIGHT_COLOR / PI;
}
)";
}

void EretheaHairStrands::finish() {
	shader.unref();
}

void EretheaHairStrands::_queue_rebuild() {
	if (rebuild_queued) {
		return;
	}
	rebuild_queued = true;
	callable_mp(this, &EretheaHairStrands::_rebuild).call_deferred();
}

void EretheaHairStrands::_rebuild() {
	rebuild_queued = false;
	EretheaHairBuilder::Result result = EretheaHairBuilder::build(source_mesh, width, strand_fraction);
	strand_count = result.stats.strands;
	source_strand_count = result.stats.source_strands;

	if (ribbons.is_null()) {
		ribbons.instantiate();
	}
	ribbons->clear_surfaces();
	for (const EretheaHairBuilder::Surface &surface : result.surfaces) {
		ribbons->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, surface.arrays, Array(), Dictionary(), surface.flags);
		ribbons->surface_set_material(ribbons->get_surface_count() - 1, material);
	}
	if (!result.surfaces.is_empty()) {
		ribbons->set_custom_aabb(result.aabb);
	}
	// The mesh's "changed" signal updates the instance when the surfaces change.
	if (get_mesh() != ribbons) {
		set_mesh(ribbons);
	}
}

void EretheaHairStrands::_update_material() {
	if (material.is_null()) {
		return;
	}
	material->set_shader_parameter("strand_width", width);
	material->set_shader_parameter("tip_width", tip_width);
	material->set_shader_parameter("color_root", color_root);
	material->set_shader_parameter("color_tip", color_tip);
	material->set_shader_parameter("roughness_value", roughness);
	material->set_shader_parameter("specular_primary", specular_primary);
	material->set_shader_parameter("specular_secondary", specular_secondary);
	material->set_shader_parameter("specular_shift_primary", specular_shift_primary);
	material->set_shader_parameter("specular_shift_secondary", specular_shift_secondary);
	material->set_shader_parameter("backlight_amount", backlight);
}

void EretheaHairStrands::rebuild() {
	_rebuild();
}

void EretheaHairStrands::_validate_property(PropertyInfo &p_property) const {
	// The mesh is made from source_mesh: it isn't saved nor edited by hand.
	if (p_property.name == "mesh") {
		p_property.usage = PROPERTY_USAGE_NONE;
	}
}

void EretheaHairStrands::set_source_mesh(const Ref<Mesh> &p_mesh) {
	source_mesh = p_mesh;
	_queue_rebuild();
}

Ref<Mesh> EretheaHairStrands::get_source_mesh() const {
	return source_mesh;
}

void EretheaHairStrands::set_width(float p_width) {
	width = MAX(p_width, 0.0f);
	_update_material();
	_queue_rebuild(); // The AABB depends on it.
}

float EretheaHairStrands::get_width() const {
	return width;
}

void EretheaHairStrands::set_tip_width(float p_tip_width) {
	tip_width = CLAMP(p_tip_width, 0.0f, 1.0f);
	_update_material();
}

float EretheaHairStrands::get_tip_width() const {
	return tip_width;
}

void EretheaHairStrands::set_strand_fraction(float p_fraction) {
	strand_fraction = CLAMP(p_fraction, 0.0f, 1.0f);
	_queue_rebuild();
}

float EretheaHairStrands::get_strand_fraction() const {
	return strand_fraction;
}

#define ERETHEA_HAIR_SETTER(m_name, m_type, m_expr) \
	void EretheaHairStrands::set_##m_name(m_type p_value) { \
		m_name = m_expr; \
		_update_material(); \
	} \
	m_type EretheaHairStrands::get_##m_name() const { \
		return m_name; \
	}

ERETHEA_HAIR_SETTER(color_root, const Color &, p_value)
ERETHEA_HAIR_SETTER(color_tip, const Color &, p_value)
ERETHEA_HAIR_SETTER(roughness, float, CLAMP(p_value, 0.0f, 1.0f))
ERETHEA_HAIR_SETTER(specular_primary, float, CLAMP(p_value, 0.0f, 1.0f))
ERETHEA_HAIR_SETTER(specular_secondary, float, CLAMP(p_value, 0.0f, 1.0f))
ERETHEA_HAIR_SETTER(specular_shift_primary, float, p_value)
ERETHEA_HAIR_SETTER(specular_shift_secondary, float, p_value)
ERETHEA_HAIR_SETTER(backlight, float, CLAMP(p_value, 0.0f, 1.0f))

#undef ERETHEA_HAIR_SETTER

int EretheaHairStrands::get_strand_count() const {
	return strand_count;
}

int EretheaHairStrands::get_source_strand_count() const {
	return source_strand_count;
}

void EretheaHairStrands::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_source_mesh", "mesh"), &EretheaHairStrands::set_source_mesh);
	ClassDB::bind_method(D_METHOD("get_source_mesh"), &EretheaHairStrands::get_source_mesh);
	ClassDB::bind_method(D_METHOD("set_width", "width"), &EretheaHairStrands::set_width);
	ClassDB::bind_method(D_METHOD("get_width"), &EretheaHairStrands::get_width);
	ClassDB::bind_method(D_METHOD("set_tip_width", "tip_width"), &EretheaHairStrands::set_tip_width);
	ClassDB::bind_method(D_METHOD("get_tip_width"), &EretheaHairStrands::get_tip_width);
	ClassDB::bind_method(D_METHOD("set_strand_fraction", "fraction"), &EretheaHairStrands::set_strand_fraction);
	ClassDB::bind_method(D_METHOD("get_strand_fraction"), &EretheaHairStrands::get_strand_fraction);
	ClassDB::bind_method(D_METHOD("set_color_root", "color"), &EretheaHairStrands::set_color_root);
	ClassDB::bind_method(D_METHOD("get_color_root"), &EretheaHairStrands::get_color_root);
	ClassDB::bind_method(D_METHOD("set_color_tip", "color"), &EretheaHairStrands::set_color_tip);
	ClassDB::bind_method(D_METHOD("get_color_tip"), &EretheaHairStrands::get_color_tip);
	ClassDB::bind_method(D_METHOD("set_roughness", "roughness"), &EretheaHairStrands::set_roughness);
	ClassDB::bind_method(D_METHOD("get_roughness"), &EretheaHairStrands::get_roughness);
	ClassDB::bind_method(D_METHOD("set_specular_primary", "value"), &EretheaHairStrands::set_specular_primary);
	ClassDB::bind_method(D_METHOD("get_specular_primary"), &EretheaHairStrands::get_specular_primary);
	ClassDB::bind_method(D_METHOD("set_specular_secondary", "value"), &EretheaHairStrands::set_specular_secondary);
	ClassDB::bind_method(D_METHOD("get_specular_secondary"), &EretheaHairStrands::get_specular_secondary);
	ClassDB::bind_method(D_METHOD("set_specular_shift_primary", "value"), &EretheaHairStrands::set_specular_shift_primary);
	ClassDB::bind_method(D_METHOD("get_specular_shift_primary"), &EretheaHairStrands::get_specular_shift_primary);
	ClassDB::bind_method(D_METHOD("set_specular_shift_secondary", "value"), &EretheaHairStrands::set_specular_shift_secondary);
	ClassDB::bind_method(D_METHOD("get_specular_shift_secondary"), &EretheaHairStrands::get_specular_shift_secondary);
	ClassDB::bind_method(D_METHOD("set_backlight", "value"), &EretheaHairStrands::set_backlight);
	ClassDB::bind_method(D_METHOD("get_backlight"), &EretheaHairStrands::get_backlight);
	ClassDB::bind_method(D_METHOD("get_strand_count"), &EretheaHairStrands::get_strand_count);
	ClassDB::bind_method(D_METHOD("get_source_strand_count"), &EretheaHairStrands::get_source_strand_count);
	ClassDB::bind_method(D_METHOD("rebuild"), &EretheaHairStrands::rebuild);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "source_mesh", PROPERTY_HINT_RESOURCE_TYPE, "Mesh"), "set_source_mesh", "get_source_mesh");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "strand_fraction", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_strand_fraction", "get_strand_fraction");

	ADD_GROUP("Ribbon", "");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "width", PROPERTY_HINT_RANGE, "0,0.05,0.0001,or_greater,suffix:m"), "set_width", "get_width");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "tip_width", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_tip_width", "get_tip_width");

	ADD_GROUP("Color", "color_");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "color_root", PROPERTY_HINT_COLOR_NO_ALPHA), "set_color_root", "get_color_root");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "color_tip", PROPERTY_HINT_COLOR_NO_ALPHA), "set_color_tip", "get_color_tip");

	ADD_GROUP("Shine", "");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "roughness", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_roughness", "get_roughness");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "specular_primary", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_specular_primary", "get_specular_primary");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "specular_secondary", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_specular_secondary", "get_specular_secondary");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "specular_shift_primary", PROPERTY_HINT_RANGE, "-1,1,0.01"), "set_specular_shift_primary", "get_specular_shift_primary");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "specular_shift_secondary", PROPERTY_HINT_RANGE, "-1,1,0.01"), "set_specular_shift_secondary", "get_specular_shift_secondary");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "backlight", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_backlight", "get_backlight");
}

EretheaHairStrands::EretheaHairStrands() {
	if (shader.is_null()) {
		shader.instantiate();
		shader->set_code(get_shader_code());
	}
	material.instantiate();
	material->set_shader(shader);
	_update_material();
}
