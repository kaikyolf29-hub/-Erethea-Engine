/**************************************************************************/
/*  erethea_motion_blur.h                                                 */
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

#include "scene/resources/compositor.h"

// Per-object and camera motion blur from the motion vectors, drawn after the
// transparent pass, before TAA, upscaling and tone mapping. It needs the
// Forward+ renderer and does nothing elsewhere.
class EretheaMotionBlur : public CompositorEffect {
	GDCLASS(EretheaMotionBlur, CompositorEffect);

	float intensity = 0.5;
	int samples = 12;
	float max_length = 0.05;

	void _render(int p_effect_callback_type, const RenderData *p_render_data);
	static void _free_render_resources();

protected:
	static void _bind_methods();
	void _validate_property(PropertyInfo &p_property) const;

public:
	void set_intensity(float p_intensity);
	float get_intensity() const;

	void set_samples(int p_samples);
	int get_samples() const;

	void set_max_length(float p_max_length);
	float get_max_length() const;

	// Frees the shader shared by all instances. Called when the module shuts down.
	static void finish();

	EretheaMotionBlur();
};
