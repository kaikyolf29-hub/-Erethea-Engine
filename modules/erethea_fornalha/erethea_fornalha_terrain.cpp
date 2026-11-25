/**************************************************************************/
/*  erethea_fornalha_terrain.cpp                                          */
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

#include "erethea_fornalha_terrain.h"

#include "core/math/math_funcs.h"
#include "scene/3d/mesh_instance_3d.h"
#include "scene/resources/image_texture.h"
#include "scene/resources/material.h"
#include "scene/resources/mesh.h"

#include "modules/erethea_terrain/erethea_terrain_data.h"

namespace EretheaFornalhaTerrain {

Heights sample_heights(const Vector<Vector3> &p_triangles, real_t p_spacing) {
	Heights result;
	if (p_triangles.size() < 3 || p_triangles.size() % 3 != 0) {
		return result;
	}

	Vector2 low(Math::INF, Math::INF);
	Vector2 high(-Math::INF, -Math::INF);
	for (const Vector3 &vertex : p_triangles) {
		low = low.min(Vector2(vertex.x, vertex.z));
		high = high.max(Vector2(vertex.x, vertex.z));
	}
	const Vector2 size = high - low;
	real_t spacing = MAX(p_spacing, real_t(0.01));
	spacing = MAX(spacing, MAX(size.x, size.y) / real_t(MAX_RESOLUTION - 1));
	result.spacing = spacing;
	result.center = low + size * 0.5;
	// The grid has to reach the far edge of the ground.
	result.resolution = Vector2i(
			MAX(int(Math::ceil(size.x / spacing - real_t(0.001))) + 1, 2),
			MAX(int(Math::ceil(size.y / spacing - real_t(0.001))) + 1, 2));
	const Vector2i resolution = result.resolution;
	const Vector2 origin = result.center - Vector2(resolution.x - 1, resolution.y - 1) * spacing * 0.5;

	Vector<real_t> &heights = result.heights;
	heights.resize(resolution.x * resolution.y);
	real_t *out = heights.ptrw();
	for (int i = 0; i < heights.size(); i++) {
		out[i] = Math::NaN;
	}

	const real_t tolerance = real_t(1e-4);
	for (int t = 0; t + 2 < p_triangles.size(); t += 3) {
		const Vector3 &a = p_triangles[t];
		const Vector3 &b = p_triangles[t + 1];
		const Vector3 &c = p_triangles[t + 2];
		// Twice the area in x and z. Sides and edges seen from above are lines.
		const real_t determinant = (b.z - c.z) * (a.x - c.x) + (c.x - b.x) * (a.z - c.z);
		if (Math::abs(determinant) < real_t(1e-9)) {
			continue;
		}
		const Vector2 low_corner = Vector2(MIN(a.x, MIN(b.x, c.x)), MIN(a.z, MIN(b.z, c.z)));
		const Vector2 high_corner = Vector2(MAX(a.x, MAX(b.x, c.x)), MAX(a.z, MAX(b.z, c.z)));
		const int x_from = MAX(int(Math::floor((low_corner.x - origin.x) / spacing - tolerance)), 0);
		const int z_from = MAX(int(Math::floor((low_corner.y - origin.y) / spacing - tolerance)), 0);
		const int x_to = MIN(int(Math::ceil((high_corner.x - origin.x) / spacing + tolerance)), resolution.x - 1);
		const int z_to = MIN(int(Math::ceil((high_corner.y - origin.y) / spacing + tolerance)), resolution.y - 1);
		for (int z = z_from; z <= z_to; z++) {
			for (int x = x_from; x <= x_to; x++) {
				const real_t px = origin.x + real_t(x) * spacing;
				const real_t pz = origin.y + real_t(z) * spacing;
				const real_t w_a = ((b.z - c.z) * (px - c.x) + (c.x - b.x) * (pz - c.z)) / determinant;
				const real_t w_b = ((c.z - a.z) * (px - c.x) + (a.x - c.x) * (pz - c.z)) / determinant;
				const real_t w_c = 1 - w_a - w_b;
				if (w_a < -tolerance || w_b < -tolerance || w_c < -tolerance) {
					continue;
				}
				const real_t height = w_a * a.y + w_b * b.y + w_c * c.y;
				real_t &cell = out[z * resolution.x + x];
				cell = Math::is_nan(cell) ? height : MAX(cell, height);
			}
		}
	}

	// What no triangle reached takes the height of its neighbors, from the
	// ones that have one outward.
	for (int i = 0; i < heights.size(); i++) {
		result.uncovered += Math::is_nan(out[i]) ? 1 : 0;
	}
	if (result.uncovered == heights.size()) {
		for (int i = 0; i < heights.size(); i++) {
			out[i] = 0;
		}
		return result;
	}
	int missing = result.uncovered;
	while (missing > 0) {
		const Vector<real_t> before = heights;
		for (int z = 0; z < resolution.y; z++) {
			for (int x = 0; x < resolution.x; x++) {
				if (!Math::is_nan(before[z * resolution.x + x])) {
					continue;
				}
				real_t sum = 0;
				int count = 0;
				const int neighbors[4][2] = { { x - 1, z }, { x + 1, z }, { x, z - 1 }, { x, z + 1 } };
				for (const auto &neighbor : neighbors) {
					if (neighbor[0] >= 0 && neighbor[1] >= 0 && neighbor[0] < resolution.x && neighbor[1] < resolution.y) {
						const real_t value = before[neighbor[1] * resolution.x + neighbor[0]];
						if (!Math::is_nan(value)) {
							sum += value;
							count++;
						}
					}
				}
				if (count > 0) {
					out[z * resolution.x + x] = sum / real_t(count);
					missing--;
				}
			}
		}
	}
	return result;
}

Ref<EretheaTerrainData> make_data(const Heights &p_heights) {
	ERR_FAIL_COND_V(!p_heights.is_valid(), Ref<EretheaTerrainData>());
	Ref<EretheaTerrainData> data;
	data.instantiate();
	data->create(Vector2(p_heights.resolution.x - 1, p_heights.resolution.y - 1) * p_heights.spacing, p_heights.spacing);
	ERR_FAIL_COND_V(data->get_resolution() != p_heights.resolution, Ref<EretheaTerrainData>());
	for (int z = 0; z < p_heights.resolution.y; z++) {
		for (int x = 0; x < p_heights.resolution.x; x++) {
			data->set_vertex_height(Vector2i(x, z), p_heights.heights[z * p_heights.resolution.x + x]);
		}
	}
	data->notify_maps_changed();
	return data;
}

Layer get_area_layer(const String &p_type) {
	const String type = p_type.to_lower();
	if (type == "vila") {
		return LAYER_DIRT;
	}
	if (type == "arena") {
		return LAYER_SAND;
	}
	if (type == "masmorra" || type == "caverna" || type == "salao" || type == "ruinas" || type == "montanha") {
		return LAYER_STONE;
	}
	return LAYER_GRASS;
}

real_t get_stone_weight(real_t p_slope_degrees) {
	const real_t t = CLAMP((p_slope_degrees - STONE_FROM_SLOPE) / (STONE_FULL_SLOPE - STONE_FROM_SLOPE), real_t(0), real_t(1));
	return t * t * (3 - 2 * t);
}

Vector<PaintArea> get_paint_areas(const EretheaFornalhaProject &p_project) {
	Vector<PaintArea> areas;
	for (int i = 0; i < p_project.areas.size(); i++) {
		PaintArea area;
		area.center = Vector2(p_project.areas[i].center.x, p_project.areas[i].center.z);
		area.radius = EretheaFornalhaImport::get_area_radius(p_project, i) * AREA_GROUND_FRACTION;
		area.layer = get_area_layer(p_project.areas[i].type);
		areas.push_back(area);
	}
	return areas;
}

void paint_materials(const Ref<EretheaTerrainData> &p_data, const Vector2 &p_origin, const Vector<PaintArea> &p_areas) {
	ERR_FAIL_COND(p_data.is_null() || p_data->get_height_map().is_null());
	const Vector2i resolution = p_data->get_resolution();
	for (int z = 0; z < resolution.y; z++) {
		for (int x = 0; x < resolution.x; x++) {
			const Vector2i vertex(x, z);
			const Vector2 local = p_data->get_vertex_position(vertex);
			const Vector2 position = p_origin + local;

			// The area that the point is the most inside of.
			Layer base = LAYER_GRASS;
			real_t edge = 0; // How much of the way to grass, from the area's ground to its edge.
			real_t best = Math::INF;
			for (const PaintArea &area : p_areas) {
				if (area.radius <= 0) {
					continue;
				}
				const real_t reach = position.distance_to(area.center) / area.radius;
				if (reach < 1 && reach < best) {
					best = reach;
					base = area.layer;
					edge = CLAMP((reach - (1 - AREA_EDGE_BLEND)) / AREA_EDGE_BLEND, real_t(0), real_t(1));
					edge = edge * edge * (3 - 2 * edge);
				}
			}

			const Vector3 normal = p_data->get_normal(local.x, local.y);
			const real_t slope = Math::rad_to_deg(Math::acos(CLAMP(normal.y, real_t(-1), real_t(1))));
			const real_t stone = get_stone_weight(slope);

			// One overlay per vertex: stone on a slope, or else grass at the edge of
			// an area. A slope wins over the edge.
			Layer overlay = LAYER_GRASS;
			real_t blend = 0;
			if (stone > 0 && stone >= edge) {
				overlay = LAYER_STONE;
				blend = stone;
			} else if (base != LAYER_GRASS) {
				overlay = LAYER_GRASS;
				blend = edge;
			}
			if (overlay == base) {
				blend = 0;
			}
			p_data->set_control(vertex, base, overlay, blend);
		}
	}
	p_data->notify_maps_changed();
}

Color get_layer_color(Layer p_layer, const Color &p_ground) {
	const Color ground = Color(CLAMP(p_ground.r, 0.05f, 1.0f), CLAMP(p_ground.g, 0.05f, 1.0f), CLAMP(p_ground.b, 0.05f, 1.0f));
	switch (p_layer) {
		case LAYER_GRASS:
			return ground;
		case LAYER_DIRT:
			// The Fornalha's packed dirt, on the color of the ground.
			return ground.lerp(Color(0.78f, 0.62f, 0.45f), 0.7f);
		case LAYER_STONE: {
			const float gray = 0.35f + 0.25f * ground.get_luminance();
			return Color(gray, gray * 0.97f, gray * 0.93f).lerp(ground, 0.1f);
		}
		case LAYER_SAND:
			return ground.lerp(Color(0.85f, 0.78f, 0.55f), 0.8f);
		default:
			return ground;
	}
}

Vector<Ref<Image>> make_layer_images(const Color &p_ground) {
	constexpr int SIZE = 64;
	Vector<Ref<Image>> images;
	for (int layer = 0; layer < LAYER_COUNT; layer++) {
		const Color color = get_layer_color(Layer(layer), p_ground);
		Ref<Image> image = Image::create_empty(SIZE, SIZE, false, Image::FORMAT_RGBA8);
		for (int y = 0; y < SIZE; y++) {
			for (int x = 0; x < SIZE; x++) {
				// Tileable: whole numbers of waves across the image.
				const float u = float(x) / SIZE * Math::TAU;
				const float v = float(y) / SIZE * Math::TAU;
				const float noise = 0.5f + 0.25f * Math::sin(u * 3 + layer) * Math::sin(v * 2) + 0.25f * Math::sin(u * 7 + v * 5 + layer * 2.0f);
				const float shade = 0.9f + 0.2f * noise;
				// The alpha is the height of the material, which decides which one shows
				// at the edge of a blend.
				const float height = 0.35f + 0.3f * (0.5f + 0.5f * Math::sin(u * 5 - v * 3 + layer));
				image->set_pixel(x, y, Color(color.r * shade, color.g * shade, color.b * shade, height));
			}
		}
		image->generate_mipmaps();
		images.push_back(image);
	}
	return images;
}

Ref<Texture2DArray> make_textures(const Color &p_ground) {
	Ref<Texture2DArray> textures;
	textures.instantiate();
	ERR_FAIL_COND_V(textures->create_from_images(make_layer_images(p_ground)) != OK, Ref<Texture2DArray>());
	return textures;
}

static MeshInstance3D *_find_ground_in(Node *p_node) {
	MeshInstance3D *mesh_instance = Object::cast_to<MeshInstance3D>(p_node);
	if (mesh_instance && mesh_instance->get_mesh().is_valid() && String(mesh_instance->get_name()).to_lower().begins_with("chao")) {
		return mesh_instance;
	}
	for (int i = 0; i < p_node->get_child_count(); i++) {
		MeshInstance3D *found = _find_ground_in(p_node->get_child(i));
		if (found) {
			return found;
		}
	}
	return nullptr;
}

MeshInstance3D *find_ground(Node *p_blockout) {
	ERR_FAIL_NULL_V(p_blockout, nullptr);
	return _find_ground_in(p_blockout);
}

Vector<Vector3> get_ground_triangles(Node *p_blockout, MeshInstance3D *p_ground) {
	Vector<Vector3> triangles;
	ERR_FAIL_NULL_V(p_ground, triangles);
	const Ref<Mesh> mesh = p_ground->get_mesh();
	ERR_FAIL_COND_V(mesh.is_null(), triangles);
	// The ground's place in the scene, up to the node that the blockout is. That
	// node's own transform comes with it, as it does where the world uses it.
	Transform3D transform;
	for (Node *node = p_ground; node; node = node->get_parent()) {
		Node3D *node_3d = Object::cast_to<Node3D>(node);
		if (node_3d) {
			transform = node_3d->get_transform() * transform;
		}
		if (node == p_blockout) {
			break;
		}
	}
	for (const Face3 &face : mesh->get_faces()) {
		for (int i = 0; i < 3; i++) {
			triangles.push_back(transform.xform(face.vertex[i]));
		}
	}
	return triangles;
}

Color get_ground_color(MeshInstance3D *p_ground) {
	Color color(0.35f, 0.5f, 0.25f);
	ERR_FAIL_NULL_V(p_ground, color);
	const Ref<Mesh> mesh = p_ground->get_mesh();
	ERR_FAIL_COND_V(mesh.is_null(), color);
	for (int surface = 0; surface < mesh->get_surface_count(); surface++) {
		Ref<BaseMaterial3D> material = p_ground->get_active_material(surface);
		if (material.is_valid()) {
			color = material->get_albedo();
		}
		const Array arrays = mesh->surface_get_arrays(surface);
		if (arrays.size() > Mesh::ARRAY_COLOR) {
			const PackedColorArray colors = arrays[Mesh::ARRAY_COLOR];
			if (!colors.is_empty()) {
				Color sum;
				for (const Color &vertex_color : colors) {
					sum += vertex_color;
				}
				const Color mean = sum / float(colors.size());
				color = Color(color.r * mean.r, color.g * mean.g, color.b * mean.b);
			}
		}
		break;
	}
	return color;
}

} // namespace EretheaFornalhaTerrain
