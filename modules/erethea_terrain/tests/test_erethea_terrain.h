/**************************************************************************/
/*  test_erethea_terrain.h                                                */
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

#include "../erethea_terrain_3d.h"
#include "../erethea_terrain_brush.h"
#include "../erethea_terrain_clipmap.h"
#include "../erethea_terrain_data.h"

#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "scene/3d/camera_3d.h"
#include "scene/main/scene_tree.h"
#include "scene/main/window.h"
#include "tests/test_macros.h"
#include "tests/test_utils.h"

#ifndef PHYSICS_3D_DISABLED
#include "scene/3d/physics/collision_shape_3d.h"
#include "scene/3d/physics/static_body_3d.h"
#include "scene/resources/3d/height_map_shape_3d.h"
#endif // PHYSICS_3D_DISABLED

namespace TestEretheaTerrain {

// A 5x4 vertex terrain, 2 m between the vertices (8 m by 6 m), with the height
// of the vertex (x, z) set to x + 10 * z.
static Ref<EretheaTerrainData> make_ramp() {
	Ref<EretheaTerrainData> data;
	data.instantiate();
	data->create(Vector2(8, 6), 2.0);
	for (int z = 0; z < 4; z++) {
		for (int x = 0; x < 5; x++) {
			data->set_vertex_height(Vector2i(x, z), x + 10 * z);
		}
	}
	data->notify_maps_changed();
	return data;
}

TEST_CASE("[Modules][EretheaTerrain] Data size follows the vertex spacing") {
	Ref<EretheaTerrainData> data = make_ramp();
	CHECK(data->get_resolution() == Vector2i(5, 4));
	CHECK(data->get_size().is_equal_approx(Vector2(8, 6)));
	CHECK(data->get_height_map()->get_format() == Image::FORMAT_RF);
	CHECK(data->get_control_map()->get_format() == Image::FORMAT_RGBA8);
	CHECK(data->get_control_map()->get_size() == data->get_height_map()->get_size());

	// The terrain is centered on the origin.
	CHECK(data->get_vertex_position(Vector2i(0, 0)).is_equal_approx(Vector2(-4, -3)));
	CHECK(data->get_vertex_position(Vector2i(4, 3)).is_equal_approx(Vector2(4, 3)));
	CHECK(data->is_inside(4, 3));
	CHECK_FALSE(data->is_inside(4.01, 0));
	CHECK_FALSE(data->is_inside(0, -3.01));

	// A bigger size keeps what was there.
	data->set_size(Vector2(12, 6));
	CHECK(data->get_resolution() == Vector2i(7, 4));
	CHECK(data->get_vertex_height(Vector2i(3, 2)) == doctest::Approx(23));
	CHECK(data->get_vertex_height(Vector2i(6, 2)) == doctest::Approx(0));
}

TEST_CASE("[Modules][EretheaTerrain] Height is interpolated between the vertices") {
	Ref<EretheaTerrainData> data = make_ramp();
	// On the vertices.
	CHECK(data->get_height(-4, -3) == doctest::Approx(0));
	CHECK(data->get_height(4, -3) == doctest::Approx(4));
	CHECK(data->get_height(-4, 3) == doctest::Approx(30));
	CHECK(data->get_height(4, 3) == doctest::Approx(34));
	CHECK(data->get_height(0, -1) == doctest::Approx(12));
	// Between them: the height is x + 10 * z in vertices, so bilinear is exact.
	CHECK(data->get_height(-3, -2) == doctest::Approx(0.5 + 5));
	CHECK(data->get_height(1, 0.5) == doctest::Approx(2.5 + 10 * 1.75));
	CHECK(data->get_height(3.9, 2.9) == doctest::Approx(3.95 + 10 * 2.95));

	// A bump in one vertex: bilinear between the 4 around it.
	data->set_vertex_height(Vector2i(2, 1), 100);
	CHECK(data->get_height(0, -1) == doctest::Approx(100));
	// Between two vertices on a side of the bump: the middle of 100 and 22.
	CHECK(data->get_height(0, 0) == doctest::Approx(61));

	// Nothing outside of the terrain.
	CHECK(Math::is_nan(data->get_height(4.5, 0)));
	CHECK(Math::is_nan(data->get_height(0, -3.5)));
}

TEST_CASE("[Modules][EretheaTerrain] Normal follows the slope") {
	Ref<EretheaTerrainData> data;
	data.instantiate();
	data->create(Vector2(8, 8), 1.0);
	CHECK(data->get_normal(0, 0).is_equal_approx(Vector3(0, 1, 0)));

	// A slope of 1 along x: height = x.
	for (int z = 0; z < 9; z++) {
		for (int x = 0; x < 9; x++) {
			data->set_vertex_height(Vector2i(x, z), x);
		}
	}
	data->notify_maps_changed();
	const Vector3 expected = Vector3(-1, 1, 0).normalized();
	CHECK(data->get_normal(0, 0).is_equal_approx(expected));
	CHECK(data->get_normal(2.3, -1.7).is_equal_approx(expected));
	// At the edge, it uses the one side that exists.
	CHECK(data->get_normal(4, 0).is_equal_approx(expected));
	CHECK(data->get_normal(-4, 4).is_equal_approx(expected));
	CHECK(data->get_normal(0, 0).y > 0);
}

TEST_CASE("[Modules][EretheaTerrain] Control map keeps materials and blend") {
	Ref<EretheaTerrainData> data = make_ramp();
	CHECK(data->get_control_base(Vector2i(1, 1)) == 0);
	CHECK(data->get_control_overlay(Vector2i(1, 1)) == 0);
	CHECK(data->get_control_blend(Vector2i(1, 1)) == doctest::Approx(0));

	data->set_control(Vector2i(1, 1), 31, 7, 0.5);
	CHECK(data->get_control_base(Vector2i(1, 1)) == 31);
	CHECK(data->get_control_overlay(Vector2i(1, 1)) == 7);
	CHECK(data->get_control_blend(Vector2i(1, 1)) == doctest::Approx(0.5).epsilon(0.01));
	// Materials go from 0 to 31, and the blend from 0 to 1.
	data->set_control(Vector2i(2, 2), 99, -4, 3.0);
	CHECK(data->get_control_base(Vector2i(2, 2)) == 31);
	CHECK(data->get_control_overlay(Vector2i(2, 2)) == 0);
	CHECK(data->get_control_blend(Vector2i(2, 2)) == doctest::Approx(1));
	// The neighbors are untouched.
	CHECK(data->get_control_base(Vector2i(0, 1)) == 0);
}

TEST_CASE("[Modules][EretheaTerrain] Data saves and loads as a resource") {
	Ref<EretheaTerrainData> data = make_ramp();
	data->set_control(Vector2i(3, 2), 5, 9, 0.25);

	for (const String extension : { "res", "tres" }) {
		const String path = TestUtils::get_temp_path("erethea_terrain_data." + extension);
		REQUIRE(ResourceSaver::save(data, path) == OK);

		Error error = OK;
		Ref<EretheaTerrainData> loaded = ResourceLoader::load(path, "EretheaTerrainData", ResourceFormatLoader::CACHE_MODE_IGNORE, &error);
		REQUIRE(error == OK);
		REQUIRE(loaded.is_valid());
		CHECK(loaded->get_resolution() == Vector2i(5, 4));
		CHECK(loaded->get_vertex_spacing() == doctest::Approx(2.0));
		CHECK(loaded->get_size().is_equal_approx(Vector2(8, 6)));
		CHECK(loaded->get_height(4, 3) == doctest::Approx(34));
		CHECK(loaded->get_height(-3, -2) == doctest::Approx(5.5));
		CHECK(loaded->get_height_range().is_equal_approx(Vector2(0, 34)));
		CHECK(loaded->get_control_base(Vector2i(3, 2)) == 5);
		CHECK(loaded->get_control_overlay(Vector2i(3, 2)) == 9);
		CHECK(loaded->get_control_blend(Vector2i(3, 2)) == doctest::Approx(0.25).epsilon(0.01));
	}
}

TEST_CASE("[Modules][EretheaTerrain] Collision heights follow the height map") {
	Ref<EretheaTerrainData> data = make_ramp();
	const Vector<real_t> heights = data->get_collision_heights();
	// Row after row of x, one row per z, like HeightMapShape3D.
	REQUIRE(heights.size() == 5 * 4);
	CHECK(heights[0] == doctest::Approx(0));
	CHECK(heights[4] == doctest::Approx(4));
	CHECK(heights[5] == doctest::Approx(10));
	CHECK(heights[5 * 3 + 4] == doctest::Approx(34));
}

// A flat terrain of 32 x 32 m, 1 m between the vertices (33 x 33 vertices),
// with a copy of its heights and materials to compare with.
static Ref<EretheaTerrainData> make_flat(real_t p_height = 5.0) {
	Ref<EretheaTerrainData> data;
	data.instantiate();
	data->create(Vector2(32, 32), 1.0, p_height);
	return data;
}

static bool maps_equal(const Ref<EretheaTerrainData> &p_a, const Ref<EretheaTerrainData> &p_b) {
	return p_a->get_height_map()->get_data() == p_b->get_height_map()->get_data() && p_a->get_control_map()->get_data() == p_b->get_control_map()->get_data();
}

TEST_CASE("[Modules][EretheaTerrain] Brush falloff goes from 1 at the center to 0 at the radius") {
	using namespace EretheaTerrainBrush;
	CHECK(get_falloff(0, 4) == doctest::Approx(1));
	CHECK(get_falloff(4, 4) == doctest::Approx(0));
	CHECK(get_falloff(9, 4) == doctest::Approx(0));
	CHECK(get_falloff(2, 4) == doctest::Approx(0.5));
	CHECK(get_falloff(1, 4) > get_falloff(3, 4));
	CHECK(get_falloff(1, 0) == doctest::Approx(0));
}

TEST_CASE("[Modules][EretheaTerrain] A dab rect is the vertices around it, inside the maps") {
	using namespace EretheaTerrainBrush;
	Ref<EretheaTerrainData> data = make_flat();
	// The vertex (16, 16) is at the origin.
	CHECK(get_dab_rect(data, Vector2(0, 0), 3) == Rect2i(13, 13, 7, 7));
	// Cut at the edge of the maps.
	CHECK(get_dab_rect(data, Vector2(-16, -16), 3) == Rect2i(0, 0, 4, 4));
	CHECK(get_dab_rect(data, Vector2(16, 16), 3) == Rect2i(29, 29, 4, 4));
	// Outside of the terrain: empty.
	CHECK(get_dab_rect(data, Vector2(100, 0), 3).size.x <= 0);
}

TEST_CASE("[Modules][EretheaTerrain] A raise dab changes only what is inside the radius") {
	using namespace EretheaTerrainBrush;
	Ref<EretheaTerrainData> data = make_flat();
	Ref<EretheaTerrainData> original = make_flat();

	Settings settings;
	settings.tool = TOOL_RAISE;
	settings.radius = 4;
	settings.strength = 1;
	const Rect2i rect = apply_dab(data, settings, Vector2(0, 0));
	CHECK(rect == Rect2i(12, 12, 9, 9));

	for (int z = 0; z < 33; z++) {
		for (int x = 0; x < 33; x++) {
			const Vector2i vertex(x, z);
			const real_t distance = data->get_vertex_position(vertex).length();
			const real_t change = data->get_vertex_height(vertex) - original->get_vertex_height(vertex);
			if (distance >= 4) {
				CHECK(change == doctest::Approx(0));
			}
		}
	}
	// 1 m at the center, less around it, never below 0.
	CHECK(data->get_vertex_height(Vector2i(16, 16)) == doctest::Approx(6));
	CHECK(data->get_vertex_height(Vector2i(18, 16)) == doctest::Approx(5.5));
	CHECK(data->get_vertex_height(Vector2i(17, 16)) > data->get_vertex_height(Vector2i(18, 16)));
	CHECK(data->get_vertex_height(Vector2i(20, 16)) == doctest::Approx(5));
	CHECK(data->get_vertex_height(Vector2i(12, 12)) == doctest::Approx(5));

	// And lower takes it back.
	settings.tool = TOOL_LOWER;
	(void)apply_dab(data, settings, Vector2(0, 0));
	CHECK(maps_equal(data, original));
}

TEST_CASE("[Modules][EretheaTerrain] Smooth and flatten move the heights toward their goals") {
	using namespace EretheaTerrainBrush;
	Ref<EretheaTerrainData> data = make_flat(0.0);
	data->set_vertex_height(Vector2i(16, 16), 9);
	data->notify_maps_changed();

	Settings settings;
	settings.tool = TOOL_SMOOTH;
	settings.radius = 6;
	settings.strength = 1;
	(void)apply_dab(data, settings, Vector2(0, 0));
	// The peak was the only one above 0, so it comes down to the average of its
	// 3x3 vertices, and its neighbors go up to the average of theirs.
	CHECK(data->get_vertex_height(Vector2i(16, 16)) == doctest::Approx(1.0));
	CHECK(data->get_vertex_height(Vector2i(17, 16)) > 0.1);
	CHECK(data->get_vertex_height(Vector2i(17, 16)) < 1.0);
	// Outside the radius it stays flat.
	CHECK(data->get_vertex_height(Vector2i(25, 16)) == doctest::Approx(0));

	settings.tool = TOOL_FLATTEN;
	settings.target_height = 3;
	settings.strength = 1;
	(void)apply_dab(data, settings, Vector2(0, 0));
	CHECK(data->get_vertex_height(Vector2i(16, 16)) == doctest::Approx(3));
	CHECK(data->get_vertex_height(Vector2i(25, 16)) == doctest::Approx(0));
}

TEST_CASE("[Modules][EretheaTerrain] Paint brushes set the materials and the weight") {
	using namespace EretheaTerrainBrush;
	Ref<EretheaTerrainData> data = make_flat();
	const Vector2i center(16, 16);

	Settings settings;
	settings.radius = 4;
	settings.strength = 1;
	settings.material = 7;
	settings.tool = TOOL_PAINT_BASE;
	(void)apply_dab(data, settings, Vector2(0, 0));
	CHECK(data->get_control_base(center) == 7);
	CHECK(data->get_control_base(Vector2i(17, 16)) == 7);
	CHECK(data->get_control_base(Vector2i(20, 16)) == 0);
	// The overlay is untouched.
	CHECK(data->get_control_overlay(center) == 0);
	CHECK(data->get_control_blend(center) == doctest::Approx(0));

	settings.material = 12;
	settings.strength = 0.4;
	settings.tool = TOOL_PAINT_OVERLAY;
	(void)apply_dab(data, settings, Vector2(0, 0));
	CHECK(data->get_control_overlay(center) == 12);
	CHECK(data->get_control_blend(center) == doctest::Approx(0.4).epsilon(0.01));
	CHECK(data->get_control_base(center) == 7);
	// A second dab with the same material adds up, to at most 1.
	(void)apply_dab(data, settings, Vector2(0, 0));
	(void)apply_dab(data, settings, Vector2(0, 0));
	CHECK(data->get_control_blend(center) == doctest::Approx(1).epsilon(0.01));

	// Another overlay material starts its weight again.
	settings.material = 3;
	(void)apply_dab(data, settings, Vector2(0, 0));
	CHECK(data->get_control_overlay(center) == 3);
	CHECK(data->get_control_blend(center) == doctest::Approx(0.4).epsilon(0.01));

	settings.tool = TOOL_ERASE_OVERLAY;
	settings.strength = 1;
	(void)apply_dab(data, settings, Vector2(0, 0));
	CHECK(data->get_control_blend(center) == doctest::Approx(0));
	CHECK(data->get_control_base(center) == 7);
}

TEST_CASE("[Modules][EretheaTerrain] A stroke undoes and redoes exactly what it changed") {
	using namespace EretheaTerrainBrush;
	Ref<EretheaTerrainData> data = make_flat();
	data->set_vertex_height(Vector2i(3, 3), 11);
	data->set_control(Vector2i(20, 8), 4, 5, 0.5);
	data->notify_maps_changed();
	Ref<EretheaTerrainData> original = make_flat();
	original->set_vertex_height(Vector2i(3, 3), 11);
	original->set_control(Vector2i(20, 8), 4, 5, 0.5);
	original->notify_maps_changed();

	Settings settings;
	settings.tool = TOOL_RAISE;
	settings.radius = 3;
	settings.strength = 1;

	Stroke stroke;
	stroke.begin(data);
	CHECK(stroke.is_active());
	// A drag to the right: the changed area grows with each dab.
	for (int i = 0; i < 8; i++) {
		stroke.dab(settings, Vector2(-8 + i * 2, 0));
	}
	Stroke::Result result;
	REQUIRE(stroke.end(result));
	CHECK_FALSE(stroke.is_active());
	CHECK_FALSE(maps_equal(data, original));

	// The stroke only kept the part that the dabs reach: 3 m around the line
	// from x = -8 to 6, at z = 0, which is 21 by 7 vertices.
	CHECK(result.rect == Rect2i(5, 13, 21, 7));
	Ref<Image> kept = result.before["height"];
	CHECK(kept->get_size() == Vector2i(21, 7));

	// Nothing outside of the area changed, and not the vertices that were already modified.
	for (int z = 0; z < 33; z++) {
		for (int x = 0; x < 33; x++) {
			if (!result.rect.has_point(Vector2i(x, z))) {
				CHECK(data->get_vertex_height(Vector2i(x, z)) == doctest::Approx(original->get_vertex_height(Vector2i(x, z))));
			}
		}
	}
	CHECK(data->get_control_overlay(Vector2i(20, 8)) == 5);
	CHECK(data->get_vertex_height(Vector2i(3, 3)) == doctest::Approx(11));

	// The undo step puts the maps back as they were, and the redo step as they ended.
	const Vector<uint8_t> sculpted = data->get_height_map()->get_data();
	data->set_region(result.rect, result.before);
	CHECK(maps_equal(data, original));
	data->set_region(result.rect, result.after);
	CHECK(data->get_height_map()->get_data() == sculpted);
	data->set_region(result.rect, result.before);
	CHECK(maps_equal(data, original));

	// The height range is the real one again after the undo.
	CHECK(data->get_height_range().is_equal_approx(Vector2(5, 11)));
}

TEST_CASE("[Modules][EretheaTerrain] A stroke outside of the terrain changes nothing") {
	using namespace EretheaTerrainBrush;
	Ref<EretheaTerrainData> data = make_flat();
	Settings settings;
	Stroke stroke;
	stroke.begin(data);
	stroke.dab(settings, Vector2(500, 500));
	Stroke::Result result;
	CHECK_FALSE(stroke.end(result));
	CHECK(maps_equal(data, make_flat()));
}

TEST_CASE("[Modules][EretheaTerrain] Regions are copies that fit back") {
	Ref<EretheaTerrainData> data = make_ramp();
	Dictionary region = data->get_region(Rect2i(1, 1, 3, 2));
	Ref<Image> heights = region["height"];
	REQUIRE(heights.is_valid());
	CHECK(heights->get_size() == Vector2i(3, 2));
	CHECK(heights->get_pixel(0, 0).r == doctest::Approx(11));
	// A copy: changing the data doesn't change it.
	data->set_vertex_height(Vector2i(1, 1), 100);
	CHECK(heights->get_pixel(0, 0).r == doctest::Approx(11));
	data->set_region(Rect2i(1, 1, 3, 2), region);
	CHECK(data->get_vertex_height(Vector2i(1, 1)) == doctest::Approx(11));
	// Cut at the edge of the maps.
	Ref<Image> cut = data->get_region(Rect2i(3, 2, 10, 10))["height"];
	CHECK(cut->get_size() == Vector2i(2, 2));
}

TEST_CASE("[Modules][EretheaTerrain] A ray finds where it enters the ground") {
	using namespace EretheaTerrainBrush;
	Ref<EretheaTerrainData> data = make_ramp();
	Vector3 hit;
	// Straight down, on a vertex and between two.
	REQUIRE(raycast(data, Vector3(0, 100, -1), Vector3(0, -1, 0), 500, hit));
	CHECK(hit.x == doctest::Approx(0));
	CHECK(hit.z == doctest::Approx(-1));
	CHECK(hit.y == doctest::Approx(data->get_height(0, -1)));
	REQUIRE(raycast(data, Vector3(1.3, 100, 0.7), Vector3(0, -1, 0), 500, hit));
	CHECK(hit.y == doctest::Approx(data->get_height(1.3, 0.7)).epsilon(0.001));
	// Slanted, from a camera: the point is on the surface, and on the ray.
	const Vector3 origin(-20, 80, 2.5);
	const Vector3 direction = (Vector3(1, data->get_height(1, 0), 0) - origin).normalized();
	REQUIRE(raycast(data, origin, direction, 500, hit));
	CHECK(hit.y == doctest::Approx(data->get_height(hit.x, hit.z)).epsilon(0.001));
	CHECK(((hit - origin).normalized()).is_equal_approx(direction));
	// Misses: beside the terrain, pointing up, too short, or from under the ground.
	CHECK_FALSE(raycast(data, Vector3(50, 100, 0), Vector3(0, -1, 0), 500, hit));
	CHECK_FALSE(raycast(data, Vector3(0, 100, 0), Vector3(0, 1, 0), 500, hit));
	CHECK_FALSE(raycast(data, Vector3(0, 100, 0), Vector3(0, -1, 0), 50, hit));
	CHECK_FALSE(raycast(data, Vector3(0, -50, 0), Vector3(0, -1, 0), 500, hit));
}

TEST_CASE("[Modules][EretheaTerrain] Clipmap levels double their spacing") {
	using namespace EretheaTerrainClipmap;
	CHECK(get_level_spacing(0.5, 0) == doctest::Approx(0.5));
	CHECK(get_level_spacing(0.5, 1) == doctest::Approx(1));
	CHECK(get_level_spacing(0.5, 4) == doctest::Approx(8));
	CHECK(get_level_half_extent(1, 0, 64) == doctest::Approx(32));
	CHECK(get_level_half_extent(1, 3, 64) == doctest::Approx(256));
	// Level 0 has no hole, the others have the extent of the level before.
	CHECK(get_hole_half_extent(1, 0, 64) == doctest::Approx(0));
	CHECK(get_hole_half_extent(1, 1, 64) == doctest::Approx(32));
	CHECK(get_hole_half_extent(1, 3, 64) == doctest::Approx(128));
}

TEST_CASE("[Modules][EretheaTerrain] Clipmap levels snap to the grid") {
	using namespace EretheaTerrainClipmap;
	const Vector2 origin(-0.5, 0);
	// Snaps to the lattice, to the nearest point.
	CHECK(snap_to_lattice(Vector2(3.4, -7.9), 2, Vector2()).is_equal_approx(Vector2(4, -8)));
	CHECK(snap_to_lattice(Vector2(0.4, 0.4), 2, origin).is_equal_approx(Vector2(-0.5, 0)));
	CHECK(snap_to_lattice(Vector2(1.6, 0.4), 2, origin).is_equal_approx(Vector2(1.5, 0)));
	// A level moves one step of the next level at a time.
	CHECK(get_level_center(Vector2(0.9, 0), 1, 0, Vector2()).is_equal_approx(Vector2(0, 0)));
	CHECK(get_level_center(Vector2(1.1, 0), 1, 0, Vector2()).is_equal_approx(Vector2(2, 0)));
	CHECK(get_level_center(Vector2(1.1, 0), 1, 2, Vector2()).is_equal_approx(Vector2(0, 0)));
	CHECK(get_level_center(Vector2(4.1, 0), 1, 2, Vector2()).is_equal_approx(Vector2(8, 0)));
}

TEST_CASE("[Modules][EretheaTerrain] Each clipmap hole fits the grid of its level") {
	using namespace EretheaTerrainClipmap;
	// For any camera, the edge of the finer level has to be on the vertices of the
	// level around it, or there would be a gap or an overlap at the hole.
	const Vector2 origins[] = { Vector2(), Vector2(-31.5, -31.5), Vector2(-0.25, 3) };
	const real_t spacings[] = { 0.5, 1, 2.5 };
	for (const Vector2 &origin : origins) {
		for (const real_t spacing : spacings) {
			for (int step = 0; step < 200; step++) {
				const Vector2 camera(Math::sin(step * 0.7) * 300 + step * 1.37, Math::cos(step * 1.3) * 200 - step * 0.91);
				for (int level = 1; level < 8; level++) {
					const real_t level_spacing = get_level_spacing(spacing, level);
					const Vector2 center = get_level_center(camera, spacing, level, origin);
					const Vector2 inner_center = get_level_center(camera, spacing, level - 1, origin);
					const real_t hole = get_hole_half_extent(spacing, level, 64);
					for (const Vector2 edge : { inner_center - Vector2(hole, hole), inner_center + Vector2(hole, hole) }) {
						const Vector2 cells = (edge - center) / level_spacing;
						// The level's vertices are on the lattice of the origin.
						const Vector2 lattice = (edge - origin) / level_spacing;
						CHECK(Math::abs(lattice.x - Math::round(lattice.x)) < 0.0001);
						CHECK(Math::abs(lattice.y - Math::round(lattice.y)) < 0.0001);
						// And the hole is inside the level, with a ring around it.
						CHECK(Math::abs(cells.x) < 32);
						CHECK(Math::abs(cells.y) < 32);
					}
					// The level's own vertices are on the lattice too.
					const Vector2 own = (center - origin) / level_spacing;
					CHECK(Math::abs(own.x - Math::round(own.x)) < 0.0001);
					CHECK(Math::abs(own.y - Math::round(own.y)) < 0.0001);
				}
			}
		}
	}
}

TEST_CASE("[Modules][EretheaTerrain] Clipmap mesh has the grid and the skirt") {
	using namespace EretheaTerrainClipmap;
	const int quads = 8;
	const LevelMesh mesh = build_level_mesh(quads, 2.0);
	CHECK(mesh.vertices.size() == get_level_vertex_count(quads));
	CHECK(mesh.uvs.size() == mesh.vertices.size());
	CHECK(mesh.indices.size() == get_level_index_count(quads));
	CHECK(get_level_vertex_count(quads) == 9 * 9 + 4 * 9);
	CHECK(get_level_index_count(quads) == 8 * 8 * 6 + 4 * 8 * 6);

	int skirt_vertices = 0;
	for (int i = 0; i < mesh.vertices.size(); i++) {
		const Vector3 vertex = mesh.vertices[i];
		CHECK(vertex.y == doctest::Approx(0));
		CHECK(Math::abs(vertex.x) <= 8.0001);
		CHECK(Math::abs(vertex.z) <= 8.0001);
		if (mesh.uvs[i].x > 0.5) {
			skirt_vertices++;
			// The skirt is on the edge.
			CHECK((Math::abs(Math::abs(vertex.x) - 8) < 0.0001 || Math::abs(Math::abs(vertex.z) - 8) < 0.0001));
		}
	}
	CHECK(skirt_vertices == 4 * 9);

	// The odd vertices of the edge, and their skirt, say which way their line goes.
	int along_x = 0;
	int along_z = 0;
	for (int i = 0; i < mesh.vertices.size(); i++) {
		if (mesh.uvs[i].y == 1) {
			along_x++;
			CHECK(Math::abs(Math::abs(mesh.vertices[i].z) - 8) < 0.0001);
		} else if (mesh.uvs[i].y == 2) {
			along_z++;
			CHECK(Math::abs(Math::abs(mesh.vertices[i].x) - 8) < 0.0001);
		} else {
			CHECK(mesh.uvs[i].y == 0);
		}
	}
	// 4 odd vertices on each of the 2 edges, with the skirt copy of each.
	CHECK(along_x == 2 * 4 * 2);
	CHECK(along_z == 2 * 4 * 2);
	for (int i = 0; i < mesh.indices.size(); i++) {
		CHECK(mesh.indices[i] >= 0);
		CHECK(mesh.indices[i] < mesh.vertices.size());
	}

	// The top faces up, and the skirt faces out, with the clockwise winding of Godot.
	// The skirt is flat here (the shader lowers it), so its triangles are checked
	// by the direction from the center to their edge.
	const int grid_indices = quads * quads * 6;
	for (int i = 0; i < grid_indices; i += 3) {
		const Plane plane(mesh.vertices[mesh.indices[i]], mesh.vertices[mesh.indices[i + 1]], mesh.vertices[mesh.indices[i + 2]]);
		CHECK(plane.normal.y > 0.99);
	}
	for (int i = grid_indices; i < mesh.indices.size(); i += 3) {
		// The shader lowers the skirt vertices: lower them the same here.
		Vector3 corners[3];
		Vector3 middle;
		for (int j = 0; j < 3; j++) {
			corners[j] = mesh.vertices[mesh.indices[i + j]];
			if (mesh.uvs[mesh.indices[i + j]].x > 0.5) {
				corners[j].y -= 1;
			}
			middle += corners[j] / 3;
		}
		const Plane plane(corners[0], corners[1], corners[2]);
		CHECK(Math::abs(plane.normal.y) < 0.0001);
		const Vector3 outward = Vector3(middle.x, 0, middle.z).normalized();
		// The triangles of a corner point to one side or the other of the center, but never inward.
		CHECK(plane.normal.dot(outward) > 0.1);
	}
}

TEST_CASE("[Modules][EretheaTerrain] Clipmap bounds contain the level and its skirt") {
	using namespace EretheaTerrainClipmap;
	const AABB bounds = get_level_bounds(Vector2(10, -20), 32, -5, 40, 4);
	CHECK(bounds.position.is_equal_approx(Vector3(-22, -9, -52)));
	CHECK(bounds.size.is_equal_approx(Vector3(64, 49, 64)));
	// A flat terrain still has a volume.
	CHECK(get_level_bounds(Vector2(), 8, 3, 3, 0).size.y > 0);
}

TEST_CASE("[Modules][EretheaTerrain][SceneTree] Terrain follows the camera and makes a collision shape") {
	Ref<EretheaTerrainData> data = make_ramp();

	Camera3D *camera = memnew(Camera3D);
	SceneTree::get_singleton()->get_root()->add_child(camera);
	camera->set_global_position(Vector3(100.3, 20, -37.2));
	camera->make_current();

	EretheaTerrain3D *terrain = memnew(EretheaTerrain3D);
	terrain->set_clipmap_levels(4);
	terrain->set_mesh_size(16);
	terrain->set_data(data);
	terrain->set_collision_layer(1 << 3);
	terrain->set_collision_mask(1 << 4);
	SceneTree::get_singleton()->get_root()->add_child(terrain);

	CHECK(terrain->get_clipmap_levels() == 4);
	CHECK(terrain->get_mesh_size() == 16);
	// A mesh size must be a multiple of 4.
	terrain->set_mesh_size(18);
	CHECK(terrain->get_mesh_size() == 16);
	terrain->set_clipmap_levels(99);
	CHECK(terrain->get_clipmap_levels() == EretheaTerrainClipmap::MAX_LEVELS);
	terrain->set_clipmap_levels(4);

	// Each level is centered near the camera, on a vertex of the level after it.
	const PackedVector2Array centers = terrain->get_level_centers();
	REQUIRE(centers.size() == 4);
	for (int level = 0; level < 4; level++) {
		const real_t spacing = 2.0 * real_t(1 << (level + 1));
		const Vector2 origin = data->get_vertex_position(Vector2i(0, 0));
		const Vector2 cells = (centers[level] - origin) / spacing;
		CHECK(Math::abs(cells.x - Math::round(cells.x)) < 0.0001);
		CHECK(Math::abs(cells.y - Math::round(cells.y)) < 0.0001);
		CHECK(centers[level].distance_to(Vector2(100.3, -37.2)) <= spacing);
	}

	// When the terrain moves, the camera is somewhere else for it.
	terrain->set_global_position(Vector3(100, 0, -50));
	SceneTree::get_singleton()->flush_transform_notifications();
	const PackedVector2Array moved = terrain->get_level_centers();
	REQUIRE(moved.size() == 4);
	CHECK(moved[0].distance_to(Vector2(0.3, 12.8)) <= 4.0);

#ifndef PHYSICS_3D_DISABLED
	// The collision is an internal body with the height map shape.
	StaticBody3D *body = Object::cast_to<StaticBody3D>(terrain->get_child(0, true));
	REQUIRE(body);
	CHECK(body->get_collision_layer() == (1u << 3));
	CHECK(body->get_collision_mask() == (1u << 4));
	CollisionShape3D *collision_shape = Object::cast_to<CollisionShape3D>(body->get_child(0, true));
	REQUIRE(collision_shape);
	Ref<HeightMapShape3D> shape = collision_shape->get_shape();
	REQUIRE(shape.is_valid());
	CHECK(shape->get_map_width() == 5);
	CHECK(shape->get_map_depth() == 4);
	CHECK(shape->get_map_data().size() == 5 * 4);
	CHECK(shape->get_map_data()[5 * 3 + 4] == doctest::Approx(34));
	// The shape has one meter between its heights: the node scales it.
	CHECK(collision_shape->get_scale().is_equal_approx(Vector3(2, 1, 2)));
	CHECK_FALSE(collision_shape->is_disabled());

	terrain->set_collision_enabled(false);
	CHECK(collision_shape->is_disabled());

	// A change in the data changes the shape.
	data->create(Vector2(16, 10), 2.0, 7.0);
	CHECK(shape->get_map_width() == 9);
	CHECK(shape->get_map_depth() == 6);
	CHECK(shape->get_map_data()[0] == doctest::Approx(7));
#endif // PHYSICS_3D_DISABLED

	memdelete(terrain);
	memdelete(camera);
}

} // namespace TestEretheaTerrain
