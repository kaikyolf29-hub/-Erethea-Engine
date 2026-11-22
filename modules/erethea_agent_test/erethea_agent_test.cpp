/**************************************************************************/
/*  erethea_agent_test.cpp                                                */
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

#include "erethea_agent_test.h"

#include "core/config/engine.h"
#include "core/config/project_settings.h"
#include "core/input/input.h"
#include "core/input/input_event.h"
#include "core/input/input_map.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/image.h"
#include "core/io/json.h"
#include "core/object/script_backtrace.h"
#include "core/os/os.h"
#include "core/version.h"
#include "scene/2d/node_2d.h"
#include "scene/main/scene_tree.h"
#include "scene/main/viewport.h"
#include "scene/main/window.h"
#include "servers/display/display_server.h"
#include "servers/rendering/rendering_server.h"

#ifndef _3D_DISABLED
#include "scene/3d/node_3d.h"
#endif

static double _round(double p_value, double p_step = 0.1) {
	return Math::snapped(p_value, p_step);
}

// Options.

EretheaAgentTestOptions EretheaAgentTestOptions::parse(const List<String> &p_args) {
	EretheaAgentTestOptions result;
	const String argument_with_folder = String(ARGUMENT) + "=";
	for (const String &arg : p_args) {
		if (arg == ARGUMENT) {
			result.enabled = true;
			result.folder = DEFAULT_FOLDER;
		} else if (arg.begins_with(argument_with_folder)) {
			result.enabled = true;
			result.folder = arg.substr(argument_with_folder.length()).strip_edges();
			if (result.folder.is_empty()) {
				result.folder = DEFAULT_FOLDER;
			}
		} else if (arg.begins_with(ARGUMENT_SECONDS)) {
			const String value = arg.substr(String(ARGUMENT_SECONDS).length()).strip_edges();
			if (!value.is_valid_float() || value.to_float() <= 0.0) {
				result.error = vformat("%s needs a number of seconds above 0, not \"%s\".", ARGUMENT_SECONDS, value);
			} else {
				result.seconds = value.to_float();
			}
		} else if (arg.begins_with(ARGUMENT_INPUTS)) {
			result.inputs_file = arg.substr(String(ARGUMENT_INPUTS).length()).strip_edges();
		}
	}
	return result;
}

// Steps.

Dictionary EretheaAgentTestStep::to_dictionary() const {
	Dictionary result;
	result["at"] = at;
	result["for"] = duration;
	switch (kind) {
		case KIND_ACTION:
			result["action"] = action;
			if (strength != 1.0f) {
				result["strength"] = strength;
			}
			break;
		case KIND_KEY:
			result["key"] = keycode_get_string(key);
			break;
		case KIND_MOUSE_BUTTON:
			switch (button) {
				case MouseButton::RIGHT:
					result["mouse_button"] = "right";
					break;
				case MouseButton::MIDDLE:
					result["mouse_button"] = "middle";
					break;
				case MouseButton::WHEEL_UP:
					result["mouse_button"] = "wheel_up";
					break;
				case MouseButton::WHEEL_DOWN:
					result["mouse_button"] = "wheel_down";
					break;
				default:
					result["mouse_button"] = "left";
					break;
			}
			break;
		case KIND_MOUSE_MOTION: {
			Array motion_array;
			motion_array.push_back(motion.x);
			motion_array.push_back(motion.y);
			result["mouse_motion"] = motion_array;
		} break;
	}
	switch (state) {
		case STATE_WAITING:
			result["result"] = "not reached";
			break;
		case STATE_HELD:
			result["result"] = "held";
			break;
		case STATE_DONE:
			result["result"] = "done";
			break;
		case STATE_SKIPPED:
			result["result"] = "skipped";
			result["reason"] = skipped_reason;
			break;
	}
	return result;
}

// Script.

static bool _read_number(const Dictionary &p_dictionary, const String &p_key, double &r_value) {
	const Variant value = p_dictionary.get(p_key, Variant());
	if (value.get_type() != Variant::INT && value.get_type() != Variant::FLOAT) {
		return false;
	}
	r_value = value;
	return true;
}

EretheaAgentTestScript EretheaAgentTestScript::parse(const Variant &p_data) {
	EretheaAgentTestScript result;
	Array steps;
	if (p_data.get_type() == Variant::ARRAY) {
		steps = p_data;
	} else if (p_data.get_type() == Variant::DICTIONARY) {
		const Dictionary data = p_data;
		const Variant steps_value = data.get("steps", Array());
		if (steps_value.get_type() != Variant::ARRAY) {
			result.error = "\"steps\" must be a list.";
			return result;
		}
		steps = steps_value;
		const Variant screenshots_value = data.get("screenshots", Array());
		if (screenshots_value.get_type() != Variant::ARRAY) {
			result.error = "\"screenshots\" must be a list of seconds.";
			return result;
		}
		for (const Variant &when : Array(screenshots_value)) {
			if ((when.get_type() != Variant::INT && when.get_type() != Variant::FLOAT) || double(when) < 0.0) {
				result.error = "\"screenshots\" must be a list of seconds.";
				return result;
			}
			result.screenshots.push_back(when);
		}
		result.screenshots.sort();
	} else {
		result.error = "The inputs must be a list of steps, or an object with \"steps\".";
		return result;
	}

	if (steps.size() > MAX_STEPS) {
		result.error = vformat("At most %d steps.", MAX_STEPS);
		return result;
	}
	for (int i = 0; i < steps.size(); i++) {
		const String where = vformat("Step %d: ", i + 1);
		if (steps[i].get_type() != Variant::DICTIONARY) {
			result.error = where + "must be an object.";
			return result;
		}
		const Dictionary data = steps[i];
		EretheaAgentTestStep step;
		if (!_read_number(data, "at", step.at) || step.at < 0.0) {
			result.error = where + "\"at\" must be the second when it starts.";
			return result;
		}
		if (data.has("action")) {
			step.kind = EretheaAgentTestStep::KIND_ACTION;
			step.action = data["action"];
			if (step.action.is_empty()) {
				result.error = where + "\"action\" must be the name of an input action.";
				return result;
			}
			double strength = 1.0;
			if (data.has("strength") && (!_read_number(data, "strength", strength) || strength < 0.0 || strength > 1.0)) {
				result.error = where + "\"strength\" must be between 0 and 1.";
				return result;
			}
			step.strength = strength;
		} else if (data.has("key")) {
			step.kind = EretheaAgentTestStep::KIND_KEY;
			step.key = find_keycode(String(data["key"]));
			if (step.key == Key::NONE) {
				result.error = where + vformat("unknown key \"%s\" (use names such as \"W\", \"Space\" or \"Shift\").", data["key"]);
				return result;
			}
		} else if (data.has("mouse_button")) {
			step.kind = EretheaAgentTestStep::KIND_MOUSE_BUTTON;
			const String name = String(data["mouse_button"]).to_lower();
			if (name == "left") {
				step.button = MouseButton::LEFT;
			} else if (name == "right") {
				step.button = MouseButton::RIGHT;
			} else if (name == "middle") {
				step.button = MouseButton::MIDDLE;
			} else if (name == "wheel_up") {
				step.button = MouseButton::WHEEL_UP;
			} else if (name == "wheel_down") {
				step.button = MouseButton::WHEEL_DOWN;
			} else {
				result.error = where + "\"mouse_button\" must be \"left\", \"right\", \"middle\", \"wheel_up\" or \"wheel_down\".";
				return result;
			}
		} else if (data.has("mouse_motion")) {
			step.kind = EretheaAgentTestStep::KIND_MOUSE_MOTION;
			step.duration = EretheaAgentTestStep::DEFAULT_MOTION_DURATION;
			const Variant motion = data["mouse_motion"];
			const Array motion_array = motion.get_type() == Variant::ARRAY ? Array(motion) : Array();
			if (motion_array.size() != 2 || !motion_array[0].is_num() || !motion_array[1].is_num()) {
				result.error = where + "\"mouse_motion\" must be [x, y] in pixels.";
				return result;
			}
			step.motion = Vector2(motion_array[0], motion_array[1]);
		} else {
			result.error = where + "needs \"action\", \"key\", \"mouse_button\" or \"mouse_motion\".";
			return result;
		}
		if (data.has("for") && (!_read_number(data, "for", step.duration) || step.duration < 0.0)) {
			result.error = where + "\"for\" must be how many seconds it is held.";
			return result;
		}
		result.steps.push_back(step);
	}
	return result;
}

static void _add_action(EretheaAgentTestScript &r_script, double p_at, double p_duration, const String &p_action, float p_strength = 1.0) {
	EretheaAgentTestStep step;
	step.kind = EretheaAgentTestStep::KIND_ACTION;
	step.at = p_at;
	step.duration = p_duration;
	step.action = p_action;
	step.strength = p_strength;
	r_script.steps.push_back(step);
}

EretheaAgentTestScript EretheaAgentTestScript::make_default(bool p_fornalha_project, const Vector<String> &p_actions, double p_seconds) {
	EretheaAgentTestScript result;
	if (p_fornalha_project) {
		// The same walk as the Fornalha's own test (Scripts/fornalha/teste_automatico.gd).
		// Its player adds these actions when it starts.
		_add_action(result, 0.8, 2.8, "frente");
		_add_action(result, 2.0, 0.1, "pular");
		_add_action(result, 3.6, 0.8, "frente", 0.8);
		_add_action(result, 3.6, 0.8, "direita", 0.6);
		for (double at : { 4.6, 5.6, 6.6, 7.6 }) {
			_add_action(result, at, 0.1, "atacar");
		}
		for (double at : { 1.2, 3.0, 4.6, 6.4, 8.2 }) {
			if (at < p_seconds) {
				result.screenshots.push_back(at);
			}
		}
	} else {
		Vector<String> actions = p_actions;
		if (actions.is_empty()) {
			actions = { "ui_right", "ui_left", "ui_up", "ui_down", "ui_accept" };
		}
		// Each action is held for a moment, in turn, until the end.
		double at = 0.5;
		int i = 0;
		while (at + 0.6 <= p_seconds && result.steps.size() < MAX_STEPS) {
			_add_action(result, at, 0.6, actions[i % actions.size()]);
			at += 0.8;
			i++;
		}
		for (double part : { 0.3, 0.6, 0.9 }) {
			result.screenshots.push_back(_round(p_seconds * part));
		}
	}
	return result;
}

// Frames.

Dictionary EretheaAgentTestFrames::get_summary() const {
	Dictionary result;
	const uint32_t count = frame_usec.size() > SKIPPED_FRAMES ? frame_usec.size() - SKIPPED_FRAMES : 0;
	result["frames"] = count;
	if (count == 0) {
		return result;
	}
	LocalVector<uint64_t> sorted;
	sorted.resize(count);
	uint64_t total = 0;
	for (uint32_t i = 0; i < count; i++) {
		sorted[i] = MAX(frame_usec[i + SKIPPED_FRAMES], uint64_t(1));
		total += sorted[i];
	}
	sorted.sort();
	const uint64_t slowest = sorted[count - 1];
	// The slowest 1% of the frames (at least one).
	const uint32_t low_count = MAX(count / 100, uint32_t(1));
	uint64_t low_total = 0;
	for (uint32_t i = count - low_count; i < count; i++) {
		low_total += sorted[i];
	}
	result["average_fps"] = _round(count * 1000000.0 / total);
	result["min_fps"] = _round(1000000.0 / slowest);
	result["low_1_percent_fps"] = _round(low_count * 1000000.0 / low_total);
	result["median_frame_ms"] = _round(sorted[count / 2] / 1000.0, 0.01);
	result["max_frame_ms"] = _round(slowest / 1000.0, 0.01);
	return result;
}

// Log.

Dictionary EretheaAgentTestLog::Entry::to_dictionary() const {
	Dictionary result;
	result["type"] = type;
	result["message"] = message;
	result["file"] = file;
	result["line"] = line;
	result["function"] = function;
	if (!script_file.is_empty()) {
		result["script_file"] = script_file;
		result["script_line"] = script_line;
	}
	result["count"] = count;
	result["first_seconds"] = _round(first_seconds, 0.01);
	return result;
}

void EretheaAgentTestLog::logv(const char *p_format, va_list p_list, bool p_err) {
	// Plain prints aren't errors; the errors come through log_error().
}

void EretheaAgentTestLog::log_error(const char *p_function, const char *p_file, int p_line, const char *p_code, const char *p_rationale, bool p_editor_notify, ErrorType p_type, const Vector<Ref<ScriptBacktrace>> &p_script_backtraces) {
	const char *message = (p_rationale && p_rationale[0]) ? p_rationale : p_code;
	String script_file;
	int script_line = 0;
	// Where the script was when the engine complained.
	for (const Ref<ScriptBacktrace> &backtrace : p_script_backtraces) {
		if (backtrace.is_valid() && backtrace->get_frame_count() > 0) {
			script_file = backtrace->get_frame_file(0);
			script_line = backtrace->get_frame_line(0);
			break;
		}
	}
	add(p_type, String::utf8(message ? message : ""), String::utf8(p_file ? p_file : ""), p_line, String::utf8(p_function ? p_function : ""), script_file, script_line);
}

void EretheaAgentTestLog::_add(Vector<Entry> &r_entries, const Entry &p_entry) {
	for (Entry &entry : r_entries) {
		if (entry.type == p_entry.type && entry.line == p_entry.line && entry.file == p_entry.file && entry.message == p_entry.message) {
			entry.count++;
			return;
		}
	}
	if (r_entries.size() < MAX_ENTRIES) {
		r_entries.push_back(p_entry);
	}
}

void EretheaAgentTestLog::add(ErrorType p_type, const String &p_message, const String &p_file, int p_line, const String &p_function, const String &p_script_file, int p_script_line) {
	Entry entry;
	switch (p_type) {
		case ERR_WARNING:
			entry.type = "warning";
			break;
		case ERR_SCRIPT:
			entry.type = "script_error";
			break;
		case ERR_SHADER:
			entry.type = "shader_error";
			break;
		default:
			entry.type = "error";
			break;
	}
	entry.message = p_message;
	entry.file = p_file;
	entry.line = p_line;
	entry.function = p_function;
	entry.script_file = p_script_file;
	entry.script_line = p_script_line;
	entry.count = 1;
	entry.first_seconds = (OS::get_singleton()->get_ticks_usec() - start_usec) / 1000000.0;

	MutexLock lock(mutex);
	if (p_type == ERR_WARNING) {
		warning_count++;
		_add(warnings, entry);
	} else {
		error_count++;
		_add(errors, entry);
	}
}

int EretheaAgentTestLog::get_error_count() {
	MutexLock lock(mutex);
	return error_count;
}

int EretheaAgentTestLog::get_warning_count() {
	MutexLock lock(mutex);
	return warning_count;
}

Array EretheaAgentTestLog::get_errors() {
	MutexLock lock(mutex);
	Array result;
	for (const Entry &entry : errors) {
		result.push_back(entry.to_dictionary());
	}
	return result;
}

Array EretheaAgentTestLog::get_warnings() {
	MutexLock lock(mutex);
	Array result;
	for (const Entry &entry : warnings) {
		result.push_back(entry.to_dictionary());
	}
	return result;
}

EretheaAgentTestLog::EretheaAgentTestLog() {
	start_usec = OS::get_singleton()->get_ticks_usec();
}

// Test.

void EretheaAgentTest::initialize() {
	options = EretheaAgentTestOptions::parse(OS::get_singleton()->get_cmdline_args());
	if (!options.enabled) {
		return;
	}
	if (Engine::get_singleton()->is_editor_hint() || Engine::get_singleton()->is_project_manager_hint()) {
		WARN_PRINT("--agent-test runs a game; the editor ignores it.");
		options.enabled = false;
		return;
	}
	log = memnew(EretheaAgentTestLog);
	OS::get_singleton()->add_logger(log);
	// The scene tree doesn't exist yet. It calls this on its first frame.
	SceneTree::add_idle_callback(&EretheaAgentTest::_idle_callback);
}

void EretheaAgentTest::_idle_callback() {
	if (added || !options.enabled) {
		return;
	}
	SceneTree *tree = SceneTree::get_singleton();
	if (!tree || !tree->get_root()) {
		return;
	}
	added = true;
	EretheaAgentTest *test = memnew(EretheaAgentTest);
	test->set_name("EretheaAgentTest");
	tree->get_root()->call_deferred(SNAME("add_child"), test);
}

Vector<String> EretheaAgentTest::get_project_actions() {
	Vector<String> result;
	List<PropertyInfo> properties;
	ProjectSettings::get_singleton()->get_property_list(&properties);
	for (const PropertyInfo &property : properties) {
		if (!property.name.begins_with("input/")) {
			continue;
		}
		const String action = property.name.trim_prefix("input/");
		// The built-in ui_ actions move through menus, not through the game.
		if (!action.begins_with("ui_") && !action.contains("/")) {
			result.push_back(action);
		}
	}
	result.sort();
	return result;
}

bool EretheaAgentTest::is_passed(const Dictionary &p_report) {
	if (!bool(p_report.get("finished", false)) || !String(p_report.get("error", "")).is_empty() || int(p_report.get("error_count", 0)) > 0) {
		return false;
	}
	const Variant player_value = p_report.get("player", Variant());
	if (player_value.get_type() == Variant::DICTIONARY && bool(Dictionary(player_value).get("fell", false))) {
		return false;
	}
	return true;
}

void EretheaAgentTest::_start() {
	running = true;
	start_usec = OS::get_singleton()->get_ticks_usec();
	last_write_usec = start_usec;

	folder = options.folder;
	if (folder.begins_with("res://") || folder.begins_with("user://")) {
		folder = ProjectSettings::get_singleton()->globalize_path(folder);
	} else if (folder.is_relative_path()) {
		// With --path, the current folder is the project's.
		folder = DirAccess::create(DirAccess::ACCESS_FILESYSTEM)->get_current_dir().path_join(folder);
	}
	DirAccess::make_dir_recursive_absolute(folder);

	if (!options.error.is_empty()) {
		_finish("bad_options");
		return;
	}
	if (options.inputs_file.is_empty()) {
		script = EretheaAgentTestScript::make_default(FileAccess::exists("res://game.project.json"), get_project_actions(), options.seconds);
	} else {
		const String text = FileAccess::get_file_as_string(options.inputs_file);
		Ref<JSON> json;
		json.instantiate();
		if (text.is_empty()) {
			script.error = vformat("Couldn't read the inputs file \"%s\".", options.inputs_file);
		} else if (json->parse(text) != OK) {
			script.error = vformat("The inputs file isn't valid JSON (line %d: %s).", json->get_error_line(), json->get_error_message());
		} else {
			script = EretheaAgentTestScript::parse(json->get_data());
		}
		if (!script.error.is_empty()) {
			options.error = script.error;
			_finish("bad_inputs");
			return;
		}
	}
	print_line(vformat("Erethea agent test: playing for %s seconds, the report goes to %s.", rtos(options.seconds), folder.path_join(REPORT_FILE)));
}

void EretheaAgentTest::_send(const EretheaAgentTestStep &p_step, bool p_pressed, const Vector2 &p_motion) {
	const Vector2 center = get_viewport()->get_visible_rect().size / 2;
	switch (p_step.kind) {
		case EretheaAgentTestStep::KIND_ACTION: {
			Ref<InputEventAction> event;
			event.instantiate();
			event->set_action(p_step.action);
			event->set_pressed(p_pressed);
			event->set_strength(p_pressed ? p_step.strength : 0.0f);
			Input::get_singleton()->parse_input_event(event);
		} break;
		case EretheaAgentTestStep::KIND_KEY: {
			Ref<InputEventKey> event;
			event.instantiate();
			event->set_keycode(p_step.key);
			event->set_physical_keycode(p_step.key);
			event->set_key_label(p_step.key);
			event->set_pressed(p_pressed);
			Input::get_singleton()->parse_input_event(event);
		} break;
		case EretheaAgentTestStep::KIND_MOUSE_BUTTON: {
			Ref<InputEventMouseButton> event;
			event.instantiate();
			event->set_button_index(p_step.button);
			event->set_pressed(p_pressed);
			event->set_button_mask(p_pressed ? mouse_button_to_mask(p_step.button) : MouseButtonMask::NONE);
			event->set_position(center);
			event->set_global_position(center);
			Input::get_singleton()->parse_input_event(event);
		} break;
		case EretheaAgentTestStep::KIND_MOUSE_MOTION: {
			if (p_motion.is_zero_approx()) {
				break;
			}
			Ref<InputEventMouseMotion> event;
			event.instantiate();
			event->set_relative(p_motion);
			event->set_relative_screen_position(p_motion);
			event->set_position(center);
			event->set_global_position(center);
			Input::get_singleton()->parse_input_event(event);
		} break;
	}
}

void EretheaAgentTest::_apply_steps(double p_delta) {
	for (EretheaAgentTestStep &step : script.steps) {
		if (step.state == EretheaAgentTestStep::STATE_WAITING && elapsed >= step.at) {
			if (step.kind == EretheaAgentTestStep::KIND_ACTION && !InputMap::get_singleton()->has_action(step.action)) {
				step.state = EretheaAgentTestStep::STATE_SKIPPED;
				step.skipped_reason = vformat("The project has no input action \"%s\".", step.action);
				continue;
			}
			_send(step, true);
			step.state = EretheaAgentTestStep::STATE_HELD;
		}
		if (step.state != EretheaAgentTestStep::STATE_HELD) {
			continue;
		}
		if (step.kind == EretheaAgentTestStep::KIND_MOUSE_MOTION) {
			// The movement is spread over the time it lasts.
			const double part = step.duration > 0.0 ? MIN(p_delta / step.duration, 1.0) : 1.0;
			_send(step, true, step.motion * part);
		}
		if (elapsed >= step.at + step.duration) {
			_send(step, false);
			step.state = EretheaAgentTestStep::STATE_DONE;
		}
	}
}

void EretheaAgentTest::_release_steps() {
	for (EretheaAgentTestStep &step : script.steps) {
		if (step.state == EretheaAgentTestStep::STATE_HELD) {
			_send(step, false);
			step.state = EretheaAgentTestStep::STATE_DONE;
		}
	}
}

// The position of a 3D or 2D node.
static bool _get_position(Object *p_object, Vector3 &r_position, bool &r_3d) {
#ifndef _3D_DISABLED
	if (Node3D *node_3d = Object::cast_to<Node3D>(p_object)) {
		r_position = node_3d->get_global_position();
		r_3d = true;
		return true;
	}
#endif
	if (Node2D *node_2d = Object::cast_to<Node2D>(p_object)) {
		const Vector2 position = node_2d->get_global_position();
		r_position = Vector3(position.x, position.y, 0);
		r_3d = false;
		return true;
	}
	return false;
}

void EretheaAgentTest::_track_player() {
	Object *object = ObjectDB::get_instance(player);
	if (player.is_valid() && !object) {
		player_freed = true;
		player = ObjectID();
	}
	if (!player.is_valid() && !player_freed) {
		// The player is the first node in the "player" group, or in "jogador"
		// in projects made with the Fornalha.
		for (const char *group : { "player", "jogador" }) {
			Node *node = get_tree()->get_first_node_in_group(group);
			if (node && _get_position(node, player_start, player_3d)) {
				player = node->get_instance_id();
				player_group = group;
				player_path = String(node->get_path());
				player_last = player_start;
				object = node;
				break;
			}
		}
	}
	Vector3 position;
	bool is_3d = false;
	if (object && _get_position(object, position, is_3d)) {
		player_distance += position.distance_to(player_last);
		player_last = position;
	}
}

void EretheaAgentTest::_sample_rendering() {
	RenderingServer *rs = RenderingServer::get_singleton();
	if (!rs) {
		return;
	}
	const uint64_t draw_calls = rs->get_rendering_info(RSE::RENDERING_INFO_TOTAL_DRAW_CALLS_IN_FRAME);
	rendering_frames++;
	draw_calls_total += draw_calls;
	draw_calls_max = MAX(draw_calls_max, draw_calls);
	primitives_max = MAX(primitives_max, rs->get_rendering_info(RSE::RENDERING_INFO_TOTAL_PRIMITIVES_IN_FRAME));
	video_memory_max = MAX(video_memory_max, rs->get_rendering_info(RSE::RENDERING_INFO_VIDEO_MEM_USED));
}

void EretheaAgentTest::_take_screenshot(int p_index) {
	// A game without a window draws nothing.
	if (DisplayServer::get_singleton()->get_name() == "headless") {
		return;
	}
	const Ref<ViewportTexture> texture = get_viewport()->get_texture();
	const Ref<Image> image = texture.is_valid() ? texture->get_image() : Ref<Image>();
	if (image.is_null() || image->is_empty()) {
		return;
	}
	const String file = folder.path_join(vformat("screenshot_%02d.png", p_index + 1));
	if (image->save_png(file) == OK) {
		screenshot_files.push_back(file);
	}
}

Dictionary EretheaAgentTest::_make_report() const {
	Dictionary report;
	report["finished"] = finished;
	report["ended"] = ended;
	report["engine"] = GODOT_VERSION_FULL_NAME;
	report["project"] = ProjectSettings::get_singleton()->get_resource_path();
	Node *scene = is_inside_tree() ? get_tree()->get_current_scene() : nullptr;
	report["scene"] = scene ? scene->get_scene_file_path() : String();
	report["headless"] = DisplayServer::get_singleton()->get_name() == "headless";
	report["seconds"] = _round(elapsed, 0.01);
	report["real_seconds"] = _round((OS::get_singleton()->get_ticks_usec() - start_usec) / 1000000.0, 0.01);
	if (!options.error.is_empty()) {
		report["error"] = options.error;
	}

	report["fps"] = frames.get_summary();
	if (!bool(report["headless"]) && rendering_frames > 0) {
		Dictionary rendering;
		rendering["draw_calls_average"] = _round(double(draw_calls_total) / rendering_frames);
		rendering["draw_calls_max"] = draw_calls_max;
		rendering["primitives_max"] = primitives_max;
		rendering["video_memory_mb_max"] = _round(video_memory_max / 1048576.0);
		report["rendering"] = rendering;
	} else {
		report["rendering"] = Variant();
	}
	Dictionary memory;
	memory["static_mb_peak"] = _round(OS::get_singleton()->get_static_memory_peak_usage() / 1048576.0);
	memory["objects"] = ObjectDB::get_object_count();
	memory["nodes"] = is_inside_tree() ? get_tree()->get_node_count() : 0;
	report["memory"] = memory;

	report["error_count"] = log ? log->get_error_count() : 0;
	report["warning_count"] = log ? log->get_warning_count() : 0;
	report["errors"] = log ? log->get_errors() : Array();
	report["warnings"] = log ? log->get_warnings() : Array();

	Array inputs;
	for (const EretheaAgentTestStep &step : script.steps) {
		inputs.push_back(step.to_dictionary());
	}
	report["inputs"] = inputs;
	report["inputs_file"] = options.inputs_file;

	if (!player_group.is_empty()) {
		Dictionary player_report;
		player_report["group"] = player_group;
		player_report["node"] = player_path;
		player_report["dimensions"] = player_3d ? 3 : 2;
		Array start;
		Array end;
		for (int i = 0; i < (player_3d ? 3 : 2); i++) {
			start.push_back(_round(player_start[i], 0.01));
			end.push_back(_round(player_last[i], 0.01));
		}
		player_report["start"] = start;
		player_report["end"] = end;
		Vector3 moved = player_last - player_start;
		if (player_3d) {
			// How far it went over the ground.
			moved.y = 0;
		}
		player_report["moved"] = _round(moved.length(), 0.01);
		player_report["distance"] = _round(player_distance, 0.01);
		player_report["freed"] = player_freed;
		player_report["fell"] = player_3d && player_last.y < player_start.y - 20.0;
		report["player"] = player_report;
	} else {
		report["player"] = Variant();
	}

	Array screenshots;
	for (const String &file : screenshot_files) {
		screenshots.push_back(file);
	}
	report["screenshots"] = screenshots;
	report["passed"] = is_passed(report);
	return report;
}

void EretheaAgentTest::_write_report() const {
	const String file = folder.path_join(REPORT_FILE);
	const String temporary = file + ".tmp";
	{
		Ref<FileAccess> output = FileAccess::open(temporary, FileAccess::WRITE);
		if (output.is_null()) {
			return;
		}
		output->store_string(JSON::stringify(_make_report(), "  ", false));
	}
	// Whoever reads the report never sees half of it.
	DirAccess::create(DirAccess::ACCESS_FILESYSTEM)->rename(temporary, file);
}

void EretheaAgentTest::_finish(const String &p_ended) {
	if (finished) {
		return;
	}
	_release_steps();
	finished = true;
	ended = p_ended;
	set_process_internal(false);
	const Dictionary report = _make_report();
	_write_report();
	print_line(OUTPUT_PREFIX + JSON::stringify(report, "", false));
	if (is_inside_tree() && p_ended != "game_quit") {
		get_tree()->quit(is_passed(report) ? 0 : (options.error.is_empty() ? 1 : 2));
	}
}

void EretheaAgentTest::_process_frame() {
	const uint64_t now = OS::get_singleton()->get_ticks_usec();
	if (last_frame_usec > 0) {
		frames.add(now - last_frame_usec);
	}
	last_frame_usec = now;

	// Game time, without Engine.time_scale.
	double delta = get_process_delta_time();
	const double time_scale = Engine::get_singleton()->get_time_scale();
	if (time_scale > 0.0) {
		delta /= time_scale;
	}
	elapsed += delta;

	_track_player();
	_sample_rendering();
	_apply_steps(delta);
	while (next_screenshot < script.screenshots.size() && elapsed >= script.screenshots[next_screenshot]) {
		_take_screenshot(next_screenshot);
		next_screenshot++;
	}

	if (elapsed >= options.seconds) {
		_finish("time");
		return;
	}
	if ((now - start_usec) / 1000000.0 > options.seconds + EXTRA_REAL_SECONDS) {
		_finish("real_time_limit");
		return;
	}
	// A game that crashes still leaves what happened until then.
	if (now - last_write_usec >= 1000000) {
		last_write_usec = now;
		_write_report();
	}
}

void EretheaAgentTest::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			set_process_mode(PROCESS_MODE_ALWAYS);
			set_process_internal(true);
			_start();
		} break;

		case NOTIFICATION_INTERNAL_PROCESS: {
			if (running && !finished) {
				_process_frame();
			}
		} break;

		case NOTIFICATION_EXIT_TREE: {
			// The game quit by itself before the end.
			if (running && !finished) {
				_finish("game_quit");
			}
		} break;
	}
}
