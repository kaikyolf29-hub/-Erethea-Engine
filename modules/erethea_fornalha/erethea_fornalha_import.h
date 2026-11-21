/**************************************************************************/
/*  erethea_fornalha_import.h                                             */
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

#include "core/math/vector3.h"
#include "core/string/ustring.h"
#include "core/templates/hash_map.h"
#include "core/templates/vector.h"
#include "core/variant/dictionary.h"

class Node;

struct EretheaFornalhaCharacter {
	String id;
	// Characters/ holds the protagonist and allies, Enemies/ enemies and bosses.
	bool player = false;
	String model; // res://Characters/<id>/<id>.glb
	String scene; // res://Characters/<id>/<id>.tscn
	real_t height = 1.8;
};

struct EretheaFornalhaArea {
	String id;
	String name;
	String type;
	Vector3 center;
	String scene; // res://World/areas/<id>.tscn
};

struct EretheaFornalhaEnemy {
	String id;
	int area = -1; // Index in EretheaFornalhaProject::areas, or -1 for none.
	Vector3 position; // Relative to the area's center.
};

// What the Fornalha left in the project: game.project.json, the models in
// Characters/ and Enemies/, the blockout in World/ and .fornalha/cena.json.
struct EretheaFornalhaProject {
	Vector<EretheaFornalhaCharacter> characters;
	Vector<EretheaFornalhaArea> areas;
	String player; // Id of the playable character.
	String world_model; // res://World/blockout.glb
	Vector3 player_spawn;
	Vector<Vector3> enemy_spawns;
	bool enemy_near = false; // An extra enemy in front of the player, for tests.
	String music;
	String sky;
	Vector<String> warnings;

	static EretheaFornalhaProject read();

	int find_character(const String &p_id) const;
	Vector<String> get_enemy_ids() const;
	int find_area(const Vector3 &p_position) const;
	Vector<EretheaFornalhaEnemy> place_enemies() const;
};

// Builds the scenes of a project made with the Fornalha: one per character, one
// per area of the blockout, the world and the main scene. A scene changed since
// it was built, or not built here, is kept unless p_force is true.
class EretheaFornalhaImport {
public:
	enum Action {
		ACTION_CREATED,
		ACTION_UPDATED,
		ACTION_UNCHANGED,
		ACTION_KEPT_EDITED,
		ACTION_KEPT_FOREIGN,
		ACTION_KEPT_UNSAVED,
		ACTION_REPLACED,
		ACTION_FAILED,
	};

	struct FileResult {
		String file;
		Action action = ACTION_FAILED;
		String message;
	};

	struct Report {
		String error; // Set when nothing was built.
		Vector<FileResult> files;
		Vector<String> warnings;
		String backup_folder;

		int count(Action p_action) const;
		int count_kept() const;
		String get_summary() const;
		Dictionary to_dictionary() const;
	};

	static constexpr const char *GAME_PROJECT_FILE = "res://game.project.json";
	static constexpr const char *RECORD_FILE = "res://.erethea/fornalha_import.json";
	static constexpr const char *WORK_FOLDER = "res://.erethea";
	static constexpr const char *MAIN_SCENE = "res://principal.tscn";
	static constexpr const char *WORLD_SCENE = "res://World/mundo.tscn";
	static constexpr const char *SCRIPTS_FOLDER = "res://Scripts/fornalha";
	static constexpr const char *SETTING_LOOPING_ANIMATIONS = "editor/erethea_fornalha/looping_animations";

	static constexpr uint32_t LAYER_WORLD = 1;
	static constexpr uint32_t LAYER_PLAYER = 2;
	static constexpr uint32_t LAYER_ENEMIES = 4;

	static void register_project_settings();
	static bool is_fornalha_project();

	static Action decide(bool p_exists, const String &p_current_hash, const String &p_recorded_hash, const String &p_new_hash, bool p_force);
	static String get_action_name(Action p_action);
	// The text of a .tscn without what changes on every save (its uid and the
	// random ids of its resources), so that two builds of a scene compare equal.
	static String normalize_scene_text(const String &p_text);
	static String hash_scene_text(const String &p_text);

	static Node *make_character(const EretheaFornalhaCharacter &p_character, Vector<String> &r_warnings);
	static Node *make_area(const EretheaFornalhaProject &p_project, int p_area, Vector<String> &r_warnings);
	static Node *make_world(const EretheaFornalhaProject &p_project, Vector<String> &r_warnings);
	static Node *make_main(const EretheaFornalhaProject &p_project, Vector<String> &r_warnings);

	static Report build(bool p_force = false);
};
