/**************************************************************************/
/*  erethea_assistant_dock.h                                              */
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

#include "../erethea_assistant_providers.h"
#include "erethea_assistant_undo.h"

#include "editor/docks/editor_dock.h"

class Button;
class HTTPRequest;
class OptionButton;
class RichTextLabel;
class TextEdit;
class Timer;

// The assistant panel: a chat with a model (Ignis on this computer, Claude
// through the Anthropic API, or another OpenAI-compatible server) that sees
// the open scene and game.project.json, and acts on the editor with the MCP
// tools. Every answer can be undone.
class EretheaAssistantDock : public EditorDock {
	GDCLASS(EretheaAssistantDock, EditorDock);

public:
	enum ProviderKind {
		PROVIDER_IGNIS,
		PROVIDER_CLAUDE,
		PROVIDER_OPENAI,
	};

	static constexpr const char *SETTING_PROVIDER = "erethea/assistant/provider";
	static constexpr const char *SETTING_IGNIS_URL = "erethea/assistant/ignis/url";
	static constexpr const char *SETTING_CLAUDE_API_KEY = "erethea/assistant/claude/api_key";
	static constexpr const char *SETTING_CLAUDE_MODEL = "erethea/assistant/claude/model";
	static constexpr const char *SETTING_CLAUDE_URL = "erethea/assistant/claude/url";
	static constexpr const char *SETTING_OPENAI_URL = "erethea/assistant/openai/url";
	static constexpr const char *SETTING_OPENAI_API_KEY = "erethea/assistant/openai/api_key";
	static constexpr const char *SETTING_OPENAI_MODEL = "erethea/assistant/openai/model";
	static constexpr const char *API_KEY_ENVIRONMENT = "ANTHROPIC_API_KEY";
	static constexpr const char *GAME_PROJECT_FILE = "res://game.project.json";

	static constexpr int MAX_TOOLS_PER_QUESTION = 30;
	static constexpr double REQUEST_TIMEOUT = 600.0;

private:
	OptionButton *provider_option = nullptr;
	Button *new_button = nullptr;
	RichTextLabel *conversation = nullptr;
	TextEdit *input = nullptr;
	Button *send_button = nullptr;
	Button *undo_button = nullptr;
	HTTPRequest *http = nullptr;
	Timer *retry_timer = nullptr;

	EretheaAssistantProvider *provider = nullptr;
	int provider_kind = -1;
	String endpoint;
	String sent_project_hash;

	bool busy = false;
	// Changes with every question, so that late results of a stopped one are
	// ignored.
	uint64_t serial = 0;
	int tools_used = 0;
	bool limit_reached = false;
	Vector<EretheaAssistantToolCall> calls;
	Vector<EretheaMcpToolResult> results;
	int call_index = 0;

	EretheaAssistantUndo undo;

	EretheaAssistantProvider *_create_provider(int p_kind, String &r_problem);
	String _make_context(bool p_include_project_file);
	void _ask(const String &p_question);
	void _send_request();
	void _request_completed(int p_result, int p_code, const PackedStringArray &p_headers, const PackedByteArray &p_body);
	void _handle_reply(const EretheaAssistantReply &p_reply);
	void _run_next_tool();
	void _tool_finished(const Dictionary &p_result, uint64_t p_serial);
	void _finish_question();
	void _stop();

	void _add_message(const String &p_who, const String &p_text);
	void _add_note(const String &p_text);
	void _add_error(const String &p_text);
	void _update_controls();
	void _update_provider_option();

	void _send_pressed();
	void _new_conversation();
	void _undo_pressed();
	void _provider_selected(int p_index);
	void _input_gui_input(const Ref<InputEvent> &p_event);

protected:
	void _notification(int p_what);

public:
	static String get_system_prompt();
	// The scene tree that get_scene_tree returns, as indented lines.
	static String format_scene_tree(const Dictionary &p_tree, int p_max_nodes);
	// A tool call as one short line.
	static String describe_call(const EretheaAssistantToolCall &p_call);
	static void define_settings();

	bool is_busy() const { return busy; }

	EretheaAssistantDock();
	~EretheaAssistantDock();
};
