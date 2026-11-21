/**************************************************************************/
/*  erethea_mcp_protocol.h                                                */
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

#include "core/templates/vector.h"
#include "core/variant/array.h"
#include "core/variant/dictionary.h"
#include "core/variant/variant.h"

// A tool that an MCP client (such as Claude Code) can call.
struct EretheaMcpTool {
	String name;
	String title;
	String description;
	// JSON Schema of the arguments.
	Dictionary input_schema;
	bool read_only = false;
	bool destructive = false;
};

// What a tool call returns: MCP content blocks, and optionally the same
// result as a JSON object for clients that read structured content.
struct EretheaMcpToolResult {
	Array content;
	Dictionary structured;
	bool is_error = false;

	static EretheaMcpToolResult text(const String &p_text);
	static EretheaMcpToolResult error(const String &p_message);
	// Answers with a JSON object, as text and as structured content.
	static EretheaMcpToolResult object(const Dictionary &p_result);
	void add_image(const String &p_base64_png);
};

class EretheaMcpToolHandler {
public:
	virtual Vector<EretheaMcpTool> get_tools() const = 0;

	// Runs a tool. Returns true with the result in `r_result`, or false when
	// the result comes later (for example, a screenshot that the running game
	// sends back): `r_pending_id` then identifies the call, and the handler
	// finishes it through EretheaMcpServer::finish_call(). A call can only
	// wait when `p_can_wait` is true.
	virtual bool call_tool(const String &p_name, const Dictionary &p_arguments, bool p_can_wait, EretheaMcpToolResult &r_result, uint64_t &r_pending_id) = 0;

	virtual ~EretheaMcpToolHandler() {}
};

// Handles MCP messages (JSON-RPC 2.0), whatever the transport.
// Only tools: `initialize`, `ping`, `tools/list` and `tools/call`.
class EretheaMcpProtocol {
public:
	// Protocol versions, newest first.
	static const char *const VERSIONS[];

	struct Session {
		String client_name;
		String protocol_version;
	};

	// What handling a message produced.
	struct Reply {
		// The response, or empty for a notification or a client's response.
		Variant response;
		// A tools/call whose result comes later: answer with tool_response().
		bool pending = false;
		uint64_t pending_id = 0;
		Variant request_id;
	};

private:
	EretheaMcpToolHandler *handler = nullptr;
	String server_name;
	String server_title;
	String server_version;
	String instructions;

	void _handle_message(const Variant &p_message, Session &r_session, bool p_can_wait, Reply &r_reply);
	Variant _handle_tools_call(const Variant &p_id, const Dictionary &p_params, const Session &p_session, bool p_can_wait, Reply &r_reply);

public:
	static Dictionary make_result(const Variant &p_id, const Variant &p_result);
	static Dictionary make_error(const Variant &p_id, int p_code, const String &p_message);
	static Dictionary tool_response(const Variant &p_id, const EretheaMcpToolResult &p_result, const Session &p_session);

	// Handles one message, or a batch (an array) of them. Tool calls inside a
	// batch can't wait.
	Reply handle(const Variant &p_message, Session &r_session);

	Array describe_tools() const;

	void setup(EretheaMcpToolHandler *p_handler, const String &p_name, const String &p_title, const String &p_version, const String &p_instructions);
};
