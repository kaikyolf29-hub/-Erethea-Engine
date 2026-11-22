/**************************************************************************/
/*  erethea_assistant_providers.cpp                                       */
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

#include "erethea_assistant_providers.h"

#include "core/io/json.h"

static String _stringify(const Variant &p_value) {
	// Keys in the order they were added, the same on every request, so that
	// the servers' prompt caches keep matching.
	return JSON::stringify(p_value, "", false);
}

static bool _parse_object(const String &p_text, Dictionary &r_object) {
	Ref<JSON> json;
	json.instantiate();
	if (json->parse(p_text) != OK || json->get_data().get_type() != Variant::DICTIONARY) {
		return false;
	}
	r_object = json->get_data();
	return true;
}

static Dictionary _object(const Variant &p_value) {
	return p_value.get_type() == Variant::DICTIONARY ? Dictionary(p_value) : Dictionary();
}

static Array _array(const Variant &p_value) {
	return p_value.get_type() == Variant::ARRAY ? Array(p_value) : Array();
}

static String _string(const Variant &p_value) {
	return p_value.get_type() == Variant::STRING || p_value.get_type() == Variant::STRING_NAME ? String(p_value) : String();
}

// Qwen3 and other models may think out loud before answering.
static String _without_thinking(const String &p_text) {
	const int end = p_text.rfind("</think>");
	return end >= 0 ? p_text.substr(end + String("</think>").length()).strip_edges() : p_text.strip_edges();
}

static int _count_images(const Array &p_mcp_content) {
	int images = 0;
	for (const Variant &block : p_mcp_content) {
		images += _string(_object(block).get("type", Variant())) == "image";
	}
	return images;
}

/* EretheaAssistantProvider */

bool EretheaAssistantProvider::is_retryable_status(int p_http_code) {
	return p_http_code == 429 || p_http_code == 500 || p_http_code == 502 || p_http_code == 503 || p_http_code == 529;
}

String EretheaAssistantProvider::get_result_text(const EretheaMcpToolResult &p_result) {
	PackedStringArray texts;
	for (const Variant &block : p_result.content) {
		const Dictionary dictionary = _object(block);
		if (_string(dictionary.get("type", Variant())) == "text") {
			texts.push_back(_string(dictionary.get("text", Variant())));
		}
	}
	return String("\n").join(texts);
}

String EretheaAssistantProvider::_error_message(const String &p_body) {
	Dictionary answer;
	if (!_parse_object(p_body, answer)) {
		const String text = p_body.strip_edges();
		return text.is_empty() ? String("no details") : text.left(300);
	}
	const Variant error = answer.get("error", Variant());
	if (error.get_type() == Variant::DICTIONARY) {
		const Dictionary details = error;
		const String message = _string(details.get("message", Variant()));
		const String code = _string(details.get("code", Variant()));
		if (!message.is_empty()) {
			return code.is_empty() || message.contains(code) ? message : vformat("%s (%s)", message, code);
		}
		return code.is_empty() ? _stringify(details).left(300) : code;
	}
	if (error.get_type() == Variant::STRING) {
		return error;
	}
	const String message = _string(answer.get("message", Variant()));
	return message.is_empty() ? p_body.left(300) : message;
}

void EretheaAssistantProvider::start(const String &p_system_prompt, const Vector<EretheaMcpTool> &p_tools) {
	system_prompt = p_system_prompt;
	tools = p_tools;
	retries_left = 1;
}

/* EretheaAssistantAnthropic */

static int _skip_space(const String &p_text, int p_at) {
	while (p_at < p_text.length() && (p_text[p_at] == ' ' || p_text[p_at] == '\t' || p_text[p_at] == '\n' || p_text[p_at] == '\r')) {
		p_at++;
	}
	return p_at;
}

// Just after the JSON string that starts at `p_at` (a quote), or -1.
static int _skip_string(const String &p_text, int p_at) {
	p_at++;
	while (p_at < p_text.length()) {
		const char32_t c = p_text[p_at];
		if (c == '\\') {
			p_at += 2;
		} else if (c == '"') {
			return p_at + 1;
		} else {
			p_at++;
		}
	}
	return -1;
}

// Just after the JSON value that starts at `p_at`, or -1.
static int _skip_value(const String &p_text, int p_at) {
	if (p_at >= p_text.length()) {
		return -1;
	}
	const char32_t first = p_text[p_at];
	if (first == '"') {
		return _skip_string(p_text, p_at);
	}
	if (first == '{' || first == '[') {
		int depth = 0;
		while (p_at < p_text.length()) {
			const char32_t c = p_text[p_at];
			if (c == '"') {
				p_at = _skip_string(p_text, p_at);
				if (p_at < 0) {
					return -1;
				}
				continue;
			}
			if (c == '{' || c == '[') {
				depth++;
			} else if (c == '}' || c == ']') {
				depth--;
				if (depth == 0) {
					return p_at + 1;
				}
			}
			p_at++;
		}
		return -1;
	}
	const int start = p_at;
	while (p_at < p_text.length() && !String(",}] \t\r\n").contains_char(p_text[p_at])) {
		p_at++;
	}
	return p_at > start ? p_at : -1;
}

bool EretheaAssistantAnthropic::find_value(const String &p_json, const String &p_key, int &r_begin, int &r_end) {
	int at = _skip_space(p_json, 0);
	if (at >= p_json.length() || p_json[at] != '{') {
		return false;
	}
	at++;
	while (true) {
		at = _skip_space(p_json, at);
		if (at >= p_json.length() || p_json[at] != '"') {
			return false;
		}
		const int key_end = _skip_string(p_json, at);
		if (key_end < 0) {
			return false;
		}
		const String key = p_json.substr(at + 1, key_end - at - 2);
		at = _skip_space(p_json, key_end);
		if (at >= p_json.length() || p_json[at] != ':') {
			return false;
		}
		const int value_begin = _skip_space(p_json, at + 1);
		const int value_end = _skip_value(p_json, value_begin);
		if (value_end < 0) {
			return false;
		}
		if (key == p_key) {
			r_begin = value_begin;
			r_end = value_end;
			return true;
		}
		at = _skip_space(p_json, value_end);
		if (at >= p_json.length() || p_json[at] != ',') {
			return false;
		}
		at++;
	}
}

Array EretheaAssistantAnthropic::to_content_blocks(const Array &p_mcp_content) {
	Array blocks;
	for (const Variant &item : p_mcp_content) {
		const Dictionary block = _object(item);
		const String type = _string(block.get("type", Variant()));
		if (type == "text") {
			Dictionary text;
			text["type"] = "text";
			text["text"] = _string(block.get("text", Variant()));
			blocks.push_back(text);
		} else if (type == "image") {
			Dictionary source;
			source["type"] = "base64";
			const String mime_type = _string(block.get("mimeType", Variant()));
			source["media_type"] = mime_type.is_empty() ? String("image/png") : mime_type;
			source["data"] = _string(block.get("data", Variant()));
			Dictionary image;
			image["type"] = "image";
			image["source"] = source;
			blocks.push_back(image);
		}
	}
	return blocks;
}

void EretheaAssistantAnthropic::setup(const String &p_api_key, const String &p_model, const String &p_url) {
	api_key = p_api_key;
	model = p_model.strip_edges().is_empty() ? String(DEFAULT_MODEL) : p_model.strip_edges();
	url = p_url;
}

void EretheaAssistantAnthropic::start(const String &p_system_prompt, const Vector<EretheaMcpTool> &p_tools) {
	EretheaAssistantProvider::start(p_system_prompt, p_tools);
	messages.clear();
	question_start = -1;
	use_fallbacks = true;
	stripped_thinking = false;
}

void EretheaAssistantAnthropic::add_user_message(const String &p_context, const String &p_text) {
	question_start = messages.size();
	retries_left = 1;
	Dictionary text;
	text["type"] = "text";
	text["text"] = p_context.is_empty() ? p_text : p_context + "\n\n" + p_text;
	Dictionary message;
	message["role"] = "user";
	message["content"] = Array{ text };
	messages.push_back(_stringify(message));
}

EretheaAssistantRequest EretheaAssistantAnthropic::make_request() const {
	Array tool_list;
	for (const EretheaMcpTool &tool : tools) {
		Dictionary definition;
		definition["name"] = tool.name;
		definition["description"] = tool.description;
		definition["input_schema"] = tool.input_schema;
		tool_list.push_back(definition);
	}
	Dictionary cache_control;
	cache_control["type"] = "ephemeral";
	Dictionary tool_choice;
	tool_choice["type"] = "auto";

	Dictionary body;
	body["model"] = model;
	body["max_tokens"] = MAX_TOKENS;
	body["cache_control"] = cache_control;
	body["system"] = system_prompt;
	body["tools"] = tool_list;
	body["tool_choice"] = tool_choice;
	if (use_fallbacks) {
		body["fallbacks"] = "default";
	}

	PackedStringArray message_list;
	for (const String &message : messages) {
		message_list.push_back(message);
	}
	// The messages go in as text, so that Claude's answers stay byte for byte.
	String text = _stringify(body);
	text = text.substr(0, text.length() - 1) + ",\"messages\":[" + String(",").join(message_list) + "]}";

	EretheaAssistantRequest request;
	request.url = url;
	request.body = text;
	request.headers.push_back("content-type: application/json");
	request.headers.push_back("x-api-key: " + api_key);
	request.headers.push_back(vformat("anthropic-version: %s", API_VERSION));
	if (use_fallbacks) {
		request.headers.push_back(vformat("anthropic-beta: %s", FALLBACK_BETA));
	}
	return request;
}

void EretheaAssistantAnthropic::_strip_thinking() {
	for (String &text : messages) {
		Dictionary message;
		if (!_parse_object(text, message) || _string(message.get("role", Variant())) != "assistant") {
			continue;
		}
		Array kept;
		for (const Variant &block : _array(message.get("content", Variant()))) {
			const String type = _string(_object(block).get("type", Variant()));
			if (type != "thinking" && type != "redacted_thinking") {
				kept.push_back(block);
			}
		}
		if (kept.is_empty()) {
			Dictionary empty;
			empty["type"] = "text";
			empty["text"] = "(no text)";
			kept.push_back(empty);
		}
		message["content"] = kept;
		text = _stringify(message);
	}
}

EretheaAssistantReply EretheaAssistantAnthropic::read_response(int p_http_code, const String &p_body) {
	EretheaAssistantReply reply;
	if (p_http_code != 200) {
		if (p_http_code == 400 && p_body.contains("bound to a different conversation") && !stripped_thinking) {
			// The history no longer matches what Claude's thinking was made in.
			// Without the thinking blocks, it can go on.
			_strip_thinking();
			stripped_thinking = true;
			reply.status = EretheaAssistantReply::STATUS_RETRY;
			return reply;
		}
		if (p_http_code == 400 && use_fallbacks && p_body.to_lower().contains("fallback")) {
			use_fallbacks = false;
			reply.status = EretheaAssistantReply::STATUS_RETRY;
			return reply;
		}
		if (is_retryable_status(p_http_code) && retries_left > 0) {
			retries_left--;
			reply.status = EretheaAssistantReply::STATUS_RETRY;
			reply.retry_after = 3.0;
			return reply;
		}
		reply.error = vformat("Claude answered with HTTP %d: %s", p_http_code, _error_message(p_body));
		cancel_question();
		return reply;
	}

	Dictionary answer;
	if (!_parse_object(p_body, answer)) {
		reply.error = "Claude's answer isn't JSON.";
		cancel_question();
		return reply;
	}
	retries_left = 1;
	const String stop_reason = _string(answer.get("stop_reason", Variant()));
	if (stop_reason == "refusal") {
		const Dictionary details = _object(answer.get("stop_details", Variant()));
		const String category = _string(details.get("category", Variant()));
		const String explanation = _string(details.get("explanation", Variant()));
		reply.status = EretheaAssistantReply::STATUS_REFUSED;
		reply.error = category.is_empty() ? String("Claude declined to answer this.") : vformat("Claude declined to answer this (%s).", category);
		if (!explanation.is_empty()) {
			reply.error += " " + explanation;
		}
		// Removing the end of the history is allowed.
		cancel_question();
		return reply;
	}

	String text;
	for (const Variant &item : _array(answer.get("content", Variant()))) {
		const Dictionary block = _object(item);
		const String type = _string(block.get("type", Variant()));
		if (type == "text") {
			text += _string(block.get("text", Variant()));
		} else if (type == "tool_use") {
			EretheaAssistantToolCall call;
			call.id = _string(block.get("id", Variant()));
			call.name = _string(block.get("name", Variant()));
			const Variant input = block.get("input", Variant());
			if (input.get_type() == Variant::DICTIONARY) {
				call.arguments = input;
			} else {
				call.invalid = "The arguments must be a JSON object.";
			}
			reply.tool_calls.push_back(call);
		} else if (type == "fallback") {
			const String to = _string(_object(block.get("to", Variant())).get("model", Variant()));
			if (!to.is_empty()) {
				reply.notes.push_back(vformat("Answered by %s.", to));
			}
		}
	}
	if (stop_reason == "max_tokens" && !reply.tool_calls.is_empty()) {
		reply.tool_calls.clear();
		reply.error = "Claude's answer reached its length limit before its actions could run. Ask for less at a time.";
		cancel_question();
		return reply;
	}

	int begin = 0;
	int end = 0;
	if (!find_value(p_body, "content", begin, end)) {
		reply.tool_calls.clear();
		reply.error = "Claude's answer has no content.";
		cancel_question();
		return reply;
	}
	messages.push_back("{\"role\":\"assistant\",\"content\":" + p_body.substr(begin, end - begin) + "}");

	reply.text = text.strip_edges();
	if (!reply.tool_calls.is_empty()) {
		reply.status = EretheaAssistantReply::STATUS_TOOLS;
	} else if (stop_reason == "pause_turn") {
		// The answer goes on in the next request.
		reply.status = EretheaAssistantReply::STATUS_RETRY;
	} else {
		reply.status = EretheaAssistantReply::STATUS_DONE;
		if (stop_reason == "max_tokens") {
			reply.notes.push_back("The answer reached its length limit.");
		}
	}
	return reply;
}

void EretheaAssistantAnthropic::add_tool_results(const Vector<EretheaAssistantToolCall> &p_calls, const Vector<EretheaMcpToolResult> &p_results) {
	Array content;
	for (int i = 0; i < p_calls.size() && i < p_results.size(); i++) {
		Array blocks = to_content_blocks(p_results[i].content);
		if (blocks.is_empty()) {
			Dictionary done;
			done["type"] = "text";
			done["text"] = p_results[i].is_error ? "Failed." : "Done.";
			blocks.push_back(done);
		}
		Dictionary result;
		result["type"] = "tool_result";
		result["tool_use_id"] = p_calls[i].id;
		result["content"] = blocks;
		if (p_results[i].is_error) {
			result["is_error"] = true;
		}
		content.push_back(result);
	}
	// All of the results in one message.
	Dictionary message;
	message["role"] = "user";
	message["content"] = content;
	messages.push_back(_stringify(message));
}

void EretheaAssistantAnthropic::cancel_question() {
	if (question_start >= 0 && question_start <= int(messages.size())) {
		messages.resize(question_start);
	}
	question_start = -1;
}

/* EretheaAssistantOpenAI */

String EretheaAssistantOpenAI::get_endpoint(const String &p_base_url) {
	String url = p_base_url.strip_edges();
	while (url.ends_with("/")) {
		url = url.left(-1);
	}
	return url.ends_with("/chat/completions") ? url : url + "/chat/completions";
}

void EretheaAssistantOpenAI::setup(const String &p_base_url, const String &p_api_key, const String &p_model) {
	base_url = p_base_url;
	api_key = p_api_key.strip_edges();
	model = p_model.strip_edges();
}

void EretheaAssistantOpenAI::start(const String &p_system_prompt, const Vector<EretheaMcpTool> &p_tools) {
	EretheaAssistantProvider::start(p_system_prompt, p_tools);
	messages.clear();
	question_start = -1;
}

void EretheaAssistantOpenAI::add_user_message(const String &p_context, const String &p_text) {
	question_start = messages.size();
	retries_left = 1;
	Dictionary message;
	message["role"] = "user";
	message["content"] = p_context.is_empty() ? p_text : p_context + "\n\n" + p_text;
	messages.push_back(message);
}

EretheaAssistantRequest EretheaAssistantOpenAI::make_request() const {
	Dictionary system;
	system["role"] = "system";
	system["content"] = system_prompt;
	Array all = Array{ system };
	all.append_array(messages);

	Array tool_list;
	for (const EretheaMcpTool &tool : tools) {
		Dictionary function;
		function["name"] = tool.name;
		function["description"] = tool.description;
		function["parameters"] = tool.input_schema;
		Dictionary definition;
		definition["type"] = "function";
		definition["function"] = function;
		tool_list.push_back(definition);
	}

	Dictionary body;
	if (!model.is_empty()) {
		body["model"] = model;
	}
	body["messages"] = all;
	body["tools"] = tool_list;
	body["tool_choice"] = "auto";

	EretheaAssistantRequest request;
	request.url = get_endpoint(base_url);
	request.body = _stringify(body);
	request.headers.push_back("Content-Type: application/json");
	if (!api_key.is_empty()) {
		request.headers.push_back("Authorization: Bearer " + api_key);
	}
	return request;
}

Dictionary EretheaAssistantOpenAI::_message_of(const Dictionary &p_answer, String &r_finish_reason, String &r_error) {
	const Array choices = _array(p_answer.get("choices", Variant()));
	if (choices.is_empty()) {
		r_error = "The answer has no choices.";
		return Dictionary();
	}
	const Dictionary choice = _object(choices[0]);
	r_finish_reason = _string(choice.get("finish_reason", Variant()));
	const Dictionary message = _object(choice.get("message", Variant()));
	if (message.is_empty()) {
		r_error = "The answer has no message.";
	}
	return message;
}

EretheaAssistantReply EretheaAssistantOpenAI::read_response(int p_http_code, const String &p_body) {
	EretheaAssistantReply reply;
	if (p_http_code != 200) {
		if (is_retryable_status(p_http_code) && retries_left > 0) {
			retries_left--;
			reply.status = EretheaAssistantReply::STATUS_RETRY;
			reply.retry_after = 3.0;
			return reply;
		}
		reply.error = vformat("The server answered with HTTP %d: %s", p_http_code, _error_message(p_body));
		cancel_question();
		return reply;
	}
	Dictionary answer;
	String finish_reason;
	String error;
	Dictionary message;
	if (!_parse_object(p_body, answer)) {
		error = "The answer isn't JSON.";
	} else {
		message = _message_of(answer, finish_reason, error);
	}
	if (!error.is_empty()) {
		reply.error = error;
		cancel_question();
		return reply;
	}
	retries_left = 1;

	const String content = _without_thinking(_string(message.get("content", Variant())));
	Array tool_calls;
	for (const Variant &item : _array(message.get("tool_calls", Variant()))) {
		const Dictionary tool_call = _object(item);
		const Dictionary function = _object(tool_call.get("function", Variant()));
		EretheaAssistantToolCall call;
		call.id = _string(tool_call.get("id", Variant()));
		if (call.id.is_empty()) {
			call.id = vformat("call_%d_%d", messages.size(), reply.tool_calls.size());
		}
		call.name = _string(function.get("name", Variant()));
		const Variant arguments = function.get("arguments", Variant());
		String arguments_text;
		if (arguments.get_type() == Variant::DICTIONARY) {
			call.arguments = arguments;
			arguments_text = _stringify(arguments);
		} else {
			arguments_text = _string(arguments);
			if (!arguments_text.strip_edges().is_empty() && !_parse_object(arguments_text, call.arguments)) {
				call.invalid = "The arguments aren't a valid JSON object.";
			}
		}
		reply.tool_calls.push_back(call);

		Dictionary sent_function;
		sent_function["name"] = call.name;
		sent_function["arguments"] = arguments_text.is_empty() ? String("{}") : arguments_text;
		Dictionary sent_call;
		sent_call["id"] = call.id;
		sent_call["type"] = "function";
		sent_call["function"] = sent_function;
		tool_calls.push_back(sent_call);
	}
	if (finish_reason == "length" && !reply.tool_calls.is_empty()) {
		reply.tool_calls.clear();
		reply.error = "The answer reached its length limit before its actions could run. Ask for less at a time.";
		cancel_question();
		return reply;
	}

	Dictionary assistant;
	assistant["role"] = "assistant";
	assistant["content"] = content.is_empty() && !tool_calls.is_empty() ? Variant() : Variant(content);
	if (!tool_calls.is_empty()) {
		assistant["tool_calls"] = tool_calls;
	}
	messages.push_back(assistant);

	reply.text = content;
	reply.status = reply.tool_calls.is_empty() ? EretheaAssistantReply::STATUS_DONE : EretheaAssistantReply::STATUS_TOOLS;
	if (finish_reason == "length") {
		reply.notes.push_back("The answer reached its length limit.");
	}
	return reply;
}

void EretheaAssistantOpenAI::add_tool_results(const Vector<EretheaAssistantToolCall> &p_calls, const Vector<EretheaMcpToolResult> &p_results) {
	for (int i = 0; i < p_calls.size() && i < p_results.size(); i++) {
		String text = get_result_text(p_results[i]);
		if (p_results[i].is_error) {
			text = "Error: " + text;
		}
		if (_count_images(p_results[i].content) > 0) {
			text += "\n[The result has a screenshot, which this server can't receive.]";
		}
		Dictionary message;
		message["role"] = "tool";
		message["tool_call_id"] = p_calls[i].id;
		message["content"] = text.is_empty() ? String("Done.") : text;
		messages.push_back(message);
	}
}

void EretheaAssistantOpenAI::cancel_question() {
	if (question_start >= 0 && question_start <= messages.size()) {
		messages.resize(question_start);
	}
	question_start = -1;
}

/* EretheaAssistantIgnis */

String EretheaAssistantIgnis::extract_json(const String &p_text) {
	const String text = _without_thinking(p_text);
	const int first = text.find_char('{');
	const int last = text.rfind_char('}');
	if (first < 0 || last < first) {
		return String();
	}
	return text.substr(first, last - first + 1);
}

static bool _matches_type(const String &p_type, Variant &r_value) {
	switch (r_value.get_type()) {
		case Variant::STRING:
			return p_type.is_empty() || p_type == "string";
		case Variant::BOOL:
			return p_type.is_empty() || p_type == "boolean";
		case Variant::ARRAY:
			return p_type.is_empty() || p_type == "array";
		case Variant::DICTIONARY:
			return p_type.is_empty() || p_type == "object";
		case Variant::INT:
			return p_type.is_empty() || p_type == "integer" || p_type == "number";
		case Variant::FLOAT: {
			if (p_type == "integer") {
				// JSON numbers arrive as floats.
				const double number = r_value;
				if (number != Math::floor(number)) {
					return false;
				}
				r_value = int64_t(number);
				return true;
			}
			return p_type.is_empty() || p_type == "number";
		}
		default:
			return false;
	}
}

bool EretheaAssistantIgnis::check_arguments(const Dictionary &p_schema, const Dictionary &p_arguments, Dictionary &r_checked, String &r_problem) {
	r_checked.clear();
	const Dictionary properties = _object(p_schema.get("properties", Variant()));
	for (const KeyValue<Variant, Variant> &argument : p_arguments) {
		const String name = argument.key;
		if (!properties.has(name)) {
			continue;
		}
		Variant value = argument.value;
		if (_matches_type(_string(_object(properties[name]).get("type", Variant())), value)) {
			r_checked[name] = value;
		}
	}
	for (const Variant &required : _array(p_schema.get("required", Variant()))) {
		if (!r_checked.has(required)) {
			r_problem = vformat("\"%s\" is missing or of the wrong type", String(required));
			return false;
		}
	}
	return true;
}

Dictionary EretheaAssistantIgnis::get_response_format(const Vector<EretheaMcpTool> &p_tools) {
	Array names;
	for (const EretheaMcpTool &tool : p_tools) {
		names.push_back(tool.name);
	}
	Dictionary tool_property;
	tool_property["type"] = "string";
	tool_property["enum"] = names;
	Dictionary arguments_property;
	arguments_property["type"] = "object";
	Dictionary action_properties;
	action_properties["tool"] = tool_property;
	action_properties["arguments"] = arguments_property;
	Dictionary action;
	action["type"] = "object";
	action["properties"] = action_properties;
	action["required"] = Array{ "tool", "arguments" };

	Dictionary reply_property;
	reply_property["type"] = "string";
	Dictionary actions_property;
	actions_property["type"] = "array";
	actions_property["items"] = action;
	Dictionary properties;
	properties["reply"] = reply_property;
	properties["actions"] = actions_property;
	Dictionary schema;
	schema["type"] = "object";
	schema["properties"] = properties;
	schema["required"] = Array{ "reply", "actions" };
	schema["additionalProperties"] = false;

	Dictionary json_schema;
	json_schema["name"] = "erethea_assistant";
	json_schema["strict"] = true;
	json_schema["schema"] = schema;
	Dictionary format;
	format["type"] = "json_schema";
	format["json_schema"] = json_schema;
	return format;
}

void EretheaAssistantIgnis::start(const String &p_system_prompt, const Vector<EretheaMcpTool> &p_tools) {
	// Without native tools, the system prompt lists them.
	String prompt = p_system_prompt + "\n\nTools (name(arguments): what it does; ? marks optional arguments):\n";
	for (const EretheaMcpTool &tool : p_tools) {
		const Dictionary properties = _object(tool.input_schema.get("properties", Variant()));
		const Array required = _array(tool.input_schema.get("required", Variant()));
		PackedStringArray arguments;
		for (const KeyValue<Variant, Variant> &property : properties) {
			const String type = _string(_object(property.value).get("type", Variant()));
			arguments.push_back(vformat("%s%s: %s", String(property.key), required.has(property.key) ? "" : "?", type.is_empty() ? String("any") : type));
		}
		String description = tool.description;
		const int sentence_end = description.find(". ");
		if (sentence_end > 0) {
			description = description.left(sentence_end + 1);
		}
		prompt += vformat("- %s(%s): %s\n", tool.name, String(", ").join(arguments), description);
	}
	prompt += vformat("\nAnswer only with a JSON object: {\"reply\": \"short answer for the person\", \"actions\": [{\"tool\": \"<tool name>\", \"arguments\": {...}}]}. "
					  "The actions run in order and you get their results, so you can act again, up to %d times per question. "
					  "When nothing more has to be done, send an empty actions list and say in reply what changed.",
			MAX_ROUNDS);

	EretheaAssistantOpenAI::start(prompt, p_tools);
	rounds = 0;
	last_call_id = 0;
}

void EretheaAssistantIgnis::add_user_message(const String &p_context, const String &p_text) {
	EretheaAssistantOpenAI::add_user_message(p_context, p_text);
	rounds = 0;
}

EretheaAssistantRequest EretheaAssistantIgnis::make_request() const {
	Dictionary system;
	system["role"] = "system";
	system["content"] = system_prompt;
	Array all = Array{ system };
	// The last messages, and the whole current question.
	int first = MAX(0, messages.size() - RECENT_MESSAGES);
	if (question_start >= 0) {
		first = MIN(first, question_start);
	}
	for (int i = first; i < messages.size(); i++) {
		all.push_back(messages[i]);
	}

	Dictionary body;
	body["model"] = model.is_empty() ? String("default") : model;
	body["messages"] = all;
	body["response_format"] = get_response_format(tools);
	body["stream"] = false;

	EretheaAssistantRequest request;
	request.url = get_endpoint(base_url.is_empty() ? String(DEFAULT_URL) : base_url);
	request.body = _stringify(body);
	request.headers.push_back("Content-Type: application/json");
	if (!api_key.is_empty()) {
		request.headers.push_back("Authorization: Bearer " + api_key);
	}
	return request;
}

EretheaAssistantReply EretheaAssistantIgnis::read_response(int p_http_code, const String &p_body) {
	EretheaAssistantReply reply;
	if (p_http_code != 200) {
		if (is_retryable_status(p_http_code) && retries_left > 0) {
			retries_left--;
			reply.status = EretheaAssistantReply::STATUS_RETRY;
			reply.retry_after = 3.0;
			return reply;
		}
		String message = _error_message(p_body);
		if (message.contains("sem_modelo") || message.contains("sem_motor")) {
			message += ". Open Ignis and click Prepare (Preparar).";
		}
		reply.error = vformat("Ignis answered with HTTP %d: %s", p_http_code, message);
		cancel_question();
		return reply;
	}
	Dictionary answer;
	String finish_reason;
	String error;
	Dictionary message;
	if (!_parse_object(p_body, answer)) {
		error = "Ignis's answer isn't JSON.";
	} else {
		message = _message_of(answer, finish_reason, error);
	}
	if (!error.is_empty()) {
		reply.error = error;
		cancel_question();
		return reply;
	}
	retries_left = 1;

	const String content = _string(message.get("content", Variant()));
	const String json_text = extract_json(content);
	Dictionary result;
	if (json_text.is_empty() || !_parse_object(json_text, result)) {
		// Not the format asked for: show the text as the answer.
		const String text = _without_thinking(content);
		Dictionary assistant;
		assistant["role"] = "assistant";
		assistant["content"] = text;
		messages.push_back(assistant);
		reply.text = text;
		reply.status = EretheaAssistantReply::STATUS_DONE;
		return reply;
	}

	Dictionary assistant;
	assistant["role"] = "assistant";
	assistant["content"] = json_text;
	messages.push_back(assistant);
	reply.text = _string(result.get("reply", Variant())).strip_edges();

	for (const Variant &item : _array(result.get("actions", Variant()))) {
		const Dictionary action = _object(item);
		const String name = _string(action.get("tool", Variant()));
		const EretheaMcpTool *tool = nullptr;
		for (const EretheaMcpTool &candidate : tools) {
			if (candidate.name == name) {
				tool = &candidate;
			}
		}
		if (!tool) {
			reply.notes.push_back(vformat("Left out an action with an unknown tool, \"%s\".", name));
			continue;
		}
		EretheaAssistantToolCall call;
		String problem;
		if (!check_arguments(tool->input_schema, _object(action.get("arguments", Variant())), call.arguments, problem)) {
			reply.notes.push_back(vformat("Left out %s: %s.", name, problem));
			continue;
		}
		call.id = vformat("ignis_%d", ++last_call_id);
		call.name = name;
		reply.tool_calls.push_back(call);
	}

	if (reply.tool_calls.is_empty()) {
		reply.status = EretheaAssistantReply::STATUS_DONE;
	} else if (rounds >= MAX_ROUNDS) {
		reply.notes.push_back(vformat("Stopped after %d rounds of actions. Ask again to go on.", MAX_ROUNDS));
		reply.tool_calls.clear();
		reply.status = EretheaAssistantReply::STATUS_DONE;
	} else {
		rounds++;
		reply.status = EretheaAssistantReply::STATUS_TOOLS;
	}
	return reply;
}

void EretheaAssistantIgnis::add_tool_results(const Vector<EretheaAssistantToolCall> &p_calls, const Vector<EretheaMcpToolResult> &p_results) {
	String text = "Results of your actions:\n";
	for (int i = 0; i < p_calls.size() && i < p_results.size(); i++) {
		String result = get_result_text(p_results[i]).strip_edges();
		if (result.length() > MAX_RESULT_CHARACTERS) {
			result = result.left(MAX_RESULT_CHARACTERS) + "...";
		}
		if (_count_images(p_results[i].content) > 0) {
			result += " [screenshot taken]";
		}
		text += vformat("- %s %s: %s\n", p_calls[i].name, p_results[i].is_error ? "failed" : "done", result.is_empty() ? String("ok") : result);
	}
	text += "Answer with the JSON object: more actions, or an empty list and your reply.";
	Dictionary message;
	message["role"] = "user";
	message["content"] = text;
	messages.push_back(message);
}

/* EretheaAssistantClaudeCode */

String EretheaAssistantClaudeCode::make_mcp_config(int p_port) {
	Dictionary server;
	server["type"] = "http";
	server["url"] = vformat("http://127.0.0.1:%d/mcp", p_port);
	Dictionary servers;
	servers[SERVER_NAME] = server;
	Dictionary config;
	config["mcpServers"] = servers;
	return _stringify(config);
}

Vector<String> EretheaAssistantClaudeCode::make_arguments(const String &p_mcp_config_path, const String &p_system_prompt, const String &p_model, const String &p_resume_session_id) {
	Vector<String> arguments;
	arguments.push_back("-p");
	arguments.push_back("--input-format");
	arguments.push_back("stream-json");
	arguments.push_back("--output-format");
	arguments.push_back("stream-json");
	arguments.push_back("--verbose");
	arguments.push_back("--mcp-config");
	arguments.push_back(p_mcp_config_path);
	arguments.push_back("--strict-mcp-config");
	// No built-in tools: an empty list. The "=" keeps the empty value as one
	// argument on every system.
	arguments.push_back("--tools=");
	arguments.push_back("--allowedTools");
	arguments.push_back(vformat("mcp__%s", SERVER_NAME));
	arguments.push_back("--permission-mode");
	arguments.push_back("dontAsk");
	arguments.push_back("--max-turns");
	arguments.push_back(itos(MAX_TURNS));
	if (!p_system_prompt.is_empty()) {
		// The editor quotes arguments with spaces, but doesn't escape quotes
		// inside them on Windows.
		arguments.push_back("--append-system-prompt");
		arguments.push_back(p_system_prompt.replace("\"", "'"));
	}
	if (!p_model.strip_edges().is_empty()) {
		arguments.push_back("--model");
		arguments.push_back(p_model.strip_edges());
	}
	if (!p_resume_session_id.is_empty()) {
		arguments.push_back("--resume");
		arguments.push_back(p_resume_session_id);
	}
	return arguments;
}

String EretheaAssistantClaudeCode::make_user_message(const String &p_text) {
	Dictionary message;
	message["role"] = "user";
	message["content"] = p_text;
	Dictionary line;
	line["type"] = "user";
	line["message"] = message;
	line["parent_tool_use_id"] = Variant();
	return _stringify(line);
}

String EretheaAssistantClaudeCode::strip_tool_prefix(const String &p_name) {
	const String prefix = vformat("mcp__%s__", SERVER_NAME);
	return p_name.begins_with(prefix) ? p_name.substr(prefix.length()) : p_name;
}

EretheaAssistantClaudeCode::Event EretheaAssistantClaudeCode::parse_line(const String &p_line) {
	Event event;
	Dictionary line;
	if (!_parse_object(p_line.strip_edges(), line)) {
		return event;
	}
	const String type = _string(line.get("type", Variant()));
	event.session_id = _string(line.get("session_id", Variant()));
	if (type == "system" && _string(line.get("subtype", Variant())) == "init") {
		event.type = Event::TYPE_INIT;
	} else if (type == "assistant") {
		event.type = Event::TYPE_ASSISTANT;
		const Dictionary message = _object(line.get("message", Variant()));
		for (const Variant &item : _array(message.get("content", Variant()))) {
			const Dictionary block = _object(item);
			const String block_type = _string(block.get("type", Variant()));
			if (block_type == "text") {
				event.text += _string(block.get("text", Variant()));
			} else if (block_type == "tool_use") {
				EretheaAssistantToolCall call;
				call.id = _string(block.get("id", Variant()));
				call.name = strip_tool_prefix(_string(block.get("name", Variant())));
				call.arguments = _object(block.get("input", Variant()));
				event.tool_calls.push_back(call);
			}
		}
		event.text = event.text.strip_edges();
	} else if (type == "result") {
		event.type = Event::TYPE_RESULT;
		event.is_error = bool(line.get("is_error", false)) || _string(line.get("subtype", Variant())).begins_with("error");
		event.text = _string(line.get("result", Variant())).strip_edges();
		PackedStringArray errors;
		for (const Variant &error : _array(line.get("errors", Variant()))) {
			errors.push_back(_string(error));
		}
		if (event.is_error) {
			event.error = errors.is_empty() ? (event.text.is_empty() ? _string(line.get("subtype", Variant())) : event.text) : String("; ").join(errors);
		}
	}
	return event;
}
