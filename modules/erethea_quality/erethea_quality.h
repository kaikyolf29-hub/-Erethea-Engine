/**************************************************************************/
/*  erethea_quality.h                                                     */
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

#include "core/object/object.h"
#include "core/variant/dictionary.h"
#include "core/variant/type_info.h"
#include "servers/rendering/rendering_device_enums.h"

// Erethea quality presets: one project setting that raises or lowers Godot's
// own rendering quality settings together (shadows, ambient occlusion,
// antialiasing, texture filtering...), chosen by hand or from the graphics
// card. It only changes settings that the project left at their default value,
// and only in the running project, never in the editor.
class EretheaQuality : public Object {
	GDCLASS(EretheaQuality, Object);

public:
	enum Preset {
		PRESET_DISABLED,
		PRESET_LOW,
		PRESET_BALANCED,
		PRESET_CINEMATIC,
		PRESET_AUTOMATIC,
	};

private:
	static inline EretheaQuality *singleton = nullptr;

	Preset active_preset = PRESET_DISABLED;
	bool preset_applied = false;

protected:
	static void _bind_methods();

public:
	static constexpr const char *PROJECT_SETTING = "rendering/erethea/quality_preset";

	static EretheaQuality *get_singleton() { return singleton; }
	static void register_project_settings();

	// The values of each preset, by project setting path. Empty for
	// PRESET_DISABLED and PRESET_AUTOMATIC.
	static Dictionary get_preset_values(Preset p_preset);
	// Which preset suits a graphics card: integrated or software ones get the
	// low preset, dedicated cards with ray tracing the cinematic one.
	static Preset recommend_preset_for_device(RenderingDeviceEnums::DeviceType p_type, bool p_raytracing);

	Preset get_project_preset() const;
	Preset get_recommended_preset() const;
	Preset get_active_preset() const { return active_preset; }
	Dictionary get_preset_settings(Preset p_preset) const;

	void apply_preset(Preset p_preset);
	void apply_project_preset();

	EretheaQuality();
	~EretheaQuality();
};

VARIANT_ENUM_CAST(EretheaQuality::Preset);
