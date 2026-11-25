/**************************************************************************/
/*  erethea_terrain_brush.cpp                                             */
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

#include "erethea_terrain_brush.h"

#include "core/math/math_funcs.h"

namespace EretheaTerrainBrush {

real_t get_falloff(real_t p_distance, real_t p_radius) {
	if (p_radius <= 0 || p_distance >= p_radius) {
		return 0;
	}
	const real_t t = 1 - MAX(p_distance, real_t(0)) / p_radius;
	return t * t * (3 - 2 * t);
}

Rect2i get_dab_rect(const Ref<EretheaTerrainData> &p_data, const Vector2 &p_center, real_t p_radius) {
	ERR_FAIL_COND_V(p_data.is_null(), Rect2i());
	const Vector2i resolution = p_data->get_resolution();
	const real_t spacing = p_data->get_vertex_spacing();
	// The position of the center, in vertices.
	const Vector2 center(p_center.x / spacing + real_t(resolution.x - 1) * 0.5, p_center.y / spacing + real_t(resolution.y - 1) * 0.5);
	const real_t radius = p_radius / spacing;
	const Vector2i from(int(Math::floor(center.x - radius)), int(Math::floor(center.y - radius)));
	const Vector2i to(int(Math::ceil(center.x + radius)), int(Math::ceil(center.y + radius)));
	return Rect2i(from, to - from + Vector2i(1, 1)).intersection(Rect2i(Vector2i(), resolution));
}

Rect2i apply_dab(const Ref<EretheaTerrainData> &p_data, const Settings &p_settings, const Vector2 &p_center) {
	const Rect2i rect = get_dab_rect(p_data, p_center, p_settings.radius);
	if (rect.size.x <= 0 || rect.size.y <= 0) {
		return Rect2i();
	}
	const Vector2i resolution = p_data->get_resolution();

	// Smoothing reads the heights as they were before the dab.
	Ref<Image> source;
	Rect2i source_rect;
	if (p_settings.tool == TOOL_SMOOTH) {
		source_rect = Rect2i(rect.position - Vector2i(1, 1), rect.size + Vector2i(2, 2)).intersection(Rect2i(Vector2i(), resolution));
		source = p_data->get_height_map()->get_region(source_rect);
	}

	for (int z = rect.position.y; z < rect.position.y + rect.size.y; z++) {
		for (int x = rect.position.x; x < rect.position.x + rect.size.x; x++) {
			const Vector2i vertex(x, z);
			const real_t falloff = get_falloff(p_data->get_vertex_position(vertex).distance_to(p_center), p_settings.radius);
			if (falloff <= 0) {
				continue;
			}
			const real_t amount = CLAMP(p_settings.strength, real_t(0), real_t(1)) * falloff;
			switch (p_settings.tool) {
				case TOOL_RAISE: {
					p_data->set_vertex_height(vertex, p_data->get_vertex_height(vertex) + amount);
				} break;
				case TOOL_LOWER: {
					p_data->set_vertex_height(vertex, p_data->get_vertex_height(vertex) - amount);
				} break;
				case TOOL_SMOOTH: {
					real_t sum = 0;
					int count = 0;
					for (int dz = -1; dz <= 1; dz++) {
						for (int dx = -1; dx <= 1; dx++) {
							const Vector2i around = Vector2i(x + dx, z + dz) - source_rect.position;
							if (around.x >= 0 && around.y >= 0 && around.x < source_rect.size.x && around.y < source_rect.size.y) {
								sum += source->get_pixel(around.x, around.y).r;
								count++;
							}
						}
					}
					const real_t height = p_data->get_vertex_height(vertex);
					p_data->set_vertex_height(vertex, Math::lerp(height, sum / real_t(count), amount));
				} break;
				case TOOL_FLATTEN: {
					p_data->set_vertex_height(vertex, Math::lerp(p_data->get_vertex_height(vertex), p_settings.target_height, amount));
				} break;
				case TOOL_PAINT_BASE: {
					p_data->set_control(vertex, p_settings.material, p_data->get_control_overlay(vertex), p_data->get_control_blend(vertex));
				} break;
				case TOOL_PAINT_OVERLAY: {
					const bool same = p_data->get_control_overlay(vertex) == p_settings.material;
					const real_t blend = (same ? p_data->get_control_blend(vertex) : 0) + amount;
					p_data->set_control(vertex, p_data->get_control_base(vertex), p_settings.material, MIN(blend, real_t(1)));
				} break;
				case TOOL_ERASE_OVERLAY: {
					p_data->set_control(vertex, p_data->get_control_base(vertex), p_data->get_control_overlay(vertex), MAX(p_data->get_control_blend(vertex) - amount, real_t(0)));
				} break;
				case TOOL_MAX: {
				} break;
			}
		}
	}
	return rect;
}

bool raycast(const Ref<EretheaTerrainData> &p_data, const Vector3 &p_origin, const Vector3 &p_direction, real_t p_max_distance, Vector3 &r_hit) {
	ERR_FAIL_COND_V(p_data.is_null(), false);
	const Vector3 direction = p_direction.normalized();
	const real_t step = MAX(p_data->get_vertex_spacing() * real_t(0.5), real_t(0.01));
	// The last sample that was over the surface, inside of the terrain.
	bool has_previous = false;
	real_t previous = 0;
	for (real_t distance = 0; distance < p_max_distance + step; distance += step) {
		const real_t at = MIN(distance, p_max_distance);
		const Vector3 point = p_origin + direction * at;
		const real_t height = p_data->get_height(point.x, point.z);
		if (Math::is_nan(height)) {
			has_previous = false;
			continue;
		}
		if (point.y >= height) {
			has_previous = true;
			previous = at;
			continue;
		}
		if (has_previous) {
			// Went under the surface between the last sample and this one.
			real_t near = previous;
			real_t far = at;
			for (int i = 0; i < 24; i++) {
				const real_t middle = (near + far) * 0.5;
				const Vector3 test = p_origin + direction * middle;
				if (test.y >= p_data->get_height(test.x, test.z)) {
					near = middle;
				} else {
					far = middle;
				}
			}
			r_hit = p_origin + direction * ((near + far) * 0.5);
			r_hit.y = p_data->get_height(r_hit.x, r_hit.z);
			return true;
		}
		// Under the surface where the ray gets in the terrain: no hit from below.
		has_previous = false;
	}
	return false;
}

void Stroke::begin(const Ref<EretheaTerrainData> &p_data) {
	data = p_data;
	active = p_data.is_valid();
	rect = Rect2i();
	before = Dictionary();
}

void Stroke::dab(const Settings &p_settings, const Vector2 &p_center) {
	if (!active) {
		return;
	}
	const Rect2i dab_rect = get_dab_rect(data, p_center, p_settings.radius);
	if (dab_rect.size.x <= 0 || dab_rect.size.y <= 0) {
		return;
	}
	// Keeps what is going to change, as it is now, before changing it.
	if (before.is_empty()) {
		rect = dab_rect;
		before = data->get_region(rect);
	} else {
		const Rect2i merged = rect.merge(dab_rect);
		if (merged != rect) {
			Dictionary grown = data->get_region(merged);
			// What was kept already is older than what the data holds now.
			const Vector2i offset = rect.position - merged.position;
			Ref<Image> old_height = before["height"];
			Ref<Image> old_control = before["control"];
			Ref<Image> new_height = grown["height"];
			Ref<Image> new_control = grown["control"];
			new_height->blit_rect(old_height, Rect2i(Vector2i(), old_height->get_size()), offset);
			new_control->blit_rect(old_control, Rect2i(Vector2i(), old_control->get_size()), offset);
			rect = merged;
			before = grown;
		}
	}
	const Rect2i changed = apply_dab(data, p_settings, p_center);
	data->notify_region_changed(changed);
}

bool Stroke::end(Result &r_result) {
	const bool changed = active && !before.is_empty();
	if (changed) {
		r_result.rect = rect;
		r_result.before = before;
		r_result.after = data->get_region(rect);
	}
	active = false;
	data.unref();
	before = Dictionary();
	return changed;
}

} // namespace EretheaTerrainBrush
