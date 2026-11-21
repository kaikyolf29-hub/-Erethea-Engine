/**************************************************************************/
/*  test_erethea_export_checks.h                                          */
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

#include "../erethea_export_checks.h"

#include "core/io/image.h"
#include "scene/3d/mesh_instance_3d.h"
#include "scene/resources/3d/primitive_meshes.h"
#include "scene/resources/image_texture.h"
#include "scene/resources/material.h"
#include "tests/test_macros.h"

namespace TestEretheaExportChecks {

static MeshInstance3D *_box(const Vector3 &p_size, const Vector3 &p_position = Vector3()) {
	Ref<BoxMesh> mesh;
	mesh.instantiate();
	mesh->set_size(p_size);
	MeshInstance3D *instance = memnew(MeshInstance3D);
	instance->set_mesh(mesh);
	instance->set_position(p_position);
	return instance;
}

static Ref<ImageTexture> _texture(int p_width, int p_height) {
	return ImageTexture::create_from_image(Image::create_empty(p_width, p_height, false, Image::FORMAT_RGBA8));
}

static int _count(const Vector<EretheaExportFinding> &p_findings, const String &p_check) {
	int total = 0;
	for (const EretheaExportFinding &finding : p_findings) {
		total += finding.check == p_check;
	}
	return total;
}

TEST_CASE("[Modules][EretheaExportChecks] Triangles and size of a model") {
	Node3D *root = memnew(Node3D);
	root->add_child(_box(Vector3(1, 2, 1)));
	MeshInstance3D *moved = _box(Vector3(1, 1, 1), Vector3(5, 0, 0));
	root->add_child(moved);
	// A box is 12 triangles.
	CHECK(EretheaExportChecks::count_triangles(root) == 24);

	const AABB bounds = EretheaExportChecks::get_model_bounds(root);
	CHECK(bounds.position.is_equal_approx(Vector3(-0.5, -1, -0.5)));
	CHECK(bounds.size.is_equal_approx(Vector3(6, 2, 1)));

	// Transforms of the parents count, but not the root's own.
	root->set_scale(Vector3(10, 10, 10));
	moved->set_scale(Vector3(1, 4, 1));
	CHECK(EretheaExportChecks::get_model_bounds(root).size.is_equal_approx(Vector3(6, 4, 1)));
	memdelete(root);
}

TEST_CASE("[Modules][EretheaExportChecks] Characters") {
	EretheaExportCheckSettings settings;
	CHECK(EretheaExportChecks::is_character("res://Characters/hero/hero.glb", settings));
	CHECK(EretheaExportChecks::is_character("res://Enemies/wolf/wolf.glb", settings));
	CHECK_FALSE(EretheaExportChecks::is_character("res://World/blockout.glb", settings));
	CHECK_FALSE(EretheaExportChecks::is_character("res://CharactersOld/hero.glb", settings));
	settings.character_folders = { "res://npcs/" };
	CHECK(EretheaExportChecks::is_character("res://npcs/guard.glb", settings));
	CHECK_FALSE(EretheaExportChecks::is_character("res://Characters/hero/hero.glb", settings));
}

TEST_CASE("[Modules][EretheaExportChecks] Models") {
	EretheaExportCheckSettings settings;
	Vector<EretheaExportFinding> findings;

	MeshInstance3D *hero = _box(Vector3(0.5, 1.8, 0.3));
	EretheaExportChecks::check_model(hero, "res://Characters/hero/hero.glb", settings, findings);
	CHECK(findings.is_empty());

	// Exported in centimeters.
	MeshInstance3D *giant = _box(Vector3(50, 180, 30));
	EretheaExportChecks::check_model(giant, "res://Characters/giant/giant.glb", settings, findings);
	REQUIRE(findings.size() == 1);
	CHECK(findings[0].check == "scale");
	CHECK(findings[0].level == EretheaExportFinding::LEVEL_WARNING);
	CHECK(findings[0].file == "res://Characters/giant/giant.glb");
	CHECK(findings[0].message.contains("180 m tall"));

	// The same box is a fine building.
	findings.clear();
	EretheaExportChecks::check_model(giant, "res://World/tower.glb", settings, findings);
	CHECK(findings.is_empty());

	// Exported in kilometers.
	MeshInstance3D *tiny = _box(Vector3(0.001, 0.002, 0.001));
	EretheaExportChecks::check_model(tiny, "res://World/house.glb", settings, findings);
	REQUIRE(findings.size() == 1);
	CHECK(findings[0].check == "scale");

	findings.clear();
	settings.max_character_triangles = 10;
	EretheaExportChecks::check_model(hero, "res://Characters/hero/hero.glb", settings, findings);
	REQUIRE(findings.size() == 1);
	CHECK(findings[0].check == "polygons");
	CHECK(findings[0].message.begins_with("12 triangles"));
	findings.clear();
	EretheaExportChecks::check_model(hero, "res://Items/sword.glb", settings, findings);
	CHECK(findings.is_empty());

	memdelete(hero);
	memdelete(giant);
	memdelete(tiny);
}

TEST_CASE("[Modules][EretheaExportChecks] Textures") {
	EretheaExportCheckSettings settings;
	settings.max_texture_size = 32;
	Vector<EretheaExportFinding> findings;

	EretheaExportChecks::check_texture(_texture(32, 32), "res://UI/icon.png", String(), settings, findings);
	CHECK(findings.is_empty());
	EretheaExportChecks::check_texture(_texture(64, 16), "res://UI/logo.png", String(), settings, findings);
	REQUIRE(findings.size() == 1);
	CHECK(findings[0].check == "texture_size");
	CHECK(findings[0].message.contains("64x16"));

	// A texture inside the model file is reported on the model.
	findings.clear();
	MeshInstance3D *hero = _box(Vector3(0.5, 1.8, 0.3));
	Ref<StandardMaterial3D> material;
	material.instantiate();
	Ref<ImageTexture> skin = _texture(16, 128);
	skin->set_name("Skin");
	material->set_texture(BaseMaterial3D::TEXTURE_ALBEDO, skin);
	// The same texture twice is reported once.
	material->set_texture(BaseMaterial3D::TEXTURE_EMISSION, skin);
	hero->set_material_override(material);
	EretheaExportChecks::check_model(hero, "res://Characters/hero/hero.glb", settings, findings);
	REQUIRE(findings.size() == 1);
	CHECK(findings[0].check == "texture_size");
	CHECK(findings[0].file == "res://Characters/hero/hero.glb");
	CHECK(findings[0].message.contains("\"Skin\""));
	memdelete(hero);
}

TEST_CASE("[Modules][EretheaExportChecks] License receipts") {
	CHECK(EretheaExportChecks::needs_receipt("res://Characters/hero/hero.glb"));
	CHECK(EretheaExportChecks::needs_receipt("res://UI/menu.PNG"));
	CHECK(EretheaExportChecks::needs_receipt("res://Audio/musica/vila.wav"));
	CHECK_FALSE(EretheaExportChecks::needs_receipt("res://Scripts/fornalha/jogador.gd"));
	CHECK_FALSE(EretheaExportChecks::needs_receipt("res://World/mundo.tscn"));
	CHECK_FALSE(EretheaExportChecks::needs_receipt("res://icon.png"));
	CHECK_FALSE(EretheaExportChecks::needs_receipt("res://blender/hero.glb"));

	const String jsonl = String(R"({"arquivo": "Characters/hero/hero.glb", "sha256": "AAA", "modelo": {"id": "trellis", "nome": "TRELLIS", "licenca": "MIT", "uso_comercial": "sim"}})") + "\n" +
			"not json\n" +
			R"({"arquivo": "Characters/wolf/wolf.glb", "sha256": "bbb", "modelo": {"id": "x", "nome": "Model X", "licenca": "CC BY-NC", "uso_comercial": "sim"}})" + "\n" +
			// Newer receipt for the same file.
			R"({"arquivo": "Characters\\wolf\\wolf.glb", "sha256": "ccc", "modelo": {"id": "x", "nome": "Model X", "licenca": "CC BY-NC", "uso_comercial": "nao"}})" + "\n" +
			R"({"arquivo": "Audio/vila.wav", "sha256": "ddd", "modelo": {"id": "y", "nome": "Model Y", "licenca": "Custom", "uso_comercial": "condicional"}})" + "\n";
	const HashMap<String, EretheaLicenseReceipt> receipts = EretheaExportChecks::parse_receipts(jsonl);
	REQUIRE(receipts.size() == 3);
	REQUIRE(receipts.has("res://Characters/hero/hero.glb"));
	CHECK(receipts["res://Characters/hero/hero.glb"].sha256 == "aaa");
	CHECK(receipts["res://Characters/hero/hero.glb"].model_name == "TRELLIS");
	REQUIRE(receipts.has("res://Characters/wolf/wolf.glb"));
	CHECK(receipts["res://Characters/wolf/wolf.glb"].commercial_use == "nao");

	// Textures Godot extracted from a model share its receipt.
	CHECK(EretheaExportChecks::is_derived_from_receipted_model("res://Characters/hero/hero_Albedo.png", receipts));
	CHECK_FALSE(EretheaExportChecks::is_derived_from_receipted_model("res://Characters/hero/face.png", receipts));
	CHECK_FALSE(EretheaExportChecks::is_derived_from_receipted_model("res://Characters/hero_Albedo.png", receipts));

	Vector<EretheaExportFinding> findings;
	EretheaExportChecks::check_license("res://Characters/hero/hero.glb", "aaa", true, receipts, findings);
	EretheaExportChecks::check_license("res://Characters/hero/hero_Albedo.png", "123", true, receipts, findings);
	CHECK(findings.is_empty());

	// Changed after its receipt.
	EretheaExportChecks::check_license("res://Characters/hero/hero.glb", "abc", true, receipts, findings);
	REQUIRE(findings.size() == 1);
	CHECK(findings[0].level == EretheaExportFinding::LEVEL_WARNING);
	CHECK(findings[0].message.contains("Changed after"));

	findings.clear();
	EretheaExportChecks::check_license("res://Characters/wolf/wolf.glb", "ccc", true, receipts, findings);
	REQUIRE(findings.size() == 1);
	CHECK(findings[0].level == EretheaExportFinding::LEVEL_ERROR);
	CHECK(findings[0].message.contains("Model X"));
	findings.clear();
	EretheaExportChecks::check_license("res://Characters/wolf/wolf.glb", "ccc", false, receipts, findings);
	CHECK(findings.is_empty());

	EretheaExportChecks::check_license("res://Audio/vila.wav", "ddd", true, receipts, findings);
	REQUIRE(findings.size() == 1);
	CHECK(findings[0].level == EretheaExportFinding::LEVEL_WARNING);

	findings.clear();
	EretheaExportChecks::check_license("res://UI/menu.png", "eee", true, receipts, findings);
	REQUIRE(findings.size() == 1);
	CHECK(findings[0].level == EretheaExportFinding::LEVEL_ERROR);
	CHECK(_count(findings, "license") == 1);
	findings.clear();
	EretheaExportChecks::check_license("res://UI/menu.png", "eee", false, receipts, findings);
	REQUIRE(findings.size() == 1);
	CHECK(findings[0].level == EretheaExportFinding::LEVEL_WARNING);
}

} // namespace TestEretheaExportChecks
