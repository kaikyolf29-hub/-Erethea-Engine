/**************************************************************************/
/*  erethea_hair_builder.cpp                                              */
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

#include "erethea_hair_builder.h"

#include "core/templates/local_vector.h"

int EretheaHairBuilder::kept_strand_count(int p_count, float p_fraction) {
	if (p_fraction >= 1.0f) {
		return p_count;
	}
	if (p_fraction <= 0.0f) {
		return 0;
	}
	return (int)Math::floor((double)p_count * (double)p_fraction);
}

bool EretheaHairBuilder::keeps_strand(int p_index, float p_fraction) {
	if (p_fraction >= 1.0f) {
		return true;
	}
	if (p_fraction <= 0.0f) {
		return false;
	}
	return Math::floor((double)(p_index + 1) * (double)p_fraction) > Math::floor((double)p_index * (double)p_fraction);
}

float EretheaHairBuilder::strand_random(int p_index) {
	uint32_t h = (uint32_t)p_index * 747796405u + 2891336453u;
	h = ((h >> ((h >> 28u) + 4u)) ^ h) * 277803737u;
	h = (h >> 22u) ^ h;
	return (float)(h & 0xFFFFFFu) / (float)0x1000000u;
}

EretheaHairBuilder::Result EretheaHairBuilder::build(const Ref<Mesh> &p_source, float p_width, float p_strand_fraction) {
	Result result;
	if (p_source.is_null()) {
		return result;
	}

	bool has_aabb = false;
	int strand_counter = 0; // Over all surfaces, so the random values don't repeat.

	for (int s = 0; s < p_source->get_surface_count(); s++) {
		const Mesh::PrimitiveType type = p_source->surface_get_primitive_type(s);
		if (type != Mesh::PRIMITIVE_LINES && type != Mesh::PRIMITIVE_LINE_STRIP) {
			continue;
		}
		const Array source = p_source->surface_get_arrays(s);
		if (source.size() <= Mesh::ARRAY_INDEX || source[Mesh::ARRAY_VERTEX].get_type() != Variant::PACKED_VECTOR3_ARRAY) {
			continue;
		}
		const Vector<Vector3> points = source[Mesh::ARRAY_VERTEX];
		const int point_count = points.size();
		Vector<int> index;
		if (source[Mesh::ARRAY_INDEX].get_type() == Variant::PACKED_INT32_ARRAY) {
			index = source[Mesh::ARRAY_INDEX];
		}
		const int entries = index.is_empty() ? point_count : index.size();
		const int segment_count = type == Mesh::PRIMITIVE_LINES ? entries / 2 : MAX(entries - 1, 0);

		Vector<int> bones;
		Vector<float> weights;
		int bones_per_vertex = 0;
		if (source[Mesh::ARRAY_BONES].get_type() == Variant::PACKED_INT32_ARRAY && source[Mesh::ARRAY_WEIGHTS].get_type() == Variant::PACKED_FLOAT32_ARRAY && point_count > 0) {
			bones = source[Mesh::ARRAY_BONES];
			weights = source[Mesh::ARRAY_WEIGHTS];
			bones_per_vertex = bones.size() / point_count;
			if ((bones_per_vertex != 4 && bones_per_vertex != 8) || bones.size() != point_count * bones_per_vertex || weights.size() != bones.size()) {
				bones_per_vertex = 0;
			}
		}

		// Chain the segments into strands: a segment that starts where the
		// previous one ended continues its strand.
		LocalVector<int> chain; // Source point indices of all strands.
		LocalVector<int> chain_start; // Where each strand starts in chain, plus the end.
		int last = -1;
		for (int k = 0; k < segment_count; k++) {
			int a;
			int b;
			if (type == Mesh::PRIMITIVE_LINES) {
				a = index.is_empty() ? 2 * k : index[2 * k];
				b = index.is_empty() ? 2 * k + 1 : index[2 * k + 1];
			} else {
				a = index.is_empty() ? k : index[k];
				b = index.is_empty() ? k + 1 : index[k + 1];
			}
			if (a < 0 || b < 0 || a >= point_count || b >= point_count || a == b) {
				last = -1;
				continue;
			}
			if (a != last || chain_start.is_empty()) {
				chain_start.push_back(chain.size());
				chain.push_back(a);
			}
			chain.push_back(b);
			last = b;
		}
		chain_start.push_back(chain.size());
		const int strands_in_surface = chain_start.size() - 1;
		result.stats.source_strands += strands_in_surface;
		if (strands_in_surface <= 0) {
			continue;
		}

		// Direction away from the middle of the hair, as a smooth shell normal.
		AABB bounds;
		for (uint32_t i = 0; i < chain.size(); i++) {
			if (i == 0) {
				bounds.position = points[chain[i]];
			} else {
				bounds.expand_to(points[chain[i]]);
			}
		}
		const Vector3 center = bounds.get_center();

		Vector<Vector3> out_vertex;
		Vector<Vector3> out_normal;
		Vector<float> out_tangent;
		Vector<Vector2> out_uv;
		Vector<Vector2> out_uv2;
		Vector<int> out_bones;
		Vector<float> out_weights;
		Vector<int> out_index;

		for (int st = 0; st < strands_in_surface; st++) {
			const int global_index = strand_counter + st;
			if (!keeps_strand(global_index, p_strand_fraction)) {
				continue;
			}
			const int from = chain_start[st];
			const int count = chain_start[st + 1] - from;
			if (count < 2) {
				continue;
			}

			LocalVector<float> arc;
			arc.resize(count);
			arc[0] = 0.0f;
			for (int i = 1; i < count; i++) {
				arc[i] = arc[i - 1] + points[chain[from + i]].distance_to(points[chain[from + i - 1]]);
			}
			const float length = arc[count - 1];
			const float random = strand_random(global_index);
			const int base = out_vertex.size();

			for (int i = 0; i < count; i++) {
				const Vector3 p = points[chain[from + i]];
				const Vector3 before = points[chain[from + MAX(i - 1, 0)]];
				const Vector3 after = points[chain[from + MIN(i + 1, count - 1)]];
				Vector3 tangent = (after - before).normalized();
				if (tangent.is_zero_approx()) {
					tangent = Vector3(0, -1, 0);
				}
				Vector3 normal = (p - center).normalized();
				if (normal.is_zero_approx()) {
					normal = Vector3(0, 1, 0);
				}
				const float along = length > 0.0f ? arc[i] / length : 0.0f;
				for (int side = 0; side < 2; side++) {
					out_vertex.push_back(p);
					out_normal.push_back(normal);
					out_tangent.push_back(tangent.x);
					out_tangent.push_back(tangent.y);
					out_tangent.push_back(tangent.z);
					out_tangent.push_back(1.0f);
					out_uv.push_back(Vector2(side == 0 ? -1.0f : 1.0f, along));
					out_uv2.push_back(Vector2(random, length));
					for (int b = 0; b < bones_per_vertex; b++) {
						out_bones.push_back(bones[chain[from + i] * bones_per_vertex + b]);
						out_weights.push_back(weights[chain[from + i] * bones_per_vertex + b]);
					}
				}
				if (!has_aabb) {
					result.aabb.position = p;
					has_aabb = true;
				} else {
					result.aabb.expand_to(p);
				}
			}
			for (int i = 0; i < count - 1; i++) {
				const int v = base + 2 * i;
				out_index.push_back(v);
				out_index.push_back(v + 1);
				out_index.push_back(v + 2);
				out_index.push_back(v + 1);
				out_index.push_back(v + 3);
				out_index.push_back(v + 2);
			}
			result.stats.strands++;
		}
		strand_counter += strands_in_surface;

		if (out_vertex.is_empty()) {
			continue;
		}
		result.stats.vertices += out_vertex.size();
		result.stats.triangles += out_index.size() / 3;

		Surface surface;
		surface.arrays.resize(Mesh::ARRAY_MAX);
		surface.arrays[Mesh::ARRAY_VERTEX] = out_vertex;
		surface.arrays[Mesh::ARRAY_NORMAL] = out_normal;
		surface.arrays[Mesh::ARRAY_TANGENT] = out_tangent;
		surface.arrays[Mesh::ARRAY_TEX_UV] = out_uv;
		surface.arrays[Mesh::ARRAY_TEX_UV2] = out_uv2;
		if (bones_per_vertex > 0) {
			surface.arrays[Mesh::ARRAY_BONES] = out_bones;
			surface.arrays[Mesh::ARRAY_WEIGHTS] = out_weights;
			if (bones_per_vertex == 8) {
				surface.flags |= Mesh::ARRAY_FLAG_USE_8_BONE_WEIGHTS;
			}
		}
		surface.arrays[Mesh::ARRAY_INDEX] = out_index;
		result.surfaces.push_back(surface);
	}

	if (has_aabb) {
		result.aabb.grow_by(MAX(p_width, 0.0f) * 0.5f);
	}
	return result;
}
