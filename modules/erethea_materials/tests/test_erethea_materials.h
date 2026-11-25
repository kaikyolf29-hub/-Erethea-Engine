/**************************************************************************/
/*  test_erethea_materials.h                                              */
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

#include "../erethea_hair_builder.h"
#include "../erethea_hair_strands.h"

#include "scene/resources/mesh.h"
#include "tests/test_macros.h"

namespace TestEretheaMaterials {

// A strand of p_points points that goes down from p_start by p_step.
static void add_strand(Vector<Vector3> &r_points, Vector<int> &r_index, const Vector3 &p_start, int p_points, bool p_strip, float p_step = 0.1f) {
	const int first = r_points.size();
	for (int i = 0; i < p_points; i++) {
		r_points.push_back(p_start + Vector3(0, -p_step * i, 0));
	}
	for (int i = 0; i < p_points - 1; i++) {
		if (p_strip) {
			if (i == 0) {
				r_index.push_back(first);
			}
			r_index.push_back(first + i + 1);
		} else {
			r_index.push_back(first + i);
			r_index.push_back(first + i + 1);
		}
	}
}

static Ref<ArrayMesh> make_lines(const Vector<Vector3> &p_points, const Vector<int> &p_index, Mesh::PrimitiveType p_type, const Vector<int> &p_bones = Vector<int>(), const Vector<float> &p_weights = Vector<float>()) {
	Array arrays;
	arrays.resize(Mesh::ARRAY_MAX);
	arrays[Mesh::ARRAY_VERTEX] = p_points;
	if (!p_index.is_empty()) {
		arrays[Mesh::ARRAY_INDEX] = p_index;
	}
	if (!p_bones.is_empty()) {
		arrays[Mesh::ARRAY_BONES] = p_bones;
		arrays[Mesh::ARRAY_WEIGHTS] = p_weights;
	}
	Ref<ArrayMesh> mesh;
	mesh.instantiate();
	mesh->add_surface_from_arrays(p_type, arrays);
	return mesh;
}

TEST_CASE("[Modules][EretheaMaterials] Turns LINES strands into ribbons") {
	Vector<Vector3> points;
	Vector<int> index;
	add_strand(points, index, Vector3(0, 0, 0), 4, false); // 3 segments.
	add_strand(points, index, Vector3(1, 0, 0), 3, false); // 2 segments.
	Ref<ArrayMesh> source = make_lines(points, index, Mesh::PRIMITIVE_LINES);

	const EretheaHairBuilder::Result result = EretheaHairBuilder::build(source, 0.01f, 1.0f);
	CHECK(result.stats.source_strands == 2);
	CHECK(result.stats.strands == 2);
	// Two vertices per point; two triangles per segment.
	CHECK(result.stats.vertices == 2 * 7);
	CHECK(result.stats.triangles == 2 * 5);
	REQUIRE(result.surfaces.size() == 1);

	const Array &arrays = result.surfaces[0].arrays;
	const Vector<Vector3> vertices = arrays[Mesh::ARRAY_VERTEX];
	const Vector<int> triangles = arrays[Mesh::ARRAY_INDEX];
	const Vector<Vector2> uv = arrays[Mesh::ARRAY_TEX_UV];
	const Vector<float> tangents = arrays[Mesh::ARRAY_TANGENT];
	CHECK(vertices.size() == 14);
	CHECK(triangles.size() == 30);
	CHECK(tangents.size() == 14 * 4);
	// The two vertices of a point share the place and differ in the side.
	CHECK(vertices[0] == vertices[1]);
	CHECK(uv[0].x == -1.0f);
	CHECK(uv[1].x == 1.0f);
	// Root to tip, by length.
	CHECK(uv[0].y == 0.0f);
	CHECK(Math::is_equal_approx(uv[2].y, 1.0f / 3.0f));
	CHECK(Math::is_equal_approx(uv[6].y, 1.0f));
	// The tangent follows the strand, root to tip.
	CHECK(Math::is_equal_approx(tangents[1], -1.0f));
	// Strands are separate: no triangle joins the last point of the first
	// strand (vertices 6 and 7) with the first of the second (8 and 9).
	for (int t = 0; t < triangles.size(); t += 3) {
		const bool first = triangles[t] < 8 || triangles[t + 1] < 8 || triangles[t + 2] < 8;
		const bool second = triangles[t] >= 8 || triangles[t + 1] >= 8 || triangles[t + 2] >= 8;
		CHECK_FALSE((first && second));
	}
}

TEST_CASE("[Modules][EretheaMaterials] Turns LINE_STRIP strands into ribbons") {
	Vector<Vector3> points;
	Vector<int> index;
	add_strand(points, index, Vector3(0, 0, 0), 5, true);
	// Without indices, a strip is the points in order.
	Ref<ArrayMesh> with_index = make_lines(points, index, Mesh::PRIMITIVE_LINE_STRIP);
	Ref<ArrayMesh> without_index = make_lines(points, Vector<int>(), Mesh::PRIMITIVE_LINE_STRIP);
	for (const Ref<ArrayMesh> &source : { with_index, without_index }) {
		const EretheaHairBuilder::Result result = EretheaHairBuilder::build(source, 0.01f, 1.0f);
		CHECK(result.stats.strands == 1);
		CHECK(result.stats.vertices == 10);
		CHECK(result.stats.triangles == 8);
	}

	// Without indices, LINES are pairs of points.
	Vector<Vector3> pairs;
	for (int i = 0; i < 3; i++) {
		pairs.push_back(Vector3(i, 0, 0));
		pairs.push_back(Vector3(i, -1, 0));
	}
	const EretheaHairBuilder::Result lines = EretheaHairBuilder::build(make_lines(pairs, Vector<int>(), Mesh::PRIMITIVE_LINES), 0.01f, 1.0f);
	CHECK(lines.stats.strands == 3);
	CHECK(lines.stats.triangles == 6);
}

TEST_CASE("[Modules][EretheaMaterials] Ignores surfaces that aren't lines") {
	Ref<ArrayMesh> source;
	source.instantiate();
	Array arrays;
	arrays.resize(Mesh::ARRAY_MAX);
	Vector<Vector3> points = { Vector3(0, 0, 0), Vector3(1, 0, 0), Vector3(0, 1, 0) };
	arrays[Mesh::ARRAY_VERTEX] = points;
	source->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
	CHECK(EretheaHairBuilder::build(source, 0.01f, 1.0f).surfaces.is_empty());
	CHECK(EretheaHairBuilder::build(Ref<Mesh>(), 0.01f, 1.0f).surfaces.is_empty());
}

TEST_CASE("[Modules][EretheaMaterials] Copies bones and weights from the source points") {
	Vector<Vector3> points;
	Vector<int> index;
	add_strand(points, index, Vector3(0, 0, 0), 3, false);
	Vector<int> bones;
	Vector<float> weights;
	for (int i = 0; i < 3; i++) {
		bones.append_array({ i, i + 1, 0, 0 });
		weights.append_array({ 0.75f, 0.25f, 0.0f, 0.0f });
	}
	const EretheaHairBuilder::Result result = EretheaHairBuilder::build(make_lines(points, index, Mesh::PRIMITIVE_LINES, bones, weights), 0.01f, 1.0f);
	REQUIRE(result.surfaces.size() == 1);
	const Vector<int> out_bones = result.surfaces[0].arrays[Mesh::ARRAY_BONES];
	const Vector<float> out_weights = result.surfaces[0].arrays[Mesh::ARRAY_WEIGHTS];
	REQUIRE(out_bones.size() == 6 * 4);
	REQUIRE(out_weights.size() == 6 * 4);
	for (int v = 0; v < 6; v++) {
		// Both vertices of point v / 2 get its bones and weights.
		CHECK(out_bones[v * 4] == v / 2);
		CHECK(out_bones[v * 4 + 1] == v / 2 + 1);
		CHECK(out_weights[v * 4] == 0.75f);
		CHECK(out_weights[v * 4 + 1] == 0.25f);
	}
	CHECK((result.surfaces[0].flags & Mesh::ARRAY_FLAG_USE_8_BONE_WEIGHTS) == 0);

	// Without bones, the ribbons have none.
	const EretheaHairBuilder::Result plain = EretheaHairBuilder::build(make_lines(points, index, Mesh::PRIMITIVE_LINES), 0.01f, 1.0f);
	REQUIRE(plain.surfaces.size() == 1);
	CHECK(plain.surfaces[0].arrays[Mesh::ARRAY_BONES].get_type() == Variant::NIL);
}

TEST_CASE("[Modules][EretheaMaterials] Draws a fraction of the strands, spread evenly") {
	Vector<Vector3> points;
	Vector<int> index;
	for (int i = 0; i < 100; i++) {
		add_strand(points, index, Vector3(i, 0, 0), 2, false);
	}
	Ref<ArrayMesh> source = make_lines(points, index, Mesh::PRIMITIVE_LINES);

	CHECK(EretheaHairBuilder::build(source, 0.01f, 1.0f).stats.strands == 100);
	CHECK(EretheaHairBuilder::build(source, 0.01f, 0.5f).stats.strands == 50);
	CHECK(EretheaHairBuilder::build(source, 0.01f, 0.25f).stats.strands == 25);
	CHECK(EretheaHairBuilder::build(source, 0.01f, 0.0f).stats.strands == 0);
	const EretheaHairBuilder::Result half = EretheaHairBuilder::build(source, 0.01f, 0.5f);
	CHECK(half.stats.source_strands == 100);
	CHECK(half.stats.vertices == 50 * 4);

	// Evenly: every strand kept is followed by one left out.
	const Vector<Vector3> vertices = half.surfaces[0].arrays[Mesh::ARRAY_VERTEX];
	for (int s = 0; s < 50; s++) {
		CHECK(Math::is_equal_approx(vertices[s * 4].x, (float)(2 * s + 1)));
	}
	CHECK(EretheaHairBuilder::kept_strand_count(10, 0.35f) == 3);
}

TEST_CASE("[Modules][EretheaMaterials] The AABB holds the points and half the width") {
	Vector<Vector3> points;
	Vector<int> index;
	add_strand(points, index, Vector3(0, 0, 0), 3, false, 1.0f); // From (0, 0, 0) to (0, -2, 0).
	add_strand(points, index, Vector3(2, 0, 1), 2, false, 1.0f); // To (2, -1, 1).
	const EretheaHairBuilder::Result result = EretheaHairBuilder::build(make_lines(points, index, Mesh::PRIMITIVE_LINES), 0.2f, 1.0f);
	CHECK(result.aabb.position.is_equal_approx(Vector3(-0.1f, -2.1f, -0.1f)));
	CHECK(result.aabb.size.is_equal_approx(Vector3(2.2f, 2.2f, 1.2f)));
}

TEST_CASE("[Modules][EretheaMaterials] Hair strands node builds its mesh") {
	Vector<Vector3> points;
	Vector<int> index;
	for (int i = 0; i < 10; i++) {
		add_strand(points, index, Vector3(i * 0.1f, 0, 0), 4, false);
	}
	EretheaHairStrands *hair = memnew(EretheaHairStrands);
	hair->set_source_mesh(make_lines(points, index, Mesh::PRIMITIVE_LINES));
	hair->set_width(0.02f);
	hair->rebuild();
	CHECK(hair->get_source_strand_count() == 10);
	CHECK(hair->get_strand_count() == 10);
	REQUIRE(hair->get_mesh().is_valid());
	CHECK(hair->get_mesh()->get_surface_count() == 1);
	CHECK(hair->get_mesh()->surface_get_material(0).is_valid());
	CHECK(hair->get_aabb().size.x >= 0.9f + 0.02f - 0.001f);

	hair->set_strand_fraction(0.5f);
	hair->rebuild();
	CHECK(hair->get_strand_count() == 5);

	hair->set_source_mesh(Ref<Mesh>());
	hair->rebuild();
	CHECK(hair->get_strand_count() == 0);
	CHECK(hair->get_mesh()->get_surface_count() == 0);
	memdelete(hair);
}

} // namespace TestEretheaMaterials
