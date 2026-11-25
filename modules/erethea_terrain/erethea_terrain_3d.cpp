/**************************************************************************/
/*  erethea_terrain_3d.cpp                                                */
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

#include "erethea_terrain_3d.h"

#include "erethea_terrain_clipmap.h"
#include "erethea_terrain_shader.h"

#include "core/config/engine.h"
#include "core/object/callable_mp.h"
#include "scene/main/viewport.h"
#include "scene/3d/camera_3d.h"
#include "servers/rendering/rendering_server.h"

#ifndef PHYSICS_3D_DISABLED
#include "scene/3d/physics/collision_shape_3d.h"
#include "scene/3d/physics/static_body_3d.h"
#include "scene/resources/3d/height_map_shape_3d.h"
#endif // PHYSICS_3D_DISABLED

#ifdef TOOLS_ENABLED
#include "editor/editor_interface.h"
#endif // TOOLS_ENABLED

using namespace EretheaTerrainClipmap;

static real_t _get_skirt_depth(real_t p_level_spacing) {
	return p_level_spacing * 2;
}

bool EretheaTerrain3D::_get_camera_position(Vector3 &r_position) const {
	if (!is_inside_tree()) {
		return false;
	}
	Camera3D *camera = get_viewport() ? get_viewport()->get_camera_3d() : nullptr;
#ifdef TOOLS_ENABLED
	if (!camera && Engine::get_singleton()->is_editor_hint() && EditorInterface::get_singleton()) {
		// The editor draws the scene with its own camera.
		SubViewport *editor_viewport = EditorInterface::get_singleton()->get_editor_viewport_3d(0);
		camera = editor_viewport ? editor_viewport->get_camera_3d() : nullptr;
	}
#endif // TOOLS_ENABLED
	if (!camera) {
		return false;
	}
	r_position = get_global_transform().affine_inverse().xform(camera->get_global_transform().origin);
	return true;
}

void EretheaTerrain3D::_create_levels() {
	_free_levels();
	if (data.is_null() || !is_inside_tree() || !get_world_3d().is_valid()) {
		return;
	}
	RenderingServer *rs = RenderingServer::get_singleton();
	const RID scenario = get_world_3d()->get_scenario();

	if (shader.is_null()) {
		shader.instantiate();
		shader->set_code(EretheaTerrainShader::get_code());
	}
	built_spacing = data->get_vertex_spacing();

	levels.resize(clipmap_levels);
	for (int i = 0; i < clipmap_levels; i++) {
		Level &level = levels[i];
		const LevelMesh level_mesh = build_level_mesh(mesh_size, get_level_spacing(built_spacing, i));

		Array arrays;
		arrays.resize(RSE::ARRAY_MAX);
		arrays[RSE::ARRAY_VERTEX] = level_mesh.vertices;
		arrays[RSE::ARRAY_TEX_UV] = level_mesh.uvs;
		arrays[RSE::ARRAY_INDEX] = level_mesh.indices;
		level.mesh = rs->mesh_create();
		rs->mesh_add_surface_from_arrays(level.mesh, RSE::PRIMITIVE_TRIANGLES, arrays);

		level.material.instantiate();
		level.material->set_shader(shader);

		level.instance = rs->instance_create2(level.mesh, scenario);
		rs->instance_geometry_set_material_override(level.instance, level.material->get_rid());
		rs->instance_set_visible(level.instance, is_visible_in_tree());
		level.center = Vector2();
		level.hole_center = Vector2();
	}
	_update_textures();
	_update_material_parameters();
	_update_transforms();
	_update_level_positions(true);
}

void EretheaTerrain3D::_free_levels() {
	RenderingServer *rs = RenderingServer::get_singleton();
	for (Level &level : levels) {
		if (level.instance.is_valid()) {
			rs->free_rid(level.instance);
		}
		if (level.mesh.is_valid()) {
			rs->free_rid(level.mesh);
		}
	}
	levels.clear();
	height_texture.unref();
	control_texture.unref();
}

void EretheaTerrain3D::_update_textures() {
	if (data.is_null() || levels.is_empty()) {
		return;
	}
	Ref<Image> heights = data->get_height_map();
	Ref<Image> control = data->get_control_map();
	if (height_texture.is_valid() && height_texture->get_size() == Size2(heights->get_size())) {
		height_texture->update(heights);
	} else {
		height_texture = ImageTexture::create_from_image(heights);
	}
	if (control_texture.is_valid() && control_texture->get_size() == Size2(control->get_size())) {
		control_texture->update(control);
	} else {
		control_texture = ImageTexture::create_from_image(control);
	}
}

void EretheaTerrain3D::_update_material_parameters() {
	if (data.is_null()) {
		return;
	}
	int layer_count = EretheaTerrainData::MAX_MATERIALS;
	if (albedo_height_textures.is_valid()) {
		layer_count = MIN(layer_count, albedo_height_textures->get_layers());
	}
	if (normal_roughness_textures.is_valid()) {
		layer_count = MIN(layer_count, normal_roughness_textures->get_layers());
	}
	layer_count = MAX(layer_count, 1);

	for (int i = 0; i < (int)levels.size(); i++) {
		const Ref<ShaderMaterial> &material = levels[i].material;
		const real_t level_spacing = get_level_spacing(built_spacing, i);
		material->set_shader_parameter("height_map", height_texture);
		material->set_shader_parameter("control_map", control_texture);
		material->set_shader_parameter("albedo_height_array", albedo_height_textures);
		material->set_shader_parameter("normal_roughness_array", normal_roughness_textures);
		material->set_shader_parameter("has_albedo_array", albedo_height_textures.is_valid());
		material->set_shader_parameter("has_normal_array", normal_roughness_textures.is_valid());
		material->set_shader_parameter("layer_count", layer_count);
		material->set_shader_parameter("map_size", data->get_resolution());
		material->set_shader_parameter("vertex_spacing", built_spacing);
		material->set_shader_parameter("level_spacing", level_spacing);
		material->set_shader_parameter("skirt_depth", _get_skirt_depth(level_spacing));
		material->set_shader_parameter("hole_half_extent", get_hole_half_extent(built_spacing, i, mesh_size));
		material->set_shader_parameter("uv_scale", uv_scale);
		material->set_shader_parameter("detiling", detiling);
		material->set_shader_parameter("triplanar", triplanar);
		material->set_shader_parameter("triplanar_slope_cos", Math::cos(Math::deg_to_rad(triplanar_slope)));
	}
}

void EretheaTerrain3D::_update_transforms() {
	RenderingServer *rs = RenderingServer::get_singleton();
	const Transform3D transform = get_global_transform();
	for (const Level &level : levels) {
		rs->instance_set_transform(level.instance, transform);
	}
}

void EretheaTerrain3D::_update_bounds() {
	if (data.is_null()) {
		return;
	}
	RenderingServer *rs = RenderingServer::get_singleton();
	const Vector2 height_range = data->get_height_range();
	for (int i = 0; i < (int)levels.size(); i++) {
		const real_t level_spacing = get_level_spacing(built_spacing, i);
		rs->instance_set_custom_aabb(levels[i].instance, get_level_bounds(levels[i].center, get_level_half_extent(built_spacing, i, mesh_size), height_range.x, height_range.y, _get_skirt_depth(level_spacing)));
	}
}

void EretheaTerrain3D::_update_level_positions(bool p_force) {
	if (levels.is_empty() || data.is_null()) {
		return;
	}
	Vector3 camera_local = last_camera_local;
	if (!_get_camera_position(camera_local) && !p_force) {
		return;
	}
	last_camera_local = camera_local;

	const Vector2 origin = data->get_vertex_position(Vector2i(0, 0));
	const Vector2 camera = Vector2(camera_local.x, camera_local.z);
	bool changed = p_force;
	LocalVector<Vector2> centers;
	centers.resize(levels.size());
	for (int i = 0; i < (int)levels.size(); i++) {
		centers[i] = get_level_center(camera, built_spacing, i, origin);
		if (centers[i] != levels[i].center) {
			changed = true;
		}
	}
	if (!changed) {
		return;
	}
	for (int i = 0; i < (int)levels.size(); i++) {
		Level &level = levels[i];
		level.center = centers[i];
		// The hole is where the next finer level is.
		level.hole_center = i > 0 ? centers[i - 1] : centers[i];
		level.material->set_shader_parameter("level_center", level.center);
		level.material->set_shader_parameter("hole_center", level.hole_center);
	}
	_update_bounds();
}

void EretheaTerrain3D::_update_collision() {
#ifndef PHYSICS_3D_DISABLED
	if (!body || data.is_null()) {
		return;
	}
	const Vector2i resolution = data->get_resolution();
	if (height_shape.is_null()) {
		height_shape.instantiate();
		collision_shape->set_shape(height_shape);
	}
	height_shape->set_map_width(resolution.x);
	height_shape->set_map_depth(resolution.y);
	height_shape->set_map_data(data->get_collision_heights());
	// The shape has one unit between its heights.
	collision_shape->set_scale(Vector3(data->get_vertex_spacing(), 1, data->get_vertex_spacing()));
#endif // PHYSICS_3D_DISABLED
}

void EretheaTerrain3D::_update_collision_properties() {
#ifndef PHYSICS_3D_DISABLED
	if (!body) {
		return;
	}
	body->set_collision_layer(collision_layer);
	body->set_collision_mask(collision_mask);
	collision_shape->set_disabled(!collision_enabled);
#endif // PHYSICS_3D_DISABLED
}

void EretheaTerrain3D::_on_data_changed() {
	if (data.is_valid() && !levels.is_empty() && data->get_vertex_spacing() != built_spacing) {
		// The grids of the levels depend on the spacing.
		_create_levels();
	} else {
		_update_textures();
		_update_material_parameters();
		_update_bounds();
	}
	_update_collision();
}

void EretheaTerrain3D::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			set_process_internal(true);
		} break;

		case NOTIFICATION_READY: {
#ifndef PHYSICS_3D_DISABLED
			body = memnew(StaticBody3D);
			body->set_name("EretheaTerrainBody");
			add_child(body, false, INTERNAL_MODE_FRONT);
			collision_shape = memnew(CollisionShape3D);
			collision_shape->set_name("EretheaTerrainShape");
			body->add_child(collision_shape, false, INTERNAL_MODE_FRONT);
			_update_collision_properties();
			_update_collision();
#endif // PHYSICS_3D_DISABLED
		} break;

		case NOTIFICATION_ENTER_WORLD: {
			_create_levels();
		} break;

		case NOTIFICATION_EXIT_WORLD: {
			_free_levels();
		} break;

		case NOTIFICATION_TRANSFORM_CHANGED: {
			_update_transforms();
			_update_level_positions(false);
		} break;

		case NOTIFICATION_VISIBILITY_CHANGED: {
			for (const Level &level : levels) {
				RenderingServer::get_singleton()->instance_set_visible(level.instance, is_visible_in_tree());
			}
		} break;

		case NOTIFICATION_INTERNAL_PROCESS: {
			_update_level_positions(false);
		} break;
	}
}

void EretheaTerrain3D::set_data(const Ref<EretheaTerrainData> &p_data) {
	if (data == p_data) {
		return;
	}
	if (data.is_valid()) {
		data->disconnect_changed(callable_mp(this, &EretheaTerrain3D::_on_data_changed));
	}
	data = p_data;
	if (data.is_valid()) {
		data->connect_changed(callable_mp(this, &EretheaTerrain3D::_on_data_changed));
	}
	if (is_inside_tree()) {
		_create_levels();
	}
	_update_collision();
}

Ref<EretheaTerrainData> EretheaTerrain3D::get_data() const {
	return data;
}

void EretheaTerrain3D::set_albedo_height_textures(const Ref<Texture2DArray> &p_textures) {
	ERR_FAIL_COND_MSG(p_textures.is_valid() && p_textures->get_layers() > MAX_LAYERS, vformat("The terrain has at most %d materials.", MAX_LAYERS));
	albedo_height_textures = p_textures;
	_update_material_parameters();
}

Ref<Texture2DArray> EretheaTerrain3D::get_albedo_height_textures() const {
	return albedo_height_textures;
}

void EretheaTerrain3D::set_normal_roughness_textures(const Ref<Texture2DArray> &p_textures) {
	ERR_FAIL_COND_MSG(p_textures.is_valid() && p_textures->get_layers() > MAX_LAYERS, vformat("The terrain has at most %d materials.", MAX_LAYERS));
	normal_roughness_textures = p_textures;
	_update_material_parameters();
}

Ref<Texture2DArray> EretheaTerrain3D::get_normal_roughness_textures() const {
	return normal_roughness_textures;
}

void EretheaTerrain3D::set_uv_scale(real_t p_scale) {
	uv_scale = p_scale;
	_update_material_parameters();
}

real_t EretheaTerrain3D::get_uv_scale() const {
	return uv_scale;
}

void EretheaTerrain3D::set_detiling(bool p_enabled) {
	detiling = p_enabled;
	_update_material_parameters();
}

bool EretheaTerrain3D::is_detiling() const {
	return detiling;
}

void EretheaTerrain3D::set_triplanar(bool p_enabled) {
	triplanar = p_enabled;
	_update_material_parameters();
}

bool EretheaTerrain3D::is_triplanar() const {
	return triplanar;
}

void EretheaTerrain3D::set_triplanar_slope(real_t p_degrees) {
	triplanar_slope = CLAMP(p_degrees, real_t(0), real_t(90));
	_update_material_parameters();
}

real_t EretheaTerrain3D::get_triplanar_slope() const {
	return triplanar_slope;
}

void EretheaTerrain3D::set_clipmap_levels(int p_levels) {
	clipmap_levels = CLAMP(p_levels, MIN_LEVELS, MAX_LEVELS);
	if (!levels.is_empty()) {
		_create_levels();
	}
}

int EretheaTerrain3D::get_clipmap_levels() const {
	return clipmap_levels;
}

void EretheaTerrain3D::set_mesh_size(int p_size) {
	// A multiple of 4, so that the holes line up with the grids.
	mesh_size = CLAMP(p_size / 4 * 4, MIN_MESH_SIZE, MAX_MESH_SIZE);
	if (!levels.is_empty()) {
		_create_levels();
	}
}

int EretheaTerrain3D::get_mesh_size() const {
	return mesh_size;
}

void EretheaTerrain3D::set_collision_enabled(bool p_enabled) {
	collision_enabled = p_enabled;
	_update_collision_properties();
}

bool EretheaTerrain3D::is_collision_enabled() const {
	return collision_enabled;
}

void EretheaTerrain3D::set_collision_layer(uint32_t p_layer) {
	collision_layer = p_layer;
	_update_collision_properties();
}

uint32_t EretheaTerrain3D::get_collision_layer() const {
	return collision_layer;
}

void EretheaTerrain3D::set_collision_mask(uint32_t p_mask) {
	collision_mask = p_mask;
	_update_collision_properties();
}

uint32_t EretheaTerrain3D::get_collision_mask() const {
	return collision_mask;
}

PackedVector2Array EretheaTerrain3D::get_level_centers() const {
	PackedVector2Array centers;
	for (const Level &level : levels) {
		centers.push_back(level.center);
	}
	return centers;
}

void EretheaTerrain3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_data", "data"), &EretheaTerrain3D::set_data);
	ClassDB::bind_method(D_METHOD("get_data"), &EretheaTerrain3D::get_data);

	ClassDB::bind_method(D_METHOD("set_albedo_height_textures", "textures"), &EretheaTerrain3D::set_albedo_height_textures);
	ClassDB::bind_method(D_METHOD("get_albedo_height_textures"), &EretheaTerrain3D::get_albedo_height_textures);
	ClassDB::bind_method(D_METHOD("set_normal_roughness_textures", "textures"), &EretheaTerrain3D::set_normal_roughness_textures);
	ClassDB::bind_method(D_METHOD("get_normal_roughness_textures"), &EretheaTerrain3D::get_normal_roughness_textures);
	ClassDB::bind_method(D_METHOD("set_uv_scale", "scale"), &EretheaTerrain3D::set_uv_scale);
	ClassDB::bind_method(D_METHOD("get_uv_scale"), &EretheaTerrain3D::get_uv_scale);
	ClassDB::bind_method(D_METHOD("set_detiling", "enabled"), &EretheaTerrain3D::set_detiling);
	ClassDB::bind_method(D_METHOD("is_detiling"), &EretheaTerrain3D::is_detiling);
	ClassDB::bind_method(D_METHOD("set_triplanar", "enabled"), &EretheaTerrain3D::set_triplanar);
	ClassDB::bind_method(D_METHOD("is_triplanar"), &EretheaTerrain3D::is_triplanar);
	ClassDB::bind_method(D_METHOD("set_triplanar_slope", "degrees"), &EretheaTerrain3D::set_triplanar_slope);
	ClassDB::bind_method(D_METHOD("get_triplanar_slope"), &EretheaTerrain3D::get_triplanar_slope);

	ClassDB::bind_method(D_METHOD("set_clipmap_levels", "levels"), &EretheaTerrain3D::set_clipmap_levels);
	ClassDB::bind_method(D_METHOD("get_clipmap_levels"), &EretheaTerrain3D::get_clipmap_levels);
	ClassDB::bind_method(D_METHOD("set_mesh_size", "size"), &EretheaTerrain3D::set_mesh_size);
	ClassDB::bind_method(D_METHOD("get_mesh_size"), &EretheaTerrain3D::get_mesh_size);

	ClassDB::bind_method(D_METHOD("set_collision_enabled", "enabled"), &EretheaTerrain3D::set_collision_enabled);
	ClassDB::bind_method(D_METHOD("is_collision_enabled"), &EretheaTerrain3D::is_collision_enabled);
	ClassDB::bind_method(D_METHOD("set_collision_layer", "layer"), &EretheaTerrain3D::set_collision_layer);
	ClassDB::bind_method(D_METHOD("get_collision_layer"), &EretheaTerrain3D::get_collision_layer);
	ClassDB::bind_method(D_METHOD("set_collision_mask", "mask"), &EretheaTerrain3D::set_collision_mask);
	ClassDB::bind_method(D_METHOD("get_collision_mask"), &EretheaTerrain3D::get_collision_mask);

	ClassDB::bind_method(D_METHOD("get_level_centers"), &EretheaTerrain3D::get_level_centers);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "data", PROPERTY_HINT_RESOURCE_TYPE, "EretheaTerrainData"), "set_data", "get_data");

	ADD_GROUP("Materials", "");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "albedo_height_textures", PROPERTY_HINT_RESOURCE_TYPE, "Texture2DArray"), "set_albedo_height_textures", "get_albedo_height_textures");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "normal_roughness_textures", PROPERTY_HINT_RESOURCE_TYPE, "Texture2DArray"), "set_normal_roughness_textures", "get_normal_roughness_textures");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "uv_scale", PROPERTY_HINT_RANGE, "0.001,10,0.001,or_greater,suffix:/m"), "set_uv_scale", "get_uv_scale");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "detiling"), "set_detiling", "is_detiling");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "triplanar"), "set_triplanar", "is_triplanar");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "triplanar_slope", PROPERTY_HINT_RANGE, "0,90,0.1,degrees"), "set_triplanar_slope", "get_triplanar_slope");

	ADD_GROUP("Clipmap", "");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "clipmap_levels", PROPERTY_HINT_RANGE, "1,16,1"), "set_clipmap_levels", "get_clipmap_levels");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "mesh_size", PROPERTY_HINT_RANGE, "8,256,4"), "set_mesh_size", "get_mesh_size");

	ADD_GROUP("Collision", "collision_");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "collision_enabled"), "set_collision_enabled", "is_collision_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "collision_layer", PROPERTY_HINT_LAYERS_3D_PHYSICS), "set_collision_layer", "get_collision_layer");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "collision_mask", PROPERTY_HINT_LAYERS_3D_PHYSICS), "set_collision_mask", "get_collision_mask");

	BIND_CONSTANT(MAX_LAYERS);
}

EretheaTerrain3D::EretheaTerrain3D() {
	set_notify_transform(true);
}

EretheaTerrain3D::~EretheaTerrain3D() {
	if (data.is_valid()) {
		data->disconnect_changed(callable_mp(this, &EretheaTerrain3D::_on_data_changed));
	}
}
