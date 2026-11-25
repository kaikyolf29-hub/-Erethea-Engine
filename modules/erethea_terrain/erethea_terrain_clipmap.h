/**************************************************************************/
/*  erethea_terrain_clipmap.h                                             */
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

#pragma once

#include "core/math/aabb.h"
#include "core/math/vector2.h"
#include "core/math/vector3.h"
#include "core/templates/vector.h"

// Pure helpers of the terrain's clipmap, kept apart from the node so that unit
// tests can check them without a rendering server.
//
// The terrain is drawn by nested square grids ("levels"), all centered near
// the camera. Level 0 has the finest spacing (the data's vertex spacing) and
// each next level doubles it, so a level covers twice the area of the previous
// one for the same number of vertices. A level draws the full square, and its
// shader discards the part that the next finer level covers (the "hole").
//
// For the hole to line up with the finer level's grid, the center of level L is
// snapped to the grid of level L + 1 (twice its own spacing), and
// mesh_size must be a multiple of 4.
namespace EretheaTerrainClipmap {

// The smallest and the largest number of quads per side of a level's mesh.
constexpr int MIN_MESH_SIZE = 8;
constexpr int MAX_MESH_SIZE = 256;
constexpr int MIN_LEVELS = 1;
constexpr int MAX_LEVELS = 16;

// The distance between the vertices of a level.
real_t get_level_spacing(real_t p_base_spacing, int p_level);

// Snaps a position to the lattice that starts at p_origin and has p_spacing
// between its points.
Vector2 snap_to_lattice(const Vector2 &p_position, real_t p_spacing, const Vector2 &p_origin);

// The center of a level for a camera at p_camera. p_origin is the position of a
// vertex of the data, so that the level's vertices land on the data's grid.
Vector2 get_level_center(const Vector2 &p_camera, real_t p_base_spacing, int p_level, const Vector2 &p_origin);

// The half size of the square that a level covers, in meters.
real_t get_level_half_extent(real_t p_base_spacing, int p_level, int p_mesh_size);

// The half size of the hole of a level, which is the area that the next finer
// level covers. It is 0 for level 0, which has no hole.
real_t get_hole_half_extent(real_t p_base_spacing, int p_level, int p_mesh_size);

// The vertices, UVs and indices of a level's mesh, on the plane y = 0, centered
// on the origin and spaced by p_spacing. Besides the (p_quads + 1)^2 grid
// vertices there is a skirt around the edge: a row of vertices with the UV x set
// to 1, which the vertex shader lowers by the skirt depth. The skirt hides the
// cracks between two levels. Triangles are clockwise, as seen from the front,
// like Godot's, and follow the diagonal of Godot's HeightMapShape3D.
struct LevelMesh {
	Vector<Vector3> vertices;
	Vector<Vector2> uvs;
	Vector<int32_t> indices;
};

LevelMesh build_level_mesh(int p_quads, real_t p_spacing);

// The number of vertices and indices of build_level_mesh().
int get_level_vertex_count(int p_quads);
int get_level_index_count(int p_quads);

// The box that contains a level drawn at p_center, whose heights are between
// p_min_height and p_max_height, in the terrain's local space. The skirt is
// included.
AABB get_level_bounds(const Vector2 &p_center, real_t p_half_extent, real_t p_min_height, real_t p_max_height, real_t p_skirt_depth);

} // namespace EretheaTerrainClipmap
