/**************************************************************************/
/*  erethea_post_effect.h                                                 */
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

// Lens and film effects (lens flare, vignette, chromatic aberration and film grain),
// drawn after the transparent pass, before TAA, upscaling and tone mapping.
// It needs the Forward+ renderer and does nothing elsewhere.
class EretheaPostEffect : public CompositorEffect {
	GDCLASS(EretheaPostEffect, CompositorEffect);

	float vignette_intensity = 0.0;
	float vignette_smoothness = 0.6;
	float chromatic_aberration = 0.0;
	float film_grain_intensity = 0.0;
	float film_grain_size = 1.5;
	float lens_flare_intensity = 0.0;
	float lens_flare_threshold = 1.0;
	int lens_flare_ghosts = 4;
	float lens_flare_ghost_spacing = 0.35;
	float lens_flare_halo_width = 0.45;

	void _render(int p_effect_callback_type, const RenderData *p_render_data);
	static void _free_render_resources();

protected:
	static void _bind_methods();
	void _validate_property(PropertyInfo &p_property) const;

public:
	void set_vignette_intensity(float p_intensity);
	float get_vignette_intensity() const;

	void set_vignette_smoothness(float p_smoothness);
	float get_vignette_smoothness() const;

	void set_chromatic_aberration(float p_amount);
	float get_chromatic_aberration() const;

	void set_film_grain_intensity(float p_intensity);
	float get_film_grain_intensity() const;

	void set_film_grain_size(float p_size);
	float get_film_grain_size() const;

	void set_lens_flare_intensity(float p_intensity);
	float get_lens_flare_intensity() const;

	void set_lens_flare_threshold(float p_threshold);
	float get_lens_flare_threshold() const;

	void set_lens_flare_ghosts(int p_ghosts);
	int get_lens_flare_ghosts() const;

	void set_lens_flare_ghost_spacing(float p_spacing);
	float get_lens_flare_ghost_spacing() const;

	void set_lens_flare_halo_width(float p_width);
	float get_lens_flare_halo_width() const;

	// True when at least one effect is on.
	bool is_active() const;

	// Frees the shader shared by all instances. Called when the module shuts down.
	static void finish();

	EretheaPostEffect();
};
