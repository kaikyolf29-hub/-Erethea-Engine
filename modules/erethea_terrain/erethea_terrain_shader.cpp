/**************************************************************************/
/*  erethea_terrain_shader.cpp                                            */
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

#include "erethea_terrain_shader.h"

namespace EretheaTerrainShader {

// Pieces of the shader, so that no string literal passes the 16 KB that some
// compilers accept.

// The inputs, the height map and the vertex stage. The vertices of a level's
// mesh are in meters, around the origin: the level's center is added here, and
// the height comes from the height map, with the same bilinear interpolation as
// EretheaTerrainData.get_height().
static const char *shader_inputs = R"(
shader_type spatial;
render_mode blend_mix, depth_draw_opaque, cull_back, diffuse_burley, specular_schlick_ggx;

uniform sampler2D height_map : filter_nearest, repeat_disable;
uniform sampler2D control_map : filter_nearest, repeat_disable;
uniform sampler2DArray albedo_height_array : source_color, filter_linear_mipmap_anisotropic, repeat_enable;
uniform sampler2DArray normal_roughness_array : filter_linear_mipmap_anisotropic, repeat_enable;
uniform bool has_albedo_array = false;
uniform bool has_normal_array = false;
uniform int layer_count = 1;

// Vertices of the maps, and the distance between them.
uniform ivec2 map_size = ivec2(2, 2);
uniform float vertex_spacing = 1.0;

// The clipmap level that this material draws.
uniform vec2 level_center = vec2(0.0);
uniform float level_spacing = 1.0;
uniform float skirt_depth = 1.0;
// The square that the next finer level covers. Not drawn here. 0 for no hole.
uniform vec2 hole_center = vec2(0.0);
uniform float hole_half_extent = 0.0;

uniform float uv_scale = 0.1;
uniform bool detiling = false;
uniform bool triplanar = false;
uniform float triplanar_slope_cos = 0.7;

varying vec3 v_local;

float fetch_height(ivec2 p) {
	return texelFetch(height_map, clamp(p, ivec2(0), map_size - 1), 0).r;
}

float height_at(vec2 xz) {
	vec2 g = clamp(xz / vertex_spacing + vec2(map_size - 1) * 0.5, vec2(0.0), vec2(map_size - 1));
	ivec2 p = min(ivec2(floor(g)), max(map_size - 2, ivec2(0)));
	vec2 f = g - vec2(p);
	float h00 = fetch_height(p);
	float h10 = fetch_height(p + ivec2(1, 0));
	float h01 = fetch_height(p + ivec2(0, 1));
	float h11 = fetch_height(p + ivec2(1, 1));
	return mix(mix(h00, h10, f.x), mix(h01, h11, f.x), f.y);
}

vec4 hash4(vec2 p) {
	vec4 q = vec4(dot(p, vec2(127.1, 311.7)), dot(p, vec2(269.5, 183.3)), dot(p, vec2(419.2, 371.9)), dot(p, vec2(83.7, 457.1)));
	return fract(sin(q) * 43758.5453);
}

void vertex() {
	vec2 xz = VERTEX.xz + level_center;
	// The skirt vertices (UV x = 1) hang below the edge of the level.
	float h = height_at(xz) - UV.x * skirt_depth;
	v_local = vec3(xz.x, h, xz.y);
	VERTEX = v_local;
}
)";

// Detiling, and the sampling of the material layers.
static const char *shader_sampling = R"(
// The sampling code of each layered texture is the same, with the name of the
// texture in the place of the word in capitals.
vec4 sample_TEXTURE(int layer, vec2 uv, vec2 dx, vec2 dy) {
	float l = float(layer);
	if (!detiling) {
		return textureGrad(TEXTURE, vec3(uv, l), dx, dy);
	}
	// Each tile has a center, with a random rotation and offset. A sample is the
	// mix of the 4 nearest tiles, which breaks the repetition of the texture.
	vec2 t = uv - 0.5;
	vec2 base = floor(t);
	vec2 f = fract(t);
	vec2 w2 = f * f * (3.0 - 2.0 * f);
	vec4 sum = vec4(0.0);
	for (int j = 0; j < 2; j++) {
		for (int i = 0; i < 2; i++) {
			vec2 id = base + vec2(float(i), float(j));
			vec4 r = hash4(id);
			float a = r.z * 6.2831853;
			mat2 rot = mat2(vec2(cos(a), sin(a)), vec2(-sin(a), cos(a)));
			vec2 center = id + 0.5;
			vec2 uv_tile = rot * (uv - center) + center + r.xy;
			float w = (i == 0 ? 1.0 - w2.x : w2.x) * (j == 0 ? 1.0 - w2.y : w2.y);
			sum += w * textureGrad(TEXTURE, vec3(uv_tile, l), rot * dx, rot * dy);
		}
	}
	return sum;
}
)";

// The fragment stage.
static const char *shader_fragment = R"(
struct Projection {
	vec2 uv;
	vec2 dx;
	vec2 dy;
};

// The weight of the overlay material B over the base material A at a control
// map texel, as the mix of the pair A and B would see it.
float overlay_weight(vec4 c, int a, int b, float fallback) {
	int base = int(floor(c.r * 255.0 + 0.5));
	int overlay = int(floor(c.g * 255.0 + 0.5));
	if (base == a && overlay == b) {
		return c.b;
	}
	if (base == b && overlay == a) {
		return 1.0 - c.b;
	}
	if (base == a || overlay == a) {
		return 0.0;
	}
	if (base == b || overlay == b) {
		return 1.0;
	}
	return fallback;
}

// A layer of the material: its albedo, height, roughness and its normal, from
// the planar projections that weights w asks for. g is the normal of the
// surface, without the normal maps.
void surface(int id, vec3 g, vec3 w, Projection px, Projection py, Projection pz, out vec4 albedo_height, out vec3 normal, out float roughness) {
	albedo_height = vec4(0.0);
	normal = vec3(0.0);
	roughness = 0.0;
	vec3 sgn = vec3(g.x < 0.0 ? -1.0 : 1.0, g.y < 0.0 ? -1.0 : 1.0, g.z < 0.0 ? -1.0 : 1.0);
	for (int axis = 0; axis < 3; axis++) {
		float weight = axis == 0 ? w.x : (axis == 1 ? w.y : w.z);
		if (weight < 0.001) {
			continue;
		}
		Projection p = pz;
		if (axis == 0) {
			p = px;
		} else if (axis == 1) {
			p = py;
		}
		vec4 ah = has_albedo_array ? sample_albedo_height_array(id, p.uv, p.dx, p.dy) : vec4(0.5, 0.5, 0.5, 0.5);
		albedo_height += ah * weight;
		vec3 tn = vec3(0.0, 0.0, 1.0);
		float rough = 0.9;
		if (has_normal_array) {
			vec4 nr = sample_normal_roughness_array(id, p.uv, p.dx, p.dy);
			tn.xy = nr.xy * 2.0 - 1.0;
			tn.z = sqrt(max(1.0 - dot(tn.xy, tn.xy), 0.0));
			rough = nr.a;
		}
		roughness += rough * weight;
		// Whiteout blend of the tangent space normal with the surface normal.
		// The images are drawn with v going down, so the side projections use
		// -y for v, and the top one uses z.
		vec3 n;
		if (axis == 0) {
			n = vec3(abs(tn.z) * g.x, tn.y + g.y, tn.x * sgn.x + g.z);
		} else if (axis == 1) {
			n = vec3(tn.x * sgn.y + g.x, abs(tn.z) * g.y, -tn.y + g.z);
		} else {
			n = vec3(tn.x * -sgn.z + g.x, tn.y + g.y, abs(tn.z) * g.z);
		}
		normal += n * weight;
	}
	normal = normalize(normal);
}

void fragment() {
	vec2 xz = v_local.xz;
	// The planar projections, with their derivatives computed before any branch
	// and before a discard, after which derivatives are not defined.
	vec3 p3 = v_local * uv_scale;
	Projection py = Projection(p3.xz, dFdx(p3.xz), dFdy(p3.xz));
	vec2 uv_x = vec2(p3.z, -p3.y);
	Projection px = Projection(uv_x, dFdx(uv_x), dFdy(uv_x));
	vec2 uv_z = vec2(p3.x, -p3.y);
	Projection pz = Projection(uv_z, dFdx(uv_z), dFdy(uv_z));

	// Nothing past the edge of the terrain.
	vec2 half_size = vec2(map_size - 1) * 0.5 * vertex_spacing + 0.001 * level_spacing;
	if (abs(xz.x) > half_size.x || abs(xz.y) > half_size.y) {
		discard;
	}
	// The finer level draws the hole. It is a bit smaller here so that a
	// rounding error doesn't open a crack between the levels.
	if (hole_half_extent > 0.0) {
		vec2 d = abs(xz - hole_center);
		float inner = hole_half_extent - 0.05 * level_spacing;
		if (d.x < inner && d.y < inner) {
			discard;
		}
	}

	// The normal of the surface, from the height map, one step of the level wide.
	float s = level_spacing;
	float hl = height_at(xz - vec2(s, 0.0));
	float hr = height_at(xz + vec2(s, 0.0));
	float hb = height_at(xz - vec2(0.0, s));
	float hf = height_at(xz + vec2(0.0, s));
	vec3 g = normalize(vec3(hl - hr, 2.0 * s, hb - hf));

	// Steeper than the slope: projects from the sides.
	vec3 w = vec3(0.0, 1.0, 0.0);
	if (triplanar) {
		float side = clamp((triplanar_slope_cos + 0.1 - g.y) / 0.2, 0.0, 1.0);
		vec2 sides = pow(abs(g.xz), vec2(4.0));
		sides /= max(sides.x + sides.y, 0.0001);
		w = vec3(side * sides.x, 1.0 - side, side * sides.y);
	}

	// The two materials at this point of the control map. The materials come
	// from the nearest vertex, the weight between them is interpolated.
	vec2 gc = clamp(xz / vertex_spacing + vec2(map_size - 1) * 0.5, vec2(0.0), vec2(map_size - 1));
	ivec2 nearest = ivec2(floor(gc + 0.5));
	vec4 c = texelFetch(control_map, nearest, 0);
	int id_a = clamp(int(floor(c.r * 255.0 + 0.5)), 0, max(layer_count - 1, 0));
	int id_b = clamp(int(floor(c.g * 255.0 + 0.5)), 0, max(layer_count - 1, 0));
	float blend = c.b;
	if (id_a != id_b) {
		ivec2 p = min(ivec2(floor(gc)), max(map_size - 2, ivec2(0)));
		vec2 f = gc - vec2(p);
		int raw_a = int(floor(c.r * 255.0 + 0.5));
		int raw_b = int(floor(c.g * 255.0 + 0.5));
		float w00 = overlay_weight(texelFetch(control_map, p, 0), raw_a, raw_b, c.b);
		float w10 = overlay_weight(texelFetch(control_map, min(p + ivec2(1, 0), map_size - 1), 0), raw_a, raw_b, c.b);
		float w01 = overlay_weight(texelFetch(control_map, min(p + ivec2(0, 1), map_size - 1), 0), raw_a, raw_b, c.b);
		float w11 = overlay_weight(texelFetch(control_map, min(p + ivec2(1, 1), map_size - 1), 0), raw_a, raw_b, c.b);
		blend = mix(mix(w00, w10, f.x), mix(w01, w11, f.x), f.y);
	}

	vec4 ah_a = vec4(0.0);
	vec3 n_a = g;
	float r_a = 0.9;
	surface(id_a, g, w, px, py, pz, ah_a, n_a, r_a);
	vec4 ah_b = ah_a;
	vec3 n_b = n_a;
	float r_b = r_a;
	if (id_b != id_a && blend > 0.0) {
		surface(id_b, g, w, px, py, pz, ah_b, n_b, r_b);
	}

	// The heights of the layers decide which one shows at the edge of a blend.
	float la = ah_a.a + (1.0 - blend);
	float lb = ah_b.a + blend;
	float top = max(la, lb) - 0.3;
	float wa = max(la - top, 0.0001);
	float wb = max(lb - top, 0.0001);
	if (id_b == id_a || blend <= 0.0) {
		wb = 0.0;
	}
	float inv = 1.0 / (wa + wb);
	wa *= inv;
	wb *= inv;

	ALBEDO = ah_a.rgb * wa + ah_b.rgb * wb;
	ROUGHNESS = r_a * wa + r_b * wb;
	METALLIC = 0.0;
	vec3 n_local = normalize(n_a * wa + n_b * wb);
	NORMAL = normalize((VIEW_MATRIX * vec4(MODEL_NORMAL_MATRIX * n_local, 0.0)).xyz);
}
)";

String get_code() {
	String sampling = shader_sampling;
	// One copy of the sampling function for each layered texture.
	String code = String(shader_inputs);
	code += sampling.replace("TEXTURE", "albedo_height_array");
	code += sampling.replace("TEXTURE", "normal_roughness_array");
	code += shader_fragment;
	return code;
}

} // namespace EretheaTerrainShader
