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

#include "core/io/config_file.h"
#include "scene/3d/camera_3d.h"
#include "scene/3d/physics/character_body_3d.h"
#include "scene/3d/physics/collision_shape_3d.h"
#include "scene/3d/physics/spring_arm_3d.h"
#include "scene/animation/animation_player.h"
#include "scene/resources/3d/capsule_shape_3d.h"
#include "scene/resources/packed_scene.h"
#include "tests/test_macros.h"

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

} // namespace TestEretheaFornalha
