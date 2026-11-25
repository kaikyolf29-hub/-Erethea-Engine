/**************************************************************************/
/*  test_erethea_dense_geometry.h                                         */
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

#include "../erethea_dense_geometry.h"

#include "scene/3d/importer_mesh_instance_3d.h"
#include "scene/3d/node_3d.h"
#include "scene/resources/material.h"
#include "tests/test_macros.h"

namespace TestEretheaDenseGeometry {

// A flat grid of p_size x p_size quads on the XZ plane, one unit each. Each
// vertex's UV and color tell where it is, to check that the attributes follow
// the vertices into the clusters.
static Ref<ImporterMesh> make_grid(int p_size, const Ref<Material> &p_material, bool p_blend_shape = false) {
	PackedVector3Array vertices;
	PackedVector3Array normals;
	PackedVector2Array uvs;
	PackedColorArray colors;
	PackedInt32Array indices;
	for (int z = 0; z <= p_size; z++) {
		for (int x = 0; x <= p_size; x++) {
			vertices.push_back(Vector3(x, 0, z));
			normals.push_back(Vector3(0, 1, 0));
			uvs.push_back(Vector2(x, z) / p_size);
			colors.push_back(Color(float(x) / p_size, float(z) / p_size, 0));
		}
	}
	for (int z = 0; z < p_size; z++) {
		for (int x = 0; x < p_size; x++) {
			const int i = z * (p_size + 1) + x;
			indices.append_array({ i, i + 1, i + p_size + 1, i + 1, i + p_size + 2, i + p_size + 1 });
		}
	}
	Array arrays;
	arrays.resize(Mesh::ARRAY_MAX);
	arrays[Mesh::ARRAY_VERTEX] = vertices;
	arrays[Mesh::ARRAY_NORMAL] = normals;
	arrays[Mesh::ARRAY_TEX_UV] = uvs;
	arrays[Mesh::ARRAY_COLOR] = colors;
	arrays[Mesh::ARRAY_INDEX] = indices;

	Ref<ImporterMesh> mesh;
	mesh.instantiate();
	mesh->set_name("grid");
	TypedArray<Array> blend_shapes;
	if (p_blend_shape) {
		mesh->add_blend_shape("raised");
		Array shape;
		shape.resize(Mesh::ARRAY_MAX);
		shape[Mesh::ARRAY_VERTEX] = vertices;
		shape[Mesh::ARRAY_NORMAL] = normals;
		blend_shapes.push_back(shape);
	}
	mesh->add_surface(Mesh::PRIMITIVE_TRIANGLES, arrays, blend_shapes, Dictionary(), p_material, "ground");
	return mesh;
}

TEST_CASE("[Modules][EretheaDenseGeometry] A dense mesh splits into compact clusters") {
	Ref<StandardMaterial3D> material;
	material.instantiate();
	const Ref<ImporterMesh> grid = make_grid(64, material);
	CHECK(EretheaDenseGeometry::count_triangles(grid) == 8192);
	CHECK(EretheaDenseGeometry::can_split(grid));

	const Vector<Ref<ImporterMesh>> clusters = EretheaDenseGeometry::split_mesh(grid, 1024);
	REQUIRE(clusters.size() == 8);

	int64_t total = 0;
	for (const Ref<ImporterMesh> &cluster : clusters) {
		const int64_t triangles = EretheaDenseGeometry::count_triangles(cluster);
		total += triangles;
		CHECK(triangles <= 1024);
		REQUIRE(cluster->get_surface_count() == 1);
		CHECK(cluster->get_surface_material(0) == material);
		CHECK(cluster->get_surface_name(0) == "ground");

		// The attributes moved with their vertices.
		const Array arrays = cluster->get_surface_arrays(0);
		const PackedVector3Array vertices = arrays[Mesh::ARRAY_VERTEX];
		const PackedVector2Array uvs = arrays[Mesh::ARRAY_TEX_UV];
		const PackedColorArray colors = arrays[Mesh::ARRAY_COLOR];
		const PackedVector3Array normals = arrays[Mesh::ARRAY_NORMAL];
		REQUIRE(uvs.size() == vertices.size());
		REQUIRE(colors.size() == vertices.size());
		REQUIRE(normals.size() == vertices.size());
		bool attributes_match = true;
		AABB bounds(vertices[0], Vector3());
		for (int i = 0; i < vertices.size(); i++) {
			attributes_match = attributes_match && uvs[i].is_equal_approx(Vector2(vertices[i].x, vertices[i].z) / 64.0) && Math::is_equal_approx(colors[i].r * 64.0f, float(vertices[i].x)) && normals[i] == Vector3(0, 1, 0);
			bounds.expand_to(vertices[i]);
		}
		CHECK(attributes_match);
		// A compact part of the grid, an eighth of it, not a scattered one.
		CHECK(bounds.size.x * bounds.size.z <= 64.0 * 64.0 / 8.0 * 1.25);
	}
	CHECK(total == 8192);
}

TEST_CASE("[Modules][EretheaDenseGeometry] Small and unsplittable meshes stay whole") {
	Ref<StandardMaterial3D> material;
	material.instantiate();
	// Fewer than twice the cluster size.
	CHECK(EretheaDenseGeometry::split_mesh(make_grid(16, material), 1024).is_empty());
	CHECK(EretheaDenseGeometry::split_mesh(make_grid(64, material), 0).is_empty());

	// Blend shapes move the vertices of the whole mesh.
	const Ref<ImporterMesh> with_shape = make_grid(64, material, true);
	CHECK_FALSE(EretheaDenseGeometry::can_split(with_shape));
	CHECK(EretheaDenseGeometry::split_mesh(with_shape, 1024).is_empty());

	// Lines aren't triangles.
	Ref<ImporterMesh> lines;
	lines.instantiate();
	Array arrays;
	arrays.resize(Mesh::ARRAY_MAX);
	arrays[Mesh::ARRAY_VERTEX] = PackedVector3Array({ Vector3(), Vector3(1, 0, 0) });
	lines->add_surface(Mesh::PRIMITIVE_LINES, arrays);
	CHECK_FALSE(EretheaDenseGeometry::can_split(lines));
	CHECK_FALSE(EretheaDenseGeometry::can_split(Ref<ImporterMesh>()));
}

TEST_CASE("[Modules][EretheaDenseGeometry] The meshes of an imported scene are split") {
	Ref<StandardMaterial3D> material;
	material.instantiate();
	Node3D *root = memnew(Node3D);

	ImporterMeshInstance3D *ground = memnew(ImporterMeshInstance3D);
	ground->set_name("Ground");
	ground->set_mesh(make_grid(64, material));
	ground->set_layer_mask(5);
	ground->set_visibility_range_end(300.0);
	root->add_child(ground);
	ground->set_owner(root);

	// Skinned meshes stay whole.
	ImporterMeshInstance3D *skinned = memnew(ImporterMeshInstance3D);
	skinned->set_name("Skinned");
	skinned->set_mesh(make_grid(64, material));
	skinned->set_skeleton_path(NodePath(".."));
	root->add_child(skinned);
	skinned->set_owner(root);

	CHECK(EretheaDenseGeometry::split_scene(root, 0) == 0);
	CHECK(EretheaDenseGeometry::split_scene(root, 1024) == 1);

	CHECK(EretheaDenseGeometry::count_triangles(ground->get_mesh()) <= 1024);
	REQUIRE(ground->get_child_count() == 7);
	int64_t total = EretheaDenseGeometry::count_triangles(ground->get_mesh());
	for (int i = 0; i < ground->get_child_count(); i++) {
		ImporterMeshInstance3D *cluster = Object::cast_to<ImporterMeshInstance3D>(ground->get_child(i));
		REQUIRE(cluster != nullptr);
		// Saved with the scene, with the settings of the original.
		CHECK(cluster->get_owner() == root);
		CHECK(cluster->get_layer_mask() == 5);
		CHECK(cluster->get_visibility_range_end() == doctest::Approx(300.0));
		total += EretheaDenseGeometry::count_triangles(cluster->get_mesh());
	}
	CHECK(total == 8192);
	CHECK(skinned->get_child_count() == 0);
	CHECK(EretheaDenseGeometry::count_triangles(skinned->get_mesh()) == 8192);

	memdelete(root);
}

} // namespace TestEretheaDenseGeometry
