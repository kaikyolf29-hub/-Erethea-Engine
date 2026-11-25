/**************************************************************************/
/*  erethea_contact_shadows.h                                             */
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

// Contact shadows of the sun (the first directional light) in screen space,
// for the Forward+ renderer on any graphics card. After the depth pre-pass, a
// short ray goes from the surface seen in each pixel towards the light, and
// the depth buffer tells if something covers it: the small, sharp shadows
// where things touch the ground that the shadow map misses. The renderer
// combines them with the light's shadow map. Where EretheaRayTracedShadows
// runs, it does nothing, since the ray traced shadows already have them.
class EretheaContactShadows : public CompositorEffect {
	GDCLASS(EretheaContactShadows, CompositorEffect);

	float length = 0.5;
	float thickness = 0.2;
	int steps = 16;
	float normal_bias = 0.01;

	void _render(int p_effect_callback_type, const RenderData *p_render_data);
	static void _free_render_resources();

protected:
	static void _bind_methods();
	void _validate_property(PropertyInfo &p_property) const;

public:
	static constexpr int MAX_STEPS = 64;

	void set_length(float p_length);
	float get_length() const;
	void set_thickness(float p_thickness);
	float get_thickness() const;
	void set_steps(int p_steps);
	int get_steps() const;
	void set_normal_bias(float p_bias);
	float get_normal_bias() const;

	// Frees the shared rendering resources, when the module ends.
	static void finish();

	EretheaContactShadows();
};
