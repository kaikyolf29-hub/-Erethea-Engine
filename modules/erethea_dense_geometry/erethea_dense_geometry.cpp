/**************************************************************************/
/*  erethea_dense_geometry.cpp                                            */
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

#include "erethea_dense_geometry.h"

#include "core/config/project_settings.h"
#include "core/math/aabb.h"
#include "core/templates/local_vector.h"
#include "core/templates/pair.h"
#include "core/templates/sort_array.h"
#include "scene/3d/importer_mesh_instance_3d.h"
#include "scene/main/node.h"

namespace {

struct Triangle {
	uint32_t surface = 0;
	// Where its three indices start in the surface's index list.
	uint32_t first_index = 0;
	Vector3 center;
};

struct CenterComparator {
	const Triangle *triangles = nullptr;
	int axis = 0;

	_FORCE_INLINE_ bool operator()(uint32_t p_a, uint32_t p_b) const {
		return triangles[p_a].center[axis] < triangles[p_b].center[axis];
	}
};

// The values of the given vertices, from a per-vertex mesh array of any type:
// each vertex has the same number of values (3 floats per vertex for a
// PackedVector3Array, 4 for tangents, 4 or 8 bones, or a few bytes for
// custom data). Returns null when the array doesn't fit the vertex count.
template <typename T>
Variant extract_values(const Variant &p_array, int p_vertex_count, const LocalVector<uint32_t> &p_vertices) {
	const T source = p_array;
	if (p_vertex_count <= 0 || source.size() % p_vertex_count != 0) {
		return Variant();
	}
	const int64_t stride = source.size() / p_vertex_count;
	T result;
	result.resize(int64_t(p_vertices.size()) * stride);
	auto *w = result.ptrw();
	const auto *r = source.ptr();
	for (uint32_t i = 0; i < p_vertices.size(); i++) {
		for (int64_t j = 0; j < stride; j++) {
			w[i * stride + j] = r[p_vertices[i] * stride + j];
		}
	}
	return result;
}

Variant extract_vertex_array(const Variant &p_array, int p_vertex_count, const LocalVector<uint32_t> &p_vertices) {
	switch (p_array.get_type()) {
		case Variant::PACKED_VECTOR3_ARRAY:
			return extract_values<PackedVector3Array>(p_array, p_vertex_count, p_vertices);
		case Variant::PACKED_VECTOR2_ARRAY:
			return extract_values<PackedVector2Array>(p_array, p_vertex_count, p_vertices);
		case Variant::PACKED_COLOR_ARRAY:
			return extract_values<PackedColorArray>(p_array, p_vertex_count, p_vertices);
		case Variant::PACKED_FLOAT32_ARRAY:
			return extract_values<PackedFloat32Array>(p_array, p_vertex_count, p_vertices);
		case Variant::PACKED_FLOAT64_ARRAY:
			return extract_values<PackedFloat64Array>(p_array, p_vertex_count, p_vertices);
		case Variant::PACKED_INT32_ARRAY:
			return extract_values<PackedInt32Array>(p_array, p_vertex_count, p_vertices);
		case Variant::PACKED_BYTE_ARRAY:
			return extract_values<PackedByteArray>(p_array, p_vertex_count, p_vertices);
		default:
			return Variant();
	}
}

int surface_vertex_count(const Array &p_arrays) {
	const PackedVector3Array vertices = p_arrays[Mesh::ARRAY_VERTEX];
	return vertices.size();
}

// The surface's index list, or one made up for a surface without indices.
PackedInt32Array surface_indices(const Array &p_arrays) {
	PackedInt32Array indices = p_arrays[Mesh::ARRAY_INDEX];
	if (indices.is_empty()) {
		const int vertex_count = surface_vertex_count(p_arrays);
		indices.resize(vertex_count - vertex_count % 3);
		int32_t *w = indices.ptrw();
		for (int i = 0; i < indices.size(); i++) {
			w[i] = i;
		}
	}
	return indices;
}

void gather_instances(Node *p_node, LocalVector<ImporterMeshInstance3D *> &r_instances) {
	ImporterMeshInstance3D *instance = Object::cast_to<ImporterMeshInstance3D>(p_node);
	if (instance != nullptr) {
		r_instances.push_back(instance);
	}
	for (int i = 0; i < p_node->get_child_count(); i++) {
		gather_instances(p_node->get_child(i), r_instances);
	}
}

} // namespace

void EretheaDenseGeometry::register_project_settings() {
	GLOBAL_DEF(PropertyInfo(Variant::INT, PROJECT_SETTING, PROPERTY_HINT_RANGE, "0,1048576,1,or_greater"), 0);
}

int64_t EretheaDenseGeometry::count_triangles(const Ref<ImporterMesh> &p_mesh) {
	if (p_mesh.is_null()) {
		return 0;
	}
	int64_t count = 0;
	for (int surface = 0; surface < p_mesh->get_surface_count(); surface++) {
		if (p_mesh->get_surface_primitive_type(surface) != Mesh::PRIMITIVE_TRIANGLES) {
			continue;
		}
		const Array arrays = p_mesh->get_surface_arrays(surface);
		const PackedInt32Array indices = arrays[Mesh::ARRAY_INDEX];
		count += (indices.is_empty() ? surface_vertex_count(arrays) : indices.size()) / 3;
	}
	return count;
}

bool EretheaDenseGeometry::can_split(const Ref<ImporterMesh> &p_mesh) {
	if (p_mesh.is_null() || p_mesh->get_surface_count() == 0 || p_mesh->get_blend_shape_count() > 0) {
		return false;
	}
	for (int surface = 0; surface < p_mesh->get_surface_count(); surface++) {
		if (p_mesh->get_surface_primitive_type(surface) != Mesh::PRIMITIVE_TRIANGLES) {
			return false;
		}
	}
	return true;
}

Vector<Ref<ImporterMesh>> EretheaDenseGeometry::split_mesh(const Ref<ImporterMesh> &p_mesh, int p_cluster_triangles) {
	Vector<Ref<ImporterMesh>> clusters;
	if (p_cluster_triangles < 1 || !can_split(p_mesh)) {
		return clusters;
	}
	const int surface_count = p_mesh->get_surface_count();

	// Every triangle of every surface, with its center.
	LocalVector<Array> arrays;
	LocalVector<PackedInt32Array> indices;
	arrays.resize(surface_count);
	indices.resize(surface_count);
	LocalVector<Triangle> triangles;
	for (int surface = 0; surface < surface_count; surface++) {
		arrays[surface] = p_mesh->get_surface_arrays(surface);
		indices[surface] = surface_indices(arrays[surface]);
		const PackedVector3Array vertices = arrays[surface][Mesh::ARRAY_VERTEX];
		const int32_t *index = indices[surface].ptr();
		for (int first = 0; first + 2 < indices[surface].size(); first += 3) {
			Triangle triangle;
			triangle.surface = surface;
			triangle.first_index = first;
			for (int corner = 0; corner < 3; corner++) {
				ERR_FAIL_INDEX_V(index[first + corner], vertices.size(), Vector<Ref<ImporterMesh>>());
				triangle.center += vertices[index[first + corner]];
			}
			triangle.center /= 3.0;
			triangles.push_back(triangle);
		}
	}
	if (triangles.size() < uint32_t(p_cluster_triangles) * 2) {
		return clusters;
	}

	// Split by the median center along the longest axis, until each part is
	// small enough. Each cluster is then a range of this order.
	LocalVector<uint32_t> order;
	order.resize(triangles.size());
	for (uint32_t i = 0; i < order.size(); i++) {
		order[i] = i;
	}
	SortArray<uint32_t, CenterComparator> sorter;
	sorter.compare.triangles = triangles.ptr();
	LocalVector<Pair<uint32_t, uint32_t>> ranges;
	LocalVector<Pair<uint32_t, uint32_t>> pending;
	pending.push_back(Pair<uint32_t, uint32_t>(0, triangles.size()));
	while (!pending.is_empty()) {
		const Pair<uint32_t, uint32_t> range = pending[pending.size() - 1];
		pending.remove_at(pending.size() - 1);
		if (range.second - range.first <= uint32_t(p_cluster_triangles)) {
			ranges.push_back(range);
			continue;
		}
		AABB bounds(triangles[order[range.first]].center, Vector3());
		for (uint32_t i = range.first + 1; i < range.second; i++) {
			bounds.expand_to(triangles[order[i]].center);
		}
		sorter.compare.axis = bounds.get_longest_axis_index();
		const uint32_t middle = range.first + (range.second - range.first) / 2;
		sorter.nth_element(range.first, range.second, middle, order.ptr());
		// The first half comes out first, so neighboring clusters stay next to
		// each other in the list.
		pending.push_back(Pair<uint32_t, uint32_t>(middle, range.second));
		pending.push_back(Pair<uint32_t, uint32_t>(range.first, middle));
	}

	// Each cluster gets the vertices that its triangles use, with all their
	// attributes, in one surface per surface of the mesh it has triangles of.
	LocalVector<LocalVector<int32_t>> new_index_of;
	new_index_of.resize(surface_count);
	for (int surface = 0; surface < surface_count; surface++) {
		new_index_of[surface].resize(surface_vertex_count(arrays[surface]));
		for (int32_t &value : new_index_of[surface]) {
			value = -1;
		}
	}
	LocalVector<uint32_t> used_vertices;
	LocalVector<int32_t> cluster_indices;
	for (const Pair<uint32_t, uint32_t> &range : ranges) {
		Ref<ImporterMesh> cluster;
		cluster.instantiate();
		for (int surface = 0; surface < surface_count; surface++) {
			used_vertices.clear();
			cluster_indices.clear();
			const int32_t *index = indices[surface].ptr();
			LocalVector<int32_t> &new_index = new_index_of[surface];
			for (uint32_t i = range.first; i < range.second; i++) {
				const Triangle &triangle = triangles[order[i]];
				if (triangle.surface != uint32_t(surface)) {
					continue;
				}
				for (int corner = 0; corner < 3; corner++) {
					const int32_t vertex = index[triangle.first_index + corner];
					if (new_index[vertex] < 0) {
						new_index[vertex] = used_vertices.size();
						used_vertices.push_back(vertex);
					}
					cluster_indices.push_back(new_index[vertex]);
				}
			}
			for (uint32_t vertex : used_vertices) {
				new_index[vertex] = -1;
			}
			if (cluster_indices.is_empty()) {
				continue;
			}

			Array cluster_arrays;
			cluster_arrays.resize(Mesh::ARRAY_MAX);
			const int vertex_count = surface_vertex_count(arrays[surface]);
			for (int array = 0; array < Mesh::ARRAY_MAX; array++) {
				if (array == Mesh::ARRAY_INDEX) {
					PackedInt32Array packed;
					packed.resize(cluster_indices.size());
					memcpy(packed.ptrw(), cluster_indices.ptr(), cluster_indices.size() * sizeof(int32_t));
					cluster_arrays[array] = packed;
					continue;
				}
				if (arrays[surface][array].get_type() == Variant::NIL) {
					continue;
				}
				const Variant values = extract_vertex_array(arrays[surface][array], vertex_count, used_vertices);
				ERR_FAIL_COND_V_MSG(values.get_type() == Variant::NIL, Vector<Ref<ImporterMesh>>(), vformat("Erethea dense geometry: can't split the mesh \"%s\", its array %d doesn't have one value per vertex.", p_mesh->get_name(), array));
				cluster_arrays[array] = values;
			}
			cluster->add_surface(Mesh::PRIMITIVE_TRIANGLES, cluster_arrays, TypedArray<Array>(), Dictionary(), p_mesh->get_surface_material(surface), p_mesh->get_surface_name(surface), p_mesh->get_surface_format(surface));
		}
		cluster->set_name(vformat("%s_cluster_%d", p_mesh->get_name(), clusters.size()));
		clusters.push_back(cluster);
	}
	return clusters;
}

int EretheaDenseGeometry::split_scene(Node *p_scene, int p_cluster_triangles) {
	if (p_scene == nullptr || p_cluster_triangles <= 0) {
		return 0;
	}
	const int cluster_triangles = MAX(p_cluster_triangles, MIN_CLUSTER_TRIANGLES);
	LocalVector<ImporterMeshInstance3D *> instances;
	gather_instances(p_scene, instances);

	int split = 0;
	for (ImporterMeshInstance3D *instance : instances) {
		// Skinned meshes move as a whole: splitting them gains nothing.
		if (instance->get_skin().is_valid() || !instance->get_skeleton_path().is_empty()) {
			continue;
		}
		const Vector<Ref<ImporterMesh>> clusters = split_mesh(instance->get_mesh(), cluster_triangles);
		if (clusters.size() < 2) {
			continue;
		}
		instance->set_mesh(clusters[0]);
		for (int i = 1; i < clusters.size(); i++) {
			ImporterMeshInstance3D *cluster = memnew(ImporterMeshInstance3D);
			cluster->set_name(vformat("%s_cluster_%d", instance->get_name(), i));
			cluster->set_mesh(clusters[i]);
			cluster->set_layer_mask(instance->get_layer_mask());
			cluster->set_cast_shadows_setting(instance->get_cast_shadows_setting());
			cluster->set_visibility_range_begin(instance->get_visibility_range_begin());
			cluster->set_visibility_range_begin_margin(instance->get_visibility_range_begin_margin());
			cluster->set_visibility_range_end(instance->get_visibility_range_end());
			cluster->set_visibility_range_end_margin(instance->get_visibility_range_end_margin());
			cluster->set_visibility_range_fade_mode(instance->get_visibility_range_fade_mode());
			instance->add_child(cluster, true);
			cluster->set_owner(p_scene);
		}
		split++;
	}
	return split;
}
