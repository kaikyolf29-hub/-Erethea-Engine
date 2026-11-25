/**************************************************************************/
/*  erethea_terrain_data.h                                                */
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

#include "core/io/image.h"
#include "core/io/resource.h"
#include "core/math/vector2.h"
#include "core/math/vector2i.h"
#include "core/math/vector3.h"

// The terrain's height map and material control map.
// A new one has no maps: call create(), or set the height map.
//
// The terrain is centered on the origin of its node. Vertex (x, z) of the maps
// is at ((x - (width - 1) / 2) * vertex_spacing, height, (z - (depth - 1) / 2) * vertex_spacing),
// which is also how HeightMapShape3D lays out its heights.
//
// The control map is an RGBA8 image of the same size as the height map. Per
// texel, R is the index of the base material (0 to 31), G the index of the
// overlay material (0 to 31), B the weight of the overlay over the base (0 is
// all base, 255 all overlay) and A is reserved (kept at 255).
class EretheaTerrainData : public Resource {
	GDCLASS(EretheaTerrainData, Resource);

public:
	static constexpr int MAX_MATERIALS = 32;
	static constexpr int MIN_RESOLUTION = 2;
	static constexpr int MAX_RESOLUTION = 8192;

private:
	real_t vertex_spacing = 1.0;
	Ref<Image> height_map;
	Ref<Image> control_map;
	real_t min_height = 0.0;
	real_t max_height = 0.0;

	void _create_maps(const Vector2i &p_resolution, real_t p_height);
	real_t _get_clamped_height(int p_x, int p_z) const;
	// The position, in vertices, of a point in meters. Not clamped.
	Vector2 _to_vertex_space(real_t p_x, real_t p_z) const;
	Vector2i _control_index(const Vector2i &p_vertex) const;

protected:
	static void _bind_methods();

public:
	void create(const Vector2 &p_size, real_t p_vertex_spacing = 1.0, real_t p_height = 0.0);

	void set_size(const Vector2 &p_size);
	Vector2 get_size() const;
	Vector2i get_resolution() const;

	void set_vertex_spacing(real_t p_spacing);
	real_t get_vertex_spacing() const;

	void set_height_map(const Ref<Image> &p_image);
	Ref<Image> get_height_map() const;
	void set_control_map(const Ref<Image> &p_image);
	Ref<Image> get_control_map() const;

	// The position, in meters, of vertex p_vertex.
	Vector2 get_vertex_position(const Vector2i &p_vertex) const;
	bool is_inside(real_t p_x, real_t p_z) const;

	real_t get_vertex_height(const Vector2i &p_vertex) const;
	void set_vertex_height(const Vector2i &p_vertex, real_t p_height);
	// The height at a point, interpolated between the 4 nearest vertices, or NaN
	// outside of the terrain.
	real_t get_height(real_t p_x, real_t p_z) const;
	// The up direction of the surface at a point, from the heights around it.
	Vector3 get_normal(real_t p_x, real_t p_z) const;
	Vector2 get_height_range() const;

	void set_control(const Vector2i &p_vertex, int p_base, int p_overlay, real_t p_blend);
	int get_control_base(const Vector2i &p_vertex) const;
	int get_control_overlay(const Vector2i &p_vertex) const;
	real_t get_control_blend(const Vector2i &p_vertex) const;

	// The heights in the order of HeightMapShape3D (row after row of x, one row per z).
	Vector<real_t> get_collision_heights() const;

	// To call after changing the maps by their vertices, to update the height
	// range and tell the nodes that draw the data.
	void notify_maps_changed();

	EretheaTerrainData();
};
