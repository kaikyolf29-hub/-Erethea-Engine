/**************************************************************************/
/*  erethea_fornalha_import.cpp                                           */
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

#include "erethea_fornalha_import.h"

#include "core/config/project_settings.h"
#include "core/io/config_file.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/json.h"
#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "core/object/script_language.h"
#include "core/os/time.h"
#include "editor/editor_node.h"
#include "editor/editor_undo_redo_manager.h"
#include "editor/file_system/editor_file_system.h"
#include "scene/3d/camera_3d.h"
#include "scene/3d/light_3d.h"
#include "scene/3d/physics/area_3d.h"
#include "scene/3d/physics/character_body_3d.h"
#include "scene/3d/physics/collision_shape_3d.h"
#include "scene/3d/physics/spring_arm_3d.h"
#include "scene/3d/world_environment.h"
#include "scene/audio/audio_stream_player.h"
#include "scene/main/canvas_layer.h"
#include "scene/resources/3d/capsule_shape_3d.h"
#include "scene/resources/3d/cylinder_shape_3d.h"
#include "scene/resources/3d/sky_material.h"
#include "scene/resources/3d/sphere_shape_3d.h"
#include "scene/resources/audio/audio_stream_wav.h"
#include "scene/resources/environment.h"
#include "scene/resources/packed_scene.h"
#include "scene/resources/sky.h"
#include "servers/audio/audio_server.h"
#include "servers/audio/effects/audio_effect_reverb.h"

#ifdef MODULE_INTERACTIVE_MUSIC_ENABLED
#include "modules/interactive_music/audio_stream_interactive.h"
#endif

static const char *MODEL_EXTENSIONS[] = { "glb", "gltf" };
static const char *SKY_FILES[] = { "res://World/ceu.exr", "res://World/ceu.hdr" };

static Dictionary _read_json(const String &p_path, Vector<String> *r_warnings = nullptr) {
	if (!FileAccess::exists(p_path)) {
		return Dictionary();
	}
	Ref<JSON> json;
	json.instantiate();
	if (json->parse(FileAccess::get_file_as_string(p_path)) != OK || json->get_data().get_type() != Variant::DICTIONARY) {
		if (r_warnings) {
			r_warnings->push_back(vformat("%s isn't valid JSON, so it was skipped.", p_path));
		}
		return Dictionary();
	}
	return json->get_data();
}

static Dictionary _dictionary(const Variant &p_value) {
	return p_value.get_type() == Variant::DICTIONARY ? Dictionary(p_value) : Dictionary();
}

static Array _array(const Variant &p_value) {
	return p_value.get_type() == Variant::ARRAY ? Array(p_value) : Array();
}

static String _string(const Variant &p_value) {
	return p_value.get_type() == Variant::STRING ? String(p_value) : String();
}

static bool _vector3(const Variant &p_value, Vector3 &r_vector) {
	const Array array = _array(p_value);
	if (array.size() < 3) {
		return false;
	}
	for (int i = 0; i < 3; i++) {
		if (!array[i].is_num()) {
			return false;
		}
	}
	r_vector = Vector3(array[0], array[1], array[2]);
	return true;
}

static bool _is_valid_id(const String &p_id) {
	return !p_id.is_empty() && p_id.is_valid_filename() && !p_id.begins_with(".");
}

static void _add_warning(Vector<String> &r_warnings, const String &p_warning) {
	if (!r_warnings.has(p_warning)) {
		r_warnings.push_back(p_warning);
	}
}

static void _scan_characters(const String &p_folder, bool p_player, Vector<EretheaFornalhaCharacter> &r_characters) {
	Ref<DirAccess> dir = DirAccess::open(p_folder);
	if (dir.is_null()) {
		return;
	}
	PackedStringArray ids = dir->get_directories();
	ids.sort();
	for (const String &id : ids) {
		if (!_is_valid_id(id)) {
			continue;
		}
		const String base = p_folder.path_join(id).path_join(id);
		EretheaFornalhaCharacter character;
		for (const char *extension : MODEL_EXTENSIONS) {
			if (FileAccess::exists(base + "." + extension)) {
				character.model = base + "." + extension;
				break;
			}
		}
		if (character.model.is_empty()) {
			continue;
		}
		character.id = id;
		character.player = p_player;
		character.scene = base + ".tscn";
		// The Forja writes the measures of the model next to it.
		const Variant height = _dictionary(_read_json(base + ".forja.json").get("estatisticas", Variant())).get("altura_m", Variant());
		if (height.is_num() && double(height) > 0.0) {
			character.height = double(height);
		}
		r_characters.push_back(character);
	}
}

static void _scan_music(Vector<EretheaFornalhaMusic> &r_tracks, Vector<String> &r_warnings) {
	Ref<DirAccess> dir = DirAccess::open(EretheaFornalhaImport::MUSIC_FOLDER);
	if (dir.is_null()) {
		return;
	}
	PackedStringArray files = dir->get_files();
	files.sort();
	for (const String &file : files) {
		if (file.get_extension().to_lower() != "wav") {
			continue;
		}
		EretheaFornalhaMusic music;
		music.situation = file.get_basename();
		music.file = String(EretheaFornalhaImport::MUSIC_FOLDER).path_join(file);
		music.info = String(EretheaFornalhaImport::MUSIC_FOLDER).path_join(music.situation + ".musica.json");
		if (!FileAccess::exists(music.info)) {
			continue; // Not described: it stays as a plain looping WAV.
		}
		const Dictionary info = _read_json(music.info, &r_warnings);
		const Variant bpm = info.get("bpm", Variant());
		const Variant bars = info.get("compassos", Variant());
		if (!_is_valid_id(music.situation) || !bpm.is_num() || double(bpm) <= 0.0 || !bars.is_num() || int(bars) <= 0) {
			_add_warning(r_warnings, vformat("%s has no valid bpm and compassos, so its music doesn't change at the end of a bar.", music.info));
			continue;
		}
		music.bpm = double(bpm);
		music.bars = int(bars);
		music.imported = FileAccess::exists(music.file + ".import");
		r_tracks.push_back(music);
	}
}

EretheaFornalhaProject EretheaFornalhaProject::read() {
	EretheaFornalhaProject project;
	const Dictionary game_project = _read_json(EretheaFornalhaImport::GAME_PROJECT_FILE, &project.warnings);

	_scan_characters("res://Characters", true, project.characters);
	_scan_characters("res://Enemies", false, project.characters);

	for (const char *extension : MODEL_EXTENSIONS) {
		const String model = String("res://World/blockout.") + extension;
		if (FileAccess::exists(model)) {
			project.world_model = model;
			break;
		}
	}
	const Dictionary info = _read_json("res://World/blockout.info.json", &project.warnings);
	_vector3(info.get("spawn_jogador", Variant()), project.player_spawn);
	for (const Variant &spawn : _array(info.get("spawn_inimigos", Variant()))) {
		Vector3 position;
		if (_vector3(spawn, position)) {
			project.enemy_spawns.push_back(position);
		}
	}

	// The blockout knows where each area is; game.project.json knows its name.
	HashMap<String, Dictionary> planned_areas;
	Vector<String> planned_order;
	for (const Variant &value : _array(_dictionary(game_project.get("mundo", Variant())).get("areas", Variant()))) {
		const Dictionary area = _dictionary(value);
		const String id = _string(area.get("id", Variant()));
		if (_is_valid_id(id) && !planned_areas.has(id)) {
			planned_areas[id] = area;
			planned_order.push_back(id);
		}
	}
	for (const Variant &value : _array(info.get("areas", Variant()))) {
		const Dictionary info_area = _dictionary(value);
		EretheaFornalhaArea area;
		area.id = _string(info_area.get("id", Variant()));
		if (!_is_valid_id(area.id)) {
			continue;
		}
		bool repeated = false;
		for (const EretheaFornalhaArea &other : project.areas) {
			repeated = repeated || other.id == area.id;
		}
		if (repeated) {
			continue;
		}
		_vector3(info_area.get("centro", Variant()), area.center);
		area.type = _string(info_area.get("tipo", Variant()));
		area.name = planned_areas.has(area.id) ? _string(planned_areas[area.id].get("nome", Variant())) : String();
		if (area.name.is_empty()) {
			area.name = area.id;
		}
		area.scene = "res://World/areas/" + area.id + ".tscn";
		area.acoustics = planned_areas.has(area.id) ? _string(planned_areas[area.id].get("acustica", Variant())) : String();
		project.areas.push_back(area);
	}
	if (project.world_model.is_empty()) {
		project.warnings.push_back("No blockout in World/ yet, so the world has no ground. Generate the world in the Fornalha (generate_blockout).");
	} else {
		for (const String &id : planned_order) {
			bool found = false;
			for (const EretheaFornalhaArea &area : project.areas) {
				found = found || area.id == id;
			}
			if (!found) {
				project.warnings.push_back(vformat("The area \"%s\" of game.project.json isn't in the blockout yet. Generate the world again in the Fornalha.", id));
			}
		}
	}

	// What the Fornalha chose when it last assembled the game.
	const Dictionary scene_manifest = _read_json("res://.fornalha/cena.json", &project.warnings);
	const String chosen = _string(scene_manifest.get("jogador", Variant()));
	const int chosen_index = project.find_character(chosen);
	if (chosen_index >= 0 && project.characters[chosen_index].player) {
		project.player = chosen;
	}
	if (project.player.is_empty()) {
		for (const Variant &value : _array(game_project.get("personagens", Variant()))) {
			const Dictionary character = _dictionary(value);
			const String id = _string(character.get("id", Variant()));
			const int index = project.find_character(id);
			if (index >= 0 && project.characters[index].player && _string(character.get("papel", Variant())).to_lower() == "protagonista") {
				project.player = id;
				break;
			}
		}
	}
	if (project.player.is_empty()) {
		for (const EretheaFornalhaCharacter &character : project.characters) {
			if (character.player) {
				project.player = character.id;
				break;
			}
		}
	}
	const Variant enemy_near = scene_manifest.get("inimigo_perto", false);
	project.enemy_near = enemy_near.get_type() == Variant::BOOL && bool(enemy_near);
	const String music = _string(scene_manifest.get("musica", Variant()));
	if (music.begins_with("res://") && FileAccess::exists(music)) {
		project.music = music;
	}
	_scan_music(project.music_tracks, project.warnings);
	const String sky = _string(scene_manifest.get("ceu", Variant()));
	if (sky.begins_with("res://") && FileAccess::exists(sky)) {
		project.sky = sky;
	} else {
		for (const char *file : SKY_FILES) {
			if (FileAccess::exists(file)) {
				project.sky = file;
				break;
			}
		}
	}
	return project;
}

String EretheaFornalhaProject::get_signature() const {
	// The models by their date too: the Fornalha writes them again in place.
	Array inputs;
	for (const EretheaFornalhaCharacter &character : characters) {
		inputs.push_back(vformat("character %s %s %s %s %d", character.id, character.player, character.model, character.height, int64_t(FileAccess::get_modified_time(character.model))));
	}
	for (const EretheaFornalhaArea &area : areas) {
		inputs.push_back(vformat("area %s %s %s %v", area.id, area.name, area.type, area.center) + (area.acoustics.is_empty() ? String() : " acustica " + area.acoustics));
	}
	inputs.push_back(vformat("world %s %d", world_model, world_model.is_empty() ? int64_t(0) : int64_t(FileAccess::get_modified_time(world_model))));
	inputs.push_back(vformat("player %s %v", player, player_spawn));
	for (const Vector3 &spawn : enemy_spawns) {
		inputs.push_back(vformat("enemy spawn %v", spawn));
	}
	inputs.push_back(vformat("enemy near %s", enemy_near));
	inputs.push_back(vformat("music %s", music));
	// Each music by its tempo and date, so that a new one builds the scenes again.
	for (const EretheaFornalhaMusic &track : music_tracks) {
		inputs.push_back(vformat("music track %s %s %d %d %d", track.situation, track.bpm, track.bars, track.imported, int64_t(FileAccess::get_modified_time(track.info))));
	}
	inputs.push_back(vformat("sky %s", sky));
	return JSON::stringify(inputs).md5_text();
}

int EretheaFornalhaProject::find_character(const String &p_id) const {
	for (int i = 0; i < characters.size(); i++) {
		if (characters[i].id == p_id) {
			return i;
		}
	}
	return -1;
}

Vector<String> EretheaFornalhaProject::get_enemy_ids() const {
	Vector<String> ids;
	for (const EretheaFornalhaCharacter &character : characters) {
		if (!character.player) {
			ids.push_back(character.id);
		}
	}
	return ids;
}

int EretheaFornalhaProject::find_area(const Vector3 &p_position) const {
	int nearest = -1;
	real_t nearest_distance = 0.0;
	for (int i = 0; i < areas.size(); i++) {
		const real_t distance = Vector2(p_position.x, p_position.z).distance_squared_to(Vector2(areas[i].center.x, areas[i].center.z));
		if (nearest < 0 || distance < nearest_distance) {
			nearest = i;
			nearest_distance = distance;
		}
	}
	return nearest;
}

Vector<EretheaFornalhaEnemy> EretheaFornalhaProject::place_enemies() const {
	Vector<EretheaFornalhaEnemy> enemies;
	const Vector<String> ids = get_enemy_ids();
	if (ids.is_empty()) {
		return enemies;
	}
	Vector<Vector3> points;
	if (enemy_near) {
		// In front of the player, who looks down -Z.
		points.push_back(player_spawn + Vector3(0, 0, -6));
	}
	points.append_array(enemy_spawns);
	for (int i = 0; i < points.size(); i++) {
		EretheaFornalhaEnemy enemy;
		enemy.id = ids[i % ids.size()];
		enemy.area = find_area(points[i]);
		enemy.position = points[i] - (enemy.area >= 0 ? areas[enemy.area].center : Vector3());
		enemies.push_back(enemy);
	}
	return enemies;
}

int EretheaFornalhaImport::Report::count(Action p_action) const {
	int total = 0;
	for (const FileResult &file : files) {
		total += file.action == p_action ? 1 : 0;
	}
	return total;
}

int EretheaFornalhaImport::Report::count_kept() const {
	return count(ACTION_KEPT_EDITED) + count(ACTION_KEPT_FOREIGN) + count(ACTION_KEPT_UNSAVED);
}

String EretheaFornalhaImport::Report::get_summary() const {
	if (!error.is_empty()) {
		return error;
	}
	const struct {
		int count;
		const char *text;
	} parts[] = {
		{ count(ACTION_CREATED), "%d created" },
		{ count(ACTION_UPDATED), "%d updated" },
		{ count(ACTION_REPLACED), "%d replaced" },
		{ count(ACTION_UNCHANGED), "%d unchanged" },
		{ count_kept(), "%d kept" },
		{ count(ACTION_FAILED), "%d failed" },
	};
	PackedStringArray texts;
	for (const auto &part : parts) {
		if (part.count > 0) {
			texts.push_back(vformat(part.text, part.count));
		}
	}
	return texts.is_empty() ? String("Nothing to build.") : "Scenes: " + String(", ").join(texts) + ".";
}

Dictionary EretheaFornalhaImport::Report::to_dictionary() const {
	Dictionary result;
	result["ok"] = error.is_empty() && count(ACTION_FAILED) == 0;
	result["summary"] = get_summary();
	if (!error.is_empty()) {
		result["error"] = error;
	}
	Array scenes;
	for (const FileResult &file : files) {
		Dictionary scene;
		scene["file"] = file.file;
		scene["result"] = get_action_name(file.action);
		if (!file.message.is_empty()) {
			scene["message"] = file.message;
		}
		scenes.push_back(scene);
	}
	result["scenes"] = scenes;
	Array warning_list;
	for (const String &warning : warnings) {
		warning_list.push_back(warning);
	}
	result["warnings"] = warning_list;
	if (!music_clips.is_empty()) {
		result["music_clips"] = Variant(PackedStringArray(music_clips));
		result["music_imports"] = Variant(PackedStringArray(music_imports));
	}
	if (!backup_folder.is_empty()) {
		result["backup_folder"] = backup_folder;
	}
	if (!audio_buses.is_empty()) {
		Array buses;
		for (const String &bus : audio_buses) {
			buses.push_back(bus);
		}
		result["audio_buses"] = buses;
		result["bus_layout"] = bus_layout;
	}
	return result;
}

void EretheaFornalhaImport::register_project_settings() {
	// The Forja's walk cycles. glTF has no loop flag, so they import as one-shot.
	PackedStringArray looping;
	looping.push_back("parado");
	looping.push_back("andar");
	looping.push_back("correr");
	looping.push_back("correr_rapido");
	GLOBAL_DEF(PropertyInfo(Variant::PACKED_STRING_ARRAY, SETTING_LOOPING_ANIMATIONS), looping);
	// Both only act in projects with a game.project.json.
	GLOBAL_DEF(SETTING_BUILD_AUTOMATICALLY, true);
	GLOBAL_DEF(SETTING_LIVE_LINK, true);
	GLOBAL_DEF(SETTING_AREA_ACOUSTICS, true);
	GLOBAL_DEF(SETTING_INTERACTIVE_MUSIC, true);
}

bool EretheaFornalhaImport::is_fornalha_project() {
	return FileAccess::exists(GAME_PROJECT_FILE);
}

EretheaFornalhaImport::Action EretheaFornalhaImport::decide(bool p_exists, const String &p_current_hash, const String &p_recorded_hash, const String &p_new_hash, bool p_force) {
	if (!p_exists) {
		return ACTION_CREATED;
	}
	if (p_current_hash == p_new_hash) {
		return ACTION_UNCHANGED;
	}
	if (!p_recorded_hash.is_empty() && p_recorded_hash == p_current_hash) {
		return ACTION_UPDATED;
	}
	if (p_force) {
		return ACTION_REPLACED;
	}
	return p_recorded_hash.is_empty() ? ACTION_KEPT_FOREIGN : ACTION_KEPT_EDITED;
}

bool EretheaFornalhaImport::should_build_automatically(bool p_built_before, const String &p_built_signature, const String &p_signature, bool p_main_scene_exists) {
	if (!p_built_before) {
		return !p_main_scene_exists;
	}
	return p_built_signature != p_signature;
}

bool EretheaFornalhaImport::was_built_before() {
	return FileAccess::exists(RECORD_FILE);
}

String EretheaFornalhaImport::get_built_signature() {
	return _string(_read_json(RECORD_FILE).get("inputs", Variant()));
}

String EretheaFornalhaImport::get_action_name(Action p_action) {
	switch (p_action) {
		case ACTION_CREATED:
			return "created";
		case ACTION_UPDATED:
			return "updated";
		case ACTION_UNCHANGED:
			return "unchanged";
		case ACTION_KEPT_EDITED:
		case ACTION_KEPT_FOREIGN:
		case ACTION_KEPT_UNSAVED:
			return "kept";
		case ACTION_REPLACED:
			return "replaced";
		case ACTION_FAILED:
			return "failed";
	}
	return "failed";
}

static String _attribute(const String &p_line, const String &p_name) {
	const String start = " " + p_name + "=\"";
	const int from = p_line.find(start);
	if (from < 0) {
		return String();
	}
	const int to = p_line.find_char('"', from + start.length());
	return to < 0 ? String() : p_line.substr(from + start.length(), to - from - start.length());
}

static String _replace_attribute(const String &p_line, const String &p_name, const String &p_value) {
	const String start = " " + p_name + "=\"";
	const int from = p_line.find(start);
	if (from < 0) {
		return p_line;
	}
	const int to = p_line.find_char('"', from + start.length());
	if (to < 0) {
		return p_line;
	}
	return p_line.substr(0, from) + (p_value.is_empty() ? String() : start + p_value + "\"") + p_line.substr(to + 1);
}

static String _replace_references(const String &p_line, const String &p_call, const HashMap<String, String> &p_ids) {
	const String start = p_call + "(\"";
	String result;
	int position = 0;
	while (true) {
		const int from = p_line.find(start, position);
		if (from < 0) {
			break;
		}
		const int to = p_line.find_char('"', from + start.length());
		if (to < 0) {
			break;
		}
		const String id = p_line.substr(from + start.length(), to - from - start.length());
		result += p_line.substr(position, from + start.length() - position);
		result += p_ids.has(id) ? p_ids[id] : id;
		position = to;
	}
	return result + p_line.substr(position);
}

String EretheaFornalhaImport::normalize_scene_text(const String &p_text) {
	const Vector<String> lines = p_text.split("\n");
	HashMap<String, String> ext_ids;
	HashMap<String, String> sub_ids;
	for (const String &line : lines) {
		const bool external = line.begins_with("[ext_resource ");
		if (!external && !line.begins_with("[sub_resource ")) {
			continue;
		}
		const String id = _attribute(line, "id");
		HashMap<String, String> &ids = external ? ext_ids : sub_ids;
		if (!id.is_empty() && !ids.has(id)) {
			ids[id] = (external ? "e" : "s") + itos(ids.size() + 1);
		}
	}
	String result;
	for (int i = 0; i < lines.size(); i++) {
		String line = lines[i];
		if (i == 0 && line.begins_with("[gd_scene ")) {
			line = _replace_attribute(line, "uid", String());
		} else if (line.begins_with("[ext_resource ")) {
			line = _replace_attribute(line, "id", ext_ids.has(_attribute(line, "id")) ? ext_ids[_attribute(line, "id")] : String());
		} else if (line.begins_with("[sub_resource ")) {
			line = _replace_attribute(line, "id", sub_ids.has(_attribute(line, "id")) ? sub_ids[_attribute(line, "id")] : String());
		}
		line = _replace_references(line, "ExtResource", ext_ids);
		line = _replace_references(line, "SubResource", sub_ids);
		result += line + "\n";
	}
	return result;
}

String EretheaFornalhaImport::hash_scene_text(const String &p_text) {
	return normalize_scene_text(p_text).sha256_text();
}

template <typename T>
static T *_add(Node *p_parent, Node *p_owner, const String &p_name) {
	T *node = memnew(T);
	node->set_name(p_name);
	p_parent->add_child(node);
	node->set_owner(p_owner);
	return node;
}

static Node *_add_instance(Node *p_parent, Node *p_owner, const String &p_path, const String &p_name, Vector<String> &r_warnings) {
	Ref<PackedScene> scene;
	if (ResourceLoader::exists(p_path, "PackedScene")) {
		// Scenes built here may have just changed on disk.
		const bool built_here = p_path.get_extension() == "tscn";
		scene = ResourceLoader::load(p_path, "PackedScene", built_here ? ResourceFormatLoader::CACHE_MODE_REPLACE : ResourceFormatLoader::CACHE_MODE_REUSE);
	}
	Node *node = scene.is_valid() ? scene->instantiate(PackedScene::GEN_EDIT_STATE_INSTANCE) : nullptr;
	if (!node) {
		_add_warning(r_warnings, vformat("%s couldn't be loaded, so it was left out. If it's new, wait for the editor to import it, then build again.", p_path));
		return nullptr;
	}
	node->set_name(p_name);
	p_parent->add_child(node);
	node->set_owner(p_owner);
	return node;
}

static void _set_script(Node *p_node, const String &p_file, Vector<String> &r_warnings) {
	const String path = String(EretheaFornalhaImport::SCRIPTS_FOLDER).path_join(p_file);
	const Ref<Script> script = ResourceLoader::exists(path) ? ResourceLoader::load(path) : Ref<Resource>();
	if (script.is_null()) {
		_add_warning(r_warnings, vformat("%s is missing, so the scenes that use it have no script. Prepare the project again in the Fornalha (prepare_project).", path));
		return;
	}
	p_node->set_script(script);
}

String EretheaFornalhaImport::set_music_import_options(const String &p_import_text, const EretheaFornalhaMusic &p_music) {
	Ref<ConfigFile> config;
	config.instantiate();
	if (config->parse(p_import_text) != OK || !config->has_section("params")) {
		return p_import_text;
	}
	const struct {
		const char *key;
		Variant value;
	} options[] = {
		{ "edit/loop_mode", 2 }, // Forward.
		{ "edit/loop_begin", 0 },
		{ "edit/loop_end", -1 },
		{ "bpm", p_music.bpm },
		{ "beat_count", p_music.get_beat_count() },
		{ "bar_beats", p_music.beats_per_bar },
	};
	bool changed = false;
	for (const auto &option : options) {
		const Variant current = config->get_value("params", option.key, Variant());
		if (current.get_type() != option.value.get_type() || current != option.value) {
			config->set_value("params", option.key, option.value);
			changed = true;
		}
	}
	return changed ? config->encode_to_text() : p_import_text;
}

Vector<String> EretheaFornalhaImport::prepare_music_imports(const EretheaFornalhaProject &p_project, Vector<String> &r_warnings) {
	Vector<String> changed;
	for (const EretheaFornalhaMusic &music : p_project.music_tracks) {
		const String import_file = music.file + ".import";
		if (!music.imported) {
			_add_warning(r_warnings, vformat("%s hasn't been imported yet, so its music was left out. Wait for the editor to import it, then build again.", music.file));
			continue;
		}
		const String text = FileAccess::get_file_as_string(import_file);
		const String new_text = set_music_import_options(text, music);
		if (new_text == text) {
			continue;
		}
		Ref<FileAccess> file = FileAccess::open(import_file, FileAccess::WRITE);
		if (file.is_null()) {
			_add_warning(r_warnings, vformat("%s couldn't be written, so its music doesn't change at the end of a bar.", import_file));
			continue;
		}
		file->store_string(new_text);
		file.unref();
		changed.push_back(music.file);
	}
	if (!changed.is_empty() && EditorFileSystem::get_singleton()) {
		EditorFileSystem::get_singleton()->reimport_files(changed);
	}
	return changed;
}

Node *EretheaFornalhaImport::make_character(const EretheaFornalhaCharacter &p_character, Vector<String> &r_warnings) {
	CharacterBody3D *root = memnew(CharacterBody3D);
	root->set_name(p_character.id.validate_node_name());
	// The Fornalha's game code: a third-person player, or an enemy that chases and attacks.
	_set_script(root, p_character.player ? "jogador.gd" : "inimigo.gd", r_warnings);
	root->set_collision_layer(p_character.player ? LAYER_PLAYER : LAYER_ENEMIES);
	root->set_collision_mask(LAYER_WORLD | (p_character.player ? LAYER_ENEMIES : LAYER_PLAYER));
	if (!_add_instance(root, root, p_character.model, "Modelo", r_warnings)) {
		memdelete(root);
		return nullptr;
	}

	const real_t height = MAX(p_character.height, real_t(0.2));
	Ref<CapsuleShape3D> capsule;
	capsule.instantiate();
	capsule->set_radius(MIN(CLAMP(height * real_t(0.18), real_t(0.15), real_t(0.6)), height * real_t(0.5)));
	capsule->set_height(height);
	CollisionShape3D *collision = _add<CollisionShape3D>(root, root, "Colisao");
	collision->set_shape(capsule);
	collision->set_position(Vector3(0, height * 0.5, 0));

	if (p_character.player) {
		SpringArm3D *arm = _add<SpringArm3D>(root, root, "BracoCamera");
		arm->set_position(Vector3(0, height * 0.85, 0));
		arm->set_rotation(Vector3(Math::deg_to_rad(real_t(-18.0)), 0, 0));
		arm->set_length(3.5);
		arm->set_margin(0.2);
		arm->set_mask(LAYER_WORLD);
		// A sphere, not a ray, so that the camera doesn't go through the corner of a wall.
		Ref<SphereShape3D> sphere;
		sphere.instantiate();
		sphere->set_radius(0.2);
		arm->set_shape(sphere);
		Camera3D *camera = _add<Camera3D>(arm, root, "Camera");
		camera->set_fov(65.0);
		camera->set_near(0.05);
		// Over the shoulder.
		camera->set_position(Vector3(0.4, 0, 0));
	}
	return root;
}

struct EretheaFornalhaAcousticsPreset {
	const char *name;
	const char *types;
	float room_size;
	float damping;
	float wet;
	float predelay_msec;
	float predelay_feedback;
	float hipass;
};

// The 15 types of area of the Fornalha's blockout, by how they sound.
static const EretheaFornalhaAcousticsPreset ACOUSTICS_PRESETS[] = {
	// Closed stone: long, bright echo.
	{ "caverna", "caverna,masmorra", 0.85, 0.25, 0.42, 60.0, 0.4, 0.0 },
	// A great hall.
	{ "salao", "castelo", 0.7, 0.4, 0.3, 35.0, 0.3, 0.0 },
	// Stone walls under the open sky.
	{ "ruinas", "ruinas,arena,coliseu", 0.55, 0.5, 0.2, 45.0, 0.25, 0.0 },
	// Houses close by: short and soft.
	{ "vila", "vila,cidade,aldeia", 0.35, 0.65, 0.12, 25.0, 0.2, 0.05 },
	// Leaves soak up the highs.
	{ "floresta", "floresta,bosque,pantano", 0.45, 0.85, 0.12, 20.0, 0.15, 0.15 },
	// Open land: a far, late echo.
	{ "montanha", "montanha,colina,vulcao", 0.9, 0.55, 0.14, 150.0, 0.35, 0.1 },
};

EretheaFornalhaAcoustics EretheaFornalhaImport::get_acoustics(const EretheaFornalhaArea &p_area) {
	const String wanted = p_area.acoustics.strip_edges().to_lower();
	const String type = p_area.type.strip_edges().to_lower();
	for (const EretheaFornalhaAcousticsPreset &preset : ACOUSTICS_PRESETS) {
		const bool chosen = wanted.is_empty() ? String(preset.types).split(",").has(type) : wanted == preset.name;
		if (chosen) {
			EretheaFornalhaAcoustics acoustics;
			acoustics.name = preset.name;
			acoustics.room_size = preset.room_size;
			acoustics.damping = preset.damping;
			acoustics.wet = preset.wet;
			acoustics.predelay_msec = preset.predelay_msec;
			acoustics.predelay_feedback = preset.predelay_feedback;
			acoustics.hipass = preset.hipass;
			return acoustics;
		}
	}
	return EretheaFornalhaAcoustics();
}

real_t EretheaFornalhaImport::get_area_radius(const EretheaFornalhaProject &p_project, int p_area) {
	ERR_FAIL_INDEX_V(p_area, p_project.areas.size(), 0.0);
	const Vector2 center(p_project.areas[p_area].center.x, p_project.areas[p_area].center.z);
	real_t nearest = Math::INF;
	for (int i = 0; i < p_project.areas.size(); i++) {
		if (i != p_area) {
			nearest = MIN(nearest, center.distance_to(Vector2(p_project.areas[i].center.x, p_project.areas[i].center.z)));
		}
	}
	return Math::is_inf(nearest) ? real_t(80.0) : CLAMP(nearest * real_t(0.5), real_t(12.0), real_t(80.0));
}

String EretheaFornalhaImport::add_reverb_buses(const EretheaFornalhaProject &p_project, Vector<String> &r_added, String &r_error) {
	r_added.clear();
	AudioServer *audio = AudioServer::get_singleton();
	if (!audio) {
		return String();
	}
	for (const EretheaFornalhaArea &area : p_project.areas) {
		const EretheaFornalhaAcoustics acoustics = get_acoustics(area);
		if (!acoustics.is_valid() || audio->get_bus_index(acoustics.get_bus_name()) >= 0) {
			continue;
		}
		audio->add_bus();
		const int bus = audio->get_bus_count() - 1;
		audio->set_bus_name(bus, acoustics.get_bus_name());
		audio->set_bus_send(bus, "Master");
		Ref<AudioEffectReverb> reverb;
		reverb.instantiate();
		reverb->set_room_size(acoustics.room_size);
		reverb->set_damping(acoustics.damping);
		reverb->set_wet(acoustics.wet);
		// Only the echo: the sound itself goes straight to its own bus.
		reverb->set_dry(0.0);
		reverb->set_predelay_msec(acoustics.predelay_msec);
		reverb->set_predelay_feedback(acoustics.predelay_feedback);
		reverb->set_hpf(acoustics.hipass);
		audio->add_bus_effect(bus, reverb);
		r_added.push_back(acoustics.get_bus_name());
	}
	if (r_added.is_empty()) {
		return String();
	}
	String path = GLOBAL_GET("audio/buses/default_bus_layout");
	if (path.is_empty()) {
		path = "res://default_bus_layout.tres";
	}
	const Error error = ResourceSaver::save(audio->generate_bus_layout(), path);
	if (error != OK) {
		r_error = vformat("%s couldn't be saved (error %d), so the areas' echo lasts only until the editor closes.", path, error);
	}
	return path;
}

Node *EretheaFornalhaImport::make_area(const EretheaFornalhaProject &p_project, int p_area, Vector<String> &r_warnings) {
	ERR_FAIL_INDEX_V(p_area, p_project.areas.size(), nullptr);
	const EretheaFornalhaArea &area = p_project.areas[p_area];
	Node3D *root = memnew(Node3D);
	root->set_name(area.id.validate_node_name());
	root->set_editor_description(vformat("Area \"%s\" (%s) of the blockout, built from game.project.json. Put here what belongs to this area, such as its enemies and items.", area.name, area.type));
	Node3D *enemies = _add<Node3D>(root, root, "Inimigos");
	HashMap<String, int> numbers;
	for (const EretheaFornalhaEnemy &enemy : p_project.place_enemies()) {
		if (enemy.area != p_area) {
			continue;
		}
		const int number = numbers.has(enemy.id) ? numbers[enemy.id] + 1 : 1;
		numbers[enemy.id] = number;
		const EretheaFornalhaCharacter &character = p_project.characters[p_project.find_character(enemy.id)];
		Node3D *node = Object::cast_to<Node3D>(_add_instance(enemies, root, character.scene, vformat("%s_%02d", enemy.id, number).validate_node_name(), r_warnings));
		if (node) {
			node->set_position(enemy.position + Vector3(0, 0.1, 0));
		}
	}

	// The sounds made inside the area echo as the area does.
	const EretheaFornalhaAcoustics acoustics = get_acoustics(area);
	if (acoustics.is_valid() && bool(GLOBAL_GET(SETTING_AREA_ACOUSTICS))) {
		Area3D *echo = _add<Area3D>(root, root, "Eco");
		echo->set_editor_description(vformat("How sounds made in this area echo (\"%s\"). The bus %s is in the project's audio buses.", acoustics.name, acoustics.get_bus_name()));
		echo->set_monitoring(false);
		echo->set_use_reverb_bus(true);
		echo->set_reverb_bus_name(acoustics.get_bus_name());
		echo->set_reverb_amount(1.0);
		echo->set_reverb_uniformity(0.5);
		CollisionShape3D *shape_node = _add<CollisionShape3D>(echo, root, "Alcance");
		Ref<CylinderShape3D> shape;
		shape.instantiate();
		shape->set_radius(get_area_radius(p_project, p_area));
		shape->set_height(80.0);
		shape_node->set_shape(shape);
		shape_node->set_position(Vector3(0, 30.0, 0));
	}
	return root;
}

Node *EretheaFornalhaImport::make_world(const EretheaFornalhaProject &p_project, Vector<String> &r_warnings) {
	Node3D *root = memnew(Node3D);
	root->set_name("Mundo");
	if (!p_project.world_model.is_empty()) {
		_add_instance(root, root, p_project.world_model, "Blockout", r_warnings);
	}

	DirectionalLight3D *sun = _add<DirectionalLight3D>(root, root, "Sol");
	sun->set_rotation_degrees(Vector3(-50, -30, 0));
	sun->set_shadow(true);

	Ref<Material> sky_material;
	if (!p_project.sky.is_empty()) {
		const Ref<Texture2D> panorama = ResourceLoader::load(p_project.sky, "Texture2D");
		if (panorama.is_valid()) {
			Ref<PanoramaSkyMaterial> material;
			material.instantiate();
			material->set_panorama(panorama);
			sky_material = material;
		}
	}
	if (sky_material.is_null()) {
		Ref<ProceduralSkyMaterial> material;
		material.instantiate();
		sky_material = material;
	}
	Ref<Sky> sky;
	sky.instantiate();
	sky->set_material(sky_material);
	Ref<Environment> environment;
	environment.instantiate();
	environment->set_background(Environment::BG_SKY);
	environment->set_sky(sky);
	environment->set_ambient_source(Environment::AMBIENT_SOURCE_SKY);
	environment->set_reflection_source(Environment::REFLECTION_SOURCE_SKY);
	environment->set_tonemapper(Environment::TONE_MAPPER_AGX);
	environment->set_ssao_enabled(true);
	environment->set_glow_enabled(true);
	WorldEnvironment *world_environment = _add<WorldEnvironment>(root, root, "Ambiente");
	world_environment->set_environment(environment);

	Node3D *areas = _add<Node3D>(root, root, "Areas");
	for (const EretheaFornalhaArea &area : p_project.areas) {
		Node3D *node = Object::cast_to<Node3D>(_add_instance(areas, root, area.scene, area.id.validate_node_name(), r_warnings));
		if (node) {
			node->set_position(area.center);
		}
	}
	return root;
}

// The script of the Musica node: the Fornalha's Sons autoload calls tocar() on
// the group, and the music starts or changes to the clip of that situation. In
// the editor of Godot, which reads no bpm from the WAV files, it changes right
// away.
static const char *MUSIC_SCRIPT = R"(extends AudioStreamPlayer
# Built by Erethea from the music in Audio/musica. Sons.tocar_musica() calls
# tocar() on the group "erethea_musica" instead of playing the file by itself.


func tocar(situacao: String) -> void:
	var clips := stream as AudioStreamInteractive
	if clips == null:
		return
	var clip := -1
	for i in clips.clip_count:
		if clips.get_clip_name(i) == situacao:
			clip = i
			break
	if clip < 0:
		return
	if not playing:
		clips.initial_clip = clip
		play()
		return
	var playback := get_stream_playback() as AudioStreamPlaybackInteractive
	if playback == null or playback.get_current_clip_index() == clip:
		return
	playback.switch_to_clip_by_name(situacao)
)";

// The music of each situation as one stream whose clips change at the end of
// the bar. Returns the situations that went in.
static Vector<String> _add_music_player(Node *p_root, const EretheaFornalhaProject &p_project, Vector<String> &r_warnings) {
	Vector<String> situations;
#ifdef MODULE_INTERACTIVE_MUSIC_ENABLED
	Ref<AudioStreamInteractive> interactive;
	interactive.instantiate();
	Vector<Ref<AudioStreamWAV>> streams;
	Vector<String> names;
	int initial = 0;
	for (const EretheaFornalhaMusic &music : p_project.music_tracks) {
		if (!music.imported) {
			continue; // The warning comes with the import options.
		}
		const Ref<AudioStreamWAV> stream = ResourceLoader::load(music.file, "AudioStreamWAV");
		if (stream.is_null()) {
			_add_warning(r_warnings, vformat("%s couldn't be loaded, so its music was left out. If it's new, wait for the editor to import it, then build again.", music.file));
			continue;
		}
		if (music.file == p_project.music) {
			initial = streams.size();
		}
		streams.push_back(stream);
		names.push_back(music.situation);
	}
	if (streams.is_empty()) {
		return situations;
	}
	ScriptLanguage *language = ScriptServer::get_language_for_extension("gd");
	Ref<Script> script;
	if (language) {
		script = Ref<Script>(Object::cast_to<Script>(ClassDB::instantiate(language->get_type())));
	}
	if (script.is_null()) {
		_add_warning(r_warnings, "GDScript isn't available, so the music doesn't change with the game.");
		return situations;
	}
	script->set_source_code(MUSIC_SCRIPT);

	interactive->set_clip_count(streams.size());
	for (int i = 0; i < streams.size(); i++) {
		interactive->set_clip_name(i, names[i]);
		interactive->set_clip_stream(i, streams[i]);
	}
	interactive->set_initial_clip(initial);
	// From any music to any other, when the bar ends, with two beats of fade.
	interactive->add_transition(AudioStreamInteractive::CLIP_ANY, AudioStreamInteractive::CLIP_ANY, AudioStreamInteractive::TRANSITION_FROM_TIME_NEXT_BAR, AudioStreamInteractive::TRANSITION_TO_TIME_START, AudioStreamInteractive::FADE_CROSS, 2);

	AudioStreamPlayer *player = _add<AudioStreamPlayer>(p_root, p_root, "Musica");
	player->set_editor_description("The music of each situation in Audio/musica. Sons.tocar_musica() asks it to play one, and it changes at the end of the bar. Built from the Audio/musica folder.");
	player->set_stream(interactive);
	player->set_volume_db(-8.0);
	player->add_to_group(EretheaFornalhaImport::MUSIC_GROUP, true);
	player->set_script(script);
	situations = names;
#endif
	return situations;
}

Node *EretheaFornalhaImport::make_main(const EretheaFornalhaProject &p_project, Vector<String> &r_warnings, Vector<String> *r_music_clips) {
	Node3D *root = memnew(Node3D);
	root->set_name("Principal");
	_set_script(root, "principal.gd", r_warnings);
	if (!p_project.music.is_empty()) {
		root->set("musica", p_project.music);
	}
	_add_instance(root, root, WORLD_SCENE, "Mundo", r_warnings);
	if (bool(GLOBAL_GET(SETTING_INTERACTIVE_MUSIC))) {
		const Vector<String> clips = _add_music_player(root, p_project, r_warnings);
		if (r_music_clips) {
			*r_music_clips = clips;
		}
	}

	const int player = p_project.find_character(p_project.player);
	if (player < 0) {
		_add_warning(r_warnings, "No playable character in Characters/ yet. Make one in the Fornalha (create_character).");
	} else {
		Node3D *node = Object::cast_to<Node3D>(_add_instance(root, root, p_project.characters[player].scene, p_project.player.validate_node_name(), r_warnings));
		if (node) {
			node->set_position(p_project.player_spawn + Vector3(0, 0.1, 0));
		}
	}

	// Enemies go in the scene of their area. Without areas, they go here.
	Node3D *enemies = nullptr;
	HashMap<String, int> numbers;
	for (const EretheaFornalhaEnemy &enemy : p_project.place_enemies()) {
		if (enemy.area >= 0) {
			continue;
		}
		if (!enemies) {
			enemies = _add<Node3D>(root, root, "Inimigos");
		}
		const int number = numbers.has(enemy.id) ? numbers[enemy.id] + 1 : 1;
		numbers[enemy.id] = number;
		const EretheaFornalhaCharacter &character = p_project.characters[p_project.find_character(enemy.id)];
		Node3D *node = Object::cast_to<Node3D>(_add_instance(enemies, root, character.scene, vformat("%s_%02d", enemy.id, number).validate_node_name(), r_warnings));
		if (node) {
			node->set_position(enemy.position + Vector3(0, 0.1, 0));
		}
	}

	CanvasLayer *hud = _add<CanvasLayer>(root, root, "HUD");
	_set_script(hud, "hud.gd", r_warnings);
	return root;
}

static bool _is_open_with_unsaved_changes(const String &p_file) {
	EditorNode *editor = EditorNode::get_singleton();
	if (!editor || !EditorUndoRedoManager::get_singleton() || !editor->is_scene_open(p_file)) {
		return false;
	}
	return EditorUndoRedoManager::get_singleton()->is_history_unsaved(EditorNode::get_editor_data().get_scene_history_id_from_path(p_file));
}

struct EretheaFornalhaBuildState {
	bool force = false;
	Dictionary recorded;
	String backup_folder;
	Vector<String> changed;
	EretheaFornalhaImport::Report *report = nullptr;
};

// Ids from the place of each node in the scene, not random ones, so that the
// same scene built twice is saved the same way.
static void _set_unique_ids(Node *p_root, Node *p_node) {
	if (p_node != p_root && p_node->get_owner() != p_root) {
		return;
	}
	const int32_t id = int32_t(String(p_root->get_path_to(p_node)).hash() & 0x7FFFFFFF);
	p_node->set_unique_scene_id(id == Node::UNIQUE_SCENE_ID_UNASSIGNED ? 1 : id);
	for (int i = 0; i < p_node->get_child_count(); i++) {
		_set_unique_ids(p_root, p_node->get_child(i));
	}
}

static String _work_file(const String &p_name) {
	return String(EretheaFornalhaImport::WORK_FOLDER).path_join(p_name);
}

static void _save_scene(Node *p_root, const String &p_file, EretheaFornalhaBuildState &r_state) {
	EretheaFornalhaImport::FileResult result;
	result.file = p_file;
	result.action = EretheaFornalhaImport::ACTION_FAILED;
	Ref<PackedScene> packed;
	if (p_root) {
		_set_unique_ids(p_root, p_root);
		packed.instantiate();
		const Error error = packed->pack(p_root);
		memdelete(p_root);
		if (error != OK) {
			packed.unref();
			result.message = vformat("Couldn't be packed (error %d).", error);
		}
	} else {
		result.message = "Couldn't be built. The warnings say why.";
	}
	if (packed.is_null()) {
		r_state.report->files.push_back(result);
		return;
	}
	if (_is_open_with_unsaved_changes(p_file)) {
		result.action = EretheaFornalhaImport::ACTION_KEPT_UNSAVED;
		result.message = "Open in the editor with unsaved changes, so it was kept. Save or close it, then build again.";
		r_state.report->files.push_back(result);
		return;
	}

	// Saved once aside to compare it with the scene on disk.
	const String draft = _work_file("draft.tscn");
	Error error = ResourceSaver::save(packed, draft);
	if (error != OK) {
		result.message = vformat("Couldn't be saved in %s (error %d).", EretheaFornalhaImport::WORK_FOLDER, error);
		r_state.report->files.push_back(result);
		return;
	}
	const String new_hash = EretheaFornalhaImport::hash_scene_text(FileAccess::get_file_as_string(draft));
	DirAccess::remove_absolute(ProjectSettings::get_singleton()->globalize_path(draft));
	const bool exists = FileAccess::exists(p_file);
	const String current_hash = exists ? EretheaFornalhaImport::hash_scene_text(FileAccess::get_file_as_string(p_file)) : String();
	result.action = EretheaFornalhaImport::decide(exists, current_hash, _string(r_state.recorded.get(p_file, Variant())), new_hash, r_state.force);

	switch (result.action) {
		case EretheaFornalhaImport::ACTION_UNCHANGED: {
			r_state.recorded[p_file] = current_hash;
		} break;
		case EretheaFornalhaImport::ACTION_KEPT_EDITED: {
			result.message = "Changed since Erethea built it, so it was kept. Build with force to replace it (a copy goes to res://.erethea/backup/).";
		} break;
		case EretheaFornalhaImport::ACTION_KEPT_FOREIGN: {
			result.message = "Not built by Erethea (the Fornalha or a person made it), so it was kept. Build with force to replace it (a copy goes to res://.erethea/backup/).";
		} break;
		case EretheaFornalhaImport::ACTION_CREATED:
		case EretheaFornalhaImport::ACTION_UPDATED:
		case EretheaFornalhaImport::ACTION_REPLACED: {
			Ref<DirAccess> dir = DirAccess::create(DirAccess::ACCESS_RESOURCES);
			if (result.action == EretheaFornalhaImport::ACTION_REPLACED) {
				const String backup = r_state.backup_folder.path_join(p_file.trim_prefix("res://"));
				dir->make_dir_recursive(backup.get_base_dir());
				if (dir->copy(p_file, backup) != OK) {
					result.action = EretheaFornalhaImport::ACTION_FAILED;
					result.message = vformat("Couldn't be copied to %s first, so it wasn't replaced.", backup);
					break;
				}
				r_state.report->backup_folder = r_state.backup_folder;
				result.message = vformat("The old one is in %s.", backup);
			}
			dir->make_dir_recursive(p_file.get_base_dir());
			error = ResourceSaver::save(packed, p_file);
			if (error != OK) {
				result.action = EretheaFornalhaImport::ACTION_FAILED;
				result.message = vformat("Couldn't be saved (error %d).", error);
				break;
			}
			r_state.recorded[p_file] = EretheaFornalhaImport::hash_scene_text(FileAccess::get_file_as_string(p_file));
			r_state.changed.push_back(p_file);
		} break;
		default:
			break;
	}
	r_state.report->files.push_back(result);
}

EretheaFornalhaImport::Report EretheaFornalhaImport::build(bool p_force) {
	Report report;
	if (!is_fornalha_project()) {
		report.error = "This project wasn't made with the Fornalha: it has no game.project.json.";
		return report;
	}
	const EretheaFornalhaProject project = EretheaFornalhaProject::read();
	report.warnings = project.warnings;

	EretheaFornalhaBuildState state;
	state.force = p_force;
	state.report = &report;
	const Dictionary record = _read_json(RECORD_FILE, &report.warnings);
	state.recorded = _dictionary(record.get("files", Variant())).duplicate();
	state.backup_folder = _work_file("backup").path_join(Time::get_singleton()->get_datetime_string_from_system().replace(":", "-").replace("T", "_"));
	DirAccess::make_dir_recursive_absolute(ProjectSettings::get_singleton()->globalize_path(WORK_FOLDER));

	// In this order, so that each scene finds the ones it uses on disk.
	for (const EretheaFornalhaCharacter &character : project.characters) {
		_save_scene(make_character(character, report.warnings), character.scene, state);
	}
	if (bool(GLOBAL_GET(SETTING_INTERACTIVE_MUSIC))) {
		report.music_imports = prepare_music_imports(project, report.warnings);
	}
	if (bool(GLOBAL_GET(SETTING_AREA_ACOUSTICS))) {
		String error;
		report.bus_layout = add_reverb_buses(project, report.audio_buses, error);
		if (!error.is_empty()) {
			report.warnings.push_back(error);
		}
	}
	for (int i = 0; i < project.areas.size(); i++) {
		_save_scene(make_area(project, i, report.warnings), project.areas[i].scene, state);
	}
	_save_scene(make_world(project, report.warnings), WORLD_SCENE, state);
	_save_scene(make_main(project, report.warnings, &report.music_clips), MAIN_SCENE, state);

	Dictionary new_record;
	new_record["format"] = 1;
	new_record["about"] = "Scenes that Erethea built from game.project.json. A scene whose text no longer matches was changed by someone, and the next build keeps it.";
	new_record["files"] = state.recorded;
	new_record["inputs"] = project.get_signature();
	Ref<FileAccess> file = FileAccess::open(RECORD_FILE, FileAccess::WRITE);
	if (file.is_valid()) {
		file->store_string(JSON::stringify(new_record, "\t", true) + "\n");
	} else {
		report.warnings.push_back(vformat("%s couldn't be written, so the next build won't know which scenes it made.", RECORD_FILE));
	}

	if (EditorFileSystem::get_singleton()) {
		for (const String &changed : state.changed) {
			EditorFileSystem::get_singleton()->update_file(changed);
		}
	}
	if (EditorNode::get_singleton()) {
		for (const String &changed : state.changed) {
			if (EditorNode::get_singleton()->is_scene_open(changed)) {
				EditorNode::get_singleton()->reload_scene(changed);
			}
		}
	}
	return report;
}
