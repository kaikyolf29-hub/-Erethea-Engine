/**************************************************************************/
/*  erethea_fornalha_terrain.h                                            */
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

#include "erethea_fornalha_import.h"

#include "core/math/color.h"
#include "core/math/vector2.h"
#include "core/math/vector2i.h"
#include "core/math/vector3.h"
#include "core/object/ref_counted.h"
#include "core/templates/vector.h"

class EretheaTerrainData;
class Mesh;
class MeshInstance3D;
class Node;
class Texture2DArray;

// The ground of the Fornalha's blockout as an EretheaTerrain3D. The blockout's
// ground is a mesh ("chao-col") with relief, flat in the areas and on the paths
// between them. Its heights are sampled on a grid of the terrain, and the
// materials are painted by simple rules from the slope and from the area that
// each point is in.
namespace EretheaFornalhaTerrain {

// The materials of the terrain, which are the layers of its texture arrays.
enum Layer {
	LAYER_GRASS,
	LAYER_DIRT,
	LAYER_STONE,
	LAYER_SAND,
	LAYER_COUNT,
};

// The distance between the vertices of the terrain: about one meter.
constexpr real_t SPACING = 1.0;
// The most vertices on a side. A bigger ground gets a wider spacing.
constexpr int MAX_RESOLUTION = 2049;
// From this slope (in degrees) the ground starts to show stone, and from the
// second one it is all stone.
constexpr real_t STONE_FROM_SLOPE = 30.0;
constexpr real_t STONE_FULL_SLOPE = 45.0;
// How much of the reach of an area's sounds is ground of its type. The last
// 30% of it blends into grass.
constexpr real_t AREA_GROUND_FRACTION = 0.7;
constexpr real_t AREA_EDGE_BLEND = 0.3;

// The heights of the ground on the grid of a terrain.
struct Heights {
	// The middle of the grid, as x and z in the space of the blockout.
	Vector2 center;
	// Vertices in x and z, with one every spacing meters.
	Vector2i resolution;
	real_t spacing = SPACING;
	// Row after row of x, one row per z.
	Vector<real_t> heights;
	// How many vertices no triangle covered. They take the height of the nearest.
	int uncovered = 0;

	bool is_valid() const { return resolution.x >= 2 && resolution.y >= 2 && heights.size() == resolution.x * resolution.y; }
};

// What an area paints on the ground around it.
struct PaintArea {
	Vector2 center;
	// How far from the center the ground is of the area's type.
	real_t radius = 0.0;
	Layer layer = LAYER_GRASS;
};

// The heights under a soup of triangles (three vertices each, in the space of the
// blockout), as the highest point of any triangle over each vertex of the grid:
// the way down from above. The grid covers the triangles, and no side of it has
// more than MAX_RESOLUTION vertices.
Heights sample_heights(const Vector<Vector3> &p_triangles, real_t p_spacing = SPACING);

// The terrain's data for those heights, all material 0.
Ref<EretheaTerrainData> make_data(const Heights &p_heights);

// Which ground an area of this type has (vila: dirt, arena: sand, masmorra,
// caverna, salao, ruinas and montanha: stone, the others grass).
Layer get_area_layer(const String &p_type);
// The weight of stone for ground of this slope, from 0 to 1.
real_t get_stone_weight(real_t p_slope_degrees);
// The areas of the project with the ground that they paint and how far it goes.
Vector<PaintArea> get_paint_areas(const EretheaFornalhaProject &p_project);

// Paints the control map of the data: the area's ground in each area, which
// fades into grass at its edge, and stone where the ground is steep. p_origin is
// where the terrain is, as x and z in the space of the blockout. Where the edge of
// an area and a steep slope meet, the slope wins.
void paint_materials(const Ref<EretheaTerrainData> &p_data, const Vector2 &p_origin, const Vector<PaintArea> &p_areas);

// One layer of plain color with a little noise per material, as the albedo
// (RGB) and the height (A) of a texture array, from the color of the blockout's
// ground. Mipmaps included.
Ref<Texture2DArray> make_textures(const Color &p_ground);
// The color of a material, from the color of the blockout's ground.
Color get_layer_color(Layer p_layer, const Color &p_ground);

// The ground of a blockout: its mesh node, found by its name ("chao"), the way
// the Fornalha names it. Null if there is none.
MeshInstance3D *find_ground(Node *p_blockout);
// Its triangles in the space of p_blockout, which is the one of its scene.
Vector<Vector3> get_ground_triangles(Node *p_blockout, MeshInstance3D *p_ground);
// Its color: the color of its material times the mean of its vertex colors.
Color get_ground_color(MeshInstance3D *p_ground);

} // namespace EretheaFornalhaTerrain
