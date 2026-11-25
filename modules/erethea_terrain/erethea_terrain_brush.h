/**************************************************************************/
/*  erethea_terrain_brush.h                                               */
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

#include "erethea_terrain_data.h"

#include "core/math/rect2i.h"
#include "core/variant/dictionary.h"

// The brushes that sculpt and paint an EretheaTerrainData, without the editor,
// so that unit tests (and scripts) can use them. The editor plugin calls them
// while the mouse is dragged over the terrain.
namespace EretheaTerrainBrush {

enum Tool {
	TOOL_RAISE,
	TOOL_LOWER,
	TOOL_SMOOTH,
	TOOL_FLATTEN,
	TOOL_PAINT_BASE,
	TOOL_PAINT_OVERLAY,
	TOOL_ERASE_OVERLAY,
	TOOL_MAX,
};

struct Settings {
	Tool tool = TOOL_RAISE;
	// In meters, from the center of the dab to where it fades out.
	real_t radius = 8.0;
	// How much one dab does at its center, from 0 to 1. See apply_dab().
	real_t strength = 0.5;
	// The index (0 to 31) of the material that the paint tools use.
	int material = 0;
	// The height in meters that the flatten tool goes to.
	real_t target_height = 0.0;
};

// How much a dab of radius p_radius acts at p_distance from its center: 1 at
// the center, 0 from the radius on, smooth in between.
real_t get_falloff(real_t p_distance, real_t p_radius);

// The vertices that a dab at p_center (x and z in meters) can change, inside the
// maps. It is empty when the dab is outside of the terrain.
Rect2i get_dab_rect(const Ref<EretheaTerrainData> &p_data, const Vector2 &p_center, real_t p_radius);

// Applies one dab to the data and returns the vertices that it could change.
// With the strength s and the falloff f of a vertex:
// - raise and lower add or remove s * f meters of height;
// - smooth moves the height s * f of the way to the average of the 3x3 vertices
//   around it, and flatten s * f of the way to the target height;
// - paint base sets the base material of every vertex that the dab reaches
//   (f > 0), whatever the strength;
// - paint overlay sets the overlay material and adds s * f to its weight (if the
//   vertex had another overlay material, the weight starts again from s * f);
// - erase overlay takes s * f from the weight.
// It doesn't tell the data that it changed: see
// EretheaTerrainData::notify_region_changed().
Rect2i apply_dab(const Ref<EretheaTerrainData> &p_data, const Settings &p_settings, const Vector2 &p_center);

// Finds where a ray, in the space of the terrain, first goes under the surface,
// by marching along it and then refining the hit. False if it misses, or starts
// under the surface.
bool raycast(const Ref<EretheaTerrainData> &p_data, const Vector3 &p_origin, const Vector3 &p_direction, real_t p_max_distance, Vector3 &r_hit);

// A drag of the brush, which is one step of the undo history. It keeps the
// vertices that the dabs changed as they were before the first one, and
// nothing else.
class Stroke {
public:
	struct Result {
		Rect2i rect;
		Dictionary before;
		Dictionary after;
	};

private:
	Ref<EretheaTerrainData> data;
	bool active = false;
	Rect2i rect;
	Dictionary before;

public:
	void begin(const Ref<EretheaTerrainData> &p_data);
	bool is_active() const { return active; }
	// Applies a dab, and tells the data. Does nothing outside of a stroke.
	void dab(const Settings &p_settings, const Vector2 &p_center);
	// Ends the stroke. Returns false if no dab changed anything.
	bool end(Result &r_result);
};

} // namespace EretheaTerrainBrush
