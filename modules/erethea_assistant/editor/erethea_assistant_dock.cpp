/**************************************************************************/
/*  erethea_assistant_dock.cpp                                            */
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

#include "erethea_assistant_dock.h"

#include "core/config/project_settings.h"
#include "core/crypto/crypto_core.h"
#include "core/input/input_event.h"
#include "core/io/file_access.h"
#include "core/io/json.h"
#include "core/object/callable_mp.h"
#include "core/os/os.h"
#include "editor/editor_data.h"
#include "editor/editor_interface.h"
#include "editor/editor_node.h"
#include "editor/editor_string_names.h"
#include "editor/settings/editor_settings.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/box_container.h"
#include "scene/gui/button.h"
#include "scene/gui/option_button.h"
#include "scene/gui/rich_text_label.h"
#include "scene/gui/text_edit.h"
#include "scene/main/http_request.h"
#include "scene/main/timer.h"

#include "modules/erethea_mcp/editor/erethea_mcp_plugin.h"
#include "modules/erethea_mcp/editor/erethea_mcp_scene_tools.h"

static const char *ACTION_SOURCE = "Assistant";

String EretheaAssistantDock::get_system_prompt() {
	return "You are the assistant inside the Erethea Engine editor, a fork of Godot 4. The person is making a game in the project open in this editor, "
		   "and you act on it with the tools: they open and change scenes, nodes and scripts, run and test the game, and see it. "
		   "Answer in the language the person writes in.\n"
		   "- Use the tools to do what is asked, instead of explaining how to do it. Look before changing: get_scene_tree, get_node_properties, read_file.\n"
		   "- Scripts are GDScript for Godot 4.\n"
		   "- Save the scenes you change with save_scene.\n"
		   "- \"Here\", \"this area\" or \"this\" mean the scene being edited, or the selected nodes; each message starts with what the editor shows.\n"
		   "- In projects made with Fornalha (they have game.project.json), the scripts in res://Scripts/fornalha/ belong to Fornalha, which replaces them: "
		   "prefer changing the exported properties in the scenes, or game.project.json and then build_fornalha_scenes.\n"
		   "- The person can undo each of your answers with one button, so act; but don't remove what you weren't asked to.\n"
		   "- When done, answer briefly, saying what changed.";
}

static void _format_node(const Dictionary &p_node, int p_depth, int p_max_nodes, int &r_count, String &r_text) {
	if (r_count >= p_max_nodes) {
		return;
	}
	r_count++;
	String line = String("  ").repeat(p_depth);
	const String path = p_node.get("path", ".");
	line += p_depth == 0 ? vformat(". %s \"%s\"", String(p_node.get("type", "")), String(p_node.get("name", ""))) : vformat("%s (%s)", path.get_file(), String(p_node.get("type", "")));
	if (p_node.has("script")) {
		line += vformat(" script=%s", String(p_node["script"]));
	}
	if (p_node.has("scene")) {
		line += vformat(" scene=%s", String(p_node["scene"]));
	}
	r_text += line + "\n";
	const Variant children = p_node.get("children", Variant());
	if (children.get_type() == Variant::ARRAY) {
		for (const Variant &child : Array(children)) {
			if (child.get_type() == Variant::DICTIONARY) {
				_format_node(child, p_depth + 1, p_max_nodes, r_count, r_text);
			}
		}
	}
}

String EretheaAssistantDock::format_scene_tree(const Dictionary &p_tree, int p_max_nodes) {
	const Variant root = p_tree.get("root", Variant());
	if (root.get_type() != Variant::DICTIONARY) {
		return String();
	}
	String text;
	int count = 0;
	_format_node(root, 0, p_max_nodes, count, text);
	const int total = p_tree.get("node_count", count);
	if (total > count || bool(p_tree.get("truncated", false))) {
		text += vformat("(%d of %d nodes shown; get_scene_tree lists them all)\n", count, total);
	}
	return text;
}

String EretheaAssistantDock::describe_call(const EretheaAssistantToolCall &p_call) {
	String arguments = JSON::stringify(p_call.arguments, "", false);
	if (arguments.length() > 160) {
		arguments = arguments.left(157) + "...";
	}
	return vformat("%s %s", p_call.name, arguments);
}

void EretheaAssistantDock::define_settings() {
	EditorSettings *settings = EditorSettings::get_singleton();
	EDITOR_DEF_BASIC(SETTING_PROVIDER, int(PROVIDER_IGNIS));
	settings->add_property_hint(PropertyInfo(Variant::INT, SETTING_PROVIDER, PROPERTY_HINT_ENUM, "Ignis (this computer),Claude (Anthropic API),OpenAI-compatible server"));
	EDITOR_DEF_BASIC(SETTING_IGNIS_URL, EretheaAssistantIgnis::DEFAULT_URL);
	EDITOR_DEF_BASIC(SETTING_CLAUDE_API_KEY, "");
	settings->add_property_hint(PropertyInfo(Variant::STRING, SETTING_CLAUDE_API_KEY, PROPERTY_HINT_PASSWORD));
	EDITOR_DEF_BASIC(SETTING_CLAUDE_MODEL, EretheaAssistantAnthropic::DEFAULT_MODEL);
	EDITOR_DEF(SETTING_CLAUDE_URL, EretheaAssistantAnthropic::API_URL);
	EDITOR_DEF_BASIC(SETTING_OPENAI_URL, "");
	settings->add_property_hint(PropertyInfo(Variant::STRING, SETTING_OPENAI_URL, PROPERTY_HINT_PLACEHOLDER_TEXT, "http://127.0.0.1:1234/v1"));
	EDITOR_DEF_BASIC(SETTING_OPENAI_API_KEY, "");
	settings->add_property_hint(PropertyInfo(Variant::STRING, SETTING_OPENAI_API_KEY, PROPERTY_HINT_PASSWORD));
	EDITOR_DEF_BASIC(SETTING_OPENAI_MODEL, "");
}

EretheaAssistantProvider *EretheaAssistantDock::_create_provider(int p_kind, String &r_problem) {
	switch (p_kind) {
		case PROVIDER_CLAUDE: {
			String key = String(EDITOR_GET(SETTING_CLAUDE_API_KEY)).strip_edges();
			if (key.is_empty()) {
				key = OS::get_singleton()->get_environment(API_KEY_ENVIRONMENT).strip_edges();
			}
			if (key.is_empty()) {
				r_problem = TTR("Claude needs an Anthropic API key: set it in Editor Settings > Erethea > Assistant > Claude, or in the ANTHROPIC_API_KEY environment variable.");
				return nullptr;
			}
			EretheaAssistantAnthropic *claude = memnew(EretheaAssistantAnthropic);
			claude->setup(key, EDITOR_GET(SETTING_CLAUDE_MODEL), EDITOR_GET(SETTING_CLAUDE_URL));
			return claude;
		}
		case PROVIDER_OPENAI: {
			const String url = String(EDITOR_GET(SETTING_OPENAI_URL)).strip_edges();
			if (url.is_empty()) {
				r_problem = TTR("Set the server's address, such as http://127.0.0.1:1234/v1, in Editor Settings > Erethea > Assistant > OpenAI.");
				return nullptr;
			}
			EretheaAssistantOpenAI *server = memnew(EretheaAssistantOpenAI);
			server->setup(url, EDITOR_GET(SETTING_OPENAI_API_KEY), EDITOR_GET(SETTING_OPENAI_MODEL));
			return server;
		}
		default: {
			EretheaAssistantIgnis *ignis = memnew(EretheaAssistantIgnis);
			ignis->setup(EDITOR_GET(SETTING_IGNIS_URL), String(), String());
			return ignis;
		}
	}
}

String EretheaAssistantDock::_make_context(bool p_include_project_file) {
	EditorInterface *editor = EditorInterface::get_singleton();
	String context = "[What the editor shows now]\n";
	context += vformat("Project: %s (%s)\n", String(GLOBAL_GET("application/config/name")), ProjectSettings::get_singleton()->get_resource_path());
	const bool fornalha = FileAccess::exists(GAME_PROJECT_FILE);
	context += vformat("Made with Fornalha: %s\n", fornalha ? "yes (it has game.project.json)" : "no");
	const PackedStringArray open_scenes = editor->get_open_scenes();
	if (!open_scenes.is_empty()) {
		context += vformat("Open scenes: %s\n", String(", ").join(open_scenes));
	}

	Node *root = EditorNode::get_singleton()->get_edited_scene();
	if (root) {
		context += vformat("Scene being edited: %s\n", root->get_scene_file_path().is_empty() ? String("(not saved yet)") : root->get_scene_file_path());
		PackedStringArray selected;
		for (Node *node : editor->get_selection()->get_top_selected_node_list()) {
			if (node == root || root->is_ancestor_of(node)) {
				selected.push_back(String(root->get_path_to(node)));
			}
		}
		if (!selected.is_empty()) {
			context += vformat("Selected nodes: %s\n", String(", ").join(selected));
		}
		Dictionary arguments;
		arguments["max_depth"] = 8;
		const EretheaMcpToolResult tree = EretheaMcpSceneTools::get_scene_tree(arguments);
		if (!tree.is_error) {
			context += "Scene tree (paths are relative to the root, \".\"):\n" + format_scene_tree(tree.structured, provider->get_context_node_limit());
		}
	} else {
		context += "No scene is being edited.\n";
	}

	if (fornalha && p_include_project_file) {
		String text = FileAccess::get_file_as_string(GAME_PROJECT_FILE);
		const int limit = provider->get_context_text_limit();
		if (text.length() > limit) {
			text = text.left(limit) + vformat("\n... (cut; read_file shows all %d characters)", text.length());
		}
		context += "game.project.json:\n" + text + "\n";
	}
	context += "[End of what the editor shows]";
	return context;
}

void EretheaAssistantDock::_ask(const String &p_question) {
	const int kind = EDITOR_GET(SETTING_PROVIDER);
	if (!provider || kind != provider_kind) {
		if (provider) {
			memdelete(provider);
			provider = nullptr;
		}
		String problem;
		provider = _create_provider(kind, problem);
		if (!provider) {
			_add_error(problem);
			return;
		}
		provider_kind = kind;
		sent_project_hash = String();
		Vector<EretheaMcpTool> tools;
		if (EretheaMcpPlugin::get_singleton()) {
			tools = EretheaMcpPlugin::get_singleton()->get_tools();
		}
		provider->start(get_system_prompt(), tools);
	}
	if (!EretheaMcpPlugin::get_singleton()) {
		_add_error(TTR("The editor's tools (the Erethea MCP plugin) aren't loaded."));
		return;
	}

	// game.project.json goes again only when it changed, unless the model only
	// sees the last messages.
	bool include_project_file = true;
	if (provider->remembers_history() && FileAccess::exists(GAME_PROJECT_FILE)) {
		const String hash = FileAccess::get_sha256(GAME_PROJECT_FILE);
		include_project_file = hash != sent_project_hash;
		sent_project_hash = hash;
	}

	_add_message(TTR("You"), p_question);
	provider->add_user_message(_make_context(include_project_file), p_question);
	busy = true;
	serial++;
	tools_used = 0;
	limit_reached = false;
	calls.clear();
	results.clear();
	call_index = 0;
	undo.begin_answer();
	_update_controls();
	_send_request();
}

void EretheaAssistantDock::_send_request() {
	const EretheaAssistantRequest request = provider->make_request();
	endpoint = request.url;
	const Error error = http->request(request.url, request.headers, HTTPClient::METHOD_POST, request.body);
	if (error != OK) {
		provider->cancel_question();
		_add_error(vformat(TTR("Couldn't send the question to %s (%s)."), request.url, error_names[error]));
		_finish_question();
	}
}

void EretheaAssistantDock::_request_completed(int p_result, int p_code, const PackedStringArray &p_headers, const PackedByteArray &p_body) {
	if (!busy) {
		return;
	}
	if (p_result != HTTPRequest::RESULT_SUCCESS) {
		String why;
		switch (p_result) {
			case HTTPRequest::RESULT_CANT_CONNECT:
			case HTTPRequest::RESULT_CANT_RESOLVE:
			case HTTPRequest::RESULT_CONNECTION_ERROR:
				why = provider_kind == PROVIDER_IGNIS ? TTR("nothing answers there. Is Ignis open?") : TTR("nothing answers there");
				break;
			case HTTPRequest::RESULT_TLS_HANDSHAKE_ERROR:
				why = TTR("the secure connection failed");
				break;
			case HTTPRequest::RESULT_TIMEOUT:
				why = TTR("it took too long to answer");
				break;
			default:
				why = vformat(TTR("error %d"), p_result);
				break;
		}
		provider->cancel_question();
		_add_error(vformat(TTR("Couldn't reach %s: %s"), endpoint, why));
		_finish_question();
		return;
	}
	const String body = String::utf8((const char *)p_body.ptr(), p_body.size());
	_handle_reply(provider->read_response(p_code, body));
}

void EretheaAssistantDock::_handle_reply(const EretheaAssistantReply &p_reply) {
	for (const String &note : p_reply.notes) {
		_add_note(note);
	}
	switch (p_reply.status) {
		case EretheaAssistantReply::STATUS_RETRY: {
			if (p_reply.retry_after > 0.0) {
				retry_timer->start(p_reply.retry_after);
			} else {
				callable_mp(this, &EretheaAssistantDock::_send_request).call_deferred();
			}
		} break;

		case EretheaAssistantReply::STATUS_REFUSED:
		case EretheaAssistantReply::STATUS_ERROR: {
			_add_error(p_reply.error);
			_finish_question();
		} break;

		case EretheaAssistantReply::STATUS_DONE: {
			_add_message(provider->get_display_name(), p_reply.text.is_empty() ? TTR("(no answer)") : p_reply.text);
			_finish_question();
		} break;

		case EretheaAssistantReply::STATUS_TOOLS: {
			if (!p_reply.text.is_empty()) {
				_add_message(provider->get_display_name(), p_reply.text);
			}
			calls = p_reply.tool_calls;
			results.clear();
			call_index = 0;
			if (limit_reached) {
				// It asked for more after being told to stop: close its calls
				// and end the question here.
				for (int i = 0; i < calls.size(); i++) {
					results.push_back(EretheaMcpToolResult::error("Not run: the limit of actions for one question was reached."));
				}
				provider->add_tool_results(calls, results);
				_add_note(vformat(TTR("Stopped after %d actions. Ask again to go on."), MAX_TOOLS_PER_QUESTION));
				_finish_question();
				return;
			}
			_run_next_tool();
		} break;
	}
}

void EretheaAssistantDock::_run_next_tool() {
	if (!busy) {
		return;
	}
	if (call_index >= calls.size()) {
		provider->add_tool_results(calls, results);
		_send_request();
		return;
	}
	const EretheaAssistantToolCall &call = calls[call_index];
	if (!call.invalid.is_empty()) {
		_add_error(vformat("%s: %s", call.name, call.invalid));
		callable_mp(this, &EretheaAssistantDock::_tool_finished).call_deferred(EretheaMcpToolResult::error(call.invalid).to_dictionary(), serial);
		return;
	}
	if (tools_used >= MAX_TOOLS_PER_QUESTION) {
		limit_reached = true;
		callable_mp(this, &EretheaAssistantDock::_tool_finished).call_deferred(EretheaMcpToolResult::error(vformat("Not run: at most %d actions per question. Stop here and tell the person what is left to do.", MAX_TOOLS_PER_QUESTION)).to_dictionary(), serial);
		return;
	}
	tools_used++;
	_add_note(String::utf8("▸ ") + describe_call(call));

	undo.before_tool(call.name, call.arguments);
	EretheaMcpSceneTools::set_action_source(ACTION_SOURCE);
	EretheaMcpToolResult result;
	const bool finished = EretheaMcpPlugin::get_singleton()->call_tool_local(call.name, call.arguments, callable_mp(this, &EretheaAssistantDock::_tool_finished).bind(serial), result);
	EretheaMcpSceneTools::set_action_source("MCP");
	if (finished) {
		// Deferred, so that the editor updates between the tools.
		callable_mp(this, &EretheaAssistantDock::_tool_finished).call_deferred(result.to_dictionary(), serial);
	}
}

void EretheaAssistantDock::_tool_finished(const Dictionary &p_result, uint64_t p_serial) {
	if (!busy || p_serial != serial || call_index >= calls.size()) {
		return;
	}
	const EretheaMcpToolResult result = EretheaMcpToolResult::from_dictionary(p_result);
	const EretheaAssistantToolCall &call = calls[call_index];
	if (call.invalid.is_empty()) {
		undo.after_tool(call.name, call.arguments, result);
	}
	if (result.is_error && call.invalid.is_empty()) {
		String text = EretheaAssistantProvider::get_result_text(result).strip_edges();
		const int line_end = text.find_char('\n');
		if (line_end > 0) {
			text = text.left(line_end);
		}
		_add_error(vformat("%s: %s", call.name, text.left(300)));
	}
	results.push_back(result);
	call_index++;
	_run_next_tool();
}

void EretheaAssistantDock::_finish_question() {
	busy = false;
	serial++;
	retry_timer->stop();
	undo.end_answer();
	_update_controls();
}

void EretheaAssistantDock::_stop() {
	if (!busy) {
		return;
	}
	http->cancel_request();
	if (EretheaMcpPlugin::get_singleton()) {
		EretheaMcpPlugin::get_singleton()->cancel_local_calls();
	}
	provider->cancel_question();
	_add_note(TTR("Stopped."));
	_finish_question();
}

void EretheaAssistantDock::_add_message(const String &p_who, const String &p_text) {
	conversation->push_bold();
	conversation->add_text(p_who + ": ");
	conversation->pop();
	conversation->add_text(p_text);
	conversation->add_newline();
}

void EretheaAssistantDock::_add_note(const String &p_text) {
	conversation->push_color(get_theme_color(SNAME("font_disabled_color"), EditorStringName(Editor)));
	conversation->add_text(p_text);
	conversation->pop();
	conversation->add_newline();
}

void EretheaAssistantDock::_add_error(const String &p_text) {
	conversation->push_color(get_theme_color(SNAME("error_color"), EditorStringName(Editor)));
	conversation->add_text(p_text);
	conversation->pop();
	conversation->add_newline();
}

void EretheaAssistantDock::_update_controls() {
	send_button->set_text(busy ? TTR("Stop") : TTR("Send"));
	send_button->set_tooltip_text(busy ? TTR("Stop this answer. What it already changed can still be undone.") : TTR("Send the question (Enter). Shift+Enter starts a new line."));
	undo_button->set_disabled(busy || !undo.can_undo());
	provider_option->set_disabled(busy);
}

void EretheaAssistantDock::_update_provider_option() {
	provider_option->select(CLAMP(int(EDITOR_GET(SETTING_PROVIDER)), 0, provider_option->get_item_count() - 1));
}

void EretheaAssistantDock::_send_pressed() {
	if (busy) {
		_stop();
		return;
	}
	const String question = input->get_text().strip_edges();
	if (question.is_empty()) {
		return;
	}
	input->clear();
	_ask(question);
}

void EretheaAssistantDock::_new_conversation() {
	_stop();
	if (provider) {
		memdelete(provider);
		provider = nullptr;
	}
	provider_kind = -1;
	sent_project_hash = String();
	conversation->clear();
	_add_note(TTR("Ask for changes to the scene or the game, such as \"put 10 enemies in the forest\". The assistant sees the open scene and acts in the editor, and each answer can be undone."));
}

void EretheaAssistantDock::_undo_pressed() {
	if (busy) {
		return;
	}
	String message;
	if (undo.undo(message)) {
		_add_note(message);
	} else {
		_add_error(message);
	}
	_update_controls();
}

void EretheaAssistantDock::_provider_selected(int p_index) {
	EditorSettings::get_singleton()->set_setting(SETTING_PROVIDER, p_index);
	EditorSettings::get_singleton()->notify_changes();
}

void EretheaAssistantDock::_input_gui_input(const Ref<InputEvent> &p_event) {
	const Ref<InputEventKey> key = p_event;
	if (key.is_valid() && key->is_pressed() && !key->is_echo() && (key->get_keycode() == Key::ENTER || key->get_keycode() == Key::KP_ENTER) && !key->is_shift_pressed()) {
		input->accept_event();
		_send_pressed();
	}
}

void EretheaAssistantDock::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			_new_conversation();
			_update_provider_option();
		} break;

		case EditorSettings::NOTIFICATION_EDITOR_SETTINGS_CHANGED: {
			if (EditorSettings::get_singleton()->check_changed_settings_in_group("erethea/assistant")) {
				_update_provider_option();
				// The next question starts again with the new settings.
				if (!busy && provider) {
					memdelete(provider);
					provider = nullptr;
					provider_kind = -1;
					_add_note(TTR("Settings changed: the next question starts a new conversation."));
				}
			}
		} break;
	}
}

EretheaAssistantDock::EretheaAssistantDock() {
	set_name("Assistant");
	set_title(TTRC("Assistant"));
	set_layout_key("Assistant");
	set_icon_name("Script");
	set_default_slot(EditorDock::DOCK_SLOT_RIGHT_UL);
	set_available_layouts(EditorDock::DOCK_LAYOUT_VERTICAL | EditorDock::DOCK_LAYOUT_FLOATING);
	set_closable(true);

	VBoxContainer *main = memnew(VBoxContainer);
	add_child(main);

	HBoxContainer *top = memnew(HBoxContainer);
	main->add_child(top);
	provider_option = memnew(OptionButton);
	provider_option->set_name("AssistantProvider");
	provider_option->set_h_size_flags(SIZE_EXPAND_FILL);
	provider_option->set_tooltip_text(TTRC("Who answers. The keys and addresses are in Editor Settings > Erethea > Assistant."));
	provider_option->add_item(TTRC("Ignis (this computer)"), PROVIDER_IGNIS);
	provider_option->add_item(TTRC("Claude (Anthropic API)"), PROVIDER_CLAUDE);
	provider_option->add_item(TTRC("OpenAI-compatible server"), PROVIDER_OPENAI);
	provider_option->connect(SceneStringName(item_selected), callable_mp(this, &EretheaAssistantDock::_provider_selected));
	top->add_child(provider_option);
	new_button = memnew(Button);
	new_button->set_name("AssistantNewConversation");
	new_button->set_text(TTRC("New"));
	new_button->set_tooltip_text(TTRC("Start a new conversation."));
	new_button->connect(SceneStringName(pressed), callable_mp(this, &EretheaAssistantDock::_new_conversation));
	top->add_child(new_button);

	conversation = memnew(RichTextLabel);
	conversation->set_name("AssistantConversation");
	conversation->set_v_size_flags(SIZE_EXPAND_FILL);
	conversation->set_scroll_follow(true);
	conversation->set_selection_enabled(true);
	conversation->set_context_menu_enabled(true);
	conversation->set_focus_mode(FOCUS_CLICK);
	conversation->set_custom_minimum_size(Size2(0, 160) * EDSCALE);
	main->add_child(conversation);

	input = memnew(TextEdit);
	input->set_name("AssistantInput");
	input->set_placeholder(TTRC("Ask for a change, such as \"make the enemies faster\"."));
	input->set_line_wrapping_mode(TextEdit::LINE_WRAPPING_BOUNDARY);
	input->set_custom_minimum_size(Size2(0, 70) * EDSCALE);
	input->connect(SceneStringName(gui_input), callable_mp(this, &EretheaAssistantDock::_input_gui_input));
	main->add_child(input);

	HBoxContainer *bottom = memnew(HBoxContainer);
	main->add_child(bottom);
	undo_button = memnew(Button);
	undo_button->set_name("AssistantUndo");
	undo_button->set_text(TTRC("Undo last answer"));
	undo_button->set_tooltip_text(TTRC("Put back what the last answer changed: scenes and files."));
	undo_button->connect(SceneStringName(pressed), callable_mp(this, &EretheaAssistantDock::_undo_pressed));
	bottom->add_child(undo_button);
	Control *spacer = memnew(Control);
	spacer->set_h_size_flags(SIZE_EXPAND_FILL);
	bottom->add_child(spacer);
	send_button = memnew(Button);
	send_button->set_name("AssistantSend");
	send_button->connect(SceneStringName(pressed), callable_mp(this, &EretheaAssistantDock::_send_pressed));
	bottom->add_child(send_button);

	http = memnew(HTTPRequest);
	http->set_timeout(REQUEST_TIMEOUT);
	http->connect("request_completed", callable_mp(this, &EretheaAssistantDock::_request_completed));
	add_child(http);

	retry_timer = memnew(Timer);
	retry_timer->set_one_shot(true);
	retry_timer->connect("timeout", callable_mp(this, &EretheaAssistantDock::_send_request));
	add_child(retry_timer);

	_update_controls();
}

EretheaAssistantDock::~EretheaAssistantDock() {
	if (provider) {
		memdelete(provider);
	}
}
