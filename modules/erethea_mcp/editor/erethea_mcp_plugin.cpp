/**************************************************************************/
/*  erethea_mcp_plugin.cpp                                                */
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

#include "erethea_mcp_plugin.h"

#include "../erethea_mcp_values.h"
#include "erethea_mcp_file_tools.h"
#include "erethea_mcp_scene_tools.h"

#include "core/config/project_settings.h"
#include "core/crypto/crypto_core.h"
#include "core/debugger/debugger_marshalls.h"
#include "core/debugger/remote_debugger.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/image.h"
#include "core/io/json.h"
#include "core/object/callable_mp.h"
#include "core/os/os.h"
#include "core/os/time.h"
#include "core/version.h"
#include "editor/debugger/editor_debugger_node.h"
#include "editor/debugger/script_editor_debugger.h"
#include "editor/editor_interface.h"
#include "editor/editor_node.h"
#include "editor/file_system/editor_file_system.h"
#include "editor/run/editor_run_bar.h"
#include "editor/settings/editor_settings.h"
#include "scene/main/viewport.h"
#include "servers/display/display_server.h"

#include "modules/modules_enabled.gen.h" // For erethea_export_checks, erethea_fornalha and erethea_agent_test.
#ifdef MODULE_ERETHEA_EXPORT_CHECKS_ENABLED
#include "editor/export/editor_export.h"

#include "modules/erethea_export_checks/erethea_export_checks.h"
#endif
#ifdef MODULE_ERETHEA_FORNALHA_ENABLED
#include "modules/erethea_fornalha/erethea_fornalha_import.h"
#endif

static const char *INSTRUCTIONS = "Erethea Engine editor (a fork of Godot 4). These tools act on the project open in this editor. "
								  "Usual flow: get_editor_state, then open_scene or create_scene, get_scene_tree, create_node and set_node_properties, "
								  "write_script and attach_script, save_scene, run_game, get_errors and take_screenshot, stop_game; check_export before exporting. "
								  "list_files, read_file and edit_file read the project's files and change scripts and other text files. "
								  "In projects made with Fornalha, build_fornalha_scenes builds the game's scenes from game.project.json. "
								  "Node paths are relative to the root of the scene being edited, which is \".\". "
#ifdef MODULE_ERETHEA_AGENT_TEST_ENABLED
								  "run_test plays the game for some seconds while pressing its controls, and reports its errors, frame rate and how far the player went. "
#endif
								  "Scene changes are undoable in the editor (Ctrl+Z), and the person can see them as you make them.";

struct EretheaMcpToolSpec {
	const char *name;
	const char *title;
	bool read_only;
	bool destructive;
	const char *description;
	const char *schema;
};

static const EretheaMcpToolSpec TOOL_SPECS[] = {
	{ "get_editor_state", "Editor state", true, false,
			"Shows what the Erethea editor has open: the project, the open scenes, the scene being edited (the one the other tools act on), and whether the game is running.",
			R"({"type": "object", "properties": {}})" },
	{ "get_scene_tree", "Scene tree", true, false,
			"Lists the nodes of the scene being edited: name, type, path, script and instanced scene. Paths are relative to the scene's root, which is \".\".",
			R"({"type": "object", "properties": {"max_depth": {"type": "integer", "minimum": 1, "description": "How many levels to list (default 12)."}}})" },
	{ "get_node_properties", "Node properties", true, false,
			"Reads a node's properties: the ones you name, or all that the Inspector shows. Vectors, colors and transforms come as Godot literals such as \"Vector3(0, 1, 0)\", which set_node_properties accepts back; resources come as their path, or as {\"type\": ...} when they live in the scene.",
			R"({"type": "object", "properties": {"node": {"type": "string", "description": "Node path from get_scene_tree; \".\" is the root."}, "properties": {"type": "array", "items": {"type": "string"}, "description": "Property names, such as [\"position\", \"mesh\"]. Leave out for all."}}, "required": ["node"]})" },
	{ "list_files", "List files", true, false,
			"Lists the project's files, with their resource type, such as scenes, scripts, models and game.project.json. Hidden folders and the editor's own files are left out.",
			R"({"type": "object", "properties": {"folder": {"type": "string", "description": "Folder to list (default \"res://\")."}, "pattern": {"type": "string", "description": "Only file names that match, such as \"*.gd\" or \"boss*\" (default all)."}, "recursive": {"type": "boolean", "description": "Also list the subfolders' files (default true)."}}})" },
	{ "read_file", "Read file", true, false,
			"Reads a text file of the project, such as a script, a scene (.tscn), game.project.json or a shader, a part at a time for long files.",
			R"({"type": "object", "properties": {"path": {"type": "string", "description": "Such as \"res://scripts/enemy.gd\"."}, "from_line": {"type": "integer", "minimum": 1, "description": "First line (default 1)."}, "max_lines": {"type": "integer", "minimum": 1, "maximum": 2000, "description": "How many lines (default 400)."}}, "required": ["path"]})" },
	{ "open_scene", "Open scene", false, false,
			"Opens a scene file (.tscn or .scn) in the editor, or switches to it if it's open, and makes it the scene being edited.",
			R"({"type": "object", "properties": {"path": {"type": "string", "description": "Such as \"res://levels/forest.tscn\"."}}, "required": ["path"]})" },
	{ "create_scene", "Create scene", false, false,
			"Creates a scene file with a single root node, saves it and opens it. Fails if the file exists.",
			R"({"type": "object", "properties": {"path": {"type": "string", "description": "The new file, such as \"res://levels/forest.tscn\"."}, "root_type": {"type": "string", "description": "Class of the root node (default \"Node3D\"), or a script's class_name."}, "root_name": {"type": "string", "description": "Name of the root node (default: from the file name)."}}, "required": ["path"]})" },
	{ "create_node", "Create node", false, false,
			"Adds a node to the scene being edited: a new node of a class (type, such as \"MeshInstance3D\") or an instance of a scene (scene, such as \"res://characters/player.tscn\"), and can set its properties at once. One undoable action.",
			R"({"type": "object", "properties": {"parent": {"type": "string", "description": "Path of the parent (default \".\", the root)."}, "type": {"type": "string", "description": "Node class or script class_name."}, "scene": {"type": "string", "description": "Scene to instance, instead of a type."}, "name": {"type": "string", "description": "Node name, made unique among its siblings."}, "properties": {"type": "object", "description": "Properties to set, as in set_node_properties."}}})" },
	{ "set_node_properties", "Set node properties", false, false,
			"Sets properties of a node in the scene being edited, in one undoable action, and returns the new values. Values: numbers, booleans and strings; vectors as [x, y, z] or \"Vector3(x, y, z)\"; colors as \"#ff8800\", [r, g, b, a] or \"Color(r, g, b, a)\"; enums as a number or a name; a resource as a path (\"res://materials/grass.tres\"), or a new one as {\"type\": \"BoxMesh\", \"properties\": {\"size\": [2, 1, 1]}}; other types as Godot literals, such as \"Transform3D(...)\".",
			R"({"type": "object", "properties": {"node": {"type": "string", "description": "Node path; \".\" is the root."}, "properties": {"type": "object", "description": "Property names and values, such as {\"position\": [0, 1, 0], \"visible\": true}."}}, "required": ["node", "properties"]})" },
	{ "delete_node", "Delete node", false, true,
			"Removes a node and its children from the scene being edited. Undoable.",
			R"({"type": "object", "properties": {"node": {"type": "string", "description": "Node path."}}, "required": ["node"]})" },
	{ "write_script", "Write script", false, true,
			"Writes a script file (GDScript .gd, or another script language of the project), creating or replacing it, and checks it: the result lists errors and warnings with their line. The file is written even with errors, so that another write_script can fix them.",
			R"({"type": "object", "properties": {"path": {"type": "string", "description": "Such as \"res://scripts/enemy.gd\"."}, "source": {"type": "string", "description": "The whole code of the script."}}, "required": ["path", "source"]})" },
	{ "edit_file", "Edit file", false, true,
			"Changes part of a script or another text file (such as game.project.json): replaces old_text, copied exactly from read_file, with new_text. Scripts are checked as with write_script. With an empty old_text, creates a file that doesn't exist. Scenes and resources are changed with the scene tools instead.",
			R"({"type": "object", "properties": {"path": {"type": "string", "description": "Such as \"res://scripts/enemy.gd\"."}, "old_text": {"type": "string", "description": "Text to replace; it must be in the file exactly once, unless replace_all."}, "new_text": {"type": "string", "description": "The text to put in its place."}, "replace_all": {"type": "boolean", "description": "Replace every occurrence of old_text (default false)."}}, "required": ["path", "new_text"]})" },
	{ "attach_script", "Attach script", false, false,
			"Attaches a script file to a node of the scene being edited, replacing its script. Undoable.",
			R"({"type": "object", "properties": {"node": {"type": "string", "description": "Node path."}, "path": {"type": "string", "description": "Script file, such as \"res://scripts/enemy.gd\"."}}, "required": ["node", "path"]})" },
	{ "save_scene", "Save scene", false, false,
			"Saves the scene being edited to its file, or to another file with path.",
			R"({"type": "object", "properties": {"path": {"type": "string", "description": "Save as this file instead (.tscn or .scn)."}}})" },
	{ "run_game", "Run game", false, false,
			"Runs the game from the editor, as its Run buttons do, and returns at once. Then get_errors reads what it prints, take_screenshot shows it and stop_game stops it. The editor saves the open scenes first, as its settings ask by default.",
			R"({"type": "object", "properties": {"scene": {"type": "string", "description": "\"main\" for the project's main scene (default), \"current\" for the scene being edited, or a scene path."}, "headless": {"type": "boolean", "description": "Without a window and without drawing, for tests. Screenshots of the game then fail."}, "arguments": {"type": "array", "items": {"type": "string"}, "description": "More command line arguments for the game."}}})" },
	{ "stop_game", "Stop game", false, false,
			"Stops the game started from the editor.",
			R"({"type": "object", "properties": {}})" },
	{ "get_errors", "Errors and output", true, false,
			"Reads the errors and warnings of the editor and of the game run from it, oldest first, with their file and line when known. Pass the returned next as since to get only newer ones. With include_output, also returns what the game prints.",
			R"({"type": "object", "properties": {"since": {"type": "integer", "description": "Only entries after this id, such as the next of an earlier call."}, "include_output": {"type": "boolean", "description": "Also the game's prints (default false)."}, "limit": {"type": "integer", "minimum": 1, "description": "At most this many entries, the newest (default 100)."}}})" },
	{ "take_screenshot", "Screenshot", true, false,
			"Takes a screenshot as a PNG image: of the running game (target \"game\", the default while it runs), of the whole editor window (\"editor\"), or of the editor's 3D or 2D view (\"3d\", \"2d\", which switch the editor to that view).",
			R"({"type": "object", "properties": {"target": {"type": "string", "enum": ["game", "editor", "3d", "2d"]}, "max_size": {"type": "integer", "minimum": 64, "maximum": 4096, "description": "Largest side in pixels (default 1280)."}}})" },
	{ "rescan_files", "Rescan files", false, false,
			"Makes the editor look for files added or changed outside it, such as assets from Fornalha, and import them.",
			R"({"type": "object", "properties": {}})" },
#ifdef MODULE_ERETHEA_AGENT_TEST_ENABLED
	{ "run_test", "Test the game", false, false,
			"Plays the game in test mode for some seconds and waits for its report: errors and warnings (with the script line), frame rate (average, minimum, 1% low), drawing numbers, and the player (the first node in the \"player\" group): where it started and ended and whether it fell off the world. While it plays, it presses the controls given in inputs, or, without them, each input action of the project in turn (in projects made with Fornalha, the Fornalha's walk: forward, jump, turn, attack). Saved files are tested, so the editor saves the open scenes first, as for run_game. In a window, it also takes screenshots and returns the last one.",
			R"({"type": "object", "properties": {"scene": {"type": "string", "description": "\"main\" for the project's main scene (default), \"current\" for the scene being edited, or a scene path."}, "seconds": {"type": "number", "minimum": 0.5, "maximum": 40, "description": "How long the game plays (default 10)."}, "headless": {"type": "boolean", "description": "Without a window (default true). In a window, screenshots and drawing numbers too."}, "inputs": {"type": "array", "description": "Controls to press, in order: objects with at (second), for (seconds held, default 0.1) and one of action (an input action, with optional strength 0-1), key (such as \"W\" or \"Space\"), mouse_button (\"left\", \"right\", \"middle\") or mouse_motion ([x, y] pixels over the time held).", "items": {"type": "object"}}, "screenshots": {"type": "array", "items": {"type": "number"}, "description": "Seconds at which to take screenshots, in a window."}}})" },
#endif
#ifdef MODULE_ERETHEA_EXPORT_CHECKS_ENABLED
	{ "check_export", "Check for export", true, false,
			"Checks the project before export: models with too many triangles, textures that are too big, models with the wrong scale (such as centimeters instead of meters), and, in projects made with Fornalha, assets without a license receipt or made with a model that can't be sold. Errors are what shouldn't ship; warnings are worth fixing.",
			R"({"type": "object", "properties": {"preset": {"type": "string", "description": "Name of an export preset, to check only what it exports (default: every file of the project)."}}})" },
#endif
#ifdef MODULE_ERETHEA_FORNALHA_ENABLED
	{ "build_fornalha_scenes", "Build scenes from game.project.json", false, false,
			"In a project made with Fornalha, builds the game's scenes from game.project.json and the assets: one per character (the player with its camera, enemies with their AI), one per area of the blockout (World/areas/, with its enemies), the world and res://principal.tscn. A scene changed since it was built, or made elsewhere, is kept unless force is true; then a copy of the old one goes to res://.erethea/backup/. Call rescan_files first if Fornalha just added assets.",
			R"({"type": "object", "properties": {"force": {"type": "boolean", "description": "Also replace the scenes that were kept (default false)."}}})" },
#endif
};

Vector<EretheaMcpTool> EretheaMcpPlugin::get_tools() const {
	Vector<EretheaMcpTool> tools;
	for (const EretheaMcpToolSpec &spec : TOOL_SPECS) {
		EretheaMcpTool tool;
		tool.name = spec.name;
		tool.title = spec.title;
		tool.description = spec.description;
		tool.read_only = spec.read_only;
		tool.destructive = spec.destructive;
		tool.input_schema = JSON::parse_string(spec.schema);
		tools.push_back(tool);
	}
	return tools;
}

bool EretheaMcpPlugin::call_tool(const String &p_name, const Dictionary &p_arguments, bool p_can_wait, EretheaMcpToolResult &r_result, uint64_t &r_pending_id) {
	print_verbose(vformat("Erethea MCP: %s", p_name));
	if (p_name == "take_screenshot") {
		return _take_screenshot(p_arguments, p_can_wait, r_result, r_pending_id);
	}
#ifdef MODULE_ERETHEA_AGENT_TEST_ENABLED
	if (p_name == "run_test") {
		return _run_test(p_arguments, p_can_wait, r_result, r_pending_id);
	}
#endif
	if (p_name == "get_editor_state") {
		r_result = _get_editor_state();
	} else if (p_name == "get_scene_tree") {
		r_result = EretheaMcpSceneTools::get_scene_tree(p_arguments);
	} else if (p_name == "get_node_properties") {
		r_result = EretheaMcpSceneTools::get_node_properties(p_arguments);
	} else if (p_name == "open_scene") {
		r_result = EretheaMcpSceneTools::open_scene(p_arguments);
	} else if (p_name == "create_scene") {
		r_result = EretheaMcpSceneTools::create_scene(p_arguments);
	} else if (p_name == "create_node") {
		r_result = EretheaMcpSceneTools::create_node(p_arguments);
	} else if (p_name == "set_node_properties") {
		r_result = EretheaMcpSceneTools::set_node_properties(p_arguments);
	} else if (p_name == "delete_node") {
		r_result = EretheaMcpSceneTools::delete_node(p_arguments);
	} else if (p_name == "write_script") {
		r_result = EretheaMcpSceneTools::write_script(p_arguments);
	} else if (p_name == "edit_file") {
		r_result = EretheaMcpFileTools::edit_file(p_arguments);
	} else if (p_name == "read_file") {
		r_result = EretheaMcpFileTools::read_file(p_arguments);
	} else if (p_name == "list_files") {
		r_result = EretheaMcpFileTools::list_files(p_arguments);
	} else if (p_name == "attach_script") {
		r_result = EretheaMcpSceneTools::attach_script(p_arguments);
	} else if (p_name == "save_scene") {
		r_result = EretheaMcpSceneTools::save_scene(p_arguments);
	} else if (p_name == "run_game") {
		r_result = _run_game(p_arguments);
	} else if (p_name == "stop_game") {
		r_result = _stop_game();
	} else if (p_name == "get_errors") {
		r_result = _get_errors(p_arguments);
	} else if (p_name == "rescan_files") {
		r_result = EretheaMcpSceneTools::rescan_files(p_arguments);
#ifdef MODULE_ERETHEA_EXPORT_CHECKS_ENABLED
	} else if (p_name == "check_export") {
		r_result = _check_export(p_arguments);
#endif
#ifdef MODULE_ERETHEA_FORNALHA_ENABLED
	} else if (p_name == "build_fornalha_scenes") {
		const EretheaFornalhaImport::Report report = EretheaFornalhaImport::build(p_arguments.get("force", false));
		r_result = report.error.is_empty() ? EretheaMcpToolResult::object(report.to_dictionary()) : EretheaMcpToolResult::error(report.error);
#endif
	} else {
		r_result = EretheaMcpToolResult::error(vformat("Unknown tool: %s", p_name));
	}
	return true;
}

void EretheaMcpPlugin::_error_handler(void *p_self, const char *p_function, const char *p_file, int p_line, const char *p_error, const char *p_message, bool p_editor_notify, ErrorHandlerType p_type) {
	// An error while logging an error must not come back here.
	static thread_local bool inside = false;
	if (inside) {
		return;
	}
	inside = true;
	EretheaMcpPlugin *self = static_cast<EretheaMcpPlugin *>(p_self);
	const char *message = (p_message && p_message[0]) ? p_message : p_error;
	self->_add_log("editor", p_type == ERR_HANDLER_WARNING ? "warning" : "error", String::utf8(message), String::utf8(p_file), p_line, String::utf8(p_function));
	inside = false;
}

void EretheaMcpPlugin::_add_log(const String &p_source, const String &p_level, const String &p_message, const String &p_file, int p_line, const String &p_function) {
	MutexLock lock(log_mutex);
	LogEntry entry;
	entry.id = ++last_log_id;
	entry.source = p_source;
	entry.level = p_level;
	entry.message = p_message;
	entry.file = p_file;
	entry.line = p_line;
	entry.function = p_function;
	log.push_back(entry);
	while (log.size() > MAX_LOG_ENTRIES) {
		log.pop_front();
	}
}

static LocalVector<ScriptEditorDebugger *> _find_debuggers() {
	LocalVector<ScriptEditorDebugger *> debuggers;
	EditorDebuggerNode *debugger_node = EditorDebuggerNode::get_singleton();
	if (!debugger_node) {
		return debuggers;
	}
	// One per game instance.
	const TypedArray<Node> found = debugger_node->find_children("*", "ScriptEditorDebugger", true, false);
	for (const Variant &node : found) {
		ScriptEditorDebugger *debugger = Object::cast_to<ScriptEditorDebugger>(node);
		if (debugger) {
			debuggers.push_back(debugger);
		}
	}
	return debuggers;
}

void EretheaMcpPlugin::_connect_debuggers() {
	const Callable callable = callable_mp(this, &EretheaMcpPlugin::_debug_data);
	for (ScriptEditorDebugger *debugger : _find_debuggers()) {
		if (!debugger->is_connected(SNAME("debug_data"), callable)) {
			debugger->connect(SNAME("debug_data"), callable);
		}
	}
}

void EretheaMcpPlugin::_debug_data(const String &p_message, const Array &p_data) {
	if (p_message == "error") {
		DebuggerMarshalls::OutputError error;
		if (!error.deserialize(p_data)) {
			return;
		}
		String file = error.source_file;
		int line = error.source_line;
		// Script errors point at the script rather than at the engine's code.
		if (!file.begins_with("res://") && !error.callstack.is_empty()) {
			file = error.callstack[0].file;
			line = error.callstack[0].line;
		}
		_add_log("game", error.warning ? "warning" : "error", error.error_descr.is_empty() ? error.error : error.error_descr, file, line, error.source_func);
	} else if (p_message == "output") {
		if (p_data.size() != 2) {
			return;
		}
		const PackedStringArray strings = p_data[0];
		const PackedInt32Array types = p_data[1];
		for (int i = 0; i < MIN(strings.size(), types.size()); i++) {
			_add_log("game", types[i] == RemoteDebugger::MESSAGE_TYPE_ERROR ? "error" : "output", strings[i]);
		}
	} else if (p_message == "game_view:get_screenshot") {
		// The answer to "scene:rq_screenshot": [id, width, height, PNG path].
		if (p_data.size() != 4) {
			return;
		}
		const int64_t request_id = p_data[0];
		const String path = p_data[3];
		for (const PendingScreenshot &shot : screenshots) {
			if (shot.target == "game" && shot.request_id == request_id) {
				const Ref<Image> image = Image::load_from_file(path);
				DirAccess::remove_absolute(path);
				_finish_screenshot(shot.call_id, image, image.is_valid() ? String() : "The game's screenshot couldn't be read.");
				return;
			}
		}
	}
}

void EretheaMcpPlugin::_finish_screenshot(uint64_t p_call_id, const Ref<Image> &p_image, const String &p_failure) {
	String target;
	int max_size = 0;
	for (uint32_t i = 0; i < screenshots.size(); i++) {
		if (screenshots[i].call_id == p_call_id) {
			target = screenshots[i].target;
			max_size = screenshots[i].max_size;
			screenshots.remove_at(i);
			break;
		}
	}
	if (!p_failure.is_empty() || p_image.is_null() || p_image->is_empty()) {
		server.finish_call(p_call_id, EretheaMcpToolResult::error(p_failure.is_empty() ? String("Nothing was drawn to capture.") : p_failure));
		return;
	}

	Ref<Image> image = p_image;
	if (image->is_compressed()) {
		image->decompress();
	}
	image->clear_mipmaps();
	if (image->get_format() != Image::FORMAT_RGBA8 && image->get_format() != Image::FORMAT_RGB8) {
		image->convert(Image::FORMAT_RGBA8);
	}
	const int largest = MAX(image->get_width(), image->get_height());
	if (largest > max_size) {
		const double scale = double(max_size) / largest;
		image->resize(MAX(1, int(image->get_width() * scale)), MAX(1, int(image->get_height() * scale)), Image::INTERPOLATE_BILINEAR);
	}
	const Vector<uint8_t> png = image->save_png_to_buffer();

	const String what = target == "game" ? String("game") : target == "editor" ? String("editor window")
																			   : vformat("editor's %s view", target.to_upper());
	EretheaMcpToolResult result = EretheaMcpToolResult::text(vformat("Screenshot of the %s, %dx%d.", what, image->get_width(), image->get_height()));
	result.add_image(CryptoCore::b64_encode_str(png.ptr(), png.size()));
	server.finish_call(p_call_id, result);
}

void EretheaMcpPlugin::_process_screenshots() {
	if (screenshots.is_empty()) {
		return;
	}
	const uint64_t now = OS::get_singleton()->get_ticks_msec();
	const bool playing = EditorRunBar::get_singleton()->is_playing();
	// _finish_screenshot() changes the list.
	const LocalVector<PendingScreenshot> current(screenshots);
	for (const PendingScreenshot &shot : current) {
		if (shot.target == "game") {
			if (!playing) {
				_finish_screenshot(shot.call_id, Ref<Image>(), "The game stopped before sending its screenshot.");
			} else if (now > shot.deadline_msec) {
				_finish_screenshot(shot.call_id, Ref<Image>(), "The game didn't send a screenshot in time. A game run with headless draws nothing.");
			}
			continue;
		}
		bool ready = false;
		for (PendingScreenshot &live : screenshots) {
			if (live.call_id == shot.call_id) {
				live.frames_left--;
				ready = live.frames_left <= 0;
			}
		}
		if (!ready) {
			continue;
		}
		Viewport *viewport = nullptr;
		if (shot.target == "editor") {
			viewport = EditorNode::get_singleton()->get_viewport();
		} else if (shot.target == "3d") {
			viewport = EditorInterface::get_singleton()->get_editor_viewport_3d(0);
		} else {
			viewport = EditorInterface::get_singleton()->get_editor_viewport_2d();
		}
		Ref<Image> image;
		if (viewport && viewport->get_texture().is_valid()) {
			image = viewport->get_texture()->get_image();
		}
		_finish_screenshot(shot.call_id, image, String());
	}
}

bool EretheaMcpPlugin::_take_screenshot(const Dictionary &p_arguments, bool p_can_wait, EretheaMcpToolResult &r_result, uint64_t &r_pending_id) {
	const bool playing = EditorRunBar::get_singleton()->is_playing();
	const String target = p_arguments.get("target", playing ? "game" : "editor");
	if (target != "game" && target != "editor" && target != "3d" && target != "2d") {
		r_result = EretheaMcpToolResult::error("\"target\" must be \"game\", \"editor\", \"3d\" or \"2d\".");
		return true;
	}
	if (!p_can_wait) {
		r_result = EretheaMcpToolResult::error("take_screenshot can't run in a batch.");
		return true;
	}

	PendingScreenshot shot;
	shot.target = target;
	shot.max_size = CLAMP(int(p_arguments.get("max_size", 1280)), 64, 4096);
	shot.deadline_msec = OS::get_singleton()->get_ticks_msec() + SCREENSHOT_TIMEOUT_MSEC;

	if (target == "game") {
		if (!playing) {
			r_result = EretheaMcpToolResult::error("The game isn't running. Start it with run_game, or capture the editor with target \"editor\".");
			return true;
		}
		_connect_debuggers();
		ScriptEditorDebugger *session = nullptr;
		for (ScriptEditorDebugger *debugger : _find_debuggers()) {
			if (!session && debugger->is_session_active()) {
				session = debugger;
			}
		}
		if (!session) {
			r_result = EretheaMcpToolResult::error("The game hasn't connected to the editor yet. Try again in a moment.");
			return true;
		}
		// Far from the ids of the editor's own Game view, which share the message.
		shot.request_id = (int64_t(1) << 40) + (++last_screenshot_request);
		session->send_message("scene:rq_screenshot", Array{ shot.request_id });
	} else {
		if (DisplayServer::get_singleton()->get_name() == "headless") {
			r_result = EretheaMcpToolResult::error("The editor runs with --headless and draws nothing, so there's nothing to capture.");
			return true;
		}
		if (target == "3d") {
			EditorInterface::get_singleton()->set_main_screen_editor("3D");
		} else if (target == "2d") {
			EditorInterface::get_singleton()->set_main_screen_editor("2D");
		}
		shot.frames_left = SCREENSHOT_FRAMES;
	}

	shot.call_id = ++last_call_id;
	screenshots.push_back(shot);
	r_pending_id = shot.call_id;
	return false;
}

void EretheaMcpPlugin::_process_tests() {
	if (tests.is_empty()) {
		return;
	}
	const uint64_t now = OS::get_singleton()->get_ticks_msec();
	// _finish_test() changes the list.
	const LocalVector<PendingTest> current(tests);
	for (const PendingTest &test : current) {
		const bool running = OS::get_singleton()->is_process_running(test.pid);
		if (running && now <= test.deadline_msec) {
			continue;
		}
		if (running) {
			OS::get_singleton()->kill(test.pid);
		}
		_finish_test(test, running);
	}
}

void EretheaMcpPlugin::_finish_test(const PendingTest &p_test, bool p_timed_out) {
	for (uint32_t i = 0; i < tests.size(); i++) {
		if (tests[i].call_id == p_test.call_id) {
			tests.remove_at(i);
			break;
		}
	}
	const String text = FileAccess::get_file_as_string(p_test.folder.path_join("report.json"));
	Ref<JSON> json;
	json.instantiate();
	if (text.is_empty() || json->parse(text) != OK || json->get_data().get_type() != Variant::DICTIONARY) {
		const String why = p_timed_out ? String("It was still running at the time limit and was stopped.") : vformat("It exited with code %d before writing it; it may have crashed. get_errors or a run_game may show why.", OS::get_singleton()->get_process_exit_code(p_test.pid));
		server.finish_call(p_test.call_id, EretheaMcpToolResult::error("The game didn't write its test report. " + why));
		return;
	}
	Dictionary report = json->get_data();
	if (p_timed_out) {
		report["stopped_by_editor"] = true;
	}
	report["report_file"] = p_test.folder.path_join("report.json");
	EretheaMcpToolResult result = EretheaMcpToolResult::object(report);
	// The last screenshot, for the agent to see the game.
	const Array screenshot_files = report.get("screenshots", Array());
	if (!screenshot_files.is_empty()) {
		Ref<Image> image;
		image.instantiate();
		if (image->load(screenshot_files[screenshot_files.size() - 1]) == OK) {
			const int largest = MAX(image->get_width(), image->get_height());
			if (largest > 1280) {
				image->resize(image->get_width() * 1280 / largest, image->get_height() * 1280 / largest, Image::INTERPOLATE_LANCZOS);
			}
			const Vector<uint8_t> png = image->save_png_to_buffer();
			result.add_image(CryptoCore::b64_encode_str(png.ptr(), png.size()));
		}
	}
	server.finish_call(p_test.call_id, result);
}

bool EretheaMcpPlugin::_run_test(const Dictionary &p_arguments, bool p_can_wait, EretheaMcpToolResult &r_result, uint64_t &r_pending_id) {
	if (!p_can_wait) {
		r_result = EretheaMcpToolResult::error("run_test can't run in a batch.");
		return true;
	}
	if (!tests.is_empty()) {
		r_result = EretheaMcpToolResult::error("A test is already running. Wait for its report.");
		return true;
	}

	String scene_path;
	const String scene = p_arguments.get("scene", "main");
	if (scene == "main") {
		scene_path = GLOBAL_GET("application/run/main_scene");
		if (scene_path.is_empty()) {
			r_result = EretheaMcpToolResult::error("The project has no main scene. Pass scene \"current\" or a scene path, or set application/run/main_scene.");
			return true;
		}
	} else if (scene == "current") {
		Node *root = EditorNode::get_singleton()->get_edited_scene();
		if (!root || root->get_scene_file_path().is_empty()) {
			r_result = EretheaMcpToolResult::error("No saved scene is being edited. Save it with save_scene, or pass a scene path.");
			return true;
		}
		scene_path = root->get_scene_file_path();
	} else {
		String error;
		if (!EretheaMcpSceneTools::project_path(scene, scene_path, error)) {
			r_result = EretheaMcpToolResult::error(error);
			return true;
		}
		if (!FileAccess::exists(scene_path)) {
			r_result = EretheaMcpToolResult::error(vformat("%s doesn't exist.", scene_path));
			return true;
		}
	}

	const double seconds = CLAMP(double(p_arguments.get("seconds", 10.0)), 0.5, double(TEST_MAX_SECONDS));
	PendingTest test;
	test.folder = OS::get_singleton()->get_user_data_dir().path_join("erethea_agent_test").path_join(Time::get_singleton()->get_datetime_string_from_system().replace(":", "-").replace("T", "_"));
	DirAccess::make_dir_recursive_absolute(test.folder);
	DirAccess::remove_absolute(test.folder.path_join("report.json"));

	List<String> arguments;
	arguments.push_back("--path");
	arguments.push_back(ProjectSettings::get_singleton()->get_resource_path());
	arguments.push_back(scene_path);
	arguments.push_back("--audio-driver");
	arguments.push_back("Dummy");
	if (bool(p_arguments.get("headless", true))) {
		arguments.push_back("--headless");
	}
	arguments.push_back("--agent-test=" + test.folder);
	arguments.push_back("--agent-test-seconds=" + rtos(seconds));

	const Variant inputs = p_arguments.get("inputs", Variant());
	const Variant screenshot_times = p_arguments.get("screenshots", Variant());
	if (inputs.get_type() != Variant::NIL || screenshot_times.get_type() != Variant::NIL) {
		if ((inputs.get_type() != Variant::NIL && inputs.get_type() != Variant::ARRAY) || (screenshot_times.get_type() != Variant::NIL && screenshot_times.get_type() != Variant::ARRAY)) {
			r_result = EretheaMcpToolResult::error("inputs and screenshots must be lists.");
			return true;
		}
		Dictionary script;
		script["steps"] = inputs.get_type() == Variant::ARRAY ? inputs : Variant(Array());
		if (screenshot_times.get_type() == Variant::ARRAY) {
			script["screenshots"] = screenshot_times;
		}
		const String inputs_file = test.folder.path_join("inputs.json");
		Ref<FileAccess> file = FileAccess::open(inputs_file, FileAccess::WRITE);
		if (file.is_null()) {
			r_result = EretheaMcpToolResult::error(vformat("Couldn't write %s.", inputs_file));
			return true;
		}
		file->store_string(JSON::stringify(script));
		file.unref();
		arguments.push_back("--agent-test-inputs=" + inputs_file);
	}

	// The test plays the files, so they have to be saved, as the Run buttons do.
	if (EDITOR_GET("run/auto_save/save_before_running")) {
		EditorInterface::get_singleton()->save_all_scenes();
	}
	if (OS::get_singleton()->create_process(OS::get_singleton()->get_executable_path(), arguments, &test.pid) != OK) {
		r_result = EretheaMcpToolResult::error("The game didn't start.");
		return true;
	}
	// The server gives up on a call after a minute.
	test.deadline_msec = OS::get_singleton()->get_ticks_msec() + TEST_DEADLINE_MSEC;
	test.call_id = ++last_call_id;
	tests.push_back(test);
	r_pending_id = test.call_id;
	return false;
}

EretheaMcpToolResult EretheaMcpPlugin::_get_editor_state() {
	EditorInterface *editor = EditorInterface::get_singleton();
	EditorRunBar *run_bar = EditorRunBar::get_singleton();
	Node *root = EditorNode::get_singleton()->get_edited_scene();

	Dictionary state;
	state["engine"] = "Erethea Engine";
	state["version"] = GODOT_VERSION_FULL_BUILD;
	state["project_name"] = GLOBAL_GET("application/config/name");
	state["project_path"] = ProjectSettings::get_singleton()->get_resource_path();
	state["main_scene"] = GLOBAL_GET("application/run/main_scene");
	state["renderer"] = GLOBAL_GET("rendering/renderer/rendering_method");
	state["open_scenes"] = EretheaMcpValues::to_json(editor->get_open_scenes());
	state["unsaved_scenes"] = EretheaMcpValues::to_json(editor->get_unsaved_scenes());
	state["edited_scene"] = root ? Variant(root->get_scene_file_path()) : Variant();
	state["playing"] = run_bar->is_playing();
	state["playing_scene"] = run_bar->is_playing() ? Variant(run_bar->get_playing_scene()) : Variant();
	state["headless"] = DisplayServer::get_singleton()->get_name() == "headless";
	state["scanning"] = EditorFileSystem::get_singleton()->is_scanning();
	return EretheaMcpToolResult::object(state);
}

EretheaMcpToolResult EretheaMcpPlugin::_run_game(const Dictionary &p_arguments) {
	EditorRunBar *run_bar = EditorRunBar::get_singleton();
	Vector<String> run_arguments;
	if (bool(p_arguments.get("headless", false))) {
		run_arguments.push_back("--headless");
	}
	const Variant extra = p_arguments.get("arguments", Array());
	if (extra.get_type() == Variant::ARRAY) {
		for (const Variant &argument : Array(extra)) {
			run_arguments.push_back(String(argument));
		}
	}

	const uint64_t log_since = last_log_id;
	const String scene = p_arguments.get("scene", "main");
	if (scene == "main") {
		if (String(GLOBAL_GET("application/run/main_scene")).is_empty()) {
			return EretheaMcpToolResult::error("The project has no main scene. Pass scene \"current\" or a scene path, or set application/run/main_scene.");
		}
		run_bar->play_main_scene(false, run_arguments);
	} else if (scene == "current") {
		Node *root = EditorNode::get_singleton()->get_edited_scene();
		if (!root || root->get_scene_file_path().is_empty()) {
			return EretheaMcpToolResult::error("No saved scene is being edited. Save it with save_scene, or pass a scene path.");
		}
		run_bar->play_current_scene(false, run_arguments);
	} else {
		String path;
		String error;
		if (!EretheaMcpSceneTools::project_path(scene, path, error)) {
			return EretheaMcpToolResult::error(error);
		}
		if (!FileAccess::exists(path)) {
			return EretheaMcpToolResult::error(vformat("%s doesn't exist.", path));
		}
		run_bar->play_custom_scene(path, run_arguments);
	}
	if (!run_bar->is_playing()) {
		return EretheaMcpToolResult::error("The game didn't start. get_errors may say why.");
	}
	_connect_debuggers();

	Dictionary result;
	result["playing"] = true;
	result["scene"] = run_bar->get_playing_scene();
	// get_errors with this since reads only what this run says.
	result["errors_since"] = int64_t(log_since);
	return EretheaMcpToolResult::object(result);
}

#ifdef MODULE_ERETHEA_EXPORT_CHECKS_ENABLED
EretheaMcpToolResult EretheaMcpPlugin::_check_export(const Dictionary &p_arguments) {
	Ref<EditorExportPreset> preset;
	const Variant preset_name = p_arguments.get("preset", Variant());
	if (preset_name.get_type() == Variant::STRING && !String(preset_name).is_empty()) {
		PackedStringArray names;
		for (int i = 0; i < EditorExport::get_singleton()->get_export_preset_count(); i++) {
			const Ref<EditorExportPreset> candidate = EditorExport::get_singleton()->get_export_preset(i);
			names.push_back(candidate->get_name());
			if (candidate->get_name() == String(preset_name)) {
				preset = candidate;
			}
		}
		if (preset.is_null()) {
			return EretheaMcpToolResult::error(names.is_empty() ? String("The project has no export presets.") : vformat("No export preset \"%s\". The presets are: %s.", String(preset_name), String(", ").join(names)));
		}
	}
	const Vector<String> files = preset.is_valid() ? EretheaExportChecks::get_files_to_export(preset) : EretheaExportChecks::get_project_files();
	const EretheaExportChecks::Report report = EretheaExportChecks::check_files(files, EretheaExportCheckSettings::from_project_settings());
	return EretheaMcpToolResult::object(report.to_dictionary());
}
#endif

EretheaMcpToolResult EretheaMcpPlugin::_stop_game() {
	EditorRunBar *run_bar = EditorRunBar::get_singleton();
	const bool was_playing = run_bar->is_playing();
	run_bar->stop_playing();
	_process_screenshots();
	Dictionary result;
	result["playing"] = false;
	result["was_playing"] = was_playing;
	return EretheaMcpToolResult::object(result);
}

EretheaMcpToolResult EretheaMcpPlugin::_get_errors(const Dictionary &p_arguments) {
	const uint64_t since = MAX(int64_t(0), int64_t(p_arguments.get("since", 0)));
	const bool include_output = p_arguments.get("include_output", false);
	const int limit = CLAMP(int(p_arguments.get("limit", 100)), 1, MAX_LOG_ENTRIES);

	MutexLock lock(log_mutex);
	LocalVector<const LogEntry *> matching;
	for (const LogEntry &entry : log) {
		if (entry.id > since && (include_output || entry.level != "output")) {
			matching.push_back(&entry);
		}
	}
	Array entries;
	int errors = 0;
	int warnings = 0;
	const uint32_t first = matching.size() > uint32_t(limit) ? matching.size() - limit : 0;
	for (uint32_t i = first; i < matching.size(); i++) {
		const LogEntry &entry = *matching[i];
		Dictionary item;
		item["id"] = int64_t(entry.id);
		item["source"] = entry.source;
		item["level"] = entry.level;
		item["message"] = entry.message;
		if (!entry.file.is_empty()) {
			item["file"] = entry.file;
			item["line"] = entry.line;
		}
		if (!entry.function.is_empty()) {
			item["function"] = entry.function;
		}
		errors += entry.level == "error";
		warnings += entry.level == "warning";
		entries.push_back(item);
	}
	Dictionary result;
	result["entries"] = entries;
	result["errors"] = errors;
	result["warnings"] = warnings;
	result["omitted"] = int(first);
	result["next"] = int64_t(last_log_id);
	return EretheaMcpToolResult::object(result);
}

Dictionary EretheaMcpPlugin::_get_status() {
	Dictionary status;
	status["name"] = "erethea";
	status["version"] = GODOT_VERSION_FULL_BUILD;
	status["project"] = GLOBAL_GET("application/config/name");
	status["project_path"] = ProjectSettings::get_singleton()->get_resource_path();
	status["ready"] = EditorFileSystem::get_singleton() && !EditorFileSystem::get_singleton()->is_scanning();
	status["playing"] = EditorRunBar::get_singleton() && EditorRunBar::get_singleton()->is_playing();
	return status;
}

void EretheaMcpPlugin::_apply_settings() {
	bool enabled = EDITOR_GET(SETTING_ENABLED);
	int port = EDITOR_GET(SETTING_PORT);
	const String port_environment = OS::get_singleton()->get_environment(PORT_ENVIRONMENT);
	if (!port_environment.is_empty()) {
		if (port_environment.is_valid_int() && port_environment.to_int() > 0 && port_environment.to_int() <= 65535) {
			enabled = true;
			port = port_environment.to_int();
		} else {
			print_error(vformat("Erethea MCP: %s must be a port number, not \"%s\".", PORT_ENVIRONMENT, port_environment));
		}
	}

	if (server.is_listening() && (!enabled || server.get_port() != port)) {
		server.stop();
		print_line("Erethea MCP: server stopped.");
	}
	if (enabled && !server.is_listening()) {
		const Error error = server.start(port);
		if (error == OK) {
			print_line(vformat("Erethea MCP: server listening on http://127.0.0.1:%d%s", port, EretheaMcpServer::ENDPOINT));
			_connect_debuggers();
		} else {
			print_error(vformat("Erethea MCP: couldn't listen on port %d (%s). Change the port in Editor Settings > Erethea > MCP.", port, error_names[error]));
		}
	}
}

void EretheaMcpPlugin::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_INTERNAL_PROCESS: {
			if (!start_attempted && EditorNode::get_singleton()->is_editor_ready()) {
				start_attempted = true;
				_apply_settings();
			}
			if (server.is_listening()) {
				const uint64_t now = OS::get_singleton()->get_ticks_msec();
				if (now - last_debugger_scan_msec >= 1000) {
					// Game instances get their debugger when they start.
					last_debugger_scan_msec = now;
					_connect_debuggers();
				}
				server.poll();
				_process_screenshots();
				_process_tests();
			}
		} break;

		case EditorSettings::NOTIFICATION_EDITOR_SETTINGS_CHANGED: {
			if (start_attempted && EditorSettings::get_singleton()->check_changed_settings_in_group("erethea/mcp")) {
				_apply_settings();
			}
		} break;

		case NOTIFICATION_EXIT_TREE: {
			server.stop();
		} break;
	}
}

EretheaMcpPlugin::EretheaMcpPlugin() {
	EDITOR_DEF_BASIC(SETTING_ENABLED, false);
	EDITOR_DEF_BASIC(SETTING_PORT, int(EretheaMcpServer::DEFAULT_PORT));
	EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::INT, SETTING_PORT, PROPERTY_HINT_RANGE, "1024,65535,1"));

	server.get_protocol().setup(this, "erethea", "Erethea Engine", GODOT_VERSION_FULL_BUILD, INSTRUCTIONS);
	server.set_status_callable(callable_mp(this, &EretheaMcpPlugin::_get_status));

	error_handler.errfunc = _error_handler;
	error_handler.userdata = this;
	add_error_handler(&error_handler);

	set_process_internal(true);
}

EretheaMcpPlugin::~EretheaMcpPlugin() {
	remove_error_handler(&error_handler);
	server.stop();
}
