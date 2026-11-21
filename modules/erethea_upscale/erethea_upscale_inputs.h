/**************************************************************************/
/*  erethea_upscale_inputs.h                                              */
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

#include "erethea_upscale_inputs.glsl.gen.h"

#include "core/string/string_name.h"
#include "core/templates/rid.h"

class RenderDataRD;
class RenderSceneBuffersRD;

// Prepares what the vendor upscalers read besides the color: the depth, the
// motion vectors and the reactive mask, copied into textures of the formats
// they accept (R32_SFLOAT, R16G16_SFLOAT and R8_UNORM) at the internal
// resolution. The motion vectors are in UV units and point to the previous
// frame, with the motion of the sky filled in from the camera's, as Godot's
// FSR 2.2 does. Only used on the rendering thread.
class EretheaUpscaleInputs {
	EretheaUpscaleInputsShaderRD shader;
	RID shader_version;
	RID pipeline;

public:
	struct Textures {
		RID depth;
		RID velocity;
		RID reactive;
	};

	// Records the compute pass that fills the textures, which are kept in the
	// render buffers under p_context.
	Textures prepare(const Ref<RenderSceneBuffersRD> &p_render_buffers, RenderDataRD *p_render_data, const StringName &p_context);

	void finish();

	~EretheaUpscaleInputs();
};
