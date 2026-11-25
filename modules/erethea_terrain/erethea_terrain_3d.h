/**************************************************************************/
/*  erethea_terrain_3d.h                                                  */
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

#include "erethea_terrain_data.h"

#include "core/templates/local_vector.h"
#include "scene/3d/node_3d.h"
#include "scene/resources/image_texture.h"
#include "scene/resources/material.h"

#ifndef PHYSICS_3D_DISABLED
class CollisionShape3D;
class HeightMapShape3D;
class StaticBody3D;
#endif // PHYSICS_3D_DISABLED

// A terrain drawn from a height map, by a clipmap: nested grids of vertices that
// follow the camera, finer near it. See erethea_terrain_clipmap.h.
class EretheaTerrain3D : public Node3D {
	GDCLASS(EretheaTerrain3D, Node3D);

public:
	static constexpr int MAX_LAYERS = EretheaTerrainData::MAX_MATERIALS;

private:
	struct Level {
		RID mesh;
		RID instance;
		Ref<ShaderMaterial> material;
		Vector2 center;
		Vector2 hole_center;
	};

	Ref<EretheaTerrainData> data;
	Ref<Texture2DArray> albedo_height_textures;
	Ref<Texture2DArray> normal_roughness_textures;
	real_t uv_scale = 0.1;
	bool detiling = false;
	bool triplanar = false;
	real_t triplanar_slope = 45.0;
	int clipmap_levels = 8;
	int mesh_size = 64;
	bool collision_enabled = true;
	uint32_t collision_layer = 1;
	uint32_t collision_mask = 1;

	// What is drawn: made when the node is in a world, freed when it leaves.
	LocalVector<Level> levels;
	Ref<Shader> shader;
	Ref<ImageTexture> height_texture;
	Ref<ImageTexture> control_texture;
	real_t built_spacing = 0.0;
	Vector3 last_camera_local;

#ifndef PHYSICS_3D_DISABLED
	StaticBody3D *body = nullptr;
	CollisionShape3D *collision_shape = nullptr;
	Ref<HeightMapShape3D> height_shape;
#endif // PHYSICS_3D_DISABLED

	void _create_levels();
	void _free_levels();
	void _update_textures();
	void _update_material_parameters();
	void _update_level_positions(bool p_force);
	void _update_transforms();
	void _update_bounds();
	void _update_collision();
	void _update_collision_properties();
	void _on_data_changed();
	// Where the camera is, in the terrain's space. False if there is none.
	bool _get_camera_position(Vector3 &r_position) const;

protected:
	void _notification(int p_what);
	static void _bind_methods();

public:
	void set_data(const Ref<EretheaTerrainData> &p_data);
	Ref<EretheaTerrainData> get_data() const;

	void set_albedo_height_textures(const Ref<Texture2DArray> &p_textures);
	Ref<Texture2DArray> get_albedo_height_textures() const;
	void set_normal_roughness_textures(const Ref<Texture2DArray> &p_textures);
	Ref<Texture2DArray> get_normal_roughness_textures() const;
	void set_uv_scale(real_t p_scale);
	real_t get_uv_scale() const;
	void set_detiling(bool p_enabled);
	bool is_detiling() const;
	void set_triplanar(bool p_enabled);
	bool is_triplanar() const;
	void set_triplanar_slope(real_t p_degrees);
	real_t get_triplanar_slope() const;

	void set_clipmap_levels(int p_levels);
	int get_clipmap_levels() const;
	void set_mesh_size(int p_size);
	int get_mesh_size() const;

	void set_collision_enabled(bool p_enabled);
	bool is_collision_enabled() const;
	void set_collision_layer(uint32_t p_layer);
	uint32_t get_collision_layer() const;
	void set_collision_mask(uint32_t p_mask);
	uint32_t get_collision_mask() const;

	// The center of each level of the clipmap, in the terrain's space. For tests.
	PackedVector2Array get_level_centers() const;

	EretheaTerrain3D();
	~EretheaTerrain3D();
};
