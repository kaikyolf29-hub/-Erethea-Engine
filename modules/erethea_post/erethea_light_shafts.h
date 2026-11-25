/**************************************************************************/
/*  erethea_light_shafts.h                                                */
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

#include "core/math/projection.h"
#include "core/math/transform_3d.h"
#include "scene/resources/compositor.h"

// Cheap screen-space light shafts ("god rays") from the sun, drawn after the
// transparent pass, before TAA, upscaling and tone mapping. They are a radial
// blur of the bright sky towards the sun's position on screen, so they cost a
// fixed number of samples per pixel and need no volumetric fog. It needs the
// Forward+ renderer and does nothing elsewhere.
class EretheaLightShafts : public CompositorEffect {
	GDCLASS(EretheaLightShafts, CompositorEffect);

	float intensity = 0.8;
	int samples = 24;
	float density = 0.9;
	float decay = 0.5;
	Color tint = Color(1, 1, 1);
	float threshold = 0.4;
	Vector3 custom_direction;

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

	void set_density(float p_density);
	float get_density() const;

	void set_decay(float p_decay);
	float get_decay() const;

	void set_tint(const Color &p_tint);
	Color get_tint() const;

	void set_threshold(float p_threshold);
	float get_threshold() const;

	void set_custom_direction(const Vector3 &p_direction);
	Vector3 get_custom_direction() const;

	// Projects the sun onto the screen. p_sun_direction points from the scene
	// to the sun, in world space. Returns how visible the shafts are, from 0
	// (the sun is behind the camera or far outside the screen) to 1, and sets
	// r_uv to the sun's position on screen in UV units, which may be outside
	// 0-1. Only perspective projections are supported.
	static float project_sun(const Projection &p_projection, const Transform3D &p_camera_transform, const Vector3 &p_sun_direction, bool p_flip_y, Vector2 &r_uv);

	// Frees the shader shared by all instances. Called when the module shuts down.
	static void finish();

	EretheaLightShafts();
};
