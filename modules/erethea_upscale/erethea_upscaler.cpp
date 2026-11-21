/**************************************************************************/
/*  erethea_upscaler.cpp                                                  */
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

#include "erethea_upscaler.h"

#include "core/config/project_settings.h"
#include "core/object/callable_mp.h"
#include "servers/rendering/rendering_server.h"

void EretheaUpscaler::register_project_settings() {
	GLOBAL_DEF(PropertyInfo(Variant::INT, PROJECT_SETTING, PROPERTY_HINT_ENUM, "FSR 2.2,AMD FSR"), MODE_FSR2);
}

EretheaUpscaler::Mode EretheaUpscaler::get_project_mode() {
	const int value = GLOBAL_GET(PROJECT_SETTING);
	return Mode(CLAMP(value, int(MODE_FSR2), int(MODE_MAX) - 1));
}

void EretheaUpscaler::_update_from_project_settings() {
	if (singleton != nullptr) {
		singleton->set_mode(get_project_mode());
	}
}

void EretheaUpscaler::initialize() {
	ERR_FAIL_COND(singleton != nullptr);
	singleton = memnew(EretheaUpscaler);
	singleton->set_mode(get_project_mode());
	ProjectSettings::get_singleton()->connect("settings_changed", callable_mp_static(&EretheaUpscaler::_update_from_project_settings));
	RendererRD::TemporalUpscaler::set_singleton(singleton);
}

void EretheaUpscaler::_finish_on_render_thread() {
	if (RendererRD::TemporalUpscaler::get_singleton() == singleton) {
		RendererRD::TemporalUpscaler::set_singleton(nullptr);
	}
	memdelete(singleton);
	singleton = nullptr;
}

void EretheaUpscaler::finish() {
	if (singleton == nullptr) {
		return;
	}
	ProjectSettings::get_singleton()->disconnect("settings_changed", callable_mp_static(&EretheaUpscaler::_update_from_project_settings));
	RenderingServer *rs = RenderingServer::get_singleton();
	if (rs != nullptr) {
		rs->call_on_render_thread(callable_mp_static(&EretheaUpscaler::_finish_on_render_thread));
	} else {
		_finish_on_render_thread();
	}
}

void EretheaUpscaler::set_mode(Mode p_mode) {
	mode = CLAMP(int(p_mode), int(MODE_FSR2), int(MODE_MAX) - 1);
}

EretheaUpscaler::Mode EretheaUpscaler::get_mode() const {
	return Mode(mode.load());
}

bool EretheaUpscaler::upscale(const Ref<RenderSceneBuffersRD> &p_render_buffers, RenderDataRD *p_render_data, double p_time_step) {
	switch (get_mode()) {
		case MODE_AMD_FSR:
			return amd_fsr.upscale(p_render_buffers, p_render_data, p_time_step);
		default:
			amd_fsr.release(p_render_buffers);
			return false;
	}
}
