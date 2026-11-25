/**************************************************************************/
/*  erethea_upscaler.h                                                    */
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

#include "amd_fsr_upscaler.h"
#include "dlss_upscaler.h"
#include "xess_upscaler.h"

#include "servers/rendering/renderer_rd/effects/temporal_upscaler.h"

#include <atomic>

// Picks the temporal upscaler that runs where Godot would run FSR 2.2, from
// the project setting rendering/erethea/temporal_upscaler. Every upscaler
// falls back to FSR 2.2 where it can't run, so projects behave the same on
// every computer, just with a different upscaler. With "Automatic", the
// upscaler comes from the graphics card: DLSS on NVIDIA, XeSS on Intel, AMD
// FSR on the others and where those can't run.
class EretheaUpscaler : public RendererRD::TemporalUpscaler {
public:
	enum Mode {
		MODE_FSR2,
		MODE_AMD_FSR,
		MODE_XESS,
		MODE_DLSS,
		MODE_AUTOMATIC,
		MODE_MAX,
	};

	static constexpr const char *PROJECT_SETTING = "rendering/erethea/temporal_upscaler";

private:
	static inline EretheaUpscaler *singleton = nullptr;

	std::atomic<int> mode = MODE_FSR2;
	AmdFsrUpscaler amd_fsr;
	XessUpscaler xess;
	DlssUpscaler dlss;

	// The upscalers that "Automatic" tries, in order, on this graphics card.
	// Only used on the rendering thread.
	Vector<int> automatic_order;

	static void _update_from_project_settings();
	static void _finish_on_render_thread();

	bool _upscale_with(Mode p_mode, const Ref<RenderSceneBuffersRD> &p_render_buffers, RenderDataRD *p_render_data, double p_time_step);
	// Frees what the other upscalers kept for these render buffers.
	void _release_except(Mode p_mode, const Ref<RenderSceneBuffersRD> &p_render_buffers);

public:
	static EretheaUpscaler *get_singleton() { return singleton; }

	static void register_project_settings();
	static Mode get_project_mode();

	// The upscalers that "Automatic" tries on a graphics card of this vendor
	// (as RenderingDevice::get_device_vendor_name() gives it), best first.
	static Vector<int> get_automatic_order(const String &p_vendor);

	// Prepares the rendering driver for the project's upscaler, before the
	// rendering device is created.
	static void prepare_rendering_driver();
	static void finish_rendering_driver();

	// Creates the upscaler and hands it to the renderer.
	static void initialize();
	// Takes it back from the renderer and frees it on the rendering thread.
	static void finish();

	void set_mode(Mode p_mode);
	Mode get_mode() const;

	virtual bool upscale(const Ref<RenderSceneBuffersRD> &p_render_buffers, RenderDataRD *p_render_data, double p_time_step) override;
};
