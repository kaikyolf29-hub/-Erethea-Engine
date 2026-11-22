/**************************************************************************/
/*  erethea_mcp_protocol.cpp                                              */
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

#include "erethea_mcp_protocol.h"

#include "core/io/json.h"

const char *const EretheaMcpProtocol::VERSIONS[] = { "2025-11-25", "2025-06-18", "2025-03-26", "2024-11-05", nullptr };

// JSON-RPC error codes.
static constexpr int INVALID_REQUEST = -32600;
static constexpr int METHOD_NOT_FOUND = -32601;
static constexpr int INVALID_PARAMS = -32602;

EretheaMcpToolResult EretheaMcpToolResult::text(const String &p_text) {
	EretheaMcpToolResult result;
	Dictionary block;
	block["type"] = "text";
	block["text"] = p_text;
	result.content.push_back(block);
	return result;
}

EretheaMcpToolResult EretheaMcpToolResult::error(const String &p_message) {
	EretheaMcpToolResult result = text(p_message);
	result.is_error = true;
	return result;
}

EretheaMcpToolResult EretheaMcpToolResult::object(const Dictionary &p_result) {
	EretheaMcpToolResult result = text(JSON::stringify(p_result, "  ", false));
	result.structured = p_result;
	return result;
}

void EretheaMcpToolResult::add_image(const String &p_base64_png) {
	Dictionary block;
	block["type"] = "image";
	block["data"] = p_base64_png;
	block["mimeType"] = "image/png";
	content.push_back(block);
}

Dictionary EretheaMcpToolResult::to_dictionary() const {
	Dictionary dictionary;
	dictionary["content"] = content;
	dictionary["structured"] = structured;
	dictionary["is_error"] = is_error;
	return dictionary;
}

EretheaMcpToolResult EretheaMcpToolResult::from_dictionary(const Dictionary &p_dictionary) {
	EretheaMcpToolResult result;
	result.content = p_dictionary.get("content", Array());
	result.structured = p_dictionary.get("structured", Dictionary());
	result.is_error = p_dictionary.get("is_error", false);
	return result;
}

// JSON numbers arrive as floats: give whole request ids back as integers.
static Variant _clean_id(const Variant &p_id) {
	if (p_id.get_type() == Variant::FLOAT) {
		const double number = p_id;
		if (number == Math::floor(number) && Math::abs(number) < 9007199254740992.0) {
			return int64_t(number);
		}
	}
	return p_id;
}

Dictionary EretheaMcpProtocol::make_result(const Variant &p_id, const Variant &p_result) {
	Dictionary message;
	message["jsonrpc"] = "2.0";
	message["id"] = _clean_id(p_id);
	message["result"] = p_result;
	return message;
}

Dictionary EretheaMcpProtocol::make_error(const Variant &p_id, int p_code, const String &p_message) {
	Dictionary error;
	error["code"] = p_code;
	error["message"] = p_message;
	Dictionary message;
	message["jsonrpc"] = "2.0";
	message["id"] = _clean_id(p_id);
	message["error"] = error;
	return message;
}

Dictionary EretheaMcpProtocol::tool_response(const Variant &p_id, const EretheaMcpToolResult &p_result, const Session &p_session) {
	Dictionary result;
	result["content"] = p_result.content;
	result["isError"] = p_result.is_error;
	// Structured content came with the 2025-06-18 version.
	const bool structured = p_session.protocol_version.is_empty() || p_session.protocol_version >= "2025-06-18";
	if (structured && !p_result.structured.is_empty()) {
		result["structuredContent"] = p_result.structured;
	}
	return make_result(p_id, result);
}

Array EretheaMcpProtocol::describe_tools() const {
	Array tools;
	ERR_FAIL_NULL_V(handler, tools);
	for (const EretheaMcpTool &tool : handler->get_tools()) {
		Dictionary annotations;
		annotations["title"] = tool.title;
		annotations["readOnlyHint"] = tool.read_only;
		annotations["destructiveHint"] = tool.destructive;
		annotations["idempotentHint"] = tool.read_only;
		annotations["openWorldHint"] = false;

		Dictionary description;
		description["name"] = tool.name;
		description["title"] = tool.title;
		description["description"] = tool.description;
		description["inputSchema"] = tool.input_schema;
		description["annotations"] = annotations;
		tools.push_back(description);
	}
	return tools;
}

Variant EretheaMcpProtocol::_handle_tools_call(const Variant &p_id, const Dictionary &p_params, const Session &p_session, bool p_can_wait, Reply &r_reply) {
	const Variant name = p_params.get("name", Variant());
	if (name.get_type() != Variant::STRING) {
		return make_error(p_id, INVALID_PARAMS, "Missing the tool name (params.name).");
	}
	bool known = false;
	for (const EretheaMcpTool &tool : handler->get_tools()) {
		known = known || tool.name == String(name);
	}
	if (!known) {
		return make_error(p_id, INVALID_PARAMS, vformat("Unknown tool: %s", name));
	}
	const Variant arguments = p_params.get("arguments", Dictionary());
	if (arguments.get_type() != Variant::DICTIONARY && arguments.get_type() != Variant::NIL) {
		return make_error(p_id, INVALID_PARAMS, "The tool arguments (params.arguments) must be an object.");
	}

	EretheaMcpToolResult result;
	uint64_t pending_id = 0;
	if (handler->call_tool(name, arguments.get_type() == Variant::DICTIONARY ? Dictionary(arguments) : Dictionary(), p_can_wait, result, pending_id)) {
		return tool_response(p_id, result, p_session);
	}
	ERR_FAIL_COND_V_MSG(!p_can_wait, make_error(p_id, INVALID_REQUEST, "This tool can't run in a batch."), "A tool waited although it couldn't.");
	r_reply.pending = true;
	r_reply.pending_id = pending_id;
	r_reply.request_id = _clean_id(p_id);
	return Variant();
}

EretheaMcpProtocol::Reply EretheaMcpProtocol::handle(const Variant &p_message, Session &r_session) {
	Reply reply;
	ERR_FAIL_NULL_V(handler, reply);

	if (p_message.get_type() != Variant::ARRAY) {
		_handle_message(p_message, r_session, true, reply);
		return reply;
	}

	// A batch, from older protocol versions. Its tool calls can't wait.
	Array responses;
	for (const Variant &message : Array(p_message)) {
		Reply item;
		_handle_message(message, r_session, false, item);
		if (item.response.get_type() != Variant::NIL) {
			responses.push_back(item.response);
		}
	}
	if (!responses.is_empty()) {
		reply.response = responses;
	}
	return reply;
}

void EretheaMcpProtocol::_handle_message(const Variant &p_message, Session &r_session, bool p_can_wait, Reply &r_reply) {
	if (p_message.get_type() != Variant::DICTIONARY) {
		r_reply.response = make_error(Variant(), INVALID_REQUEST, "A JSON-RPC message must be an object.");
		return;
	}
	const Dictionary message = p_message;
	const Variant method = message.get("method", Variant());
	const Variant id = message.get("id", Variant());
	if (method.get_type() != Variant::STRING) {
		// A response from the client (the server asks nothing), or garbage.
		if (!message.has("result") && !message.has("error")) {
			r_reply.response = make_error(id, INVALID_REQUEST, "Missing the \"method\" field.");
		}
		return;
	}
	if (id.get_type() == Variant::NIL) {
		// A notification (notifications/initialized, notifications/cancelled...).
		return;
	}
	const Variant params_variant = message.get("params", Dictionary());
	const Dictionary params = params_variant.get_type() == Variant::DICTIONARY ? Dictionary(params_variant) : Dictionary();

	const String name = method;
	if (name == "initialize") {
		const String requested = params.get("protocolVersion", "");
		String version = VERSIONS[0];
		for (int i = 0; VERSIONS[i]; i++) {
			if (requested == VERSIONS[i]) {
				version = requested;
			}
		}
		r_session.protocol_version = version;
		const Variant client_info = params.get("clientInfo", Dictionary());
		if (client_info.get_type() == Variant::DICTIONARY) {
			r_session.client_name = Dictionary(client_info).get("name", "");
		}

		Dictionary tools;
		tools["listChanged"] = false;
		Dictionary capabilities;
		capabilities["tools"] = tools;
		Dictionary server_info;
		server_info["name"] = server_name;
		server_info["title"] = server_title;
		server_info["version"] = server_version;
		Dictionary result;
		result["protocolVersion"] = version;
		result["capabilities"] = capabilities;
		result["serverInfo"] = server_info;
		result["instructions"] = instructions;
		r_reply.response = make_result(id, result);
	} else if (name == "ping") {
		r_reply.response = make_result(id, Dictionary());
	} else if (name == "tools/list") {
		Dictionary result;
		result["tools"] = describe_tools();
		r_reply.response = make_result(id, result);
	} else if (name == "tools/call") {
		r_reply.response = _handle_tools_call(id, params, r_session, p_can_wait, r_reply);
	} else {
		r_reply.response = make_error(id, METHOD_NOT_FOUND, vformat("Method not supported: %s", name));
	}
}

void EretheaMcpProtocol::setup(EretheaMcpToolHandler *p_handler, const String &p_name, const String &p_title, const String &p_version, const String &p_instructions) {
	handler = p_handler;
	server_name = p_name;
	server_title = p_title;
	server_version = p_version;
	instructions = p_instructions;
}
