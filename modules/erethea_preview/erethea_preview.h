/**************************************************************************/
/*  erethea_preview.h                                                     */
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

#include "core/templates/list.h"
#include "scene/3d/node_3d.h"
#include "scene/main/scene_tree.h"

class Camera3D;
class DirectionalLight3D;
class MeshInstance3D;

// Erethea: the preview scene of the Fornalha. A glTF asset is loaded at
// runtime, standing on a floor, under a sun with shadows and a procedural sky,
// and the camera frames it and turns slowly around it. The asset can be
// replaced at any time, and it is reloaded when the file changes on disk.
class EretheaPreview : public Node3D {
	GDCLASS(EretheaPreview, Node3D);

public:
	static constexpr float CAMERA_FOV = 40.0f;
	static constexpr float CAMERA_PITCH = 20.0f;
	static constexpr float FRAMING_MARGIN = 1.15f;
	static constexpr double RELOAD_POLL_SECONDS = 0.5;

private:
	Camera3D *camera = nullptr;
	DirectionalLight3D *sun = nullptr;
	MeshInstance3D *ground = nullptr;
	Node *asset = nullptr;

	String asset_path;
	uint64_t asset_modified = 0;
	uint64_t pending_modified = 0;
	double reload_timer = 0.0;
	bool auto_reload = true;

	AABB asset_bounds = AABB(Vector3(-0.5, 0, -0.5), Vector3(1, 1, 1));
	float orbit_speed = 20.0f;
	float orbit_angle = 35.0f;

	void _build_scene();
	void _update_camera();
	void _check_file_changed();
	static void _collect_bounds(Node *p_node, Node3D *p_root, AABB &r_bounds, bool &r_found);

protected:
	void _notification(int p_what);
	static void _bind_methods();

public:
	// How far the camera must be so a sphere of this radius fits in the view,
	// for a vertical field of view in degrees and the aspect ratio of the
	// window. It's the narrowest of the two fields of view that decides.
	static float get_camera_distance(float p_radius, float p_fov_degrees, float p_aspect);
	// Where the camera is, for a point it looks at.
	static Vector3 get_camera_position(const Vector3 &p_target, float p_distance, float p_yaw_degrees, float p_pitch_degrees);

	Error load_asset(const String &p_path);
	void clear_asset();
	String get_asset_path() const { return asset_path; }
	bool has_asset() const { return asset != nullptr; }
	AABB get_asset_bounds() const { return asset_bounds; }
	Camera3D *get_camera() const { return camera; }

	void set_auto_reload(bool p_enabled) { auto_reload = p_enabled; }
	bool is_auto_reload() const { return auto_reload; }
	void set_orbit_speed(float p_degrees_per_second) { orbit_speed = p_degrees_per_second; }
	float get_orbit_speed() const { return orbit_speed; }
	void set_orbit_angle(float p_degrees);
	float get_orbit_angle() const { return orbit_angle; }

	EretheaPreview();
};

// What the command line asks for.
struct EretheaPreviewOptions {
	static constexpr const char *ARGUMENT = "--erethea-preview=";
	static constexpr const char *ARGUMENT_CAPTURE = "--erethea-preview-capture=";
	static constexpr const char *ARGUMENT_FRAMES = "--erethea-preview-frames=";
	static constexpr const char *ARGUMENT_CONTROL = "--erethea-preview-control=";
	static constexpr const char *ARGUMENT_QUALITY = "--erethea-preview-quality=";
	static constexpr int DEFAULT_CAPTURE_FRAMES = 10;
	static constexpr int MIN_CAPTURE_FRAMES = 3;

	bool enabled = false;
	String asset;
	String capture;
	int capture_frames = DEFAULT_CAPTURE_FRAMES;
	String control;
	String quality;
	String error;

	static EretheaPreviewOptions parse(const List<String> &p_args);
};

// The main loop of the preview, chosen by --erethea-preview=<file>. It needs
// no project: it builds the scene itself, applies the options and saves the
// capture.
class EretheaPreviewLoop : public SceneTree {
	GDCLASS(EretheaPreviewLoop, SceneTree);

	EretheaPreviewOptions options;
	EretheaPreview *preview = nullptr;
	int frames_with_asset = 0;
	bool finished = false;
	String control_content;
	double control_timer = 0.0;

	void _on_asset_loaded(const String &p_path);
	void _poll_control_file(double p_delta);
	void _save_capture();
	void _apply_quality();

public:
	virtual void initialize() override;
	virtual bool process(double p_time) override;

	EretheaPreviewLoop();
};
