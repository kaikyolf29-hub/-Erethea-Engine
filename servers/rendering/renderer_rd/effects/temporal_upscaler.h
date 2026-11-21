/**************************************************************************/
/*  temporal_upscaler.h                                                   */
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

namespace RendererRD {

// Erethea: Lets a module replace FSR 2.2 with another temporal upscaler
// (for example AMD FSR 3.1/4 through its own runtime). The renderer calls
// upscale() on the render thread where it would run FSR 2.2; returning false
// lets FSR 2.2 run as usual.
class TemporalUpscaler {
	static inline TemporalUpscaler *singleton = nullptr;

public:
	static TemporalUpscaler *get_singleton() { return singleton; }
	static void set_singleton(TemporalUpscaler *p_upscaler) { singleton = p_upscaler; }

	// Reads the internal color, depth and velocity of p_render_buffers and
	// writes its upscaled texture.
	virtual bool upscale(const Ref<RenderSceneBuffersRD> &p_render_buffers, RenderDataRD *p_render_data, double p_time_step) = 0;

	virtual ~TemporalUpscaler() {}
};

} // namespace RendererRD
