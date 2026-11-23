/**************************************************************************/
/*  erethea_preview.cpp                                                   */
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

#include "erethea_preview.h"

#include "core/config/engine.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/image.h"
#include "core/object/callable_mp.h"
#include "core/os/os.h"
#include "modules/gltf/gltf_document.h"
#include "modules/gltf/gltf_state.h"
#include "scene/3d/camera_3d.h"
#include "scene/3d/light_3d.h"
#include "scene/3d/mesh_instance_3d.h"
#include "scene/3d/visual_instance_3d.h"
#include "scene/3d/world_environment.h"
#include "scene/main/viewport.h"
#include "scene/main/window.h"
#include "scene/resources/3d/primitive_meshes.h"
#include "scene/resources/environment.h"
#include "scene/resources/material.h"
#include "scene/resources/sky.h"

#ifdef MODULE_ERETHEA_QUALITY_ENABLED
#include "modules/erethea_quality/erethea_quality.h"
#endif

// Preview scene.

EretheaPreview::EretheaPreview() {
	set_name("EretheaPreview");
}

float EretheaPreview::get_camera_distance(float p_radius, float p_fov_degrees, float p_aspect) {
	const float vertical = Math::deg_to_rad(CLAMP(p_fov_degrees, 1.0f, 170.0f)) * 0.5f;
	const float horizontal = Math::atan(Math::tan(vertical) * MAX(p_aspect, 0.01f));
	return MAX(p_radius, 0.001f) / Math::sin(MIN(vertical, horizontal)) * FRAMING_MARGIN;
}

Vector3 EretheaPreview::get_camera_position(const Vector3 &p_target, float p_distance, float p_yaw_degrees, float p_pitch_degrees) {
	const float yaw = Math::deg_to_rad(p_yaw_degrees);
	const float pitch = Math::deg_to_rad(p_pitch_degrees);
	return p_target + p_distance * Vector3(Math::sin(yaw) * Math::cos(pitch), Math::sin(pitch), Math::cos(yaw) * Math::cos(pitch));
}

void EretheaPreview::_build_scene() {
	// Procedural sky, lit by the sun, with the sky as ambient and reflected light.
	Ref<ProceduralSkyMaterial> sky_material;
	sky_material.instantiate();
	Ref<Sky> sky;
	sky.instantiate();
	sky->set_material(sky_material);
	Ref<Environment> environment;
	environment.instantiate();
	environment->set_background(Environment::BG_SKY);
	environment->set_sky(sky);
	environment->set_ambient_source(Environment::AMBIENT_SOURCE_SKY);
	environment->set_reflection_source(Environment::REFLECTION_SOURCE_SKY);
	environment->set_tonemapper(Environment::TONE_MAPPER_FILMIC);
	WorldEnvironment *world_environment = memnew(WorldEnvironment);
	world_environment->set_name("WorldEnvironment");
	world_environment->set_environment(environment);
	add_child(world_environment);

	sun = memnew(DirectionalLight3D);
	sun->set_name("Sun");
	sun->set_rotation_degrees(Vector3(-50, -35, 0));
	sun->set_shadow(true);
	add_child(sun);

	ground = memnew(MeshInstance3D);
	ground->set_name("Ground");
	Ref<PlaneMesh> plane;
	plane.instantiate();
	ground->set_mesh(plane);
	Ref<StandardMaterial3D> ground_material;
	ground_material.instantiate();
	ground_material->set_albedo(Color(0.38, 0.38, 0.4));
	ground_material->set_roughness(0.9);
	ground->set_material_override(ground_material);
	add_child(ground);

	camera = memnew(Camera3D);
	camera->set_name("Camera");
	camera->set_fov(CAMERA_FOV);
	camera->set_current(true);
	add_child(camera);
}

void EretheaPreview::_collect_bounds(Node *p_node, Node3D *p_root, AABB &r_bounds, bool &r_found) {
	GeometryInstance3D *geometry = Object::cast_to<GeometryInstance3D>(p_node);
	if (geometry && geometry->is_visible()) {
		const AABB local = geometry->get_aabb();
		if (local.size != Vector3()) {
			const Transform3D to_root = p_root->get_global_transform().affine_inverse() * geometry->get_global_transform();
			const AABB aabb = to_root.xform(local);
			if (r_found) {
				r_bounds.merge_with(aabb);
			} else {
				r_bounds = aabb;
				r_found = true;
			}
		}
	}
	for (int i = 0; i < p_node->get_child_count(); i++) {
		_collect_bounds(p_node->get_child(i), p_root, r_bounds, r_found);
	}
}

void EretheaPreview::clear_asset() {
	if (asset) {
		remove_child(asset);
		memdelete(asset);
		asset = nullptr;
	}
	asset_path = String();
	asset_modified = 0;
	pending_modified = 0;
}

Error EretheaPreview::load_asset(const String &p_path) {
	ERR_FAIL_COND_V_MSG(!camera, ERR_UNCONFIGURED, "The preview isn't in the scene tree yet.");
	const String extension = p_path.get_extension().to_lower();
	ERR_FAIL_COND_V_MSG(extension != "glb" && extension != "gltf", ERR_FILE_UNRECOGNIZED, vformat("The preview loads .glb and .gltf files, not \"%s\".", p_path));

	Ref<GLTFDocument> document;
	document.instantiate();
	Ref<GLTFState> state;
	state.instantiate();
	Error err = document->append_from_file(p_path, state, 0);
	if (err != OK) {
		ERR_FAIL_V_MSG(err, vformat("Couldn't read the glTF file \"%s\".", p_path));
	}
	Node *scene = document->generate_scene(state);
	ERR_FAIL_NULL_V_MSG(scene, ERR_PARSE_ERROR, vformat("Couldn't build a scene from \"%s\".", p_path));
	Node3D *model = Object::cast_to<Node3D>(scene);
	if (!model) {
		// A glTF file without a 3D root node: wrap it.
		model = memnew(Node3D);
		model->add_child(scene);
	}

	// Only a good file replaces the asset on screen.
	clear_asset();
	asset = model;
	asset_path = p_path;
	asset_modified = FileAccess::get_modified_time(p_path);
	pending_modified = asset_modified;
	add_child(asset);

	// Stand the asset on the floor, at the center.
	model->set_position(Vector3());
	AABB bounds;
	bool found = false;
	_collect_bounds(model, model, bounds, found);
	if (!found) {
		bounds = AABB(Vector3(-0.5, 0, -0.5), Vector3(1, 1, 1));
	}
	const Vector3 anchor = bounds.get_center() * Vector3(1, 0, 1) + Vector3(0, bounds.position.y, 0);
	model->set_position(-anchor);
	asset_bounds = AABB(bounds.position - anchor, bounds.size);

	// The size of the scene follows the size of the asset.
	const float radius = MAX(asset_bounds.size.length() * 0.5f, 0.001f);
	ground->set_scale(Vector3(radius * 12.0f, 1, radius * 12.0f));
	ground->set_position(Vector3(0, -radius * 0.001f, 0));
	Ref<PlaneMesh> plane = ground->get_mesh();
	if (plane.is_valid()) {
		plane->set_size(Vector2(1, 1));
	}
	_update_camera();
	emit_signal(SNAME("asset_loaded"), p_path);
	return OK;
}

void EretheaPreview::set_orbit_angle(float p_degrees) {
	orbit_angle = Math::fposmod(p_degrees, 360.0f);
	if (camera) {
		_update_camera();
	}
}

void EretheaPreview::_update_camera() {
	if (!camera || !is_inside_tree()) {
		return;
	}
	const Vector2 size = get_viewport()->get_visible_rect().size;
	const float aspect = size.y > 0 ? size.x / size.y : 1.0f;
	const Vector3 target = asset_bounds.get_center();
	const float radius = MAX(asset_bounds.size.length() * 0.5f, 0.001f);
	const float distance = get_camera_distance(radius, CAMERA_FOV, aspect);
	camera->set_position(get_camera_position(target, distance, orbit_angle, CAMERA_PITCH));
	camera->look_at(target);
	camera->set_near(MAX(0.001f, (distance - radius) * 0.5f));
	camera->set_far((distance + radius * 24.0f) * 2.0f);
	sun->set_param(Light3D::PARAM_SHADOW_MAX_DISTANCE, distance + radius * 2.0f);
}

void EretheaPreview::_check_file_changed() {
	if (asset_path.is_empty() || !FileAccess::exists(asset_path)) {
		return;
	}
	const uint64_t modified = FileAccess::get_modified_time(asset_path);
	if (modified == asset_modified) {
		pending_modified = modified;
		return;
	}
	// The file changed. Wait until it stops changing, so a file that is
	// still being written isn't read half way.
	if (modified != pending_modified) {
		pending_modified = modified;
		return;
	}
	const String path = asset_path;
	if (load_asset(path) != OK) {
		// Don't try again until the file changes once more.
		asset_modified = modified;
	}
}

void EretheaPreview::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			_build_scene();
			_update_camera();
			set_process(true);
		} break;

		case NOTIFICATION_PROCESS: {
			const double delta = get_process_delta_time();
			orbit_angle = Math::fposmod(orbit_angle + float(orbit_speed * delta), 360.0f);
			_update_camera();
			if (auto_reload) {
				reload_timer += delta;
				if (reload_timer >= RELOAD_POLL_SECONDS) {
					reload_timer = 0.0;
					_check_file_changed();
				}
			}
		} break;
	}
}

void EretheaPreview::_bind_methods() {
	ClassDB::bind_method(D_METHOD("load_asset", "path"), &EretheaPreview::load_asset);
	ClassDB::bind_method(D_METHOD("clear_asset"), &EretheaPreview::clear_asset);
	ClassDB::bind_method(D_METHOD("get_asset_path"), &EretheaPreview::get_asset_path);
	ClassDB::bind_method(D_METHOD("has_asset"), &EretheaPreview::has_asset);
	ClassDB::bind_method(D_METHOD("get_asset_bounds"), &EretheaPreview::get_asset_bounds);
	ClassDB::bind_method(D_METHOD("get_camera"), &EretheaPreview::get_camera);

	ClassDB::bind_method(D_METHOD("set_auto_reload", "enabled"), &EretheaPreview::set_auto_reload);
	ClassDB::bind_method(D_METHOD("is_auto_reload"), &EretheaPreview::is_auto_reload);
	ClassDB::bind_method(D_METHOD("set_orbit_speed", "degrees_per_second"), &EretheaPreview::set_orbit_speed);
	ClassDB::bind_method(D_METHOD("get_orbit_speed"), &EretheaPreview::get_orbit_speed);
	ClassDB::bind_method(D_METHOD("set_orbit_angle", "degrees"), &EretheaPreview::set_orbit_angle);
	ClassDB::bind_method(D_METHOD("get_orbit_angle"), &EretheaPreview::get_orbit_angle);

	ADD_SIGNAL(MethodInfo("asset_loaded", PropertyInfo(Variant::STRING, "path")));

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "auto_reload"), "set_auto_reload", "is_auto_reload");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "orbit_speed", PROPERTY_HINT_RANGE, "-360,360,0.1,or_less,or_greater,suffix:°/s"), "set_orbit_speed", "get_orbit_speed");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "orbit_angle", PROPERTY_HINT_RANGE, "0,360,0.1,degrees"), "set_orbit_angle", "get_orbit_angle");
}

// Command line.

EretheaPreviewOptions EretheaPreviewOptions::parse(const List<String> &p_args) {
	EretheaPreviewOptions options;
	for (const String &arg : p_args) {
		if (arg.begins_with(ARGUMENT)) {
			options.enabled = true;
			options.asset = arg.trim_prefix(ARGUMENT);
		} else if (arg.begins_with(ARGUMENT_CAPTURE)) {
			options.capture = arg.trim_prefix(ARGUMENT_CAPTURE);
		} else if (arg.begins_with(ARGUMENT_FRAMES)) {
			const String value = arg.trim_prefix(ARGUMENT_FRAMES);
			if (!value.is_valid_int()) {
				options.error = vformat("%s needs a number of frames, not \"%s\".", String(ARGUMENT_FRAMES).trim_suffix("="), value);
			} else {
				options.capture_frames = MAX(int(value.to_int()), MIN_CAPTURE_FRAMES);
			}
		} else if (arg.begins_with(ARGUMENT_CONTROL)) {
			options.control = arg.trim_prefix(ARGUMENT_CONTROL);
		} else if (arg.begins_with(ARGUMENT_QUALITY)) {
			options.quality = arg.trim_prefix(ARGUMENT_QUALITY).to_lower();
		}
	}
	return options;
}

// Main loop.

EretheaPreviewLoop::EretheaPreviewLoop() {
}

void EretheaPreviewLoop::initialize() {
	SceneTree::initialize();
	options = EretheaPreviewOptions::parse(OS::get_singleton()->get_cmdline_args());
	if (!options.error.is_empty()) {
		ERR_PRINT(options.error);
	}

	preview = memnew(EretheaPreview);
	preview->connect(SNAME("asset_loaded"), callable_mp(this, &EretheaPreviewLoop::_on_asset_loaded));
	get_root()->add_child(preview);
	get_root()->set_title("Erethea Preview");
	_apply_quality();

	if (!options.capture.is_empty()) {
		// A still image: the same angle every time.
		preview->set_orbit_speed(0.0f);
	}
	if (!options.asset.is_empty() && preview->load_asset(options.asset) != OK && !options.capture.is_empty()) {
		finished = true;
		quit(1);
	}
}

void EretheaPreviewLoop::_on_asset_loaded(const String &p_path) {
	// The capture waits for the new asset to be drawn.
	frames_with_asset = 0;
	get_root()->set_title(vformat("Erethea Preview: %s", p_path.get_file()));
	print_line(vformat("Erethea preview: showing %s", p_path));
}

void EretheaPreviewLoop::_apply_quality() {
	if (options.quality.is_empty()) {
		// Like a new game: the project's preset, which is "Disabled" by default,
		// so the preview draws like the game does with default settings.
		return;
	}
#ifdef MODULE_ERETHEA_QUALITY_ENABLED
	static const char *names[] = { "disabled", "low", "balanced", "cinematic", "automatic" };
	for (int i = 0; i < 5; i++) {
		if (options.quality == names[i]) {
			if (EretheaQuality::get_singleton()) {
				EretheaQuality::get_singleton()->apply_preset(EretheaQuality::Preset(i));
			}
			return;
		}
	}
	ERR_PRINT(vformat("Unknown quality preset \"%s\". Use low, balanced, cinematic or automatic.", options.quality));
#else
	WARN_PRINT("This build has no Erethea quality presets; --erethea-preview-quality is ignored.");
#endif
}

void EretheaPreviewLoop::_poll_control_file(double p_delta) {
	control_timer += p_delta;
	if (options.control.is_empty() || control_timer < EretheaPreview::RELOAD_POLL_SECONDS) {
		return;
	}
	control_timer = 0.0;
	if (!FileAccess::exists(options.control)) {
		return;
	}
	// The control file holds the path of the asset to show. The host writes a
	// new path to show another asset without restarting the preview.
	const String content = FileAccess::get_file_as_string(options.control).strip_edges();
	if (content.is_empty() || content == control_content) {
		return;
	}
	control_content = content;
	if (content != preview->get_asset_path()) {
		preview->load_asset(content);
	}
}

void EretheaPreviewLoop::_save_capture() {
	finished = true;
	Ref<ViewportTexture> texture = get_root()->get_texture();
	Ref<Image> image = texture.is_valid() ? texture->get_image() : Ref<Image>();
	if (image.is_null() || image->is_empty()) {
		ERR_PRINT("The preview has no image to capture: the renderer draws nothing (is it running with --headless?).");
		quit(1);
		return;
	}
	String path = options.capture;
	if (path.is_relative_path()) {
		path = DirAccess::create(DirAccess::ACCESS_FILESYSTEM)->get_current_dir().path_join(path);
	}
	DirAccess::make_dir_recursive_absolute(path.get_base_dir());
	if (image->save_png(path) != OK) {
		ERR_PRINT(vformat("Couldn't save the capture to \"%s\".", path));
		quit(1);
		return;
	}
	print_line(vformat("Erethea preview: capture saved to %s (%d x %d).", path, image->get_width(), image->get_height()));
	quit(0);
}

bool EretheaPreviewLoop::process(double p_time) {
	const bool result = SceneTree::process(p_time);
	if (finished || !preview) {
		return result;
	}
	_poll_control_file(p_time);
	if (preview->has_asset()) {
		frames_with_asset++;
	}
	if (!options.capture.is_empty() && frames_with_asset >= options.capture_frames) {
		_save_capture();
	}
	return result;
}
