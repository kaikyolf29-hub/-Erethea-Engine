/**************************************************************************/
/*  cube_lut.h                                                            */
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
#include "core/math/color.h"
#include "core/math/vector3.h"
#include "core/templates/vector.h"

// Color lookup table in the .cube text format (Adobe Cube LUT 1.0 and the
// DaVinci Resolve variant with a 1D shaper before the 3D table).
class CubeLUT {
	String title;

	int size_1d = 0;
	Vector<Vector3> table_1d;
	Vector3 domain_1d_min = Vector3(0, 0, 0);
	Vector3 domain_1d_max = Vector3(1, 1, 1);

	int size_3d = 0;
	Vector<Vector3> table_3d;
	Vector3 domain_3d_min = Vector3(0, 0, 0);
	Vector3 domain_3d_max = Vector3(1, 1, 1);

	static Vector3 _remap(const Vector3 &p_value, const Vector3 &p_min, const Vector3 &p_max);

public:
	static constexpr int MAX_SIZE_1D = 65536;
	static constexpr int MAX_SIZE_3D = 256;

	Error parse(const String &p_text, String *r_error = nullptr);

	String get_title() const { return title; }
	int get_size_1d() const { return size_1d; }
	int get_size_3d() const { return size_3d; }

	// Returns the LUT's output for an input color, with linear interpolation.
	Color sample(const Color &p_color) const;

	// Edge length of the texture built by make_texture_slices().
	int get_texture_size() const;

	// Builds the depth slices of a 3D texture for
	// Environment.adjustment_color_correction (red along X, green along Y,
	// blue along the slices), in FORMAT_RGBA8. The renderer samples that
	// texture at the color itself, so texel i is sampled exactly at
	// (i + 0.5) / size: each texel stores the LUT's output for that input,
	// except the first and last ones, which keep the outputs for 0 and 1.
	// r_clipped is set when some output was outside the 0-1 range.
	Vector<Ref<Image>> make_texture_slices(bool *r_clipped = nullptr) const;
};
