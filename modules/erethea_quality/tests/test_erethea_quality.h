/**************************************************************************/
/*  test_erethea_quality.h                                                */
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

#include "../erethea_quality.h"

#include "tests/test_macros.h"

namespace TestEretheaQuality {

TEST_CASE("[Modules][EretheaQuality] Recommended preset follows the graphics card") {
	namespace RDE = RenderingDeviceEnums;
	CHECK(EretheaQuality::recommend_preset_for_device(RDE::DEVICE_TYPE_DISCRETE_GPU, true) == EretheaQuality::PRESET_CINEMATIC);
	CHECK(EretheaQuality::recommend_preset_for_device(RDE::DEVICE_TYPE_DISCRETE_GPU, false) == EretheaQuality::PRESET_BALANCED);
	CHECK(EretheaQuality::recommend_preset_for_device(RDE::DEVICE_TYPE_INTEGRATED_GPU, true) == EretheaQuality::PRESET_LOW);
	CHECK(EretheaQuality::recommend_preset_for_device(RDE::DEVICE_TYPE_INTEGRATED_GPU, false) == EretheaQuality::PRESET_LOW);
	CHECK(EretheaQuality::recommend_preset_for_device(RDE::DEVICE_TYPE_CPU, false) == EretheaQuality::PRESET_LOW);
	CHECK(EretheaQuality::recommend_preset_for_device(RDE::DEVICE_TYPE_VIRTUAL_GPU, false) == EretheaQuality::PRESET_LOW);
	CHECK(EretheaQuality::recommend_preset_for_device(RDE::DEVICE_TYPE_OTHER, false) == EretheaQuality::PRESET_LOW);
}

TEST_CASE("[Modules][EretheaQuality] Disabled and automatic have no values of their own") {
	CHECK(EretheaQuality::get_preset_values(EretheaQuality::PRESET_DISABLED).is_empty());
	CHECK(EretheaQuality::get_preset_values(EretheaQuality::PRESET_AUTOMATIC).is_empty());
}

TEST_CASE("[Modules][EretheaQuality] Presets raise quality in order") {
	const Dictionary low = EretheaQuality::get_preset_values(EretheaQuality::PRESET_LOW);
	const Dictionary balanced = EretheaQuality::get_preset_values(EretheaQuality::PRESET_BALANCED);
	const Dictionary cinematic = EretheaQuality::get_preset_values(EretheaQuality::PRESET_CINEMATIC);
	REQUIRE(!low.is_empty());
	CHECK(low.keys() == balanced.keys());
	CHECK(low.keys() == cinematic.keys());

	// Higher is better for these settings.
	const char *higher_is_better[] = {
		"rendering/textures/default_filters/anisotropic_filtering_level",
		"rendering/lights_and_shadows/directional_shadow/size",
		"rendering/lights_and_shadows/directional_shadow/soft_shadow_filter_quality",
		"rendering/lights_and_shadows/positional_shadow/atlas_size",
		"rendering/lights_and_shadows/positional_shadow/soft_shadow_filter_quality",
		"rendering/environment/ssao/quality",
		"rendering/environment/ssil/quality",
		"rendering/environment/glow/upscale_mode",
		"rendering/environment/subsurface_scattering/subsurface_scattering_quality",
		"rendering/camera/depth_of_field/depth_of_field_bokeh_quality",
		"rendering/environment/volumetric_fog/volume_size",
		"rendering/environment/volumetric_fog/volume_depth",
		"rendering/global_illumination/sdfgi/probe_ray_count",
		"rendering/scaling_3d/scale",
	};
	for (const char *path : higher_is_better) {
		INFO(path);
		REQUIRE(low.has(path));
		CHECK(double(low[path]) <= double(balanced[path]));
		CHECK(double(balanced[path]) <= double(cinematic[path]));
	}
	// Lower is better: the mesh LOD switches later.
	const char *lod = "rendering/mesh_lod/lod_change/threshold_pixels";
	CHECK(double(low[lod]) >= double(balanced[lod]));
	CHECK(double(balanced[lod]) >= double(cinematic[lod]));

	// Half size ambient occlusion and indirect light only up to balanced.
	CHECK(bool(low["rendering/environment/ssao/half_size"]));
	CHECK_FALSE(bool(cinematic["rendering/environment/ssao/half_size"]));
}

TEST_CASE("[Modules][EretheaQuality] Values keep the type of their setting") {
	const Dictionary balanced = EretheaQuality::get_preset_values(EretheaQuality::PRESET_BALANCED);
	CHECK(balanced["rendering/lights_and_shadows/directional_shadow/size"].get_type() == Variant::INT);
	CHECK(balanced["rendering/scaling_3d/scale"].get_type() == Variant::FLOAT);
	CHECK(balanced["rendering/environment/ssao/half_size"].get_type() == Variant::BOOL);
}

} // namespace TestEretheaQuality
