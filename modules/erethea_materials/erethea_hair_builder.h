/**************************************************************************/
/*  erethea_hair_builder.h                                                */
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

#include "core/math/aabb.h"
#include "core/variant/array.h"
#include "scene/resources/mesh.h"

// Turns the line surfaces of a mesh (one strand of hair = a chain of connected
// segments) into camera-facing ribbons that the hair shader opens on the GPU.
// Kept apart from the node so it can be tested without a renderer.
class EretheaHairBuilder {
public:
	struct Stats {
		int strands = 0; // Strands drawn.
		int source_strands = 0; // Strands found in the source mesh.
		int vertices = 0;
		int triangles = 0;
	};

	struct Surface {
		Array arrays;
		uint64_t flags = 0; // ARRAY_FLAG_* to create the surface with.
	};

	struct Result {
		Vector<Surface> surfaces;
		AABB aabb; // Of the points, grown by half the width.
		Stats stats;
	};

	// Per vertex of the ribbons: VERTEX (the point; two vertices on the same
	// place), NORMAL (away from the middle of the hair), TANGENT (along the
	// strand, root to tip), UV (x: side -1 or +1, y: 0 at the root to 1 at the
	// tip, by length), UV2 (x: random value per strand, 0 to 1; y: length of the
	// strand in meters), BONES and WEIGHTS copied from the source point.
	static Result build(const Ref<Mesh> &p_source, float p_width, float p_strand_fraction);

	// Number of strands kept from p_count when drawing p_fraction of them.
	static int kept_strand_count(int p_count, float p_fraction);
	// Whether the strand p_index of p_count is drawn: the kept ones are spread evenly.
	static bool keeps_strand(int p_index, float p_fraction);
	// Stable value from 0 to 1 for a strand, for variation between strands.
	static float strand_random(int p_index);
};
