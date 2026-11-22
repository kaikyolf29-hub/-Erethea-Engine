/**************************************************************************/
/*  erethea_mcp_plugin.h                                                  */
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

#include "../erethea_mcp_server.h"

#include "core/os/mutex.h"
#include "core/os/process_id.h"
#include "editor/plugins/editor_plugin.h"

#include "modules/modules_enabled.gen.h" // For erethea_export_checks.

class Image;

// Runs the MCP server inside the editor, so that agents such as Claude Code
// can open scenes, add nodes, write scripts, run the game, read its errors
// and see it. Off by default: Editor Settings > Erethea > MCP, or the
// ERETHEA_MCP_PORT environment variable.
class EretheaMcpPlugin : public EditorPlugin, public EretheaMcpToolHandler {
	GDCLASS(EretheaMcpPlugin, EditorPlugin);

public:
	static constexpr const char *SETTING_ENABLED = "erethea/mcp/enabled";
	static constexpr const char *SETTING_PORT = "erethea/mcp/port";
	static constexpr const char *PORT_ENVIRONMENT = "ERETHEA_MCP_PORT";

private:
	static constexpr int MAX_LOG_ENTRIES = 2000;
	static constexpr uint64_t SCREENSHOT_TIMEOUT_MSEC = 10000;
	// Frames to wait after showing an editor view, so that it's drawn.
	static constexpr int SCREENSHOT_FRAMES = 3;

	EretheaMcpServer server;
	bool start_attempted = false;

	// Errors, warnings and prints, from the editor and from the game.
	struct LogEntry {
		uint64_t id = 0;
		String source;
		String level;
		String message;
		String file;
		int line = 0;
		String function;
	};
	Mutex log_mutex;
	List<LogEntry> log;
	uint64_t last_log_id = 0;
	ErrorHandlerList error_handler;

	struct PendingScreenshot {
		uint64_t call_id = 0;
		String target;
		int max_size = 0;
		// The game's answer.
		int64_t request_id = 0;
		uint64_t deadline_msec = 0;
		// An editor view, after a few frames.
		int frames_left = 0;
	};
	LocalVector<PendingScreenshot> screenshots;

	// A game run by run_test, until it exits.
	struct PendingTest {
		uint64_t call_id = 0;
		ProcessID pid = 0;
		String folder;
		uint64_t deadline_msec = 0;
	};
	static constexpr int TEST_MAX_SECONDS = 40;
	static constexpr uint64_t TEST_DEADLINE_MSEC = 55000;
	LocalVector<PendingTest> tests;
	uint64_t last_call_id = 0;
	int64_t last_screenshot_request = 0;
	uint64_t last_debugger_scan_msec = 0;

	static void _error_handler(void *p_self, const char *p_function, const char *p_file, int p_line, const char *p_error, const char *p_message, bool p_editor_notify, ErrorHandlerType p_type);
	void _add_log(const String &p_source, const String &p_level, const String &p_message, const String &p_file = String(), int p_line = 0, const String &p_function = String());

	void _apply_settings();
	void _connect_debuggers();
	void _debug_data(const String &p_message, const Array &p_data);
	void _process_screenshots();
	void _finish_screenshot(uint64_t p_call_id, const Ref<Image> &p_image, const String &p_failure);
	Dictionary _get_status();

	EretheaMcpToolResult _get_editor_state();
	EretheaMcpToolResult _run_game(const Dictionary &p_arguments);
	EretheaMcpToolResult _stop_game();
	EretheaMcpToolResult _get_errors(const Dictionary &p_arguments);
#ifdef MODULE_ERETHEA_EXPORT_CHECKS_ENABLED
	EretheaMcpToolResult _check_export(const Dictionary &p_arguments);
#endif
	bool _take_screenshot(const Dictionary &p_arguments, bool p_can_wait, EretheaMcpToolResult &r_result, uint64_t &r_pending_id);
	bool _run_test(const Dictionary &p_arguments, bool p_can_wait, EretheaMcpToolResult &r_result, uint64_t &r_pending_id);
	void _process_tests();
	void _finish_test(const PendingTest &p_test, bool p_timed_out);

protected:
	void _notification(int p_what);

public:
	virtual String get_plugin_name() const override { return "Erethea MCP"; }

	virtual Vector<EretheaMcpTool> get_tools() const override;
	virtual bool call_tool(const String &p_name, const Dictionary &p_arguments, bool p_can_wait, EretheaMcpToolResult &r_result, uint64_t &r_pending_id) override;

	EretheaMcpPlugin();
	~EretheaMcpPlugin();
};
