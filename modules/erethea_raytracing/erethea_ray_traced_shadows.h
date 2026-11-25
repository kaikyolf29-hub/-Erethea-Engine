/**************************************************************************/
/*  erethea_ray_traced_shadows.h                                          */
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

class Node;

// Ray traced shadow of the sun (the first directional light), for the
// Forward+ renderer on graphics cards with ray tracing (ray queries). After
// the depth pre-pass, rays go from the surface seen in each pixel towards the
// light, against the geometry around the camera (visible or not), and the
// renderer uses the result instead of the light's shadow map, or together with
// it. With the light's angular size, the shadows are soft where they are far
// from what casts them, and sharp where they touch it.
//
// Meshes and MultiMeshes cast ray traced shadows; skinned meshes and
// particles don't yet. On cards without ray tracing, it does nothing and the
// shadow map stays.
class EretheaRayTracedShadows : public CompositorEffect {
	GDCLASS(EretheaRayTracedShadows, CompositorEffect);

	float max_distance = 100.0;
	float light_size = 0.5;
	int samples = 4;
	float normal_bias = 0.02;

	void _render(int p_effect_callback_type, const RenderData *p_render_data);
	static void _free_render_resources();

	static void _add_to_node(Node *p_node);
	static void _add_to_tree(Node *p_node);

protected:
	static void _bind_methods();
	void _validate_property(PropertyInfo &p_property) const;

public:
	static constexpr int MAX_SAMPLES = 16;

	// The project setting that adds this effect to the running scenes, so the
	// sun casts ray traced shadows without changing them.
	static constexpr const char *SUN_SHADOWS_SETTING = "rendering/erethea/ray_tracing/sun_shadows";
	enum SunShadowsMode {
		SUN_SHADOWS_DISABLED,
		// With the "Automatic" quality preset, on graphics cards for which it
		// picks "Cinematic" (discrete, with ray tracing).
		SUN_SHADOWS_AUTOMATIC,
		// On every graphics card with ray tracing.
		SUN_SHADOWS_ENABLED,
	};

	static void register_project_settings();
	// Whether the setting adds the effect, for a graphics card that supports it
	// or not, and with the quality preset that "Automatic" picked.
	static bool is_added_to_scenes(int p_setting, bool p_supported, bool p_automatic_cinematic_quality);
	// Adds the effect to the compositors of the running project's
	// WorldEnvironment nodes (creating one where there is none) and to the
	// cameras' own compositors, when the project setting asks for it.
	static void add_to_running_scenes();

	void set_max_distance(float p_distance);
	float get_max_distance() const;
	void set_light_size(float p_degrees);
	float get_light_size() const;
	void set_samples(int p_samples);
	int get_samples() const;
	void set_normal_bias(float p_bias);
	float get_normal_bias() const;

	// Whether the current rendering device can trace rays from compute shaders.
	static bool is_supported();

	// Frees the shared rendering resources, when the module ends.
	static void finish();

	EretheaRayTracedShadows();
};
