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
class Variant;

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
	// "acustica" of the area in game.project.json, to choose how it sounds
	// instead of by its type. Optional.
	String acoustics;
};

// How an area sounds: the reverb of the audio bus that its sounds go through.
struct EretheaFornalhaAcoustics {
	String name; // caverna, salao, ruinas, vila, floresta or montanha.
	float room_size = 0.5;
	float damping = 0.5;
	float wet = 0.2;
	float predelay_msec = 50.0;
	float predelay_feedback = 0.3;
	float hipass = 0.0;

	bool is_valid() const { return !name.is_empty(); }
	String get_bus_name() const { return "Eco_" + name; }
};

// A music of the Fornalha: Audio/musica/<situation>.wav, with the tempo that
// <situation>.musica.json says.
struct EretheaFornalhaMusic {
	String situation; // The file's name: exploracao, combate...
	String file; // res://Audio/musica/<situation>.wav
	String info; // res://Audio/musica/<situation>.musica.json
	double bpm = 0.0;
	int bars = 0;
	int beats_per_bar = 4;
	// Whether the editor has imported the WAV file (it has a .import file).
	bool imported = false;

	int get_beat_count() const { return bars * beats_per_bar; }
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
	// The music of each situation, by file name.
	Vector<EretheaFornalhaMusic> music_tracks;
	Vector<String> warnings;

	static EretheaFornalhaProject read();

	// Changes when anything the scenes are built from changes, such as a new
	// character or a model the Fornalha made again.
	String get_signature() const;

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
		// The reverb buses added for the areas' echo, and the file they went to.
		Vector<String> audio_buses;
		String bus_layout;
		// The situations that change the music in the main scene, and the WAV
		// files whose import options were set for them.
		Vector<String> music_clips;
		Vector<String> music_imports;
		// The textures of the models marked as used in 3D, to be compressed.
		Vector<String> compressed_textures;

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
	static constexpr const char *SETTING_BUILD_AUTOMATICALLY = "editor/erethea_fornalha/build_automatically";
	static constexpr const char *SETTING_LIVE_LINK = "editor/erethea_fornalha/live_link";
	static constexpr const char *SETTING_AREA_ACOUSTICS = "editor/erethea_fornalha/area_acoustics";
	static constexpr const char *SETTING_INTERACTIVE_MUSIC = "editor/erethea_fornalha/interactive_music";
	static constexpr const char *SETTING_TERRAIN_FROM_BLOCKOUT = "editor/erethea_fornalha/terrain_from_blockout";
	static constexpr const char *SETTING_ERETHEA_DEFAULTS_APPLIED = "editor/erethea_fornalha/erethea_defaults_applied";
	// The terrain made from the blockout's ground, and its materials.
	static constexpr const char *TERRAIN_DATA_FILE = "res://World/terreno.res";
	static constexpr const char *TERRAIN_MATERIALS_FILE = "res://World/terreno_materiais.res";
	static constexpr const char *MUSIC_FOLDER = "res://Audio/musica";
	static constexpr const char *MUSIC_GROUP = "erethea_musica";

	static constexpr uint32_t LAYER_WORLD = 1;
	static constexpr uint32_t LAYER_PLAYER = 2;
	static constexpr uint32_t LAYER_ENEMIES = 4;

	static void register_project_settings();
	static bool is_fornalha_project();
	// Gives the project the settings that new Erethea projects start with
	// (p_defaults), each one once: the Fornalha writes its own project.godot,
	// so its projects would otherwise keep Godot's defaults. A setting the
	// project already changed stays as it is, and so does one given before, so
	// that turning it off again sticks. Returns the settings it changed; save
	// the project settings afterwards.
	static Vector<String> apply_erethea_defaults(const HashMap<String, Variant> &p_defaults);

	static Action decide(bool p_exists, const String &p_current_hash, const String &p_recorded_hash, const String &p_new_hash, bool p_force);
	// Build by itself when what the scenes come from changed since the last
	// build. Before the first build, only in a project with no main scene yet:
	// one whose scenes the Fornalha assembles waits for someone to build once.
	static bool should_build_automatically(bool p_built_before, const String &p_built_signature, const String &p_signature, bool p_main_scene_exists);
	static bool was_built_before();
	static String get_built_signature();
	static String get_action_name(Action p_action);
	// The text of a .tscn without what changes on every save (its uid and the
	// random ids of its resources), so that two builds of a scene compare equal.
	static String normalize_scene_text(const String &p_text);
	static String hash_scene_text(const String &p_text);

	// By "acustica", or else by the area's type; invalid for a type that has none.
	static EretheaFornalhaAcoustics get_acoustics(const EretheaFornalhaArea &p_area);
	// How far an area's sound reaches from its center: half the way to the
	// nearest other area, between 12 and 80 m.
	static real_t get_area_radius(const EretheaFornalhaProject &p_project, int p_area);
	// How far from the camera the sun casts shadows in the world: across it,
	// from Godot's 100 m to 300 m, so that the four cascades keep their detail.
	static real_t get_sun_shadow_distance(const EretheaFornalhaProject &p_project);
	// Adds the reverb buses that the areas use, and that the project doesn't
	// have yet, to its audio bus layout, and saves it. Returns the layout's
	// file when it changed.
	static String add_reverb_buses(const EretheaFornalhaProject &p_project, Vector<String> &r_added, String &r_error);

	// The import options that make a WAV file music that changes at the end of
	// a bar. Returns the text of its .import file with them set, or the same
	// text when they already were.
	static String set_music_import_options(const String &p_import_text, const EretheaFornalhaMusic &p_music);
	// Does so for the .import file of each music, and asks the editor to import
	// them again. Returns the WAV files that changed.
	static Vector<String> prepare_music_imports(const EretheaFornalhaProject &p_project, Vector<String> &r_warnings);
	// The image files that the models use (the characters', the enemies' and
	// the world's) and that the texture importer would compress on its own once
	// they are seen in 3D. Godot only sees that when the editor draws them, so
	// imported without a window, as the Fornalha does, they would stay
	// uncompressed. Marks them as used in 3D, which compresses them (and lets
	// Erethea stream their mipmaps), and returns them.
	static Vector<String> mark_model_textures_used_in_3d(const EretheaFornalhaProject &p_project);

	static Node *make_character(const EretheaFornalhaCharacter &p_character, Vector<String> &r_warnings);
	static Node *make_area(const EretheaFornalhaProject &p_project, int p_area, Vector<String> &r_warnings);
	static Node *make_world(const EretheaFornalhaProject &p_project, Vector<String> &r_warnings);
	// With interactive music, r_music_clips gets the situations the Musica node can play.
	static Node *make_main(const EretheaFornalhaProject &p_project, Vector<String> &r_warnings, Vector<String> *r_music_clips = nullptr);

	static Report build(bool p_force = false);
};
