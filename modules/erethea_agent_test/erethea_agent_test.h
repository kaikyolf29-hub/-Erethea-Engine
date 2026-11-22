/**************************************************************************/
/*  erethea_agent_test.h                                                  */
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

#include "core/input/input_enums.h"
#include "core/io/logger.h"
#include "core/os/keyboard.h"
#include "core/os/mutex.h"
#include "core/templates/local_vector.h"
#include "core/variant/dictionary.h"
#include "scene/main/node.h"

// Erethea: the test mode for agents. With --agent-test=<folder>, the game runs
// for some seconds, its controls are pressed by a script, and a report with
// the errors, the frame rate and how far the player went is written to
// <folder>/report.json. The process exits with 0 when the test passed.

struct EretheaAgentTestOptions {
	static constexpr const char *ARGUMENT = "--agent-test";
	static constexpr const char *ARGUMENT_SECONDS = "--agent-test-seconds=";
	static constexpr const char *ARGUMENT_INPUTS = "--agent-test-inputs=";
	static constexpr const char *DEFAULT_FOLDER = "user://agent_test";
	static constexpr double DEFAULT_SECONDS = 10.0;

	bool enabled = false;
	String folder;
	double seconds = DEFAULT_SECONDS;
	String inputs_file;
	String error;

	static EretheaAgentTestOptions parse(const List<String> &p_args);
};

// One control pressed by the test: an input action, a key, a mouse button or
// a mouse movement, from "at" seconds for "for" seconds.
struct EretheaAgentTestStep {
	enum Kind {
		KIND_ACTION,
		KIND_KEY,
		KIND_MOUSE_BUTTON,
		KIND_MOUSE_MOTION,
	};

	enum State {
		STATE_WAITING,
		STATE_HELD,
		STATE_DONE,
		STATE_SKIPPED,
	};

	static constexpr double DEFAULT_DURATION = 0.1;
	static constexpr double DEFAULT_MOTION_DURATION = 0.5;

	Kind kind = KIND_ACTION;
	double at = 0.0;
	double duration = DEFAULT_DURATION;
	String action;
	float strength = 1.0;
	Key key = Key::NONE;
	MouseButton button = MouseButton::NONE;
	Vector2 motion;

	State state = STATE_WAITING;
	String skipped_reason;

	Dictionary to_dictionary() const;
};

// What the test presses, and when it takes screenshots.
struct EretheaAgentTestScript {
	static constexpr int MAX_STEPS = 1000;

	Vector<EretheaAgentTestStep> steps;
	Vector<double> screenshots;
	String error;

	// From a JSON file: {"steps": [...], "screenshots": [...]}, or only the list of steps.
	static EretheaAgentTestScript parse(const Variant &p_data);
	// Without a file. Projects made with the Fornalha get the walk of its own
	// test (forward, jump, turn, attack). Other projects get each of their
	// input actions pressed in turn.
	static EretheaAgentTestScript make_default(bool p_fornalha_project, const Vector<String> &p_actions, double p_seconds);
};

// Frame times, for the frame rate summary.
class EretheaAgentTestFrames {
	LocalVector<uint64_t> frame_usec;

public:
	// The first frames load the scene and would only pull the numbers down.
	static constexpr uint32_t SKIPPED_FRAMES = 5;

	void add(uint64_t p_usec) { frame_usec.push_back(p_usec); }
	uint32_t get_count() const { return frame_usec.size(); }
	Dictionary get_summary() const;
};

// Collects the errors and warnings printed while the game runs. Owned by the
// OS, like every logger.
class EretheaAgentTestLog : public Logger {
public:
	static constexpr int MAX_ENTRIES = 100;

	struct Entry {
		String type;
		String message;
		String file;
		int line = 0;
		String function;
		String script_file;
		int script_line = 0;
		int count = 0;
		double first_seconds = 0.0;

		Dictionary to_dictionary() const;
	};

private:
	Mutex mutex;
	uint64_t start_usec = 0;
	Vector<Entry> errors;
	Vector<Entry> warnings;
	int error_count = 0;
	int warning_count = 0;

	void _add(Vector<Entry> &r_entries, const Entry &p_entry);

public:
	virtual void logv(const char *p_format, va_list p_list, bool p_err) override _PRINTF_FORMAT_ATTRIBUTE_2_0;
	virtual void log_error(const char *p_function, const char *p_file, int p_line, const char *p_code, const char *p_rationale, bool p_editor_notify = false, ErrorType p_type = ERR_ERROR, const Vector<Ref<ScriptBacktrace>> &p_script_backtraces = {}) override;

	void add(ErrorType p_type, const String &p_message, const String &p_file, int p_line, const String &p_function, const String &p_script_file = String(), int p_script_line = 0);
	int get_error_count();
	int get_warning_count();
	Array get_errors();
	Array get_warnings();

	EretheaAgentTestLog();
};

class EretheaAgentTest : public Node {
	GDCLASS(EretheaAgentTest, Node);

	static inline EretheaAgentTestOptions options;
	static inline EretheaAgentTestLog *log = nullptr;
	static inline bool added = false;

	static void _idle_callback();

	EretheaAgentTestScript script;
	EretheaAgentTestFrames frames;
	String folder;
	bool running = false;
	bool finished = false;
	String ended;
	double elapsed = 0.0;
	uint64_t start_usec = 0;
	uint64_t last_frame_usec = 0;
	uint64_t last_write_usec = 0;
	int next_screenshot = 0;
	Vector<String> screenshot_files;

	ObjectID player;
	String player_group;
	String player_path;
	bool player_3d = false;
	bool player_freed = false;
	Vector3 player_start;
	Vector3 player_last;
	double player_distance = 0.0;

	uint64_t rendering_frames = 0;
	uint64_t draw_calls_total = 0;
	uint64_t draw_calls_max = 0;
	uint64_t primitives_max = 0;
	uint64_t video_memory_max = 0;

	void _start();
	void _process_frame();
	void _send(const EretheaAgentTestStep &p_step, bool p_pressed, const Vector2 &p_motion = Vector2());
	void _apply_steps(double p_delta);
	void _release_steps();
	void _track_player();
	void _sample_rendering();
	void _take_screenshot(int p_index);
	Dictionary _make_report() const;
	void _write_report() const;
	void _finish(const String &p_ended);

protected:
	void _notification(int p_what);

public:
	static constexpr const char *REPORT_FILE = "report.json";
	static constexpr const char *OUTPUT_PREFIX = "ERETHEA_AGENT_TEST ";
	// Without a frame for this long (in real time, over the test's own
	// seconds), the test ends anyway.
	static constexpr double EXTRA_REAL_SECONDS = 60.0;

	static void initialize();
	static bool is_enabled() { return options.enabled; }
	static Vector<String> get_project_actions();
	static bool is_passed(const Dictionary &p_report);
};
