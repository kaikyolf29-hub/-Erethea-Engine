/**************************************************************************/
/*  erethea_terrain_data.cpp                                              */
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

#include "erethea_terrain_data.h"

#include "core/math/math_funcs.h"
#include "core/object/class_db.h"

void EretheaTerrainData::_create_maps(const Vector2i &p_resolution, real_t p_height) {
	height_map = Image::create_empty(p_resolution.x, p_resolution.y, false, Image::FORMAT_RF);
	height_map->fill(Color(p_height, 0, 0, 1));
	// Material 0 everywhere.
	control_map = Image::create_empty(p_resolution.x, p_resolution.y, false, Image::FORMAT_RGBA8);
	control_map->fill(Color(0, 0, 0, 1));
	min_height = p_height;
	max_height = p_height;
}

void EretheaTerrainData::create(const Vector2 &p_size, real_t p_vertex_spacing, real_t p_height) {
	ERR_FAIL_COND_MSG(p_vertex_spacing <= 0, "The vertex spacing must be above 0.");
	ERR_FAIL_COND_MSG(p_size.x <= 0 || p_size.y <= 0, "The size must be above 0.");
	const Vector2i resolution(
			CLAMP(int(Math::round(p_size.x / p_vertex_spacing)) + 1, MIN_RESOLUTION, MAX_RESOLUTION),
			CLAMP(int(Math::round(p_size.y / p_vertex_spacing)) + 1, MIN_RESOLUTION, MAX_RESOLUTION));
	vertex_spacing = p_vertex_spacing;
	_create_maps(resolution, p_height);
	emit_changed();
	notify_property_list_changed();
}

void EretheaTerrainData::set_size(const Vector2 &p_size) {
	ERR_FAIL_COND_MSG(p_size.x <= 0 || p_size.y <= 0, "The size must be above 0.");
	const Vector2i resolution(
			CLAMP(int(Math::round(p_size.x / vertex_spacing)) + 1, MIN_RESOLUTION, MAX_RESOLUTION),
			CLAMP(int(Math::round(p_size.y / vertex_spacing)) + 1, MIN_RESOLUTION, MAX_RESOLUTION));
	if (resolution == get_resolution()) {
		return;
	}
	// Keeps what fits, from the first vertex on. The rest is flat, material 0.
	Ref<Image> old_height = height_map;
	Ref<Image> old_control = control_map;
	_create_maps(resolution, 0.0);
	if (old_height.is_valid() && old_control.is_valid()) {
		const Rect2i overlap(0, 0, MIN(old_height->get_width(), resolution.x), MIN(old_height->get_height(), resolution.y));
		height_map->blit_rect(old_height, overlap, Point2i());
		control_map->blit_rect(old_control, overlap, Point2i());
	}
	notify_maps_changed();
	notify_property_list_changed();
}

Vector2 EretheaTerrainData::get_size() const {
	const Vector2i resolution = get_resolution();
	return Vector2(resolution.x - 1, resolution.y - 1) * vertex_spacing;
}

Vector2i EretheaTerrainData::get_resolution() const {
	if (height_map.is_null()) {
		return Vector2i();
	}
	return Vector2i(height_map->get_width(), height_map->get_height());
}

void EretheaTerrainData::set_vertex_spacing(real_t p_spacing) {
	ERR_FAIL_COND_MSG(p_spacing <= 0, "The vertex spacing must be above 0.");
	vertex_spacing = p_spacing;
	emit_changed();
	notify_property_list_changed();
}

real_t EretheaTerrainData::get_vertex_spacing() const {
	return vertex_spacing;
}

void EretheaTerrainData::set_height_map(const Ref<Image> &p_image) {
	ERR_FAIL_COND(p_image.is_null());
	ERR_FAIL_COND_MSG(p_image->get_width() < MIN_RESOLUTION || p_image->get_height() < MIN_RESOLUTION, "The height map must be at least 2x2.");
	ERR_FAIL_COND_MSG(p_image->get_width() > MAX_RESOLUTION || p_image->get_height() > MAX_RESOLUTION, vformat("The height map can be at most %dx%d.", MAX_RESOLUTION, MAX_RESOLUTION));
	Ref<Image> image = p_image;
	if (image->get_format() != Image::FORMAT_RF || image->has_mipmaps()) {
		image = image->duplicate();
		image->clear_mipmaps();
		image->convert(Image::FORMAT_RF);
	}
	const bool same_size = height_map.is_valid() && control_map.is_valid() && height_map->get_size() == image->get_size();
	height_map = image;
	if (!same_size) {
		control_map = Image::create_empty(image->get_width(), image->get_height(), false, Image::FORMAT_RGBA8);
		control_map->fill(Color(0, 0, 0, 1));
	}
	notify_maps_changed();
	notify_property_list_changed();
}

Ref<Image> EretheaTerrainData::get_height_map() const {
	return height_map;
}

void EretheaTerrainData::set_control_map(const Ref<Image> &p_image) {
	ERR_FAIL_COND(p_image.is_null());
	ERR_FAIL_COND_MSG(height_map.is_null() || p_image->get_size() != height_map->get_size(), "The control map must have the same size as the height map.");
	Ref<Image> image = p_image;
	if (image->get_format() != Image::FORMAT_RGBA8 || image->has_mipmaps()) {
		image = image->duplicate();
		image->clear_mipmaps();
		image->convert(Image::FORMAT_RGBA8);
	}
	control_map = image;
	emit_changed();
}

Ref<Image> EretheaTerrainData::get_control_map() const {
	return control_map;
}

Vector2 EretheaTerrainData::get_vertex_position(const Vector2i &p_vertex) const {
	const Vector2i resolution = get_resolution();
	return Vector2((real_t(p_vertex.x) - real_t(resolution.x - 1) * 0.5) * vertex_spacing, (real_t(p_vertex.y) - real_t(resolution.y - 1) * 0.5) * vertex_spacing);
}

Vector2 EretheaTerrainData::_to_vertex_space(real_t p_x, real_t p_z) const {
	const Vector2i resolution = get_resolution();
	return Vector2(p_x / vertex_spacing + real_t(resolution.x - 1) * 0.5, p_z / vertex_spacing + real_t(resolution.y - 1) * 0.5);
}

bool EretheaTerrainData::is_inside(real_t p_x, real_t p_z) const {
	if (height_map.is_null()) {
		return false;
	}
	const Vector2i resolution = get_resolution();
	const Vector2 vertex = _to_vertex_space(p_x, p_z);
	return vertex.x >= 0 && vertex.y >= 0 && vertex.x <= real_t(resolution.x - 1) && vertex.y <= real_t(resolution.y - 1);
}

real_t EretheaTerrainData::get_vertex_height(const Vector2i &p_vertex) const {
	ERR_FAIL_COND_V(height_map.is_null(), 0.0);
	ERR_FAIL_INDEX_V(p_vertex.x, height_map->get_width(), 0.0);
	ERR_FAIL_INDEX_V(p_vertex.y, height_map->get_height(), 0.0);
	const float *heights = reinterpret_cast<const float *>(height_map->ptr());
	return heights[p_vertex.y * height_map->get_width() + p_vertex.x];
}

void EretheaTerrainData::set_vertex_height(const Vector2i &p_vertex, real_t p_height) {
	ERR_FAIL_COND(height_map.is_null());
	ERR_FAIL_INDEX(p_vertex.x, height_map->get_width());
	ERR_FAIL_INDEX(p_vertex.y, height_map->get_height());
	float *heights = reinterpret_cast<float *>(height_map->ptrw());
	heights[p_vertex.y * height_map->get_width() + p_vertex.x] = float(p_height);
}

real_t EretheaTerrainData::_get_clamped_height(int p_x, int p_z) const {
	const float *heights = reinterpret_cast<const float *>(height_map->ptr());
	const int width = height_map->get_width();
	return heights[CLAMP(p_z, 0, height_map->get_height() - 1) * width + CLAMP(p_x, 0, width - 1)];
}

real_t EretheaTerrainData::get_height(real_t p_x, real_t p_z) const {
	if (!is_inside(p_x, p_z)) {
		return Math::NaN;
	}
	const Vector2i resolution = get_resolution();
	const Vector2 vertex = _to_vertex_space(p_x, p_z);
	// The last quad includes its far edge.
	const int x0 = MIN(int(Math::floor(vertex.x)), resolution.x - 2);
	const int z0 = MIN(int(Math::floor(vertex.y)), resolution.y - 2);
	const real_t fx = vertex.x - real_t(x0);
	const real_t fz = vertex.y - real_t(z0);
	const real_t h00 = _get_clamped_height(x0, z0);
	const real_t h10 = _get_clamped_height(x0 + 1, z0);
	const real_t h01 = _get_clamped_height(x0, z0 + 1);
	const real_t h11 = _get_clamped_height(x0 + 1, z0 + 1);
	return Math::lerp(Math::lerp(h00, h10, fx), Math::lerp(h01, h11, fx), fz);
}

Vector3 EretheaTerrainData::get_normal(real_t p_x, real_t p_z) const {
	if (height_map.is_null()) {
		return Vector3(0, 1, 0);
	}
	// Central differences, one vertex spacing to each side, where the edge of
	// the terrain allows.
	const Vector2i resolution = get_resolution();
	const Vector2 half_size = Vector2(resolution.x - 1, resolution.y - 1) * 0.5 * vertex_spacing;
	const real_t x = CLAMP(p_x, -half_size.x, half_size.x);
	const real_t z = CLAMP(p_z, -half_size.y, half_size.y);
	const real_t x_left = MAX(x - vertex_spacing, -half_size.x);
	const real_t x_right = MIN(x + vertex_spacing, half_size.x);
	const real_t z_back = MAX(z - vertex_spacing, -half_size.y);
	const real_t z_front = MIN(z + vertex_spacing, half_size.y);
	const real_t dx = (get_height(x_right, z) - get_height(x_left, z)) / (x_right - x_left);
	const real_t dz = (get_height(x, z_front) - get_height(x, z_back)) / (z_front - z_back);
	return Vector3(-dx, 1, -dz).normalized();
}

Vector2 EretheaTerrainData::get_height_range() const {
	return Vector2(min_height, max_height);
}

Vector2i EretheaTerrainData::_control_index(const Vector2i &p_vertex) const {
	ERR_FAIL_COND_V(control_map.is_null(), Vector2i(-1, -1));
	ERR_FAIL_INDEX_V(p_vertex.x, control_map->get_width(), Vector2i(-1, -1));
	ERR_FAIL_INDEX_V(p_vertex.y, control_map->get_height(), Vector2i(-1, -1));
	return p_vertex;
}

void EretheaTerrainData::set_control(const Vector2i &p_vertex, int p_base, int p_overlay, real_t p_blend) {
	if (_control_index(p_vertex).x < 0) {
		return;
	}
	const int width = control_map->get_width();
	uint8_t *pixel = control_map->ptrw() + (p_vertex.y * width + p_vertex.x) * 4;
	pixel[0] = uint8_t(CLAMP(p_base, 0, MAX_MATERIALS - 1));
	pixel[1] = uint8_t(CLAMP(p_overlay, 0, MAX_MATERIALS - 1));
	pixel[2] = uint8_t(CLAMP(int(Math::round(p_blend * 255)), 0, 255));
	pixel[3] = 255;
}

int EretheaTerrainData::get_control_base(const Vector2i &p_vertex) const {
	if (_control_index(p_vertex).x < 0) {
		return 0;
	}
	return control_map->ptr()[(p_vertex.y * control_map->get_width() + p_vertex.x) * 4];
}

int EretheaTerrainData::get_control_overlay(const Vector2i &p_vertex) const {
	if (_control_index(p_vertex).x < 0) {
		return 0;
	}
	return control_map->ptr()[(p_vertex.y * control_map->get_width() + p_vertex.x) * 4 + 1];
}

real_t EretheaTerrainData::get_control_blend(const Vector2i &p_vertex) const {
	if (_control_index(p_vertex).x < 0) {
		return 0;
	}
	return real_t(control_map->ptr()[(p_vertex.y * control_map->get_width() + p_vertex.x) * 4 + 2]) / 255;
}

Vector<real_t> EretheaTerrainData::get_collision_heights() const {
	Vector<real_t> heights;
	ERR_FAIL_COND_V(height_map.is_null(), heights);
	const int count = height_map->get_width() * height_map->get_height();
	heights.resize(count);
	real_t *out = heights.ptrw();
	const float *in = reinterpret_cast<const float *>(height_map->ptr());
	for (int i = 0; i < count; i++) {
		out[i] = in[i];
	}
	return heights;
}

void EretheaTerrainData::notify_maps_changed() {
	if (height_map.is_valid()) {
		const float *heights = reinterpret_cast<const float *>(height_map->ptr());
		const int count = height_map->get_width() * height_map->get_height();
		float lowest = heights[0];
		float highest = heights[0];
		for (int i = 1; i < count; i++) {
			lowest = MIN(lowest, heights[i]);
			highest = MAX(highest, heights[i]);
		}
		min_height = lowest;
		max_height = highest;
	}
	emit_changed();
}

void EretheaTerrainData::_bind_methods() {
	ClassDB::bind_method(D_METHOD("create", "size", "vertex_spacing", "height"), &EretheaTerrainData::create, DEFVAL(1.0), DEFVAL(0.0));

	ClassDB::bind_method(D_METHOD("set_size", "size"), &EretheaTerrainData::set_size);
	ClassDB::bind_method(D_METHOD("get_size"), &EretheaTerrainData::get_size);
	ClassDB::bind_method(D_METHOD("get_resolution"), &EretheaTerrainData::get_resolution);
	ClassDB::bind_method(D_METHOD("set_vertex_spacing", "vertex_spacing"), &EretheaTerrainData::set_vertex_spacing);
	ClassDB::bind_method(D_METHOD("get_vertex_spacing"), &EretheaTerrainData::get_vertex_spacing);
	ClassDB::bind_method(D_METHOD("set_height_map", "height_map"), &EretheaTerrainData::set_height_map);
	ClassDB::bind_method(D_METHOD("get_height_map"), &EretheaTerrainData::get_height_map);
	ClassDB::bind_method(D_METHOD("set_control_map", "control_map"), &EretheaTerrainData::set_control_map);
	ClassDB::bind_method(D_METHOD("get_control_map"), &EretheaTerrainData::get_control_map);

	ClassDB::bind_method(D_METHOD("get_vertex_position", "vertex"), &EretheaTerrainData::get_vertex_position);
	ClassDB::bind_method(D_METHOD("is_inside", "x", "z"), &EretheaTerrainData::is_inside);
	ClassDB::bind_method(D_METHOD("get_vertex_height", "vertex"), &EretheaTerrainData::get_vertex_height);
	ClassDB::bind_method(D_METHOD("set_vertex_height", "vertex", "height"), &EretheaTerrainData::set_vertex_height);
	ClassDB::bind_method(D_METHOD("get_height", "x", "z"), &EretheaTerrainData::get_height);
	ClassDB::bind_method(D_METHOD("get_normal", "x", "z"), &EretheaTerrainData::get_normal);
	ClassDB::bind_method(D_METHOD("get_height_range"), &EretheaTerrainData::get_height_range);

	ClassDB::bind_method(D_METHOD("set_control", "vertex", "base", "overlay", "blend"), &EretheaTerrainData::set_control);
	ClassDB::bind_method(D_METHOD("get_control_base", "vertex"), &EretheaTerrainData::get_control_base);
	ClassDB::bind_method(D_METHOD("get_control_overlay", "vertex"), &EretheaTerrainData::get_control_overlay);
	ClassDB::bind_method(D_METHOD("get_control_blend", "vertex"), &EretheaTerrainData::get_control_blend);

	ClassDB::bind_method(D_METHOD("notify_maps_changed"), &EretheaTerrainData::notify_maps_changed);

	// The size is only for the inspector: what is saved is the maps.
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "size", PROPERTY_HINT_RANGE, "2,8192,0.1,or_greater,suffix:m", PROPERTY_USAGE_EDITOR), "set_size", "get_size");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "vertex_spacing", PROPERTY_HINT_RANGE, "0.01,64,0.01,or_greater,suffix:m"), "set_vertex_spacing", "get_vertex_spacing");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "height_map", PROPERTY_HINT_RESOURCE_TYPE, "Image", PROPERTY_USAGE_STORAGE), "set_height_map", "get_height_map");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "control_map", PROPERTY_HINT_RESOURCE_TYPE, "Image", PROPERTY_USAGE_STORAGE), "set_control_map", "get_control_map");

	BIND_CONSTANT(MAX_MATERIALS);
}

// There are no maps until create() or set_height_map(), so that the default
// values of the properties are empty.
EretheaTerrainData::EretheaTerrainData() {
}
