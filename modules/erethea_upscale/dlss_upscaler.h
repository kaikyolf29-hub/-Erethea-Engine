/**************************************************************************/
/*  dlss_upscaler.h                                                       */
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

// Upscales with NVIDIA DLSS Super Resolution through NVIDIA's Streamline,
// whose files go next to the executable. Erethea doesn't ship them: NVIDIA's
// license lets developers distribute them only inside their own games, so each
// developer downloads them from NVIDIA (erethea/tools/fetch_streamline.py).
// Needs Windows, an NVIDIA RTX graphics card, the Forward+ renderer and, for
// now, the Direct3D 12 rendering driver. Only touched on the rendering thread,
// except for prepare_rendering_driver() and finish_rendering_driver().
class DlssUpscaler {
public:
	// Streamline's loader, which the others are found through.
	static constexpr const char *LIBRARY = "sl.interposer.dll";

	// The runtime for a rendering driver ("d3d12"), or nothing for the drivers
	// DLSS doesn't support here yet.
	static Vector<String> get_runtime_libraries(const String &p_rendering_driver);

	// Whether this build can run DLSS with a rendering driver (Windows, and
	// the driver built in).
	static bool is_supported_by_build(const String &p_rendering_driver);

	// Streamline has to start before the rendering device is created. For
	// projects that use DLSS, called on the main thread before that.
	static void prepare_rendering_driver();
	static void finish_rendering_driver();

	// Returns false when DLSS can't run, so that FSR 2.2 runs instead.
	bool upscale(const Ref<RenderSceneBuffersRD> &p_render_buffers, RenderDataRD *p_render_data, double p_time_step);

	// Frees the DLSS resources of render buffers that stopped using DLSS.
	void release(const Ref<RenderSceneBuffersRD> &p_render_buffers);

	// Frees everything except Streamline, which stays loaded.
	void finish();

	~DlssUpscaler();
};
