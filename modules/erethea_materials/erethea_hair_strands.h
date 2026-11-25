/**************************************************************************/
/*  erethea_hair_strands.h                                                */
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

#include "scene/3d/mesh_instance_3d.h"
#include "scene/resources/material.h"

class ArrayMesh;
class Shader;

// Hair drawn strand by strand. The strands come as a mesh of lines; the node
// turns them into ribbons that the shader opens towards the camera.
class EretheaHairStrands : public MeshInstance3D {
	GDCLASS(EretheaHairStrands, MeshInstance3D);

	Ref<Mesh> source_mesh;
	float width = 0.0015f;
	float tip_width = 0.25f;
	float strand_fraction = 1.0f;
	Color color_root = Color(0.075f, 0.045f, 0.03f);
	Color color_tip = Color(0.2f, 0.12f, 0.08f);
	float roughness = 0.45f;
	float specular_primary = 0.06f;
	float specular_secondary = 0.1f;
	float specular_shift_primary = -0.08f;
	float specular_shift_secondary = 0.12f;
	float backlight = 0.4f;

	Ref<ShaderMaterial> material;
	Ref<ArrayMesh> ribbons;
	int strand_count = 0;
	int source_strand_count = 0;
	bool rebuild_queued = false;

	static Ref<Shader> shader;

	void _queue_rebuild();
	void _rebuild();
	void _update_material();

protected:
	static void _bind_methods();
	void _validate_property(PropertyInfo &p_property) const;

public:
	static String get_shader_code();
	static void finish();

	void set_source_mesh(const Ref<Mesh> &p_mesh);
	Ref<Mesh> get_source_mesh() const;
	void set_width(float p_width);
	float get_width() const;
	void set_tip_width(float p_tip_width);
	float get_tip_width() const;
	void set_strand_fraction(float p_fraction);
	float get_strand_fraction() const;
	void set_color_root(const Color &p_color);
	Color get_color_root() const;
	void set_color_tip(const Color &p_color);
	Color get_color_tip() const;
	void set_roughness(float p_roughness);
	float get_roughness() const;
	void set_specular_primary(float p_value);
	float get_specular_primary() const;
	void set_specular_secondary(float p_value);
	float get_specular_secondary() const;
	void set_specular_shift_primary(float p_value);
	float get_specular_shift_primary() const;
	void set_specular_shift_secondary(float p_value);
	float get_specular_shift_secondary() const;
	void set_backlight(float p_value);
	float get_backlight() const;

	int get_strand_count() const;
	int get_source_strand_count() const;
	void rebuild();

	EretheaHairStrands();
};
