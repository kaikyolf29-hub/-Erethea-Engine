/**************************************************************************/
/*  amd_fsr_upscaler.h                                                    */
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

#include "core/object/ref_counted.h"

class RenderDataRD;
class RenderSceneBuffersRD;

// Upscales with AMD FSR through AMD's signed runtime
// (amd_fidelityfx_loader_vk.dll and amd_fidelityfx_upscaler_vk.dll next to
// the executable). The runtime picks FSR 4 on the graphics cards that support
// it and FSR 3.1 on the others. Needs Windows, Vulkan and the Forward+
// renderer. Only touched on the rendering thread.
class AmdFsrUpscaler {
public:
	// AMD's runtime, loaded from the folder of the executable.
	static constexpr const char *RUNTIME_LIBRARIES[2] = { "amd_fidelityfx_loader_vk.dll", "amd_fidelityfx_upscaler_vk.dll" };

	// Returns false when AMD FSR can't run, so that FSR 2.2 runs instead.
	bool upscale(const Ref<RenderSceneBuffersRD> &p_render_buffers, RenderDataRD *p_render_data, double p_time_step);

	// Frees the FSR contexts of render buffers that stopped using AMD FSR.
	void release(const Ref<RenderSceneBuffersRD> &p_render_buffers);

	// Frees everything, including the runtime.
	void finish();

	// Whether this build can load AMD's runtime at all (Windows with Vulkan).
	static bool is_supported_by_build();

	~AmdFsrUpscaler();
};
