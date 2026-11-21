/**************************************************************************/
/*  xess_upscaler.h                                                       */
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
#include "core/string/ustring.h"
#include "core/templates/vector.h"

class RenderDataRD;
class RenderSceneBuffersRD;

// Upscales with Intel XeSS Super Resolution through Intel's libxess.dll next
// to the executable. It runs on Intel Arc and Iris Xe graphics, and on other
// graphics cards with Shader Model 6.4 (DP4a) through its cross-vendor path.
// Needs Windows, the Forward+ renderer and, for now, the Direct3D 12 rendering
// driver. Only touched on the rendering thread.
class XessUpscaler {
public:
	static constexpr const char *LIBRARY = "libxess.dll";

	// The runtime for a rendering driver ("d3d12"), or nothing for the drivers
	// XeSS doesn't support here yet.
	static Vector<String> get_runtime_libraries(const String &p_rendering_driver);

	// Whether this build can run XeSS with a rendering driver (Windows, and
	// the driver built in).
	static bool is_supported_by_build(const String &p_rendering_driver);

	// Returns false when XeSS can't run, so that FSR 2.2 runs instead.
	bool upscale(const Ref<RenderSceneBuffersRD> &p_render_buffers, RenderDataRD *p_render_data, double p_time_step);

	// Frees the XeSS contexts of render buffers that stopped using XeSS.
	void release(const Ref<RenderSceneBuffersRD> &p_render_buffers);

	// Frees everything except the runtime, which stays loaded.
	void finish();

	~XessUpscaler();
};
