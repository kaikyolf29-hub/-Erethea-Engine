/**************************************************************************/
/*  erethea_mcp_server.h                                                  */
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

#include "erethea_mcp_protocol.h"

#include "core/io/stream_peer_tcp.h"
#include "core/io/tcp_server.h"
#include "core/templates/hash_map.h"
#include "core/templates/list.h"
#include "core/variant/callable.h"

struct EretheaMcpHttpRequest {
	String method;
	String path;
	// Header names in lower case.
	HashMap<String, String> headers;
	PackedByteArray body;
	// HTTP/1.1 keeps the connection unless asked to close it.
	bool keep_alive = true;

	String get_header(const String &p_name) const;
};

struct EretheaMcpHttpResponse {
	int status = 200;
	String content_type;
	CharString body;
	Vector<String> headers;

	// A tool call whose result comes later.
	bool pending = false;
	uint64_t pending_id = 0;
	Variant request_id;
	EretheaMcpProtocol::Session session;

	void set_json(int p_status, const Variant &p_json);
};

// The minimum of HTTP/1.1 that MCP's "Streamable HTTP" transport needs, with
// JSON responses only (no server-sent events).
namespace EretheaMcpHttp {

enum ParseStatus {
	PARSE_INCOMPLETE,
	PARSE_OK,
	PARSE_ERROR,
};

constexpr int MAX_HEADER_SIZE = 16 * 1024;
constexpr int MAX_BODY_SIZE = 16 * 1024 * 1024;

// Parses the request at the start of `p_buffer`. With PARSE_OK, `r_consumed`
// is its size; with PARSE_ERROR, `r_status` is the HTTP status to answer.
ParseStatus parse_request(const PackedByteArray &p_buffer, EretheaMcpHttpRequest &r_request, int &r_consumed, int &r_status);

// Protection against DNS rebinding and against web pages: only requests made
// for this computer, from this computer.
bool is_local_host(const String &p_host);
bool is_local_origin(const String &p_origin);

String status_text(int p_status);
PackedByteArray serialize(const EretheaMcpHttpResponse &p_response, bool p_close);

} // namespace EretheaMcpHttp

// MCP server on 127.0.0.1. Everything runs on the thread that calls poll(),
// the editor's main thread, so tools can use the editor directly.
class EretheaMcpServer {
public:
	static constexpr uint16_t DEFAULT_PORT = 11481;
	static constexpr const char *ENDPOINT = "/mcp";
	// Idle connections close after this.
	static constexpr uint64_t IDLE_TIMEOUT_MSEC = 120000;

private:
	static constexpr int MAX_CLIENTS = 16;
	static constexpr int MAX_SESSIONS = 64;
	// Longest wait for a tool call that answers later.
	static constexpr uint64_t CALL_TIMEOUT_MSEC = 60000;

	struct Client {
		Ref<StreamPeerTCP> peer;
		PackedByteArray buffer;
		uint64_t last_activity_msec = 0;
		bool close_after_response = false;
		bool done = false;

		// A tool call waiting for its result.
		bool waiting = false;
		uint64_t pending_id = 0;
		uint64_t deadline_msec = 0;
		Variant request_id;
		EretheaMcpProtocol::Session session;
		Vector<String> response_headers;
	};

	Ref<TCPServer> tcp_server;
	List<Client> clients;
	HashMap<String, EretheaMcpProtocol::Session> sessions;
	EretheaMcpProtocol protocol;
	Callable status_callable;
	uint16_t port = 0;
	// A tool can run the editor's main loop, such as while it shows a progress
	// dialog, which comes back to poll() before the tool returns.
	bool polling = false;

	String _new_session_id() const;
	void _send(Client &p_client, const EretheaMcpHttpResponse &p_response);
	void _read(Client &p_client);
	void _process_buffer(Client &p_client);

public:
	EretheaMcpProtocol &get_protocol() { return protocol; }

	// Called for GET /status, which tells clients such as Fornalha that the
	// editor is there. Returns a Dictionary.
	void set_status_callable(const Callable &p_callable) { status_callable = p_callable; }

	// Handles one request without the network (also used by the tests).
	EretheaMcpHttpResponse handle_request(const EretheaMcpHttpRequest &p_request);

	Error start(uint16_t p_port);
	void stop();
	bool is_listening() const;
	uint16_t get_port() const { return port; }

	void poll();

	// Sends the result of a tool call that waited.
	void finish_call(uint64_t p_pending_id, const EretheaMcpToolResult &p_result);

	~EretheaMcpServer();
};
