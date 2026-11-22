/**************************************************************************/
/*  erethea_mcp_file_tools.cpp                                            */
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

#include "erethea_mcp_file_tools.h"

#include "erethea_mcp_scene_tools.h"

#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/object/script_language.h"
#include "editor/file_system/editor_file_system.h"

namespace EretheaMcpFileTools {

static constexpr int DEFAULT_MAX_LINES = 400;
static constexpr int MAX_LINES = 2000;
static constexpr int MAX_READ_SIZE = 1024 * 1024;
static constexpr int MAX_LISTED_FILES = 500;

static const char *TEXT_EXTENSIONS[] = {
	"gd", "gdshader", "gdshaderinc", "cs", "glsl", "tscn", "tres", "godot", "json", "jsonl", "cfg", "ini",
	"txt", "md", "csv", "xml", "yml", "yaml", "toml", "import"
};
static const char *NOT_EDITABLE_EXTENSIONS[] = { "tscn", "tres", "godot", "import" };

static EretheaMcpToolResult _error(const String &p_message) {
	return EretheaMcpToolResult::error(p_message);
}

static int _int(const Dictionary &p_arguments, const String &p_name, int p_default) {
	const Variant value = p_arguments.get(p_name, Variant());
	return value.get_type() == Variant::INT || value.get_type() == Variant::FLOAT ? int(value) : p_default;
}

bool is_text_file(const String &p_path) {
	const String extension = p_path.get_extension().to_lower();
	for (const char *text_extension : TEXT_EXTENSIONS) {
		if (extension == text_extension) {
			return true;
		}
	}
	return false;
}

bool is_editable_text_file(const String &p_path) {
	if (!is_text_file(p_path)) {
		return false;
	}
	const String extension = p_path.get_extension().to_lower();
	for (const char *not_editable : NOT_EDITABLE_EXTENSIONS) {
		if (extension == not_editable) {
			return false;
		}
	}
	return true;
}

int replace_text(const String &p_text, const String &p_old, const String &p_new, bool p_all, String &r_text) {
	if (p_old.is_empty()) {
		return 0;
	}
	const int count = p_text.count(p_old);
	if (count == 1 || (p_all && count > 1)) {
		r_text = p_text.replace(p_old, p_new);
	}
	return count;
}

static bool _read_text(const String &p_path, String &r_text, String &r_error) {
	Error error = OK;
	Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::READ, &error);
	if (file.is_null()) {
		r_error = vformat("%s couldn't be read (%s).", p_path, error_names[error]);
		return false;
	}
	if (file->get_length() > MAX_READ_SIZE) {
		r_error = vformat("%s is too big to read here (%d KB).", p_path, file->get_length() / 1024);
		return false;
	}
	r_text = file->get_as_utf8_string();
	return true;
}

EretheaMcpToolResult read_file(const Dictionary &p_arguments) {
	String path;
	String error;
	if (!EretheaMcpSceneTools::project_path(p_arguments.get("path", Variant()), path, error)) {
		return _error(error);
	}
	if (!FileAccess::exists(path)) {
		return _error(vformat("%s doesn't exist. list_files shows the project's files.", path));
	}
	if (!is_text_file(path)) {
		return _error(vformat("%s isn't a text file. Scenes, scripts, JSON and other text files can be read.", path));
	}
	String text;
	if (!_read_text(path, text, error)) {
		return _error(error);
	}

	const Vector<String> lines = text.split("\n");
	const int total = lines.size();
	const int from = CLAMP(_int(p_arguments, "from_line", 1), 1, MAX(total, 1));
	const int count = CLAMP(_int(p_arguments, "max_lines", DEFAULT_MAX_LINES), 1, MAX_LINES);
	const int to = MIN(from + count - 1, total);
	String part;
	for (int i = from - 1; i < to; i++) {
		part += lines[i];
		if (i < to - 1) {
			part += "\n";
		}
	}

	Dictionary result;
	result["path"] = path;
	result["lines"] = total;
	result["from_line"] = from;
	result["to_line"] = to;
	result["text"] = part;
	if (to < total) {
		result["more"] = vformat("Lines %d to %d of %d. Pass from_line %d for the next ones.", from, to, total, to + 1);
	}
	return EretheaMcpToolResult::object(result);
}

static void _list(const String &p_folder, const String &p_pattern, bool p_recursive, Array &r_files, Array &r_folders, bool &r_truncated) {
	Ref<DirAccess> dir = DirAccess::open(p_folder);
	if (dir.is_null()) {
		return;
	}
	// Folders that the editor doesn't show either.
	if (dir->file_exists(".gdignore")) {
		return;
	}
	PackedStringArray files = dir->get_files();
	files.sort();
	EditorFileSystem *file_system = EditorFileSystem::get_singleton();
	for (const String &file : files) {
		if (file.begins_with(".") || file.ends_with(".import") || file.ends_with(".uid") || !file.matchn(p_pattern)) {
			continue;
		}
		if (r_files.size() >= MAX_LISTED_FILES) {
			r_truncated = true;
			return;
		}
		const String path = p_folder.path_join(file);
		Dictionary entry;
		entry["path"] = path;
		const String type = file_system ? file_system->get_file_type(path) : String();
		if (!type.is_empty()) {
			entry["type"] = type;
		}
		r_files.push_back(entry);
	}
	PackedStringArray folders = dir->get_directories();
	folders.sort();
	for (const String &folder : folders) {
		if (folder.begins_with(".")) {
			continue;
		}
		const String path = p_folder.path_join(folder);
		if (p_recursive) {
			_list(path, p_pattern, true, r_files, r_folders, r_truncated);
			if (r_truncated) {
				return;
			}
		} else {
			r_folders.push_back(path);
		}
	}
}

EretheaMcpToolResult list_files(const Dictionary &p_arguments) {
	String folder = "res://";
	String error;
	const Variant folder_argument = p_arguments.get("folder", Variant());
	if (folder_argument.get_type() == Variant::STRING && !String(folder_argument).strip_edges().is_empty() && String(folder_argument).strip_edges() != "res://") {
		if (!EretheaMcpSceneTools::project_path(folder_argument, folder, error)) {
			return _error(error);
		}
	}
	if (!DirAccess::dir_exists_absolute(folder)) {
		return _error(vformat("The folder %s doesn't exist.", folder));
	}
	const Variant pattern_argument = p_arguments.get("pattern", Variant());
	const String pattern = pattern_argument.get_type() == Variant::STRING && !String(pattern_argument).is_empty() ? String(pattern_argument) : String("*");
	const bool recursive = p_arguments.get("recursive", true);

	Array files;
	Array folders;
	bool truncated = false;
	_list(folder, pattern, recursive, files, folders, truncated);

	Dictionary result;
	result["folder"] = folder;
	result["files"] = files;
	if (!recursive) {
		result["folders"] = folders;
	}
	if (truncated) {
		result["truncated"] = vformat("Only the first %d files. Narrow the folder or the pattern.", MAX_LISTED_FILES);
	}
	return EretheaMcpToolResult::object(result);
}

static ScriptLanguage *_language_for(const String &p_path) {
	const String extension = p_path.get_extension().to_lower();
	for (int i = 0; i < ScriptServer::get_language_count(); i++) {
		ScriptLanguage *language = ScriptServer::get_language(i);
		if (language->get_extension() == extension) {
			return language;
		}
	}
	return nullptr;
}

EretheaMcpToolResult edit_file(const Dictionary &p_arguments) {
	String path;
	String error;
	if (!EretheaMcpSceneTools::project_path(p_arguments.get("path", Variant()), path, error)) {
		return _error(error);
	}
	if (!is_editable_text_file(path)) {
		if (is_text_file(path)) {
			return _error(vformat("%s is kept by the editor: change scenes with the scene tools (open_scene, set_node_properties, save_scene).", path));
		}
		return _error(vformat("%s isn't a text file that can be edited here.", path));
	}
	const Variant old_variant = p_arguments.get("old_text", Variant());
	const Variant new_variant = p_arguments.get("new_text", Variant());
	if (new_variant.get_type() != Variant::STRING) {
		return _error("Missing \"new_text\".");
	}
	const String old_text = old_variant.get_type() == Variant::STRING ? String(old_variant) : String();
	const String new_text = new_variant;
	const bool all = p_arguments.get("replace_all", false);

	String source;
	int replacements = 0;
	if (!FileAccess::exists(path)) {
		if (!old_text.is_empty()) {
			return _error(vformat("%s doesn't exist. To create it, leave old_text empty.", path));
		}
		source = new_text;
		replacements = 1;
	} else {
		String text;
		if (!_read_text(path, text, error)) {
			return _error(error);
		}
		if (old_text.is_empty()) {
			return _error("\"old_text\" must be text from the file, to replace with new_text. read_file shows the file.");
		}
		replacements = replace_text(text, old_text, new_text, all, source);
		if (replacements == 0) {
			return _error(vformat("old_text isn't in %s. Copy it exactly from read_file, with the same spaces and tabs.", path));
		}
		if (replacements > 1 && !all) {
			return _error(vformat("old_text is in %s %d times. Include more lines around it, or pass replace_all.", path, replacements));
		}
	}

	if (_language_for(path)) {
		// Checked, written and reloaded like any script.
		EretheaMcpToolResult result = EretheaMcpSceneTools::write_script(Dictionary{ { "path", path }, { "source", source } });
		if (!result.structured.is_empty()) {
			Dictionary structured = result.structured;
			structured["replacements"] = replacements;
			const bool is_error = result.is_error;
			result = EretheaMcpToolResult::object(structured);
			result.is_error = is_error;
		}
		return result;
	}

	Error write_error = DirAccess::make_dir_recursive_absolute(ProjectSettings::get_singleton()->globalize_path(path.get_base_dir()));
	Ref<FileAccess> file;
	if (write_error == OK) {
		file = FileAccess::open(path, FileAccess::WRITE, &write_error);
	}
	if (file.is_null()) {
		return _error(vformat("%s couldn't be written (%s).", path, error_names[write_error]));
	}
	file->store_string(source);
	file.unref();
	EditorFileSystem::get_singleton()->update_file(path);

	Dictionary result;
	result["path"] = path;
	result["replacements"] = replacements;
	return EretheaMcpToolResult::object(result);
}

} // namespace EretheaMcpFileTools
