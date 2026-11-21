/**************************************************************************/
/*  test_cube_lut.h                                                       */
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

#include "../cube_lut.h"
#include "../erethea_post_effect.h"

#include "tests/test_macros.h"

namespace TestCubeLUT {

// Identity table: red changes fastest, then green, then blue.
static String make_identity_cube(int p_size) {
	String text = "# Identity\nTITLE \"Identity\"\n" + vformat("LUT_3D_SIZE %d\n", p_size);
	for (int b = 0; b < p_size; b++) {
		for (int g = 0; g < p_size; g++) {
			for (int r = 0; r < p_size; r++) {
				const float scale = 1.0f / (p_size - 1);
				text += vformat("%f %f %f\n", r * scale, g * scale, b * scale);
			}
		}
	}
	return text;
}

static bool is_close(const Color &p_a, const Color &p_b, float p_tolerance = 0.001) {
	return Math::abs(p_a.r - p_b.r) <= p_tolerance && Math::abs(p_a.g - p_b.g) <= p_tolerance && Math::abs(p_a.b - p_b.b) <= p_tolerance;
}

TEST_CASE("[Modules][CubeLUT] Parses a 3D table") {
	CubeLUT lut;
	String error;
	REQUIRE(lut.parse(make_identity_cube(5), &error) == OK);
	CHECK(error.is_empty());
	CHECK(lut.get_title() == "Identity");
	CHECK(lut.get_size_1d() == 0);
	CHECK(lut.get_size_3d() == 5);

	CHECK(is_close(lut.sample(Color(0, 0, 0)), Color(0, 0, 0)));
	CHECK(is_close(lut.sample(Color(1, 1, 1)), Color(1, 1, 1)));
	CHECK(is_close(lut.sample(Color(0.1, 0.6, 0.85)), Color(0.1, 0.6, 0.85)));
	// Inputs outside the domain are clamped.
	CHECK(is_close(lut.sample(Color(-1, 2, 0.5)), Color(0, 1, 0.5)));
}

TEST_CASE("[Modules][CubeLUT] Follows the table order (red fastest, blue slowest)") {
	// Swaps red and blue: the output for input (r, g, b) is (b, g, r).
	String text = "LUT_3D_SIZE 2\n";
	for (int b = 0; b < 2; b++) {
		for (int g = 0; g < 2; g++) {
			for (int r = 0; r < 2; r++) {
				text += vformat("%d %d %d\n", b, g, r);
			}
		}
	}
	CubeLUT lut;
	REQUIRE(lut.parse(text) == OK);
	CHECK(is_close(lut.sample(Color(1, 0, 0)), Color(0, 0, 1)));
	CHECK(is_close(lut.sample(Color(0, 0, 1)), Color(1, 0, 0)));
	CHECK(is_close(lut.sample(Color(0.25, 0.5, 0.75)), Color(0.75, 0.5, 0.25)));
}

TEST_CASE("[Modules][CubeLUT] Applies a 1D table per channel") {
	// Squares each channel, with 3 points: 0, 0.25, 1.
	CubeLUT lut;
	REQUIRE(lut.parse("LUT_1D_SIZE 3\n0 0 0\n0.25 0.25 0.25\n1 1 1\n") == OK);
	CHECK(lut.get_size_1d() == 3);
	CHECK(is_close(lut.sample(Color(0.5, 0.25, 1)), Color(0.25, 0.125, 1)));
}

TEST_CASE("[Modules][CubeLUT] Uses the domain to scale the inputs") {
	CubeLUT lut;
	REQUIRE(lut.parse("DOMAIN_MIN 0 0 0\nDOMAIN_MAX 2 2 2\nLUT_1D_SIZE 2\n0 0 0\n1 1 1\n") == OK);
	CHECK(is_close(lut.sample(Color(1, 0.5, 2)), Color(0.5, 0.25, 1)));

	// DaVinci Resolve's form, with a 1D shaper before the 3D table.
	const String resolve = "LUT_1D_SIZE 2\nLUT_1D_INPUT_RANGE 0 4\nLUT_3D_SIZE 2\n"
						   "0 0 0\n1 1 1\n" // 1D shaper.
						   "0 0 0\n1 0 0\n0 1 0\n1 1 0\n0 0 1\n1 0 1\n0 1 1\n1 1 1\n"; // 3D identity.
	REQUIRE(lut.parse(resolve) == OK);
	CHECK(lut.get_size_1d() == 2);
	CHECK(lut.get_size_3d() == 2);
	CHECK(is_close(lut.sample(Color(2, 1, 0)), Color(0.5, 0.25, 0)));
}

TEST_CASE("[Modules][CubeLUT] Rejects broken files") {
	CubeLUT lut;
	String error;
	CHECK(lut.parse("0 0 0\n1 1 1\n", &error) == ERR_PARSE_ERROR);
	CHECK(!error.is_empty());
	CHECK(lut.parse("LUT_3D_SIZE 2\n0 0 0\n", &error) == ERR_PARSE_ERROR);
	CHECK(error.contains("Expected 8"));
	CHECK(lut.parse("LUT_3D_SIZE 1\n0 0 0\n") == ERR_PARSE_ERROR);
	CHECK(lut.parse("LUT_3D_SIZE 512\n") == ERR_PARSE_ERROR);
	CHECK(lut.parse("LUT_1D_SIZE 2\n0 0\n1 1 1\n", &error) == ERR_PARSE_ERROR);
	CHECK(error.begins_with("Line 2"));
	CHECK(lut.parse("DOMAIN_MIN 1 1 1\nDOMAIN_MAX 0 1 1\nLUT_1D_SIZE 2\n0 0 0\n1 1 1\n") == ERR_PARSE_ERROR);
}

TEST_CASE("[Modules][CubeLUT] Texture matches how the renderer samples it") {
	CubeLUT lut;
	REQUIRE(lut.parse(make_identity_cube(33)) == OK);
	const int size = lut.get_texture_size();
	CHECK(size == 65);

	bool clipped = true;
	const Vector<Ref<Image>> slices = lut.make_texture_slices(&clipped);
	CHECK_FALSE(clipped);
	REQUIRE(slices.size() == size);
	REQUIRE(slices[0]->get_width() == size);
	REQUIRE(slices[0]->get_height() == size);
	CHECK(slices[0]->get_format() == Image::FORMAT_RGBA8);

	// The first and last texels keep black and white.
	CHECK(is_close(slices[0]->get_pixel(0, 0), Color(0, 0, 0)));
	CHECK(is_close(slices[size - 1]->get_pixel(size - 1, size - 1), Color(1, 1, 1)));

	// Texel 10 is sampled at (10 + 0.5) / 65, so an identity table stores that value.
	const float expected = 10.5f / size;
	const Color texel = slices[20]->get_pixel(10, 5);
	CHECK(texel.r == doctest::Approx(expected).epsilon(0.01));
	CHECK(texel.g == doctest::Approx(5.5f / size).epsilon(0.01));
	CHECK(texel.b == doctest::Approx(20.5f / size).epsilon(0.01));

	// Outputs outside 0-1 are reported.
	REQUIRE(lut.parse("LUT_1D_SIZE 2\n0 0 0\n1.5 1 1\n") == OK);
	lut.make_texture_slices(&clipped);
	CHECK(clipped);
}

} // namespace TestCubeLUT

namespace TestEretheaPostEffect {

TEST_CASE("[Modules][EretheaPostEffect] Starts with every effect off") {
	Ref<EretheaPostEffect> effect;
	effect.instantiate();
	CHECK_FALSE(effect->is_active());

	effect->set_film_grain_intensity(0.3);
	CHECK(effect->is_active());

	// Values are kept in range.
	effect->set_vignette_intensity(3.0);
	CHECK(effect->get_vignette_intensity() == doctest::Approx(1.0));
	effect->set_film_grain_size(0.0);
	CHECK(effect->get_film_grain_size() == doctest::Approx(1.0));
}

} // namespace TestEretheaPostEffect
