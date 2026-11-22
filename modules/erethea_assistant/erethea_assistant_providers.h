/**************************************************************************/
/*  erethea_assistant_providers.h                                         */
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

#include "core/templates/local_vector.h"

#include "modules/erethea_mcp/erethea_mcp_protocol.h"

// What the assistant panel sends to and reads from the models: one class per
// kind of server. They only build requests and read answers, without the
// network or the editor, so that tests can check them.

struct EretheaAssistantToolCall {
	String id;
	String name;
	Dictionary arguments;
	// Set when the call can't run, such as arguments that aren't JSON: the
	// panel answers with this error instead of running the tool.
	String invalid;
};

struct EretheaAssistantRequest {
	String url;
	Vector<String> headers;
	String body;
};

struct EretheaAssistantReply {
	enum Status {
		// The answer is complete: `text` is what the model said.
		STATUS_DONE,
		// Run `tool_calls`, give their results to add_tool_results() and ask
		// again with make_request().
		STATUS_TOOLS,
		// Ask again with make_request(), after `retry_after` seconds.
		STATUS_RETRY,
		// The model declined to answer. The question left the history.
		STATUS_REFUSED,
		// Something went wrong: `error` says what. The question left the
		// history, so that the conversation can go on.
		STATUS_ERROR,
	};

	Status status = STATUS_ERROR;
	String text;
	Vector<EretheaAssistantToolCall> tool_calls;
	String error;
	double retry_after = 0.0;
	// Things worth showing, such as actions that were left out.
	Vector<String> notes;
};

class EretheaAssistantProvider {
public:
	// Statuses that are worth one more try, after a pause.
	static bool is_retryable_status(int p_http_code);
	// The text of the MCP content blocks of a tool's result.
	static String get_result_text(const EretheaMcpToolResult &p_result);

protected:
	String system_prompt;
	Vector<EretheaMcpTool> tools;
	// Tries left for the current request, after an error worth retrying.
	int retries_left = 1;

	static String _error_message(const String &p_body);

public:
	virtual String get_display_name() const = 0;
	// Models with a short context get a smaller summary of the editor.
	virtual int get_context_node_limit() const { return 400; }
	virtual int get_context_text_limit() const { return 20000; }
	// Whether the whole conversation is sent each time. If not, the context
	// that rarely changes (game.project.json) goes with every question.
	virtual bool remembers_history() const { return true; }

	// Starts a new conversation. The system prompt and the tools stay the
	// same until the next one.
	virtual void start(const String &p_system_prompt, const Vector<EretheaMcpTool> &p_tools);
	// A question, after a summary of what the editor shows.
	virtual void add_user_message(const String &p_context, const String &p_text) = 0;
	virtual EretheaAssistantRequest make_request() const = 0;
	// Reads the server's answer. With STATUS_DONE or STATUS_TOOLS, the answer
	// joins the history.
	virtual EretheaAssistantReply read_response(int p_http_code, const String &p_body) = 0;
	// The results of the calls of the last STATUS_TOOLS, in the same order.
	virtual void add_tool_results(const Vector<EretheaAssistantToolCall> &p_calls, const Vector<EretheaMcpToolResult> &p_results) = 0;
	// Removes the last question and what followed it, such as when the person
	// stops it or the server can't be reached.
	virtual void cancel_question() = 0;
	virtual int get_message_count() const = 0;

	virtual ~EretheaAssistantProvider() {}
};

// Claude, through the Anthropic API, with native tool use.
//
// Claude Opus 5.5 ties its thinking blocks to the conversation that produced
// them, so the history is never edited: the system prompt and the tools stay
// the same, the editor's context goes in the user messages, and the model's
// answers go back byte for byte, as the raw JSON of their `content`. Only the
// end of the history can be removed (a stopped question, a refusal).
class EretheaAssistantAnthropic : public EretheaAssistantProvider {
public:
	static constexpr const char *API_URL = "https://api.anthropic.com/v1/messages";
	static constexpr const char *API_VERSION = "2026-10-21";
	static constexpr const char *FALLBACK_BETA = "server-side-fallback-2026-07-01";
	static constexpr const char *DEFAULT_MODEL = "claude-opus-5-5";
	static constexpr int MAX_TOKENS = 16000;

private:
	String url = API_URL;
	String api_key;
	String model = DEFAULT_MODEL;
	// Each message as JSON text, as it is sent.
	LocalVector<String> messages;
	int question_start = -1;
	bool use_fallbacks = true;
	bool stripped_thinking = false;

	void _strip_thinking();

public:
	// Finds the value of `p_key` in the JSON object `p_json`, without parsing
	// it, so that it can be sent back exactly as it came.
	static bool find_value(const String &p_json, const String &p_key, int &r_begin, int &r_end);
	// MCP content blocks as Anthropic content blocks.
	static Array to_content_blocks(const Array &p_mcp_content);

	void setup(const String &p_api_key, const String &p_model, const String &p_url = API_URL);
	bool is_using_fallbacks() const { return use_fallbacks; }
	String get_message(int p_index) const { return messages[p_index]; }

	virtual String get_display_name() const override { return "Claude"; }
	virtual void start(const String &p_system_prompt, const Vector<EretheaMcpTool> &p_tools) override;
	virtual void add_user_message(const String &p_context, const String &p_text) override;
	virtual EretheaAssistantRequest make_request() const override;
	virtual EretheaAssistantReply read_response(int p_http_code, const String &p_body) override;
	virtual void add_tool_results(const Vector<EretheaAssistantToolCall> &p_calls, const Vector<EretheaMcpToolResult> &p_results) override;
	virtual void cancel_question() override;
	virtual int get_message_count() const override { return messages.size(); }
};

// A server with the OpenAI chat completions API and "tools" of type
// "function", such as LM Studio, Ollama or a hosted model.
class EretheaAssistantOpenAI : public EretheaAssistantProvider {
protected:
	String base_url;
	String api_key;
	String model;
	Array messages;
	int question_start = -1;

	static Dictionary _message_of(const Dictionary &p_answer, String &r_finish_reason, String &r_error);

public:
	// "http://host:port/v1" + "/chat/completions".
	static String get_endpoint(const String &p_base_url);

	void setup(const String &p_base_url, const String &p_api_key, const String &p_model);
	Array get_messages() const { return messages; }

	virtual String get_display_name() const override { return "OpenAI-compatible"; }
	virtual void start(const String &p_system_prompt, const Vector<EretheaMcpTool> &p_tools) override;
	virtual void add_user_message(const String &p_context, const String &p_text) override;
	virtual EretheaAssistantRequest make_request() const override;
	virtual EretheaAssistantReply read_response(int p_http_code, const String &p_body) override;
	virtual void add_tool_results(const Vector<EretheaAssistantToolCall> &p_calls, const Vector<EretheaMcpToolResult> &p_results) override;
	virtual void cancel_question() override;
	virtual int get_message_count() const override { return messages.size(); }
};

// Ignis, the local model server. Small models may not handle "tools", so, as
// the Fornalha does with Ignis, the answer is a JSON object that a schema
// constrains: {"reply": "...", "actions": [{"tool": ..., "arguments": {...}}]}.
// The actions are checked against the tools' schemas, run, and their results
// sent back, for a few rounds. Only the last messages are sent.
class EretheaAssistantIgnis : public EretheaAssistantOpenAI {
public:
	static constexpr const char *DEFAULT_URL = "http://127.0.0.1:11470/v1";
	static constexpr int MAX_ROUNDS = 4;
	static constexpr int RECENT_MESSAGES = 6;
	static constexpr int MAX_RESULT_CHARACTERS = 2000;

private:
	int rounds = 0;
	int last_call_id = 0;

public:
	// The JSON object in a model's text: after the last </think>, from the
	// first { to the last }.
	static String extract_json(const String &p_text);
	// Keeps the arguments that the tool's schema knows, of the right type.
	// Returns false, with `r_problem`, when a required one is missing.
	static bool check_arguments(const Dictionary &p_schema, const Dictionary &p_arguments, Dictionary &r_checked, String &r_problem);
	static Dictionary get_response_format(const Vector<EretheaMcpTool> &p_tools);

	virtual String get_display_name() const override { return "Ignis"; }
	virtual int get_context_node_limit() const override { return 150; }
	virtual int get_context_text_limit() const override { return 6000; }
	virtual bool remembers_history() const override { return false; }

	virtual void start(const String &p_system_prompt, const Vector<EretheaMcpTool> &p_tools) override;
	virtual void add_user_message(const String &p_context, const String &p_text) override;
	virtual EretheaAssistantRequest make_request() const override;
	virtual EretheaAssistantReply read_response(int p_http_code, const String &p_body) override;
	virtual void add_tool_results(const Vector<EretheaAssistantToolCall> &p_calls, const Vector<EretheaMcpToolResult> &p_results) override;
};
