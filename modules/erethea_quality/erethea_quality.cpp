/**************************************************************************/
/*  erethea_quality.cpp                                                   */
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

#include "erethea_quality.h"

#include "core/config/project_settings.h"
#include "core/object/class_db.h"
#include "scene/main/scene_tree.h"
#include "scene/main/window.h"
#include "servers/rendering/rendering_device.h"
#include "servers/rendering/rendering_server.h"

#include <initializer_list>

namespace {

struct PresetEntry {
	const char *path;
	Variant::Type type;
	// Values for the low, balanced and cinematic presets.
	double values[3];
	// Settings of effects that only Forward+ draws, or that only it supports
	// with these values (FSR 2, SMAA). Mobile and Compatibility skip them.
	bool forward_plus_only;
};

// Balanced is close to Godot's defaults, with sharper textures and SMAA.
// Cinematic uses FSR 2 at native resolution as its antialiasing.
const PresetEntry PRESET_ENTRIES[] = {
	{ "rendering/anti_aliasing/quality/screen_space_aa", Variant::INT, { 1, 2, 0 }, true },
	{ "rendering/scaling_3d/mode", Variant::INT, { 1, 0, 2 }, true },
	{ "rendering/scaling_3d/scale", Variant::FLOAT, { 0.77, 1.0, 1.0 }, true },
	{ "rendering/textures/default_filters/anisotropic_filtering_level", Variant::INT, { 1, 3, 4 }, false },
	{ "rendering/mesh_lod/lod_change/threshold_pixels", Variant::FLOAT, { 2.0, 1.0, 0.75 }, false },
	{ "rendering/lights_and_shadows/directional_shadow/size", Variant::INT, { 2048, 4096, 8192 }, false },
	{ "rendering/lights_and_shadows/directional_shadow/soft_shadow_filter_quality", Variant::INT, { 1, 2, 4 }, false },
	{ "rendering/lights_and_shadows/positional_shadow/atlas_size", Variant::INT, { 2048, 4096, 8192 }, false },
	{ "rendering/lights_and_shadows/positional_shadow/soft_shadow_filter_quality", Variant::INT, { 1, 2, 4 }, false },
	{ "rendering/environment/ssao/quality", Variant::INT, { 1, 2, 3 }, true },
	{ "rendering/environment/ssao/half_size", Variant::BOOL, { 1, 1, 0 }, true },
	{ "rendering/environment/ssil/quality", Variant::INT, { 1, 2, 3 }, true },
	{ "rendering/environment/ssil/half_size", Variant::BOOL, { 1, 1, 0 }, true },
	{ "rendering/environment/glow/upscale_mode", Variant::INT, { 0, 1, 1 }, true },
	{ "rendering/environment/subsurface_scattering/subsurface_scattering_quality", Variant::INT, { 1, 1, 2 }, true },
	{ "rendering/camera/depth_of_field/depth_of_field_bokeh_quality", Variant::INT, { 0, 1, 2 }, true },
	{ "rendering/environment/volumetric_fog/volume_size", Variant::INT, { 48, 64, 96 }, true },
	{ "rendering/environment/volumetric_fog/volume_depth", Variant::INT, { 48, 64, 96 }, true },
	{ "rendering/global_illumination/sdfgi/probe_ray_count", Variant::INT, { 0, 1, 2 }, true },
};

Variant entry_value(const PresetEntry &p_entry, int p_index) {
	const double value = p_entry.values[p_index];
	switch (p_entry.type) {
		case Variant::BOOL:
			return value != 0.0;
		case Variant::INT:
			return int(value);
		default:
			return value;
	}
}

} // namespace

void EretheaQuality::register_project_settings() {
	GLOBAL_DEF_BASIC(PropertyInfo(Variant::INT, PROJECT_SETTING, PROPERTY_HINT_ENUM, "Disabled,Low,Balanced,Cinematic,Automatic"), PRESET_DISABLED);
}

Dictionary EretheaQuality::get_preset_values(Preset p_preset) {
	Dictionary values;
	if (p_preset < PRESET_LOW || p_preset > PRESET_CINEMATIC) {
		return values;
	}
	for (const PresetEntry &entry : PRESET_ENTRIES) {
		values[entry.path] = entry_value(entry, p_preset - PRESET_LOW);
	}
	return values;
}

EretheaQuality::Preset EretheaQuality::recommend_preset_for_device(RenderingDeviceEnums::DeviceType p_type, bool p_raytracing) {
	if (p_type != RenderingDeviceEnums::DEVICE_TYPE_DISCRETE_GPU) {
		return PRESET_LOW;
	}
	return p_raytracing ? PRESET_CINEMATIC : PRESET_BALANCED;
}

EretheaQuality::Preset EretheaQuality::get_project_preset() const {
	const int preset = GLOBAL_GET(PROJECT_SETTING);
	return Preset(CLAMP(preset, int(PRESET_DISABLED), int(PRESET_AUTOMATIC)));
}

EretheaQuality::Preset EretheaQuality::get_recommended_preset() const {
	const RenderingDevice *device = RenderingDevice::get_singleton();
	if (device == nullptr) {
		// The Compatibility renderer.
		return PRESET_LOW;
	}
	const bool raytracing = device->has_feature(RenderingDevice::SUPPORTS_RAY_QUERY) || device->has_feature(RenderingDevice::SUPPORTS_RAYTRACING_PIPELINE);
	return recommend_preset_for_device(RenderingServer::get_singleton()->get_video_adapter_type(), raytracing);
}

Dictionary EretheaQuality::get_preset_settings(Preset p_preset) const {
	if (p_preset == PRESET_AUTOMATIC) {
		p_preset = get_recommended_preset();
	}
	const Dictionary values = get_preset_values(p_preset);
	const bool forward_plus = RenderingServer::get_singleton() && RenderingServer::get_singleton()->get_current_rendering_method() == "forward_plus";
	ProjectSettings *project_settings = ProjectSettings::get_singleton();

	Dictionary settings;
	for (const PresetEntry &entry : PRESET_ENTRIES) {
		if (!values.has(entry.path) || (entry.forward_plus_only && !forward_plus) || !project_settings->has_setting(entry.path)) {
			continue;
		}
		// Keep what the project chose on purpose.
		if (project_settings->get_setting(entry.path) != project_settings->property_get_revert(entry.path)) {
			continue;
		}
		settings[entry.path] = values[entry.path];
	}
	return settings;
}

void EretheaQuality::apply_project_preset() {
	// A script already chose a preset, for example from the game's options.
	if (preset_applied) {
		return;
	}
	apply_preset(get_project_preset());
}

void EretheaQuality::apply_preset(Preset p_preset) {
	ERR_FAIL_INDEX(p_preset, PRESET_AUTOMATIC + 1);
	preset_applied = true;
	active_preset = p_preset == PRESET_AUTOMATIC ? get_recommended_preset() : p_preset;
	const Dictionary settings = get_preset_settings(active_preset);
	static const char *preset_names[] = { "Disabled", "Low", "Balanced", "Cinematic", "Automatic" };
	print_verbose(vformat("Erethea quality preset: %s (%d settings changed).", preset_names[active_preset], settings.size()));
	if (settings.is_empty()) {
		return;
	}

	const auto value = [&settings](const char *p_path) -> Variant {
		return settings.has(p_path) ? settings[p_path] : GLOBAL_GET(p_path);
	};
	const auto has_any = [&settings](std::initializer_list<const char *> p_paths) -> bool {
		for (const char *path : p_paths) {
			if (settings.has(path)) {
				return true;
			}
		}
		return false;
	};

	// The same calls as EditorNode::_update_from_settings(), for the settings
	// that the rendering server reads when it starts.
	RenderingServer *rs = RenderingServer::get_singleton();
	if (has_any({ "rendering/lights_and_shadows/directional_shadow/size" })) {
		rs->directional_shadow_atlas_set_size(value("rendering/lights_and_shadows/directional_shadow/size"), GLOBAL_GET("rendering/lights_and_shadows/directional_shadow/16_bits"));
	}
	if (has_any({ "rendering/lights_and_shadows/directional_shadow/soft_shadow_filter_quality" })) {
		rs->directional_soft_shadow_filter_set_quality(RSE::ShadowQuality(int(value("rendering/lights_and_shadows/directional_shadow/soft_shadow_filter_quality"))));
	}
	if (has_any({ "rendering/lights_and_shadows/positional_shadow/soft_shadow_filter_quality" })) {
		rs->positional_soft_shadow_filter_set_quality(RSE::ShadowQuality(int(value("rendering/lights_and_shadows/positional_shadow/soft_shadow_filter_quality"))));
	}
	if (has_any({ "rendering/environment/ssao/quality", "rendering/environment/ssao/half_size" })) {
		rs->environment_set_ssao_quality(RSE::EnvironmentSSAOQuality(int(value("rendering/environment/ssao/quality"))), value("rendering/environment/ssao/half_size"), GLOBAL_GET("rendering/environment/ssao/adaptive_target"), GLOBAL_GET("rendering/environment/ssao/blur_passes"), GLOBAL_GET("rendering/environment/ssao/fadeout_from"), GLOBAL_GET("rendering/environment/ssao/fadeout_to"));
	}
	if (has_any({ "rendering/environment/ssil/quality", "rendering/environment/ssil/half_size" })) {
		rs->environment_set_ssil_quality(RSE::EnvironmentSSILQuality(int(value("rendering/environment/ssil/quality"))), value("rendering/environment/ssil/half_size"), GLOBAL_GET("rendering/environment/ssil/adaptive_target"), GLOBAL_GET("rendering/environment/ssil/blur_passes"), GLOBAL_GET("rendering/environment/ssil/fadeout_from"), GLOBAL_GET("rendering/environment/ssil/fadeout_to"));
	}
	if (has_any({ "rendering/environment/glow/upscale_mode" })) {
		rs->environment_glow_set_use_bicubic_upscale(int(value("rendering/environment/glow/upscale_mode")) > 0);
	}
	if (has_any({ "rendering/environment/subsurface_scattering/subsurface_scattering_quality" })) {
		rs->sub_surface_scattering_set_quality(RSE::SubSurfaceScatteringQuality(int(value("rendering/environment/subsurface_scattering/subsurface_scattering_quality"))));
	}
	if (has_any({ "rendering/camera/depth_of_field/depth_of_field_bokeh_quality" })) {
		rs->camera_attributes_set_dof_blur_quality(RSE::DOFBlurQuality(int(value("rendering/camera/depth_of_field/depth_of_field_bokeh_quality"))), GLOBAL_GET("rendering/camera/depth_of_field/depth_of_field_use_jitter"));
	}
	if (has_any({ "rendering/environment/volumetric_fog/volume_size", "rendering/environment/volumetric_fog/volume_depth" })) {
		rs->environment_set_volumetric_fog_volume_size(value("rendering/environment/volumetric_fog/volume_size"), value("rendering/environment/volumetric_fog/volume_depth"));
	}
	if (has_any({ "rendering/global_illumination/sdfgi/probe_ray_count" })) {
		rs->environment_set_sdfgi_ray_count(RSE::EnvironmentSDFGIRayCount(int(value("rendering/global_illumination/sdfgi/probe_ray_count"))));
	}

	// The settings that SceneTree gives to its root viewport when it starts.
	SceneTree *tree = SceneTree::get_singleton();
	Window *root = tree ? tree->get_root() : nullptr;
	if (root == nullptr) {
		return;
	}
	if (has_any({ "rendering/anti_aliasing/quality/screen_space_aa" })) {
		root->set_screen_space_aa(Viewport::ScreenSpaceAA(int(value("rendering/anti_aliasing/quality/screen_space_aa"))));
	}
	if (has_any({ "rendering/scaling_3d/mode" })) {
		root->set_scaling_3d_mode(Viewport::Scaling3DMode(int(value("rendering/scaling_3d/mode"))));
	}
	if (has_any({ "rendering/scaling_3d/scale" })) {
		root->set_scaling_3d_scale(value("rendering/scaling_3d/scale"));
	}
	if (has_any({ "rendering/textures/default_filters/anisotropic_filtering_level" })) {
		root->set_anisotropic_filtering_level(Viewport::AnisotropicFiltering(int(value("rendering/textures/default_filters/anisotropic_filtering_level"))));
	}
	if (has_any({ "rendering/mesh_lod/lod_change/threshold_pixels" })) {
		root->set_mesh_lod_threshold(value("rendering/mesh_lod/lod_change/threshold_pixels"));
	}
	if (has_any({ "rendering/lights_and_shadows/positional_shadow/atlas_size" })) {
		root->set_positional_shadow_atlas_size(value("rendering/lights_and_shadows/positional_shadow/atlas_size"));
	}
}

void EretheaQuality::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_project_preset"), &EretheaQuality::get_project_preset);
	ClassDB::bind_method(D_METHOD("get_recommended_preset"), &EretheaQuality::get_recommended_preset);
	ClassDB::bind_method(D_METHOD("get_active_preset"), &EretheaQuality::get_active_preset);
	ClassDB::bind_method(D_METHOD("get_preset_settings", "preset"), &EretheaQuality::get_preset_settings);
	ClassDB::bind_method(D_METHOD("apply_preset", "preset"), &EretheaQuality::apply_preset);

	BIND_ENUM_CONSTANT(PRESET_DISABLED);
	BIND_ENUM_CONSTANT(PRESET_LOW);
	BIND_ENUM_CONSTANT(PRESET_BALANCED);
	BIND_ENUM_CONSTANT(PRESET_CINEMATIC);
	BIND_ENUM_CONSTANT(PRESET_AUTOMATIC);
}

EretheaQuality::EretheaQuality() {
	singleton = this;
}

EretheaQuality::~EretheaQuality() {
	if (singleton == this) {
		singleton = nullptr;
	}
}
