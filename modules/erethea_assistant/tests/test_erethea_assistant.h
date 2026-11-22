/**************************************************************************/
/*  test_erethea_assistant.h                                              */
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

#include "../editor/erethea_assistant_dock.h"
#include "../erethea_assistant_providers.h"

#include "core/io/json.h"
#include "tests/test_macros.h"

namespace TestEretheaAssistant {

static Vector<EretheaMcpTool> _tools() {
	Vector<EretheaMcpTool> tools;
	EretheaMcpTool create_node;
	create_node.name = "create_node";
	create_node.description = "Adds a node. One undoable action.";
	create_node.input_schema = JSON::parse_string(R"({"type": "object", "properties": {"parent": {"type": "string"}, "type": {"type": "string"}, "name": {"type": "string"}, "properties": {"type": "object"}}})");
	tools.push_back(create_node);
	EretheaMcpTool read_file;
	read_file.name = "read_file";
	read_file.description = "Reads a text file.";
	read_file.input_schema = JSON::parse_string(R"({"type": "object", "properties": {"path": {"type": "string"}, "max_lines": {"type": "integer"}}, "required": ["path"]})");
	tools.push_back(read_file);
	return tools;
}

static Dictionary _body(const EretheaAssistantRequest &p_request) {
	Ref<JSON> json;
	json.instantiate();
	REQUIRE(json->parse(p_request.body) == OK);
	return json->get_data();
}

static bool _has_header(const EretheaAssistantRequest &p_request, const String &p_header) {
	for (const String &header : p_request.headers) {
		if (header == p_header) {
			return true;
		}
	}
	return false;
}

TEST_CASE("[Erethea][Assistant] JSON values are found without parsing") {
	const String json = R"( {"id": "msg_1", "nested": {"content": "not this"}, "text": "a } \" { ]", "content" : [ {"type": "text", "text": "x}"} , 1.50 ], "after": true})";
	int begin = 0;
	int end = 0;
	REQUIRE(EretheaAssistantAnthropic::find_value(json, "content", begin, end));
	CHECK(json.substr(begin, end - begin) == R"([ {"type": "text", "text": "x}"} , 1.50 ])");
	REQUIRE(EretheaAssistantAnthropic::find_value(json, "after", begin, end));
	CHECK(json.substr(begin, end - begin) == "true");
	CHECK_FALSE(EretheaAssistantAnthropic::find_value(json, "missing", begin, end));
	CHECK_FALSE(EretheaAssistantAnthropic::find_value("[1, 2]", "content", begin, end));
	CHECK_FALSE(EretheaAssistantAnthropic::find_value(R"({"content": [1, 2)", "content", begin, end));
}

TEST_CASE("[Erethea][Assistant] Claude requests and tool use") {
	EretheaAssistantAnthropic claude;
	claude.setup("test-key", "");
	claude.start("System prompt.", _tools());
	claude.add_user_message("[Editor]", "Add an enemy.");

	EretheaAssistantRequest request = claude.make_request();
	CHECK(request.url == "https://api.anthropic.com/v1/messages");
	CHECK(_has_header(request, "x-api-key: test-key"));
	CHECK(_has_header(request, "anthropic-version: 2026-10-21"));
	CHECK(_has_header(request, "anthropic-beta: server-side-fallback-2026-07-01"));
	Dictionary body = _body(request);
	CHECK(String(body["model"]) == "claude-opus-5-5");
	CHECK(int(body["max_tokens"]) == 16000);
	CHECK(String(body["system"]) == "System prompt.");
	CHECK(String(Dictionary(body["tool_choice"])["type"]) == "auto");
	CHECK(String(Dictionary(body["cache_control"])["type"]) == "ephemeral");
	CHECK(String(body["fallbacks"]) == "default");
	// Claude Opus 5.5 always thinks, and rejects sampling parameters.
	CHECK_FALSE(body.has("thinking"));
	CHECK_FALSE(body.has("temperature"));
	CHECK(Array(body["tools"]).size() == 2);
	CHECK(String(Dictionary(Array(body["tools"])[1])["name"]) == "read_file");
	CHECK(Dictionary(Array(body["tools"])[1]).has("input_schema"));
	const Array messages = body["messages"];
	REQUIRE(messages.size() == 1);
	CHECK(String(Dictionary(Array(Dictionary(messages[0])["content"])[0])["text"]) == "[Editor]\n\nAdd an enemy.");

	// The answer's content goes back byte for byte, odd spacing and escapes
	// included, as Claude's thinking blocks require.
	const String content = R"([{"type":"thinking","thinking":"","signature":"c2ln+=="},{"type":"text","text":"Adding it."},  {"type":"tool_use","id":"toolu_1","name":"create_node","input":{"type":"CharacterBody3D","name":"Enemy","properties":{"position":[0, 1.0, 2]}}}])";
	const String answer = R"({"id":"msg_1","type":"message","role":"assistant","content":)" + content + R"(,"stop_reason":"tool_use","usage":{"input_tokens":10}})";
	EretheaAssistantReply reply = claude.read_response(200, answer);
	REQUIRE(reply.status == EretheaAssistantReply::STATUS_TOOLS);
	CHECK(reply.text == "Adding it.");
	REQUIRE(reply.tool_calls.size() == 1);
	CHECK(reply.tool_calls[0].id == "toolu_1");
	CHECK(reply.tool_calls[0].name == "create_node");
	CHECK(String(reply.tool_calls[0].arguments["name"]) == "Enemy");
	REQUIRE(claude.get_message_count() == 2);
	CHECK(claude.get_message(1) == R"({"role":"assistant","content":)" + content + "}");

	EretheaMcpToolResult result = EretheaMcpToolResult::text("Created ./Enemy.");
	result.add_image("iVBORw0KGgo=");
	claude.add_tool_results(reply.tool_calls, Vector<EretheaMcpToolResult>{ result });
	request = claude.make_request();
	CHECK(request.body.contains(R"(,"messages":[)"));
	CHECK(request.body.contains(claude.get_message(1)));
	body = _body(request);
	const Dictionary results_message = Array(body["messages"])[2];
	CHECK(String(results_message["role"]) == "user");
	const Dictionary tool_result = Array(results_message["content"])[0];
	CHECK(String(tool_result["type"]) == "tool_result");
	CHECK(String(tool_result["tool_use_id"]) == "toolu_1");
	CHECK_FALSE(tool_result.has("is_error"));
	const Array blocks = tool_result["content"];
	REQUIRE(blocks.size() == 2);
	CHECK(String(Dictionary(blocks[0])["text"]) == "Created ./Enemy.");
	const Dictionary image = blocks[1];
	CHECK(String(image["type"]) == "image");
	CHECK(String(Dictionary(image["source"])["type"]) == "base64");
	CHECK(String(Dictionary(image["source"])["media_type"]) == "image/png");
	CHECK(String(Dictionary(image["source"])["data"]) == "iVBORw0KGgo=");

	reply = claude.read_response(200, R"({"content":[{"type":"text","text":"Done: one enemy."}],"stop_reason":"end_turn"})");
	CHECK(reply.status == EretheaAssistantReply::STATUS_DONE);
	CHECK(reply.text == "Done: one enemy.");
	CHECK(claude.get_message_count() == 4);
}

TEST_CASE("[Erethea][Assistant] Claude refusals, retries and errors") {
	EretheaAssistantAnthropic claude;
	claude.setup("key", "claude-opus-5-5");
	claude.start("System.", _tools());
	claude.add_user_message("", "First.");
	REQUIRE(claude.read_response(200, R"({"content":[{"type":"text","text":"Hi."}],"stop_reason":"end_turn"})").status == EretheaAssistantReply::STATUS_DONE);
	REQUIRE(claude.get_message_count() == 2);

	SUBCASE("A refusal leaves the history as before the question") {
		claude.add_user_message("", "Second.");
		const EretheaAssistantReply reply = claude.read_response(200, R"({"content":[],"stop_reason":"refusal","stop_details":{"type":"refusal","category":"cyber","explanation":"Not this."}})");
		CHECK(reply.status == EretheaAssistantReply::STATUS_REFUSED);
		CHECK(reply.error.contains("cyber"));
		CHECK(claude.get_message_count() == 2);
	}

	SUBCASE("Busy servers get one more try, after a pause") {
		claude.add_user_message("", "Second.");
		EretheaAssistantReply reply = claude.read_response(529, R"({"type":"error","error":{"type":"overloaded_error","message":"Overloaded"}})");
		CHECK(reply.status == EretheaAssistantReply::STATUS_RETRY);
		CHECK(reply.retry_after == doctest::Approx(3.0));
		reply = claude.read_response(529, R"({"type":"error","error":{"type":"overloaded_error","message":"Overloaded"}})");
		CHECK(reply.status == EretheaAssistantReply::STATUS_ERROR);
		CHECK(reply.error.contains("Overloaded"));
		CHECK(claude.get_message_count() == 2);
	}

	SUBCASE("Thinking blocks bound to another conversation are left out") {
		claude.add_user_message("", "Second.");
		REQUIRE(claude.read_response(200, R"({"content":[{"type":"thinking","thinking":"","signature":"abc"},{"type":"tool_use","id":"t1","name":"read_file","input":{"path":"res://a.gd"}}],"stop_reason":"tool_use"})").status == EretheaAssistantReply::STATUS_TOOLS);
		CHECK(claude.get_message(3).contains("thinking"));
		const EretheaAssistantReply reply = claude.read_response(400, R"({"type":"error","error":{"type":"invalid_request_error","message":"messages.3.content.0: Invalid `signature` in `thinking` block. The block is bound to a different conversation."}})");
		CHECK(reply.status == EretheaAssistantReply::STATUS_RETRY);
		CHECK_FALSE(claude.get_message(3).contains("thinking"));
		CHECK(claude.get_message(3).contains("tool_use"));
	}

	SUBCASE("Without fallbacks when the server doesn't take them") {
		claude.add_user_message("", "Second.");
		const EretheaAssistantReply reply = claude.read_response(400, R"({"type":"error","error":{"type":"invalid_request_error","message":"fallbacks: Extra inputs are not permitted"}})");
		CHECK(reply.status == EretheaAssistantReply::STATUS_RETRY);
		const EretheaAssistantRequest request = claude.make_request();
		CHECK_FALSE(_body(request).has("fallbacks"));
		CHECK_FALSE(_has_header(request, "anthropic-beta: server-side-fallback-2026-07-01"));
	}

	SUBCASE("A cut answer doesn't run its tools") {
		claude.add_user_message("", "Second.");
		const EretheaAssistantReply reply = claude.read_response(200, R"({"content":[{"type":"tool_use","id":"t1","name":"read_file","input":{}}],"stop_reason":"max_tokens"})");
		CHECK(reply.status == EretheaAssistantReply::STATUS_ERROR);
		CHECK(reply.tool_calls.is_empty());
		CHECK(claude.get_message_count() == 2);
	}

	SUBCASE("Stopping a question removes it") {
		claude.add_user_message("", "Second.");
		REQUIRE(claude.read_response(200, R"({"content":[{"type":"tool_use","id":"t1","name":"read_file","input":{"path":"res://a.gd"}}],"stop_reason":"tool_use"})").status == EretheaAssistantReply::STATUS_TOOLS);
		claude.cancel_question();
		CHECK(claude.get_message_count() == 2);
	}
}

TEST_CASE("[Erethea][Assistant] OpenAI-compatible servers") {
	EretheaAssistantOpenAI server;
	server.setup("http://127.0.0.1:1234/v1/", "secret", "local-model");
	server.start("System.", _tools());
	server.add_user_message("[Editor]", "Read the script.");

	EretheaAssistantRequest request = server.make_request();
	CHECK(request.url == "http://127.0.0.1:1234/v1/chat/completions");
	CHECK(_has_header(request, "Authorization: Bearer secret"));
	Dictionary body = _body(request);
	CHECK(String(body["model"]) == "local-model");
	CHECK(String(Dictionary(Array(body["messages"])[0])["role"]) == "system");
	const Dictionary tool = Array(body["tools"])[1];
	CHECK(String(tool["type"]) == "function");
	CHECK(String(Dictionary(tool["function"])["name"]) == "read_file");

	EretheaAssistantReply reply = server.read_response(200, R"({"choices":[{"finish_reason":"tool_calls","message":{"role":"assistant","content":null,"tool_calls":[{"id":"call_1","type":"function","function":{"name":"read_file","arguments":"{\"path\": \"res://player.gd\"}"}},{"id":"call_2","type":"function","function":{"name":"read_file","arguments":"{not json"}}]}}]})");
	REQUIRE(reply.status == EretheaAssistantReply::STATUS_TOOLS);
	REQUIRE(reply.tool_calls.size() == 2);
	CHECK(String(reply.tool_calls[0].arguments["path"]) == "res://player.gd");
	CHECK(reply.tool_calls[0].invalid.is_empty());
	CHECK_FALSE(reply.tool_calls[1].invalid.is_empty());

	EretheaMcpToolResult image_result = EretheaMcpToolResult::text("extends Node");
	image_result.add_image("abc");
	server.add_tool_results(reply.tool_calls, Vector<EretheaMcpToolResult>{ image_result, EretheaMcpToolResult::error("Bad arguments.") });
	const Array messages = server.get_messages();
	REQUIRE(messages.size() == 4);
	CHECK(String(Dictionary(messages[2])["role"]) == "tool");
	CHECK(String(Dictionary(messages[2])["tool_call_id"]) == "call_1");
	CHECK(String(Dictionary(messages[2])["content"]).begins_with("extends Node"));
	CHECK(String(Dictionary(messages[2])["content"]).contains("screenshot"));
	CHECK(String(Dictionary(messages[3])["content"]) == "Error: Bad arguments.");

	reply = server.read_response(200, R"({"choices":[{"finish_reason":"stop","message":{"role":"assistant","content":"<think>hmm</think>It extends Node."}}]})");
	CHECK(reply.status == EretheaAssistantReply::STATUS_DONE);
	CHECK(reply.text == "It extends Node.");

	server.add_user_message("", "Again.");
	reply = server.read_response(401, R"({"error":{"message":"Invalid key","code":"invalid_api_key"}})");
	CHECK(reply.status == EretheaAssistantReply::STATUS_ERROR);
	CHECK(reply.error.contains("Invalid key"));
	CHECK(server.get_messages().size() == 5);
}

TEST_CASE("[Erethea][Assistant] Ignis answers with checked actions") {
	CHECK(EretheaAssistantIgnis::extract_json("<think>{\"no\": 1}</think>\nSure: {\"reply\": \"ok\", \"actions\": []} done") == R"({"reply": "ok", "actions": []})");
	CHECK(EretheaAssistantIgnis::extract_json("no json here").is_empty());

	const Vector<EretheaMcpTool> tools = _tools();
	Dictionary checked;
	String problem;
	CHECK(EretheaAssistantIgnis::check_arguments(tools[1].input_schema, JSON::parse_string(R"({"path": "res://a.gd", "max_lines": 20.0, "color": "red"})"), checked, problem));
	CHECK(checked.size() == 2);
	CHECK(checked["max_lines"].get_type() == Variant::INT);
	CHECK(EretheaAssistantIgnis::check_arguments(tools[1].input_schema, JSON::parse_string(R"({"path": "res://a.gd", "max_lines": 2.5})"), checked, problem));
	CHECK_FALSE(checked.has("max_lines"));
	CHECK_FALSE(EretheaAssistantIgnis::check_arguments(tools[1].input_schema, JSON::parse_string(R"({"path": 5})"), checked, problem));
	CHECK(problem.contains("path"));

	EretheaAssistantIgnis ignis;
	ignis.setup("", "", "");
	ignis.start("System.", tools);
	for (int i = 0; i < 5; i++) {
		ignis.add_user_message("", vformat("Old question %d.", i));
		REQUIRE(ignis.read_response(200, R"({"choices":[{"message":{"content":"{\"reply\": \"ok\", \"actions\": []}"}}]})").status == EretheaAssistantReply::STATUS_DONE);
	}
	ignis.add_user_message("[Editor]", "Add enemies.");
	EretheaAssistantRequest request = ignis.make_request();
	CHECK(request.url == "http://127.0.0.1:11470/v1/chat/completions");
	Dictionary body = _body(request);
	CHECK(String(body["model"]) == "default");
	CHECK_FALSE(body.has("tools"));
	CHECK(String(Dictionary(body["response_format"])["type"]) == "json_schema");
	const Array messages = body["messages"];
	// The system prompt and the last messages only.
	CHECK(messages.size() == 1 + EretheaAssistantIgnis::RECENT_MESSAGES);
	CHECK(String(Dictionary(messages[0])["content"]).contains("- read_file(path: string, max_lines?: integer): Reads a text file."));
	CHECK(String(Dictionary(messages[messages.size() - 1])["content"]) == "[Editor]\n\nAdd enemies.");

	const String answer = R"({"choices":[{"message":{"content":"<think>plan</think>{\"reply\": \"Adding.\", \"actions\": [{\"tool\": \"create_node\", \"arguments\": {\"type\": \"Node3D\", \"name\": \"Enemy\", \"speed\": 3}}, {\"tool\": \"fly\", \"arguments\": {}}, {\"tool\": \"read_file\", \"arguments\": {}}]}"}}]})";
	for (int round = 1; round <= EretheaAssistantIgnis::MAX_ROUNDS; round++) {
		const EretheaAssistantReply reply = ignis.read_response(200, answer);
		REQUIRE(reply.status == EretheaAssistantReply::STATUS_TOOLS);
		CHECK(reply.text == "Adding.");
		REQUIRE(reply.tool_calls.size() == 1);
		CHECK(reply.tool_calls[0].name == "create_node");
		CHECK_FALSE(reply.tool_calls[0].arguments.has("speed"));
		CHECK(reply.notes.size() == 2);
		ignis.add_tool_results(reply.tool_calls, Vector<EretheaMcpToolResult>{ EretheaMcpToolResult::text("Created ./Enemy.") });
	}
	const EretheaAssistantReply last = ignis.read_response(200, answer);
	CHECK(last.status == EretheaAssistantReply::STATUS_DONE);
	CHECK(last.tool_calls.is_empty());

	ignis.add_user_message("", "Again.");
	const EretheaAssistantReply error = ignis.read_response(503, R"({"error":{"code":"sem_modelo","message":"Nenhum modelo carregado"}})");
	CHECK(error.status == EretheaAssistantReply::STATUS_RETRY);
	const EretheaAssistantReply final_error = ignis.read_response(503, R"({"error":{"code":"sem_modelo","message":"Nenhum modelo carregado"}})");
	CHECK(final_error.status == EretheaAssistantReply::STATUS_ERROR);
	CHECK(final_error.error.contains("Prepare"));
}

TEST_CASE("[Erethea][Assistant] Scene tree summary") {
	const Dictionary tree = JSON::parse_string(R"({"scene": "res://main.tscn", "node_count": 4, "truncated": false, "root": {"name": "Main", "type": "Node3D", "path": ".", "children": [
		{"name": "Player", "type": "CharacterBody3D", "path": "Player", "script": "res://player.gd", "children": [{"name": "Camera", "type": "Camera3D", "path": "Player/Camera"}]},
		{"name": "Enemy", "type": "Node3D", "path": "Enemy", "scene": "res://enemy.tscn"}]}})");
	CHECK(EretheaAssistantDock::format_scene_tree(tree, 100) == ". Node3D \"Main\"\n  Player (CharacterBody3D) script=res://player.gd\n    Camera (Camera3D)\n  Enemy (Node3D) scene=res://enemy.tscn\n");
	CHECK(EretheaAssistantDock::format_scene_tree(tree, 2) == ". Node3D \"Main\"\n  Player (CharacterBody3D) script=res://player.gd\n(2 of 4 nodes shown; get_scene_tree lists them all)\n");

	EretheaAssistantToolCall call;
	call.name = "create_node";
	call.arguments["name"] = "Enemy";
	CHECK(EretheaAssistantDock::describe_call(call) == R"(create_node {"name":"Enemy"})");
}

TEST_CASE("[Erethea][Assistant] Claude Code command line and messages") {
	const Dictionary config = JSON::parse_string(EretheaAssistantClaudeCode::make_mcp_config(11481));
	CHECK(String(Dictionary(Dictionary(config["mcpServers"])["erethea"])["type"]) == "http");
	CHECK(String(Dictionary(Dictionary(config["mcpServers"])["erethea"])["url"]) == "http://127.0.0.1:11481/mcp");

	Vector<String> arguments = EretheaAssistantClaudeCode::make_arguments("/tmp/mcp.json", "Say \"hi\".", "", "");
	CHECK(arguments.find("-p") >= 0);
	CHECK(arguments[arguments.find("--input-format") + 1] == "stream-json");
	CHECK(arguments[arguments.find("--output-format") + 1] == "stream-json");
	CHECK(arguments.find("--verbose") >= 0);
	CHECK(arguments[arguments.find("--mcp-config") + 1] == "/tmp/mcp.json");
	CHECK(arguments.find("--strict-mcp-config") >= 0);
	// No built-in tools: only the editor's, which go through the undo history.
	CHECK(arguments.find("--tools=") >= 0);
	CHECK(arguments[arguments.find("--allowedTools") + 1] == "mcp__erethea");
	CHECK(arguments[arguments.find("--permission-mode") + 1] == "dontAsk");
	CHECK(arguments[arguments.find("--append-system-prompt") + 1] == "Say 'hi'.");
	CHECK(arguments.find("--model") < 0);
	CHECK(arguments.find("--resume") < 0);
	CHECK(arguments.find("--bare") < 0);
	arguments = EretheaAssistantClaudeCode::make_arguments("/tmp/mcp.json", "", "sonnet", "session-1");
	CHECK(arguments[arguments.find("--model") + 1] == "sonnet");
	CHECK(arguments[arguments.find("--resume") + 1] == "session-1");

	const Dictionary message = JSON::parse_string(EretheaAssistantClaudeCode::make_user_message("Add \"a\" tree.\nNow."));
	CHECK(String(message["type"]) == "user");
	CHECK(Dictionary(message["message"])["role"] == Variant("user"));
	CHECK(String(Dictionary(message["message"])["content"]) == "Add \"a\" tree.\nNow.");
	CHECK(message.has("parent_tool_use_id"));
	CHECK_FALSE(EretheaAssistantClaudeCode::make_user_message("x").contains("\n"));

	EretheaAssistantClaudeCode::Event event = EretheaAssistantClaudeCode::parse_line(R"({"type":"system","subtype":"init","session_id":"abc","tools":[],"mcp_servers":[{"name":"erethea","status":"connected"}]})");
	CHECK(event.type == EretheaAssistantClaudeCode::Event::TYPE_INIT);
	CHECK(event.session_id == "abc");
	event = EretheaAssistantClaudeCode::parse_line(R"({"type":"assistant","session_id":"abc","message":{"role":"assistant","content":[{"type":"text","text":"Placing it. "},{"type":"tool_use","id":"t1","name":"mcp__erethea__create_node","input":{"name":"Tree"}}]},"parent_tool_use_id":null})");
	CHECK(event.type == EretheaAssistantClaudeCode::Event::TYPE_ASSISTANT);
	CHECK(event.text == "Placing it.");
	REQUIRE(event.tool_calls.size() == 1);
	CHECK(event.tool_calls[0].name == "create_node");
	CHECK(String(event.tool_calls[0].arguments["name"]) == "Tree");
	event = EretheaAssistantClaudeCode::parse_line(R"({"type":"result","subtype":"success","is_error":false,"result":"Placed a tree.","session_id":"abc","num_turns":2})");
	CHECK(event.type == EretheaAssistantClaudeCode::Event::TYPE_RESULT);
	CHECK_FALSE(event.is_error);
	CHECK(event.text == "Placed a tree.");
	event = EretheaAssistantClaudeCode::parse_line(R"({"type":"result","subtype":"error_max_turns","is_error":true,"errors":["Reached the turn limit"],"session_id":"abc"})");
	CHECK(event.is_error);
	CHECK(event.error == "Reached the turn limit");
	event = EretheaAssistantClaudeCode::parse_line(R"({"type":"result","subtype":"success","is_error":true,"result":"Not logged in. Please run /login"})");
	CHECK(event.is_error);
	CHECK(event.error.contains("login"));
	CHECK(EretheaAssistantClaudeCode::parse_line("not json").type == EretheaAssistantClaudeCode::Event::TYPE_OTHER);
	CHECK(EretheaAssistantClaudeCode::parse_line(R"({"type":"stream_event"})").type == EretheaAssistantClaudeCode::Event::TYPE_OTHER);
	CHECK(EretheaAssistantClaudeCode::strip_tool_prefix("mcp__erethea__save_scene") == "save_scene");
	CHECK(EretheaAssistantClaudeCode::strip_tool_prefix("Read") == "Read");
}

} // namespace TestEretheaAssistant
