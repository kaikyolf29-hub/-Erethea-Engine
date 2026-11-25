/**************************************************************************/
/*  test_erethea_fornalha.h                                               */
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

#include "../editor/erethea_fornalha_link.h"
#include "../editor/erethea_fornalha_plugin.h"
#include "../erethea_fornalha_import.h"
#include "../erethea_fornalha_terrain.h"

#include "core/config/project_settings.h"
#include "core/io/config_file.h"
#include "editor/editor_node.h"
#include "scene/3d/camera_3d.h"
#include "scene/3d/light_3d.h"
#include "scene/3d/mesh_instance_3d.h"
#include "scene/3d/physics/character_body_3d.h"
#include "scene/3d/physics/collision_shape_3d.h"
#include "scene/3d/physics/spring_arm_3d.h"
#include "scene/animation/animation_player.h"
#include "scene/resources/3d/capsule_shape_3d.h"
#include "scene/resources/3d/primitive_meshes.h"
#include "scene/resources/image_texture.h"
#include "scene/resources/material.h"
#include "scene/resources/mesh.h"
#include "scene/resources/packed_scene.h"
#include "tests/test_macros.h"

#include "modules/erethea_terrain/erethea_terrain_data.h"

namespace TestEretheaFornalha {

TEST_CASE("[Modules][EretheaFornalha] Keeping scenes changed by hand") {
	using Import = EretheaFornalhaImport;
	CHECK(Import::decide(false, "", "", "new", false) == Import::ACTION_CREATED);
	CHECK(Import::decide(true, "same", "old", "same", false) == Import::ACTION_UNCHANGED);
	CHECK(Import::decide(true, "same", "", "same", false) == Import::ACTION_UNCHANGED);
	// Built here and not touched since: built again.
	CHECK(Import::decide(true, "built", "built", "new", false) == Import::ACTION_UPDATED);
	// Changed since it was built, or never built here: kept, unless forced.
	CHECK(Import::decide(true, "edited", "built", "new", false) == Import::ACTION_KEPT_EDITED);
	CHECK(Import::decide(true, "montar", "", "new", false) == Import::ACTION_KEPT_FOREIGN);
	CHECK(Import::decide(true, "edited", "built", "new", true) == Import::ACTION_REPLACED);
	CHECK(Import::decide(true, "montar", "", "new", true) == Import::ACTION_REPLACED);
}

TEST_CASE("[Modules][EretheaFornalha] Comparing two saves of a scene") {
	const String first = R"([gd_scene load_steps=3 format=3 uid="uid://b1"]

[ext_resource type="Script" uid="uid://s" path="res://Scripts/fornalha/jogador.gd" id="1_ab12c"]
[ext_resource type="PackedScene" uid="uid://m" path="res://Characters/hero/hero.glb" id="2_xyz98"]

[sub_resource type="CapsuleShape3D" id="CapsuleShape3D_q1w2e"]
height = 1.8

[node name="hero" type="CharacterBody3D"]
script = ExtResource("1_ab12c")

[node name="Modelo" parent="." instance=ExtResource("2_xyz98")]

[node name="Colisao" type="CollisionShape3D" parent="."]
shape = SubResource("CapsuleShape3D_q1w2e")
)";
	// Same scene saved again: another uid, other random ids.
	const String second = R"([gd_scene load_steps=3 format=3 uid="uid://c9"]

[ext_resource type="Script" uid="uid://s" path="res://Scripts/fornalha/jogador.gd" id="1_zz00z"]
[ext_resource type="PackedScene" uid="uid://m" path="res://Characters/hero/hero.glb" id="2_kk11k"]

[sub_resource type="CapsuleShape3D" id="CapsuleShape3D_pp22p"]
height = 1.8

[node name="hero" type="CharacterBody3D"]
script = ExtResource("1_zz00z")

[node name="Modelo" parent="." instance=ExtResource("2_kk11k")]

[node name="Colisao" type="CollisionShape3D" parent="."]
shape = SubResource("CapsuleShape3D_pp22p")
)";
	CHECK(EretheaFornalhaImport::hash_scene_text(first) == EretheaFornalhaImport::hash_scene_text(second));
	CHECK(EretheaFornalhaImport::normalize_scene_text(first).contains("script = ExtResource(\"e1\")"));
	CHECK(EretheaFornalhaImport::normalize_scene_text(first).contains("shape = SubResource(\"s1\")"));
	CHECK_FALSE(EretheaFornalhaImport::normalize_scene_text(first).contains("uid://b1"));
	// A real change shows.
	CHECK(EretheaFornalhaImport::hash_scene_text(first) != EretheaFornalhaImport::hash_scene_text(second.replace("height = 1.8", "height = 2.0")));
}

TEST_CASE("[Modules][EretheaFornalha] Placing enemies in the areas") {
	EretheaFornalhaProject project;
	EretheaFornalhaCharacter hero;
	hero.id = "hero";
	hero.player = true;
	EretheaFornalhaCharacter wolf;
	wolf.id = "wolf";
	EretheaFornalhaCharacter orc;
	orc.id = "orc";
	project.characters.push_back(hero);
	project.characters.push_back(wolf);
	project.characters.push_back(orc);
	EretheaFornalhaArea village;
	village.id = "vila";
	EretheaFornalhaArea forest;
	forest.id = "floresta";
	forest.center = Vector3(0, 0, -46);
	project.areas.push_back(village);
	project.areas.push_back(forest);
	project.player_spawn = Vector3(0, 0.2, 5);
	project.enemy_spawns.push_back(Vector3(3, 0.1, -44));
	project.enemy_spawns.push_back(Vector3(-2, 0.1, -48));
	project.enemy_spawns.push_back(Vector3(4, 0.1, 2));

	CHECK(project.get_enemy_ids() == Vector<String>({ "wolf", "orc" }));
	CHECK(project.find_area(Vector3(1, 50, -40)) == 1); // Height doesn't count.
	Vector<EretheaFornalhaEnemy> enemies = project.place_enemies();
	REQUIRE(enemies.size() == 3);
	// The kinds of enemy take turns at the spawn points.
	CHECK(enemies[0].id == "wolf");
	CHECK(enemies[1].id == "orc");
	CHECK(enemies[2].id == "wolf");
	CHECK(enemies[0].area == 1);
	CHECK(enemies[0].position.is_equal_approx(Vector3(3, 0.1, 2))); // Relative to the forest.
	CHECK(enemies[2].area == 0);

	project.enemy_near = true;
	enemies = project.place_enemies();
	REQUIRE(enemies.size() == 4);
	CHECK(enemies[0].position.is_equal_approx(Vector3(0, 0.2, -1))); // In front of the player.
	CHECK(enemies[0].area == 0);

	project.areas.clear();
	enemies = project.place_enemies();
	CHECK(enemies[1].area == -1);
	CHECK(enemies[1].position.is_equal_approx(Vector3(3, 0.1, -44)));

	project.characters.remove_at(2);
	project.characters.remove_at(1);
	CHECK(project.place_enemies().is_empty());
}

static Ref<PackedScene> _model(const String &p_path) {
	Node3D *root = memnew(Node3D);
	root->set_name("Model");
	Ref<PackedScene> scene;
	scene.instantiate();
	scene->pack(root);
	memdelete(root);
	scene->set_path(p_path);
	return scene;
}

TEST_CASE("[SceneTree][Modules][EretheaFornalha] The sun of the world") {
	EretheaFornalhaProject project;
	// No areas yet: Godot's distance.
	CHECK(EretheaFornalhaImport::get_sun_shadow_distance(project) == doctest::Approx(100.0));
	EretheaFornalhaArea village;
	village.id = "vila";
	village.center = Vector3(40, 0, 30);
	project.areas.push_back(village);
	EretheaFornalhaArea forest;
	forest.id = "floresta";
	forest.center = Vector3(-60, 0, 0);
	project.areas.push_back(forest);
	// Each area reaches half way to the other (52.2 m): the forest's edge is
	// 112.2 m from the center of the world, so the world is 224.4 m across.
	CHECK(EretheaFornalhaImport::get_sun_shadow_distance(project) == doctest::Approx(224.4).epsilon(0.01));
	EretheaFornalhaArea mountain;
	mountain.id = "montanha";
	mountain.center = Vector3(0, 0, 400);
	project.areas.push_back(mountain);
	CHECK(EretheaFornalhaImport::get_sun_shadow_distance(project) == doctest::Approx(300.0));

	Vector<String> warnings;
	Node *world = EretheaFornalhaImport::make_world(project, warnings);
	DirectionalLight3D *sun = Object::cast_to<DirectionalLight3D>(world->get_node_or_null(NodePath("Sol")));
	REQUIRE(sun != nullptr);
	CHECK(sun->has_shadow());
	CHECK(sun->is_blend_splits_enabled());
	CHECK(sun->get_param(Light3D::PARAM_SHADOW_MAX_DISTANCE) == doctest::Approx(300.0));
	memdelete(world);
}

TEST_CASE("[SceneTree][Modules][EretheaFornalha] Character scenes") {
	const Ref<PackedScene> model = _model("res://Characters/hero/hero.glb");
	EretheaFornalhaCharacter hero;
	hero.id = "hero";
	hero.player = true;
	hero.model = model->get_path();
	hero.height = 1.6;

	Vector<String> warnings;
	Node *root = EretheaFornalhaImport::make_character(hero, warnings);
	REQUIRE(root != nullptr);
	CharacterBody3D *body = Object::cast_to<CharacterBody3D>(root);
	REQUIRE(body != nullptr);
	CHECK(body->get_name() == StringName("hero"));
	CHECK(body->get_collision_layer() == EretheaFornalhaImport::LAYER_PLAYER);
	CHECK(body->get_collision_mask() == (EretheaFornalhaImport::LAYER_WORLD | EretheaFornalhaImport::LAYER_ENEMIES));
	// The project has no Fornalha scripts here.
	CHECK(warnings.size() == 1);
	CHECK(warnings[0].contains("jogador.gd"));

	Node *model_node = root->get_node_or_null(NodePath("Modelo"));
	REQUIRE(model_node != nullptr);
	CHECK(model_node->get_scene_file_path() == model->get_path());
	CHECK(model_node->get_owner() == root);
	CollisionShape3D *collision = Object::cast_to<CollisionShape3D>(root->get_node_or_null(NodePath("Colisao")));
	REQUIRE(collision != nullptr);
	const Ref<CapsuleShape3D> capsule = collision->get_shape();
	REQUIRE(capsule.is_valid());
	CHECK(capsule->get_height() == doctest::Approx(1.6));
	CHECK(collision->get_position().y == doctest::Approx(0.8));
	SpringArm3D *arm = Object::cast_to<SpringArm3D>(root->get_node_or_null(NodePath("BracoCamera")));
	REQUIRE(arm != nullptr);
	CHECK(Object::cast_to<Camera3D>(arm->get_node_or_null(NodePath("Camera"))) != nullptr);

	// The model stays linked to its file, so a new export of it shows up in the scene.
	Ref<PackedScene> packed;
	packed.instantiate();
	CHECK(packed->pack(root) == OK);
	const Ref<SceneState> state = packed->get_state();
	bool linked = false;
	for (int i = 0; i < state->get_node_count(); i++) {
		if (state->get_node_name(i) == StringName("Modelo")) {
			const Ref<PackedScene> instance = state->get_node_instance(i);
			linked = instance.is_valid() && instance->get_path() == model->get_path();
		}
	}
	CHECK(linked);
	memdelete(root);

	EretheaFornalhaCharacter wolf;
	wolf.id = "wolf";
	wolf.model = model->get_path();
	wolf.height = 0.3; // A small one: the capsule still fits.
	warnings.clear();
	root = EretheaFornalhaImport::make_character(wolf, warnings);
	REQUIRE(root != nullptr);
	CHECK(Object::cast_to<CollisionObject3D>(root)->get_collision_layer() == EretheaFornalhaImport::LAYER_ENEMIES);
	CHECK(root->get_node_or_null(NodePath("BracoCamera")) == nullptr);
	const Ref<CapsuleShape3D> small = Object::cast_to<CollisionShape3D>(root->get_node(NodePath("Colisao")))->get_shape();
	CHECK(small->get_radius() * 2 <= small->get_height() + CMP_EPSILON);
	memdelete(root);

	// Without its model there is no character.
	wolf.model = "res://Enemies/missing/missing.glb";
	warnings.clear();
	ERR_PRINT_OFF;
	CHECK(EretheaFornalhaImport::make_character(wolf, warnings) == nullptr);
	ERR_PRINT_ON;
	CHECK(warnings[warnings.size() - 1].contains("missing.glb"));
}

static Ref<Animation> _animation() {
	Ref<Animation> animation;
	animation.instantiate();
	animation->set_length(1.0);
	return animation;
}

TEST_CASE("[Modules][EretheaFornalha] Walk cycles loop") {
	Node3D *scene = memnew(Node3D);
	AnimationPlayer *player = memnew(AnimationPlayer);
	scene->add_child(player);
	Ref<AnimationLibrary> library;
	library.instantiate();
	library->add_animation("andar", _animation());
	library->add_animation("atacar", _animation());
	library->add_animation("correr", _animation());
	player->add_animation_library("", library);

	PackedStringArray names;
	names.push_back("andar");
	names.push_back("correr");
	Dictionary settings;
	Dictionary run;
	run["settings/loop_mode"] = 0; // Set in the import settings: that wins.
	settings["correr"] = run;

	CHECK(EretheaFornalhaAnimationLoops::loop_animations(scene, names, settings) == 1);
	CHECK(library->get_animation("andar")->get_loop_mode() == Animation::LOOP_LINEAR);
	CHECK(library->get_animation("atacar")->get_loop_mode() == Animation::LOOP_NONE);
	CHECK(library->get_animation("correr")->get_loop_mode() == Animation::LOOP_NONE);
	memdelete(scene);
}

TEST_CASE("[Modules][EretheaFornalha] Building scenes by themselves") {
	using Import = EretheaFornalhaImport;
	// Never built: only a project with nothing assembled yet.
	CHECK(Import::should_build_automatically(false, "", "a", false));
	CHECK_FALSE(Import::should_build_automatically(false, "", "a", true));
	// Built before: when what the scenes come from changed.
	CHECK(Import::should_build_automatically(true, "a", "b", true));
	CHECK(Import::should_build_automatically(true, "", "b", true));
	CHECK_FALSE(Import::should_build_automatically(true, "a", "a", true));
	CHECK_FALSE(Import::should_build_automatically(true, "a", "a", false));

	EretheaFornalhaProject project;
	EretheaFornalhaCharacter hero;
	hero.id = "heroi";
	hero.player = true;
	hero.model = "res://Characters/heroi/heroi.glb";
	project.characters.push_back(hero);
	EretheaFornalhaArea village;
	village.id = "vila";
	village.name = "Vila";
	project.areas.push_back(village);
	project.player = "heroi";
	const String signature = project.get_signature();
	CHECK(signature.length() == 32);
	CHECK(project.get_signature() == signature);

	EretheaFornalhaProject with_enemy = project;
	EretheaFornalhaCharacter wolf;
	wolf.id = "lobo";
	wolf.model = "res://Enemies/lobo/lobo.glb";
	with_enemy.characters.push_back(wolf);
	CHECK(with_enemy.get_signature() != signature);

	EretheaFornalhaProject renamed = project;
	renamed.areas.write[0].name = "Aldeia";
	CHECK(renamed.get_signature() != signature);

	EretheaFornalhaProject taller = project;
	taller.characters.write[0].height = 2.1;
	CHECK(taller.get_signature() != signature);

	// Warnings say nothing about what is built.
	EretheaFornalhaProject warned = project;
	warned.warnings.push_back("No blockout in World/ yet.");
	CHECK(warned.get_signature() == signature);
}

TEST_CASE("[Modules][EretheaFornalha] Erethea's settings in projects from the Fornalha") {
	using Import = EretheaFornalhaImport;
	ProjectSettings *project_settings = ProjectSettings::get_singleton();
	const Variant applied_before = GLOBAL_GET(Import::SETTING_ERETHEA_DEFAULTS_APPLIED);
	const String chosen = "erethea_tests/fornalha_defaults/chosen";
	const String untouched = "erethea_tests/fornalha_defaults/untouched";
	const String later = "erethea_tests/fornalha_defaults/later";
	GLOBAL_DEF(chosen, 1);
	GLOBAL_DEF(untouched, 1);
	GLOBAL_DEF(later, 1);
	// The project chose this one on purpose.
	project_settings->set_setting(chosen, 2);

	HashMap<String, Variant> defaults;
	defaults[chosen] = 3;
	defaults[untouched] = 3;
	Vector<String> changed = Import::apply_erethea_defaults(defaults);
	CHECK(changed.size() == 1);
	CHECK(changed.has(untouched));
	CHECK(int(GLOBAL_GET(chosen)) == 2);
	CHECK(int(GLOBAL_GET(untouched)) == 3);
	const PackedStringArray applied = GLOBAL_GET(Import::SETTING_ERETHEA_DEFAULTS_APPLIED);
	CHECK(applied.has(chosen));
	CHECK(applied.has(untouched));

	// Turned back off by hand, it stays off the next time the project opens.
	project_settings->set_setting(untouched, 1);
	CHECK(Import::apply_erethea_defaults(defaults).is_empty());
	CHECK(int(GLOBAL_GET(untouched)) == 1);

	// A setting that a later Erethea starts new projects with still arrives.
	defaults[later] = 3;
	changed = Import::apply_erethea_defaults(defaults);
	CHECK(changed.size() == 1);
	CHECK(changed.has(later));
	CHECK(int(GLOBAL_GET(later)) == 3);

	// Erethea's own list names settings that exist.
	const HashMap<String, Variant> erethea_defaults = EditorNode::erethea_get_initial_settings();
	CHECK(int(erethea_defaults["rendering/erethea/quality_preset"]) == 4);
	for (const KeyValue<String, Variant> &E : erethea_defaults) {
		CHECK_MESSAGE(project_settings->has_setting(E.key), E.key);
		CHECK_MESSAGE(EditorNode::get_initial_settings().has(E.key), E.key);
	}

	project_settings->set_setting(Import::SETTING_ERETHEA_DEFAULTS_APPLIED, applied_before);
	project_settings->clear(chosen);
	project_settings->clear(untouched);
	project_settings->clear(later);
}

TEST_CASE("[Modules][EretheaFornalha] Messages from the Fornalha") {
	int64_t version = -1;
	PackedStringArray files;
	String websocket_path = "unset";

	// The answer of GET /fornalha/ponte.
	CHECK(EretheaFornalhaLink::parse_bridge_message(R"({"versao": 3, "arquivos": ["Characters/heroi/heroi.glb"]})", version, files, websocket_path));
	CHECK(version == 3);
	REQUIRE(files.size() == 1);
	CHECK(files[0] == "Characters/heroi/heroi.glb");
	CHECK(websocket_path.is_empty());

	// A Fornalha that has a WebSocket says where.
	CHECK(EretheaFornalhaLink::parse_bridge_message(R"({"versao": 4, "arquivos": [], "websocket": "/fornalha/ponte/ws"})", version, files, websocket_path));
	CHECK(version == 4);
	CHECK(files.is_empty());
	CHECK(websocket_path == "/fornalha/ponte/ws");
	// Only a path on the Fornalha itself.
	CHECK(EretheaFornalhaLink::parse_bridge_message(R"({"versao": 4, "websocket": "ws://example.com/"})", version, files, websocket_path));
	CHECK(websocket_path.is_empty());

	// Pushed through the WebSocket.
	CHECK(EretheaFornalhaLink::parse_bridge_message(R"({"tipo": "ponte", "versao": 5, "arquivos": ["Enemies/lobo/lobo.glb", 7]})", version, files, websocket_path));
	CHECK(version == 5);
	CHECK(files.size() == 1);

	// Other messages are left alone.
	CHECK_FALSE(EretheaFornalhaLink::parse_bridge_message(R"({"tipo": "outra", "versao": 6})", version, files, websocket_path));
	CHECK_FALSE(EretheaFornalhaLink::parse_bridge_message(R"({"arquivos": []})", version, files, websocket_path));
	CHECK_FALSE(EretheaFornalhaLink::parse_bridge_message("[1, 2]", version, files, websocket_path));
	CHECK_FALSE(EretheaFornalhaLink::parse_bridge_message("not json", version, files, websocket_path));
	CHECK(version == 5);

	const Dictionary hello = EretheaFornalhaLink::make_hello("/jogos/vila/", true);
	CHECK(hello["tipo"] == "ola");
	CHECK(hello["projeto"] == "/jogos/vila/");
	CHECK(bool(hello["monta_cenas"]));
	CHECK(String(hello["programa"]).length() > 0);
}

TEST_CASE("[Modules][EretheaFornalha] How each area sounds") {
	EretheaFornalhaArea area;
	area.type = "caverna";
	EretheaFornalhaAcoustics acoustics = EretheaFornalhaImport::get_acoustics(area);
	CHECK(acoustics.name == "caverna");
	CHECK(acoustics.get_bus_name() == "Eco_caverna");
	area.type = "masmorra";
	CHECK(EretheaFornalhaImport::get_acoustics(area).name == "caverna");
	area.type = "Coliseu";
	CHECK(EretheaFornalhaImport::get_acoustics(area).name == "ruinas");
	area.type = "pantano";
	CHECK(EretheaFornalhaImport::get_acoustics(area).name == "floresta");
	area.type = "vulcao";
	CHECK(EretheaFornalhaImport::get_acoustics(area).name == "montanha");
	area.type = "castelo";
	CHECK(EretheaFornalhaImport::get_acoustics(area).name == "salao");
	area.type = "aldeia";
	CHECK(EretheaFornalhaImport::get_acoustics(area).name == "vila");
	// "acustica" in game.project.json wins over the type.
	area.acoustics = "caverna";
	CHECK(EretheaFornalhaImport::get_acoustics(area).name == "caverna");
	// Unknown: no echo.
	area.acoustics = String();
	area.type = "espaco";
	CHECK_FALSE(EretheaFornalhaImport::get_acoustics(area).is_valid());

	// The 15 types of the Fornalha's blockout all have one.
	const char *types[] = { "vila", "cidade", "aldeia", "floresta", "bosque", "pantano", "montanha", "colina", "vulcao", "masmorra", "caverna", "castelo", "ruinas", "arena", "coliseu" };
	for (const char *type : types) {
		area.type = type;
		CHECK_MESSAGE(EretheaFornalhaImport::get_acoustics(area).is_valid(), type);
	}

	EretheaFornalhaProject project;
	EretheaFornalhaArea village;
	village.center = Vector3(0, 0, 0);
	project.areas.push_back(village);
	CHECK(EretheaFornalhaImport::get_area_radius(project, 0) == doctest::Approx(80.0));
	EretheaFornalhaArea forest;
	forest.center = Vector3(0, 5, -40);
	project.areas.push_back(forest);
	// Half the way to the nearest area, on the ground.
	CHECK(EretheaFornalhaImport::get_area_radius(project, 0) == doctest::Approx(20.0));
	EretheaFornalhaArea camp;
	camp.center = Vector3(10, 0, 0);
	project.areas.push_back(camp);
	CHECK(EretheaFornalhaImport::get_area_radius(project, 0) == doctest::Approx(12.0));
	CHECK(EretheaFornalhaImport::get_area_radius(project, 1) == doctest::Approx(20.0));
}

TEST_CASE("[Modules][EretheaFornalha] How the music changes") {
	EretheaFornalhaMusic music;
	music.situation = "combate";
	music.file = "res://Audio/musica/combate.wav";
	music.bpm = 120.0;
	music.bars = 16;
	CHECK(music.get_beat_count() == 64);

	const String imported = "[remap]\n\nimporter=\"wav\"\ntype=\"AudioStreamWAV\"\n\n[deps]\n\nsource_file=\"res://Audio/musica/combate.wav\"\n\n[params]\n\nforce/8_bit=false\nedit/loop_mode=0\nedit/loop_begin=0\nedit/loop_end=-1\ncompress/mode=2\n";
	const String prepared = EretheaFornalhaImport::set_music_import_options(imported, music);
	CHECK(prepared != imported);

	Ref<ConfigFile> config;
	config.instantiate();
	REQUIRE(config->parse(prepared) == OK);
	// Looping forward over the whole file, with the tempo of the music.
	CHECK(int(config->get_value("params", "edit/loop_mode")) == 2);
	CHECK(int(config->get_value("params", "edit/loop_begin")) == 0);
	CHECK(int(config->get_value("params", "edit/loop_end")) == -1);
	CHECK(double(config->get_value("params", "bpm")) == doctest::Approx(120.0));
	CHECK(int(config->get_value("params", "beat_count")) == 64);
	CHECK(int(config->get_value("params", "bar_beats")) == 4);
	// What the file already had is kept.
	CHECK(bool(config->get_value("params", "force/8_bit")) == false);
	CHECK(int(config->get_value("params", "compress/mode")) == 2);
	CHECK(String(config->get_value("deps", "source_file")) == "res://Audio/musica/combate.wav");

	// Nothing to change the second time, so the file isn't written again.
	CHECK(EretheaFornalhaImport::set_music_import_options(prepared, music) == prepared);
	// A new tempo does change it.
	music.bpm = 90.0;
	CHECK(EretheaFornalhaImport::set_music_import_options(prepared, music) != prepared);
	// A file that isn't an import file is left alone.
	CHECK(EretheaFornalhaImport::set_music_import_options("not an import file", music) == "not an import file");

	// A new music, or one that was imported since, builds the scenes again.
	EretheaFornalhaProject project;
	const String signature = project.get_signature();
	project.music_tracks.push_back(music);
	const String with_music = project.get_signature();
	CHECK(with_music != signature);
	project.music_tracks.write[0].imported = true;
	CHECK(project.get_signature() != with_music);

	EretheaFornalhaImport::Report report;
	CHECK_FALSE(report.to_dictionary().has("music_clips"));
	report.music_clips.push_back("combate");
	report.music_clips.push_back("vila");
	const Dictionary dictionary = report.to_dictionary();
	CHECK(PackedStringArray(dictionary["music_clips"]) == PackedStringArray({ "combate", "vila" }));
}

// Two triangles that make a quad over [0, w] x [0, d] in x and z, with the
// height that p_height gives at each corner.
static void _add_quad(Vector<Vector3> &r_triangles, real_t p_x0, real_t p_z0, real_t p_x1, real_t p_z1, real_t p_y00, real_t p_y10, real_t p_y01, real_t p_y11) {
	const Vector3 a(p_x0, p_y00, p_z0);
	const Vector3 b(p_x1, p_y10, p_z0);
	const Vector3 c(p_x0, p_y01, p_z1);
	const Vector3 d(p_x1, p_y11, p_z1);
	for (const Vector3 &vertex : { a, b, c, b, d, c }) {
		r_triangles.push_back(vertex);
	}
}

TEST_CASE("[Modules][EretheaFornalha] Terrain heights come from the ground seen from above") {
	using namespace EretheaFornalhaTerrain;
	// A ramp of 10 by 6 meters that goes up half a meter per meter in x.
	Vector<Vector3> ramp;
	_add_quad(ramp, 0, 0, 10, 6, 0, 5, 0, 5);
	Heights heights = sample_heights(ramp);
	REQUIRE(heights.is_valid());
	CHECK(heights.resolution == Vector2i(11, 7));
	CHECK(heights.spacing == doctest::Approx(1.0));
	CHECK(heights.center.is_equal_approx(Vector2(5, 3)));
	CHECK(heights.uncovered == 0);
	for (int z = 0; z < 7; z++) {
		for (int x = 0; x < 11; x++) {
			CHECK(heights.heights[z * 11 + x] == doctest::Approx(x * 0.5).epsilon(0.001));
		}
	}

	// The terrain's data has those heights, centered where the ground is.
	Ref<EretheaTerrainData> data = make_data(heights);
	REQUIRE(data.is_valid());
	CHECK(data->get_resolution() == Vector2i(11, 7));
	CHECK(data->get_size().is_equal_approx(Vector2(10, 6)));
	CHECK(data->get_height(-5 + 4, 0) == doctest::Approx(2.0));
	CHECK(data->get_height(5, 3) == doctest::Approx(5.0));

	// A box: the top counts, not the bottom or the sides.
	Vector<Vector3> box;
	_add_quad(box, -4, -4, 4, 4, 2, 2, 2, 2);
	_add_quad(box, -4, -4, 4, 4, 0, 0, 0, 0);
	// A wall, which has no area seen from above.
	for (const Vector3 &vertex : { Vector3(-4, 0, 4), Vector3(4, 0, 4), Vector3(4, 2, 4), Vector3(-4, 0, 4), Vector3(4, 2, 4), Vector3(-4, 2, 4) }) {
		box.push_back(vertex);
	}
	heights = sample_heights(box);
	REQUIRE(heights.is_valid());
	CHECK(heights.resolution == Vector2i(9, 9));
	for (const real_t height : heights.heights) {
		CHECK(height == doctest::Approx(2.0));
	}

	// Where nothing is above a vertex, it takes what its neighbors have.
	Vector<Vector3> half;
	half.push_back(Vector3(0, 3, 0));
	half.push_back(Vector3(8, 3, 0));
	half.push_back(Vector3(0, 3, 8));
	heights = sample_heights(half);
	REQUIRE(heights.is_valid());
	CHECK(heights.uncovered > 0);
	CHECK(heights.uncovered < heights.heights.size() / 2);
	for (const real_t height : heights.heights) {
		CHECK(height == doctest::Approx(3.0));
	}
}

TEST_CASE("[Modules][EretheaFornalha] A big ground gets a wider spacing") {
	using namespace EretheaFornalhaTerrain;
	Vector<Vector3> big;
	_add_quad(big, -3000, -3000, 3000, 3000, 1, 1, 1, 1);
	const Heights heights = sample_heights(big);
	REQUIRE(heights.is_valid());
	CHECK(heights.resolution.x <= MAX_RESOLUTION);
	CHECK(heights.resolution.y <= MAX_RESOLUTION);
	CHECK(heights.spacing > 1.0);
	// And it still reaches the far edge.
	CHECK((heights.resolution.x - 1) * heights.spacing >= 6000.0 - 0.01);
	// Nothing to sample from nothing.
	CHECK_FALSE(sample_heights(Vector<Vector3>()).is_valid());
}

TEST_CASE("[Modules][EretheaFornalha] Each area has the ground of its type") {
	using namespace EretheaFornalhaTerrain;
	CHECK(get_area_layer("vila") == LAYER_DIRT);
	CHECK(get_area_layer("arena") == LAYER_SAND);
	for (const char *type : { "masmorra", "caverna", "salao", "ruinas", "montanha" }) {
		CHECK(get_area_layer(type) == LAYER_STONE);
	}
	CHECK(get_area_layer("floresta") == LAYER_GRASS);
	CHECK(get_area_layer("Vila") == LAYER_DIRT);
	CHECK(get_area_layer("") == LAYER_GRASS);
	CHECK(get_area_layer("algo novo") == LAYER_GRASS);

	CHECK(get_stone_weight(0) == doctest::Approx(0));
	CHECK(get_stone_weight(STONE_FROM_SLOPE) == doctest::Approx(0));
	CHECK(get_stone_weight(STONE_FULL_SLOPE) == doctest::Approx(1));
	CHECK(get_stone_weight(80) == doctest::Approx(1));
	CHECK(get_stone_weight(37.5) == doctest::Approx(0.5));

	// Each area paints as far as 70% of the reach of its sounds.
	EretheaFornalhaProject project;
	EretheaFornalhaArea village;
	village.id = "vila";
	village.type = "vila";
	village.center = Vector3(0, 0, 0);
	EretheaFornalhaArea forest;
	forest.id = "floresta";
	forest.type = "floresta";
	forest.center = Vector3(0, 0, -40);
	project.areas.push_back(village);
	project.areas.push_back(forest);
	const Vector<PaintArea> areas = get_paint_areas(project);
	REQUIRE(areas.size() == 2);
	CHECK(areas[0].layer == LAYER_DIRT);
	CHECK(areas[1].layer == LAYER_GRASS);
	CHECK(areas[0].center.is_equal_approx(Vector2(0, 0)));
	CHECK(areas[1].center.is_equal_approx(Vector2(0, -40)));
	CHECK(areas[0].radius == doctest::Approx(EretheaFornalhaImport::get_area_radius(project, 0) * 0.7));
	CHECK(areas[0].radius == doctest::Approx(14.0));
}

// A flat terrain of 40 by 40 meters at 1 m, which the blockout puts at p_origin.
static Ref<EretheaTerrainData> _make_flat_ground() {
	Ref<EretheaTerrainData> data;
	data.instantiate();
	data->create(Vector2(40, 40), 1.0, 0.0);
	return data;
}

TEST_CASE("[Modules][EretheaFornalha] Painting the materials of the terrain") {
	using namespace EretheaFornalhaTerrain;
	Ref<EretheaTerrainData> data = _make_flat_ground();
	// The vertex (20, 20) is the middle: x and z are 0 there.
	const Vector2i middle(20, 20);

	// Flat and in no area: grass, with nothing on it.
	paint_materials(data, Vector2(), Vector<PaintArea>());
	for (int z = 0; z < 41; z += 8) {
		for (int x = 0; x < 41; x += 8) {
			CHECK(data->get_control_base(Vector2i(x, z)) == LAYER_GRASS);
			CHECK(data->get_control_blend(Vector2i(x, z)) == doctest::Approx(0));
		}
	}

	// In a village: dirt inside, a blend into grass at its edge, grass past it.
	Vector<PaintArea> areas;
	areas.push_back({ Vector2(0, 0), 10.0, LAYER_DIRT });
	paint_materials(data, Vector2(), areas);
	CHECK(data->get_control_base(middle) == LAYER_DIRT);
	CHECK(data->get_control_blend(middle) == doctest::Approx(0));
	CHECK(data->get_control_base(Vector2i(25, 20)) == LAYER_DIRT); // 5 m from the center
	CHECK(data->get_control_blend(Vector2i(25, 20)) == doctest::Approx(0));
	// 9 m from the center: 90% of the way out, in the blend.
	CHECK(data->get_control_base(Vector2i(29, 20)) == LAYER_DIRT);
	CHECK(data->get_control_overlay(Vector2i(29, 20)) == LAYER_GRASS);
	CHECK(data->get_control_blend(Vector2i(29, 20)) > 0.3);
	CHECK(data->get_control_blend(Vector2i(29, 20)) < 1.0);
	// Further out the blend grows, and past the radius it is grass.
	CHECK(data->get_control_blend(Vector2i(29, 20)) > data->get_control_blend(Vector2i(28, 20)));
	CHECK(data->get_control_base(Vector2i(32, 20)) == LAYER_GRASS);
	CHECK(data->get_control_blend(Vector2i(32, 20)) == doctest::Approx(0));

	// The area is where it is in the blockout, and the terrain is where it is: an
	// area at x = 100 paints the terrain whose middle is at x = 100.
	areas.write[0].center = Vector2(100, 0);
	paint_materials(data, Vector2(100, 0), areas);
	CHECK(data->get_control_base(middle) == LAYER_DIRT);
	paint_materials(data, Vector2(0, 0), areas);
	CHECK(data->get_control_base(middle) == LAYER_GRASS);

	// Each type of area, its own ground.
	for (const Layer layer : { LAYER_SAND, LAYER_STONE }) {
		areas.write[0] = { Vector2(0, 0), 10.0, layer };
		paint_materials(data, Vector2(), areas);
		CHECK(data->get_control_base(middle) == layer);
	}

	// A steep slope shows stone: 60 degrees from x = 8 on, grass before it.
	data = _make_flat_ground();
	const real_t rise = Math::tan(Math::deg_to_rad(real_t(60)));
	for (int z = 0; z < 41; z++) {
		for (int x = 0; x < 41; x++) {
			const real_t meters = real_t(x - 20);
			data->set_vertex_height(Vector2i(x, z), meters > 8 ? (meters - 8) * rise : 0.0);
		}
	}
	data->notify_maps_changed();
	paint_materials(data, Vector2(), Vector<PaintArea>());
	CHECK(data->get_control_base(Vector2i(2, 20)) == LAYER_GRASS);
	CHECK(data->get_control_blend(Vector2i(2, 20)) == doctest::Approx(0));
	CHECK(data->get_control_base(Vector2i(34, 20)) == LAYER_GRASS);
	CHECK(data->get_control_overlay(Vector2i(34, 20)) == LAYER_STONE);
	CHECK(data->get_control_blend(Vector2i(34, 20)) == doctest::Approx(1).epsilon(0.01));

	// On a slope in a village, the slope wins over the edge of the village.
	areas.write[0] = { Vector2(0, 0), 40.0, LAYER_DIRT };
	paint_materials(data, Vector2(), areas);
	CHECK(data->get_control_base(Vector2i(34, 20)) == LAYER_DIRT);
	CHECK(data->get_control_overlay(Vector2i(34, 20)) == LAYER_STONE);
	CHECK(data->get_control_blend(Vector2i(34, 20)) == doctest::Approx(1).epsilon(0.01));
	// And on stone ground, a slope has nothing to add.
	areas.write[0] = { Vector2(0, 0), 40.0, LAYER_STONE };
	paint_materials(data, Vector2(), areas);
	CHECK(data->get_control_base(Vector2i(34, 20)) == LAYER_STONE);
	CHECK(data->get_control_blend(Vector2i(34, 20)) == doctest::Approx(0));
}

TEST_CASE("[Modules][EretheaFornalha] The materials of the terrain are plain colors of the ground") {
	using namespace EretheaFornalhaTerrain;
	const Color ground(0.3f, 0.5f, 0.2f);
	// Grass is the color of the ground; the others are other colors from it.
	CHECK(get_layer_color(LAYER_GRASS, ground).is_equal_approx(ground));
	CHECK(get_layer_color(LAYER_DIRT, ground).r > get_layer_color(LAYER_DIRT, ground).b);
	CHECK(get_layer_color(LAYER_SAND, ground).get_luminance() > ground.get_luminance());
	CHECK(get_layer_color(LAYER_STONE, ground).get_luminance() < get_layer_color(LAYER_SAND, ground).get_luminance());
	for (int a = 0; a < LAYER_COUNT; a++) {
		for (int b = a + 1; b < LAYER_COUNT; b++) {
			CHECK_FALSE(get_layer_color(Layer(a), ground).is_equal_approx(get_layer_color(Layer(b), ground)));
		}
	}

	const Ref<Texture2DArray> textures = make_textures(ground);
	REQUIRE(textures.is_valid());
	CHECK(textures->get_layers() == LAYER_COUNT);
	CHECK(textures->get_width() == 64);
	CHECK(textures->get_height() == 64);
	CHECK(textures->has_mipmaps());
	// The grass layer has the color of the ground, to within the noise; its alpha
	// is a height between 0 and 1.
	const Vector<Ref<Image>> images = make_layer_images(ground);
	REQUIRE(images.size() == LAYER_COUNT);
	Color mean;
	for (int y = 0; y < 64; y++) {
		for (int x = 0; x < 64; x++) {
			const Color pixel = images[LAYER_GRASS]->get_pixel(x, y);
			mean += pixel;
			CHECK(pixel.a > 0.0f);
			CHECK(pixel.a < 1.0f);
		}
	}
	mean /= 64.0f * 64.0f;
	CHECK(mean.r == doctest::Approx(ground.r).epsilon(0.1));
	CHECK(mean.g == doctest::Approx(ground.g).epsilon(0.1));
	CHECK(mean.b == doctest::Approx(ground.b).epsilon(0.1));
	// Made again, the same: building the scenes again must not change them.
	CHECK(make_layer_images(ground)[LAYER_STONE]->get_data() == images[LAYER_STONE]->get_data());
}

TEST_CASE("[Modules][EretheaFornalha] Finding the ground of the blockout") {
	using namespace EretheaFornalhaTerrain;
	Node3D *root = memnew(Node3D);
	root->set_name("blockout");
	root->set_position(Vector3(10, 0, 0));
	MeshInstance3D *house = memnew(MeshInstance3D);
	house->set_name("vila_casa");
	Ref<BoxMesh> house_mesh;
	house_mesh.instantiate();
	house->set_mesh(house_mesh);
	root->add_child(house);
	// The ground can be in a node of its own, as the importer makes it.
	Node3D *holder = memnew(Node3D);
	holder->set_position(Vector3(0, 1, 0));
	root->add_child(holder);
	MeshInstance3D *ground = memnew(MeshInstance3D);
	ground->set_name("chao");
	Ref<BoxMesh> ground_mesh;
	ground_mesh.instantiate();
	ground_mesh->set_size(Vector3(20, 2, 20));
	ground->set_mesh(ground_mesh);
	Ref<StandardMaterial3D> material;
	material.instantiate();
	material->set_albedo(Color(0.2f, 0.4f, 0.1f));
	ground->set_surface_override_material(0, material);
	holder->add_child(ground);

	CHECK(find_ground(root) == ground);
	CHECK(get_ground_color(ground).is_equal_approx(Color(0.2f, 0.4f, 0.1f)));

	// Its triangles are where the world has them: the top is at 1 + 1 = 2 m, and the
	// whole ground is 10 m to the side of the origin.
	const Vector<Vector3> triangles = get_ground_triangles(root, ground);
	CHECK(triangles.size() == 36);
	const Heights heights = sample_heights(triangles);
	REQUIRE(heights.is_valid());
	CHECK(heights.center.is_equal_approx(Vector2(10, 0)));
	CHECK(heights.resolution == Vector2i(21, 21));
	for (const real_t height : heights.heights) {
		CHECK(height == doctest::Approx(2.0));
	}

	// With no ground in it, nothing is found.
	Node3D *empty = memnew(Node3D);
	CHECK(find_ground(empty) == nullptr);
	memdelete(empty);
	memdelete(root);
}

} // namespace TestEretheaFornalha
