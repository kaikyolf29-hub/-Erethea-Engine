/**************************************************************************/
/*  test_erethea_upscale.h                                                */
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

#include "../amd_fsr_upscaler.h"
#include "../dlss_upscaler.h"
#include "../erethea_upscaler.h"
#include "../xess_upscaler.h"

#include "core/config/project_settings.h"
#include "servers/rendering/renderer_rd/storage_rd/render_scene_buffers_rd.h"
#include "tests/test_macros.h"

namespace TestEretheaUpscale {

TEST_CASE("[Modules][EretheaUpscale] Project setting is off by default") {
	EretheaUpscaler::register_project_settings();
	CHECK(ProjectSettings::get_singleton()->has_setting(EretheaUpscaler::PROJECT_SETTING));
	CHECK(int(ProjectSettings::get_singleton()->property_get_revert(EretheaUpscaler::PROJECT_SETTING)) == EretheaUpscaler::MODE_FSR2);
	CHECK(EretheaUpscaler::get_project_mode() == EretheaUpscaler::MODE_FSR2);
}

TEST_CASE("[Modules][EretheaUpscale] Modes stay in range and FSR 2.2 is left to Godot") {
	EretheaUpscaler upscaler;
	CHECK(upscaler.get_mode() == EretheaUpscaler::MODE_FSR2);

	upscaler.set_mode(EretheaUpscaler::Mode(100));
	CHECK(upscaler.get_mode() == EretheaUpscaler::MODE_AUTOMATIC);
	upscaler.set_mode(EretheaUpscaler::MODE_AMD_FSR);
	CHECK(upscaler.get_mode() == EretheaUpscaler::MODE_AMD_FSR);
	upscaler.set_mode(EretheaUpscaler::Mode(-1));
	CHECK(upscaler.get_mode() == EretheaUpscaler::MODE_FSR2);

	// With FSR 2.2 the module doesn't upscale, so the renderer runs its own.
	CHECK_FALSE(upscaler.upscale(Ref<RenderSceneBuffersRD>(), nullptr, 1.0 / 60.0));
}

TEST_CASE("[Modules][EretheaUpscale] Automatic picks the upscaler of the graphics card's vendor") {
	// DLSS on NVIDIA and XeSS on Intel, then AMD FSR where those can't run.
	const Vector<int> nvidia = EretheaUpscaler::get_automatic_order("NVIDIA");
	REQUIRE(nvidia.size() == 2);
	CHECK(nvidia[0] == EretheaUpscaler::MODE_DLSS);
	CHECK(nvidia[1] == EretheaUpscaler::MODE_AMD_FSR);

	const Vector<int> intel = EretheaUpscaler::get_automatic_order("Intel");
	REQUIRE(intel.size() == 2);
	CHECK(intel[0] == EretheaUpscaler::MODE_XESS);
	CHECK(intel[1] == EretheaUpscaler::MODE_AMD_FSR);

	// AMD FSR on AMD and on everything else.
	const String others[] = { "AMD", "Microsoft", "Unknown", "" };
	for (const String &vendor : others) {
		const Vector<int> order = EretheaUpscaler::get_automatic_order(vendor);
		REQUIRE(order.size() == 1);
		CHECK(order[0] == EretheaUpscaler::MODE_AMD_FSR);
	}
}

TEST_CASE("[Modules][EretheaUpscale] AMD FSR runtime") {
	const Vector<String> vulkan = AmdFsrUpscaler::get_runtime_libraries("vulkan");
	REQUIRE(vulkan.size() == 2);
	CHECK(vulkan[0] == "amd_fidelityfx_loader_vk.dll");
	CHECK(vulkan[1] == "amd_fidelityfx_upscaler_vk.dll");

	const Vector<String> d3d12 = AmdFsrUpscaler::get_runtime_libraries("d3d12");
	REQUIRE(d3d12.size() == 2);
	CHECK(d3d12[0] == "amd_fidelityfx_loader_dx12.dll");
	CHECK(d3d12[1] == "amd_fidelityfx_upscaler_dx12.dll");

	CHECK(AmdFsrUpscaler::get_runtime_libraries("opengl3").is_empty());
	CHECK_FALSE(AmdFsrUpscaler::is_supported_by_build("opengl3"));
	CHECK_FALSE(AmdFsrUpscaler::is_supported_by_build("metal"));

#if defined(WINDOWS_ENABLED) && defined(VULKAN_ENABLED)
	CHECK(AmdFsrUpscaler::is_supported_by_build("vulkan"));
#else
	CHECK_FALSE(AmdFsrUpscaler::is_supported_by_build("vulkan"));
#endif
#if defined(WINDOWS_ENABLED) && defined(D3D12_ENABLED)
	CHECK(AmdFsrUpscaler::is_supported_by_build("d3d12"));
#else
	CHECK_FALSE(AmdFsrUpscaler::is_supported_by_build("d3d12"));
#endif
}

TEST_CASE("[Modules][EretheaUpscale] XeSS runtime") {
	for (const String driver : { "d3d12", "vulkan" }) {
		const Vector<String> libraries = XessUpscaler::get_runtime_libraries(driver);
		REQUIRE(libraries.size() == 1);
		CHECK(libraries[0] == "libxess.dll");
	}

	CHECK(XessUpscaler::get_runtime_libraries("opengl3").is_empty());
	CHECK_FALSE(XessUpscaler::is_supported_by_build("opengl3"));

#if defined(WINDOWS_ENABLED) && defined(RD_ENABLED) && defined(D3D12_ENABLED)
	CHECK(XessUpscaler::is_supported_by_build("d3d12"));
#else
	CHECK_FALSE(XessUpscaler::is_supported_by_build("d3d12"));
#endif
#if defined(WINDOWS_ENABLED) && defined(RD_ENABLED) && defined(VULKAN_ENABLED)
	CHECK(XessUpscaler::is_supported_by_build("vulkan"));
#else
	CHECK_FALSE(XessUpscaler::is_supported_by_build("vulkan"));
#endif
}

TEST_CASE("[Modules][EretheaUpscale] DLSS runtime") {
	// NVIDIA's Streamline: the interposer, its common plugin, the DLSS plugin
	// and DLSS itself.
	const Vector<String> d3d12 = DlssUpscaler::get_runtime_libraries("d3d12");
	REQUIRE(d3d12.size() == 4);
	CHECK(d3d12[0] == "sl.interposer.dll");
	CHECK(d3d12[1] == "sl.common.dll");
	CHECK(d3d12[2] == "sl.dlss.dll");
	CHECK(d3d12[3] == "nvngx_dlss.dll");

	// Vulkan comes later.
	CHECK(DlssUpscaler::get_runtime_libraries("vulkan").is_empty());
	CHECK_FALSE(DlssUpscaler::is_supported_by_build("vulkan"));
	CHECK(DlssUpscaler::get_runtime_libraries("opengl3").is_empty());
	CHECK_FALSE(DlssUpscaler::is_supported_by_build("opengl3"));

#if defined(WINDOWS_ENABLED) && defined(RD_ENABLED) && defined(D3D12_ENABLED)
	CHECK(DlssUpscaler::is_supported_by_build("d3d12"));
#else
	CHECK_FALSE(DlssUpscaler::is_supported_by_build("d3d12"));
#endif
}

} // namespace TestEretheaUpscale
