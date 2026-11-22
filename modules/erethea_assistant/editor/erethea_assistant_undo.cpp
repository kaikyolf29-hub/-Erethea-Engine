/**************************************************************************/
/*  erethea_assistant_undo.cpp                                            */
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

#include "erethea_assistant_undo.h"

#include "core/config/project_settings.h"
#include "core/crypto/crypto_core.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/resource_loader.h"
#include "core/object/script_language.h"
#include "editor/editor_data.h"
#include "editor/editor_interface.h"
#include "editor/editor_node.h"
#include "editor/editor_undo_redo_manager.h"
#include "editor/file_system/editor_file_system.h"
#include "editor/script/script_editor_plugin.h"

#include "modules/erethea_mcp/editor/erethea_mcp_scene_tools.h"
#include "modules/modules_enabled.gen.h" // For erethea_fornalha.
#ifdef MODULE_ERETHEA_FORNALHA_ENABLED
#include "modules/erethea_fornalha/erethea_fornalha_import.h"
#endif

bool EretheaAssistantUndo::changes_scene(const String &p_tool) {
	return p_tool == "create_node" || p_tool == "set_node_properties" || p_tool == "delete_node" || p_tool == "attach_script";
}

bool EretheaAssistantUndo::_read(const String &p_path, Vector<uint8_t> &r_content) {
	r_content.clear();
	if (!FileAccess::exists(p_path)) {
		return false;
	}
	r_content = FileAccess::get_file_as_bytes(p_path);
	return true;
}

String EretheaAssistantUndo::_hash(const Vector<uint8_t> &p_content) {
	unsigned char hash[32];
	CryptoCore::sha256(p_content.ptr(), p_content.size(), hash);
	return String::hex_encode_buffer(hash, 32);
}

bool EretheaAssistantUndo::_write(const String &p_path, const Vector<uint8_t> &p_content) {
	DirAccess::make_dir_recursive_absolute(ProjectSettings::get_singleton()->globalize_path(p_path.get_base_dir()));
	Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::WRITE);
	if (file.is_null()) {
		return false;
	}
	file->store_buffer(p_content.ptr(), p_content.size());
	return true;
}

void EretheaAssistantUndo::_add_candidate(const String &p_path, Reason p_reason) {
	if (p_path.is_empty() || _find_file(p_path)) {
		// Only the state before the answer counts.
		return;
	}
	for (const Candidate &candidate : candidates) {
		if (candidate.path == p_path) {
			return;
		}
	}
	if (FileAccess::exists(p_path) && FileAccess::get_size(p_path) > MAX_FILE_SIZE) {
		return;
	}
	Candidate candidate;
	candidate.path = p_path;
	candidate.reason = p_reason;
	candidate.existed = _read(p_path, candidate.content);
	candidates.push_back(candidate);
}

void EretheaAssistantUndo::_add_scene_history() {
	Node *root = EditorNode::get_singleton()->get_edited_scene();
	if (!root) {
		return;
	}
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	const int history_id = undo_redo->get_history_id_for_object(root);
	for (const SceneState &scene : scenes) {
		if (scene.history_id == history_id) {
			return;
		}
	}
	UndoRedo *history = undo_redo->get_history_undo_redo(history_id);
	if (!history) {
		return;
	}
	SceneState scene;
	scene.history_id = history_id;
	scene.scene_path = root->get_scene_file_path();
	scene.version_before = history->get_version();
	scenes.push_back(scene);
}

EretheaAssistantUndo::FileState *EretheaAssistantUndo::_find_file(const String &p_path) {
	for (FileState &file : files) {
		if (file.path == p_path) {
			return &file;
		}
	}
	return nullptr;
}

EretheaAssistantUndo::SceneState *EretheaAssistantUndo::_find_scene(const String &p_path) {
	for (SceneState &scene : scenes) {
		if (!p_path.is_empty() && scene.scene_path == p_path) {
			return &scene;
		}
	}
	return nullptr;
}

static void _list_scenes(const String &p_folder, Vector<String> &r_files, int p_limit) {
	Ref<DirAccess> dir = DirAccess::open(p_folder);
	if (dir.is_null()) {
		return;
	}
	dir->set_include_hidden(false);
	dir->list_dir_begin();
	for (String name = dir->get_next(); !name.is_empty() && r_files.size() < p_limit; name = dir->get_next()) {
		if (name.begins_with(".")) {
			continue;
		}
		const String path = p_folder.path_join(name);
		if (dir->current_is_dir()) {
			_list_scenes(path, r_files, p_limit);
		} else if (name.get_extension() == "tscn") {
			r_files.push_back(path);
		}
	}
	dir->list_dir_end();
}

void EretheaAssistantUndo::begin_answer() {
	previous_files = files;
	previous_scenes = scenes;
	previous_finished = finished;
	candidates.clear();
	files.clear();
	scenes.clear();
	finished = false;
	recording = true;
}

void EretheaAssistantUndo::before_tool(const String &p_tool, const Dictionary &p_arguments) {
	if (!recording) {
		return;
	}
	candidates.clear();
	if (changes_scene(p_tool)) {
		_add_scene_history();
		return;
	}

	String path;
	String error;
	if (p_tool == "write_script" || p_tool == "edit_file") {
		if (EretheaMcpSceneTools::project_path(p_arguments.get("path", Variant()), path, error)) {
			_add_candidate(path, REASON_WRITTEN);
		}
	} else if (p_tool == "create_scene") {
		if (EretheaMcpSceneTools::project_path(p_arguments.get("path", Variant()), path, error)) {
			_add_candidate(path, REASON_CREATED);
		}
	} else if (p_tool == "save_scene") {
		if (p_arguments.has("path")) {
			EretheaMcpSceneTools::project_path(p_arguments["path"], path, error);
		} else if (EditorNode::get_singleton()->get_edited_scene()) {
			path = EditorNode::get_singleton()->get_edited_scene()->get_scene_file_path();
		}
		_add_candidate(path, REASON_SAVED);
	} else if (p_tool == "run_game" || p_tool == "run_test") {
		// The editor saves the open scenes before running them.
		for (const String &scene : EditorInterface::get_singleton()->get_open_scenes()) {
			_add_candidate(scene, REASON_SAVED);
		}
	} else if (p_tool == "build_fornalha_scenes") {
		Vector<String> scene_files;
		_list_scenes("res://", scene_files, MAX_SCAN_FILES);
		for (const String &scene : scene_files) {
			_add_candidate(scene, REASON_REBUILT);
		}
#ifdef MODULE_ERETHEA_FORNALHA_ENABLED
		_add_candidate(EretheaFornalhaImport::RECORD_FILE, REASON_WRITTEN);
#endif
	}
}

void EretheaAssistantUndo::after_tool(const String &p_tool, const Dictionary &p_arguments, const EretheaMcpToolResult &p_result) {
	if (!recording) {
		return;
	}
	for (const Candidate &candidate : candidates) {
		Vector<uint8_t> now;
		const bool exists = _read(candidate.path, now);
		if (exists == candidate.existed && now == candidate.content) {
			continue;
		}
		FileState file;
		file.path = candidate.path;
		file.reason = candidate.reason;
		file.existed = candidate.existed;
		file.before = candidate.content;
		if (file.reason == REASON_CREATED && file.existed) {
			file.reason = REASON_WRITTEN;
		}
		files.push_back(file);
	}
	candidates.clear();
}

void EretheaAssistantUndo::end_answer() {
	recording = false;
	candidates.clear();
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	for (uint32_t i = 0; i < scenes.size();) {
		UndoRedo *history = undo_redo->get_history_undo_redo(scenes[i].history_id);
		if (!history || history->get_version() == scenes[i].version_before) {
			// Nothing changed in it.
			scenes.remove_at(i);
			continue;
		}
		scenes[i].version_after = history->get_version();
		i++;
	}
	for (FileState &file : files) {
		Vector<uint8_t> now;
		file.exists_after = _read(file.path, now);
		file.hash_after = _hash(now);
	}
	finished = !files.is_empty() || !scenes.is_empty();
	if (!finished) {
		files = previous_files;
		scenes = previous_scenes;
		finished = previous_finished;
	}
	previous_files.clear();
	previous_scenes.clear();
	previous_finished = false;
}

bool EretheaAssistantUndo::can_undo() const {
	return finished && !recording;
}

static int _scene_index_of_history(int p_history_id) {
	EditorData &data = EditorNode::get_editor_data();
	for (int i = 0; i < data.get_edited_scene_count(); i++) {
		if (data.get_scene_history_id(i) == p_history_id) {
			return i;
		}
	}
	return -1;
}

bool EretheaAssistantUndo::_check_unchanged(String &r_reason) const {
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	for (const SceneState &scene : scenes) {
		const String name = scene.scene_path.is_empty() ? String("an unsaved scene") : scene.scene_path;
		if (_scene_index_of_history(scene.history_id) < 0) {
			r_reason = vformat("%s was closed after the answer", name);
			return false;
		}
		UndoRedo *history = undo_redo->get_history_undo_redo(scene.history_id);
		if (!history || history->get_version() != scene.version_after) {
			r_reason = vformat("%s was changed after the answer", name);
			return false;
		}
	}
	for (const FileState &file : files) {
		Vector<uint8_t> now;
		const bool exists = _read(file.path, now);
		if (exists != file.exists_after || _hash(now) != file.hash_after) {
			r_reason = vformat("%s was changed after the answer", file.path);
			return false;
		}
	}
	return true;
}

void EretheaAssistantUndo::_remove_file(const String &p_path) {
	DirAccess::remove_absolute(ProjectSettings::get_singleton()->globalize_path(p_path));
	// The editor gives scripts and other resources a .uid file next to them.
	const String uid_file = p_path + ".uid";
	if (FileAccess::exists(uid_file)) {
		DirAccess::remove_absolute(ProjectSettings::get_singleton()->globalize_path(uid_file));
	}
}

void EretheaAssistantUndo::_reload_script(const String &p_path) {
	if (ResourceCache::has(p_path) && FileAccess::exists(p_path)) {
		Ref<Script> script = ResourceCache::get_ref(p_path);
		if (script.is_valid()) {
			script->set_source_code(FileAccess::get_file_as_string(p_path));
			script->reload(true);
		}
	}
}

bool EretheaAssistantUndo::undo(String &r_message) {
	if (!can_undo()) {
		r_message = "There's no answer to undo.";
		return false;
	}
	String reason;
	if (!_check_unchanged(reason)) {
		r_message = vformat("Not undone, because %s. Undo it in the editor (Ctrl+Z) instead, or keep it.", reason);
		return false;
	}

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	int scene_changes = 0;
	for (const SceneState &scene : scenes) {
		UndoRedo *history = undo_redo->get_history_undo_redo(scene.history_id);
		// A limit, in case the history doesn't count as expected.
		for (int guard = 0; guard < 10000 && history && history->get_version() > scene.version_before && history->has_undo(); guard++) {
			if (!undo_redo->undo_history(scene.history_id)) {
				break;
			}
			scene_changes++;
		}
	}

	EditorNode *editor = EditorNode::get_singleton();
	EditorFileSystem *file_system = EditorFileSystem::get_singleton();
	int restored = 0;
	bool scripts_changed = false;
	for (int i = int(files.size()) - 1; i >= 0; i--) {
		const FileState &file = files[i];
		const bool open = editor->is_scene_open(file.path);
		switch (file.reason) {
			case REASON_SAVED: {
				if (open && _find_scene(file.path)) {
					// Its history went back: the file follows.
					editor->save_scene_if_open(file.path);
					restored++;
				} else if (!file.existed && !open) {
					_remove_file(file.path);
					restored++;
				}
				// Otherwise the save kept the person's own changes.
			} break;

			case REASON_REBUILT: {
				if (file.existed) {
					_write(file.path, file.before);
				} else {
					_remove_file(file.path);
				}
				if (open && file.existed) {
					editor->reload_scene(file.path);
				}
				restored++;
			} break;

			case REASON_WRITTEN: {
				if (file.existed) {
					_write(file.path, file.before);
					_reload_script(file.path);
				} else {
					_remove_file(file.path);
				}
				scripts_changed = true;
				restored++;
			} break;

			case REASON_CREATED: {
				if (open) {
					EditorInterface::get_singleton()->open_scene_from_path(file.path);
					if (editor->get_edited_scene() && editor->get_edited_scene()->get_scene_file_path() == file.path) {
						editor->close_scene();
					}
				}
				_remove_file(file.path);
				restored++;
			} break;
		}
		if (file_system) {
			file_system->update_file(file.path);
		}
	}
	if (scripts_changed && ScriptEditor::get_singleton()) {
		ScriptEditor::get_singleton()->reload_scripts();
	}
	clear();

	PackedStringArray parts;
	if (scene_changes > 0) {
		parts.push_back(vformat("%d scene change%s", scene_changes, scene_changes == 1 ? "" : "s"));
	}
	if (restored > 0) {
		parts.push_back(vformat("%d file%s", restored, restored == 1 ? "" : "s"));
	}
	r_message = parts.is_empty() ? String("Undid the last answer.") : vformat("Undid the last answer: %s.", String(" and ").join(parts));
	return true;
}

void EretheaAssistantUndo::clear() {
	candidates.clear();
	files.clear();
	scenes.clear();
	previous_files.clear();
	previous_scenes.clear();
	recording = false;
	finished = false;
	previous_finished = false;
}
