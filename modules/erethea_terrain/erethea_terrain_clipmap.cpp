/**************************************************************************/
/*  erethea_terrain_clipmap.cpp                                           */
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

#include "erethea_terrain_clipmap.h"

#include "core/math/math_funcs.h"

namespace EretheaTerrainClipmap {

real_t get_level_spacing(real_t p_base_spacing, int p_level) {
	return p_base_spacing * real_t(int64_t(1) << p_level);
}

Vector2 snap_to_lattice(const Vector2 &p_position, real_t p_spacing, const Vector2 &p_origin) {
	if (p_spacing <= 0) {
		return p_position;
	}
	return p_origin + ((p_position - p_origin) / p_spacing).round() * p_spacing;
}

Vector2 get_level_center(const Vector2 &p_camera, real_t p_base_spacing, int p_level, const Vector2 &p_origin) {
	return snap_to_lattice(p_camera, get_level_spacing(p_base_spacing, p_level + 1), p_origin);
}

real_t get_level_half_extent(real_t p_base_spacing, int p_level, int p_mesh_size) {
	return get_level_spacing(p_base_spacing, p_level) * real_t(p_mesh_size) * 0.5;
}

real_t get_hole_half_extent(real_t p_base_spacing, int p_level, int p_mesh_size) {
	if (p_level <= 0) {
		return 0;
	}
	return get_level_half_extent(p_base_spacing, p_level - 1, p_mesh_size);
}

int get_level_vertex_count(int p_quads) {
	const int side = p_quads + 1;
	return side * side + 4 * side;
}

int get_level_index_count(int p_quads) {
	return p_quads * p_quads * 6 + 4 * p_quads * 6;
}

LevelMesh build_level_mesh(int p_quads, real_t p_spacing) {
	LevelMesh mesh;
	ERR_FAIL_COND_V(p_quads < 1, mesh);

	const int side = p_quads + 1;
	const real_t half = real_t(p_quads) * 0.5 * p_spacing;

	mesh.vertices.resize(get_level_vertex_count(p_quads));
	mesh.uvs.resize(get_level_vertex_count(p_quads));
	mesh.indices.resize(get_level_index_count(p_quads));
	Vector3 *vertices = mesh.vertices.ptrw();
	Vector2 *uvs = mesh.uvs.ptrw();
	int32_t *indices = mesh.indices.ptrw();

	// The grid. Vertex (x, z) is at index z * side + x.
	for (int z = 0; z < side; z++) {
		for (int x = 0; x < side; x++) {
			vertices[z * side + x] = Vector3(real_t(x) * p_spacing - half, 0, real_t(z) * p_spacing - half);
			uvs[z * side + x] = Vector2(0, 0);
		}
	}

	// The vertices at the odd positions of the edge are the ones that the next
	// level doesn't have: they sit between two of its vertices. The shader
	// puts them on the line between those two. UV y says which way the line goes:
	// 1 along x (the north and south edges), 2 along z (the west and east).
	for (int i = 1; i < p_quads; i += 2) {
		uvs[i].y = 1;
		uvs[p_quads * side + i].y = 1;
		uvs[i * side].y = 2;
		uvs[i * side + p_quads].y = 2;
	}

	int index = 0;
	for (int z = 0; z < p_quads; z++) {
		for (int x = 0; x < p_quads; x++) {
			const int32_t v00 = z * side + x;
			const int32_t v10 = v00 + 1;
			const int32_t v01 = v00 + side;
			const int32_t v11 = v01 + 1;
			// The same diagonal as HeightMapShape3D: (x + 1, z) to (x, z + 1).
			indices[index++] = v00;
			indices[index++] = v10;
			indices[index++] = v01;
			indices[index++] = v10;
			indices[index++] = v11;
			indices[index++] = v01;
		}
	}

	// The skirt: four rows of copies of the edge vertices, with UV x = 1. Each
	// edge lists its vertices from the grid index of its first one, going by
	// edge_step. The quads face outward.
	struct Edge {
		int first;
		int step;
		bool flip; // Whether the edge runs the other way around the grid.
	};
	const Edge edges[4] = {
		{ 0, 1, true }, // z = 0 (north), x increasing.
		{ p_quads * side, 1, false }, // z = quads (south).
		{ 0, side, false }, // x = 0 (west), z increasing.
		{ p_quads, side, true }, // x = quads (east).
	};
	int skirt_vertex = side * side;
	for (const Edge &edge : edges) {
		const int skirt_first = skirt_vertex;
		for (int i = 0; i < side; i++) {
			vertices[skirt_vertex] = vertices[edge.first + i * edge.step];
			uvs[skirt_vertex] = Vector2(1, uvs[edge.first + i * edge.step].y);
			skirt_vertex++;
		}
		for (int i = 0; i < p_quads; i++) {
			const int32_t a = edge.first + i * edge.step;
			const int32_t b = a + edge.step;
			const int32_t a_skirt = skirt_first + i;
			const int32_t b_skirt = a_skirt + 1;
			if (!edge.flip) {
				indices[index++] = a;
				indices[index++] = b;
				indices[index++] = b_skirt;
				indices[index++] = a;
				indices[index++] = b_skirt;
				indices[index++] = a_skirt;
			} else {
				indices[index++] = a;
				indices[index++] = a_skirt;
				indices[index++] = b_skirt;
				indices[index++] = a;
				indices[index++] = b_skirt;
				indices[index++] = b;
			}
		}
	}
	return mesh;
}

AABB get_level_bounds(const Vector2 &p_center, real_t p_half_extent, real_t p_min_height, real_t p_max_height, real_t p_skirt_depth) {
	const real_t bottom = p_min_height - p_skirt_depth;
	// Never flat, a flat box can be culled wrongly.
	const real_t height = MAX(p_max_height - bottom, real_t(0.01));
	return AABB(Vector3(p_center.x - p_half_extent, bottom, p_center.y - p_half_extent), Vector3(p_half_extent * 2, height, p_half_extent * 2));
}

} // namespace EretheaTerrainClipmap
