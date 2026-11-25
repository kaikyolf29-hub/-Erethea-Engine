/**************************************************************************/
/*  test_erethea_preview.h                                                */
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

#include "../erethea_preview.h"

#include "tests/test_macros.h"

namespace TestEretheaPreview {

static List<String> make_args(std::initializer_list<const char *> p_args) {
	List<String> result;
	for (const char *arg : p_args) {
		result.push_back(arg);
	}
	return result;
}

TEST_CASE("[Erethea][Preview] Command line options") {
	EretheaPreviewOptions options = EretheaPreviewOptions::parse(make_args({ "--path", "game" }));
	CHECK_FALSE(options.enabled);

	options = EretheaPreviewOptions::parse(make_args({ "--erethea-preview=model.glb" }));
	CHECK(options.enabled);
	CHECK(options.asset == "model.glb");
	CHECK(options.capture.is_empty());
	CHECK(options.capture_frames == EretheaPreviewOptions::DEFAULT_CAPTURE_FRAMES);

	options = EretheaPreviewOptions::parse(make_args({ "--erethea-preview=C:/a b/model.glb", "--erethea-preview-capture=out.png", "--erethea-preview-frames=30", "--erethea-preview-control=next.txt", "--erethea-preview-quality=Cinematic" }));
	CHECK(options.enabled);
	CHECK(options.asset == "C:/a b/model.glb");
	CHECK(options.capture == "out.png");
	CHECK(options.capture_frames == 30);
	CHECK(options.control == "next.txt");
	CHECK(options.quality == "cinematic");
	CHECK(options.error.is_empty());

	options = EretheaPreviewOptions::parse(make_args({ "--erethea-preview=model.glb", "--erethea-preview-frames=1" }));
	CHECK(options.capture_frames == EretheaPreviewOptions::MIN_CAPTURE_FRAMES);

	options = EretheaPreviewOptions::parse(make_args({ "--erethea-preview=model.glb", "--erethea-preview-frames=lots" }));
	CHECK_FALSE(options.error.is_empty());
}

TEST_CASE("[Erethea][Preview] The camera distance fits the asset in the window") {
	// A unit sphere seen with a 40 degree field of view, on a square window.
	const float square = EretheaPreview::get_camera_distance(1.0f, 40.0f, 1.0f);
	CHECK(square == doctest::Approx(EretheaPreview::FRAMING_MARGIN / Math::sin(Math::deg_to_rad(20.0f))));
	// A wide window is limited by its height, a narrow one by its width.
	CHECK(EretheaPreview::get_camera_distance(1.0f, 40.0f, 16.0f / 9.0f) == doctest::Approx(square));
	CHECK(EretheaPreview::get_camera_distance(1.0f, 40.0f, 0.5f) > square);
	// Bigger assets need the camera farther away, in proportion.
	CHECK(EretheaPreview::get_camera_distance(4.0f, 40.0f, 1.0f) == doctest::Approx(4.0f * square));
	// The camera is always outside of the asset.
	CHECK(EretheaPreview::get_camera_distance(2.0f, 40.0f, 1.0f) > 2.0f);
	CHECK(EretheaPreview::get_camera_distance(0.0f, 40.0f, 1.0f) > 0.0f);
}

TEST_CASE("[Erethea][Preview] The camera turns around the target") {
	const Vector3 target(1, 2, 3);
	const Vector3 front = EretheaPreview::get_camera_position(target, 10.0f, 0.0f, 0.0f);
	CHECK(front.is_equal_approx(Vector3(1, 2, 13)));
	const Vector3 side = EretheaPreview::get_camera_position(target, 10.0f, 90.0f, 0.0f);
	CHECK(side.is_equal_approx(Vector3(11, 2, 3)));
	const Vector3 above = EretheaPreview::get_camera_position(target, 10.0f, 45.0f, 90.0f);
	CHECK(above.is_equal_approx(Vector3(1, 12, 3)));
	// The distance doesn't change with the angles.
	CHECK(EretheaPreview::get_camera_position(target, 7.0f, 123.0f, 20.0f).distance_to(target) == doctest::Approx(7.0f));
}

} // namespace TestEretheaPreview
