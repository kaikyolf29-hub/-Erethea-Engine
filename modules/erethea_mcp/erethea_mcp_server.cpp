/**************************************************************************/
/*  erethea_mcp_server.cpp                                                */
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

#include "erethea_mcp_server.h"

#include "core/io/json.h"
#include "core/os/os.h"

static constexpr int PARSE_ERROR_CODE = -32700;

String EretheaMcpHttpRequest::get_header(const String &p_name) const {
	const String *value = headers.getptr(p_name.to_lower());
	return value ? *value : String();
}

void EretheaMcpHttpResponse::set_json(int p_status, const Variant &p_json) {
	status = p_status;
	content_type = "application/json";
	body = JSON::stringify(p_json, "", false).utf8();
}

static Dictionary _error_body(const String &p_message) {
	Dictionary error;
	error["error"] = p_message;
	return error;
}

namespace EretheaMcpHttp {

ParseStatus parse_request(const PackedByteArray &p_buffer, EretheaMcpHttpRequest &r_request, int &r_consumed, int &r_status) {
	const int size = p_buffer.size();
	const uint8_t *data = p_buffer.ptr();

	int header_end = -1;
	for (int i = 3; i < size && i < MAX_HEADER_SIZE; i++) {
		if (data[i - 3] == '\r' && data[i - 2] == '\n' && data[i - 1] == '\r' && data[i] == '\n') {
			header_end = i + 1;
			break;
		}
	}
	if (header_end == -1) {
		if (size >= MAX_HEADER_SIZE) {
			r_status = 431;
			return PARSE_ERROR;
		}
		return PARSE_INCOMPLETE;
	}

	const Vector<String> lines = String::utf8((const char *)data, header_end - 4).split("\r\n");
	const Vector<String> request_line = lines[0].split(" ", false);
	if (request_line.size() != 3 || !request_line[2].begins_with("HTTP/1.")) {
		r_status = 400;
		return PARSE_ERROR;
	}

	r_request = EretheaMcpHttpRequest();
	r_request.method = request_line[0];
	const String &target = request_line[1];
	const int query = target.find_char('?');
	r_request.path = query == -1 ? target : target.substr(0, query);
	for (int i = 1; i < lines.size(); i++) {
		const int colon = lines[i].find_char(':');
		if (colon <= 0) {
			r_status = 400;
			return PARSE_ERROR;
		}
		const String name = lines[i].substr(0, colon).strip_edges().to_lower();
		const String value = lines[i].substr(colon + 1).strip_edges();
		String *existing = r_request.headers.getptr(name);
		if (existing) {
			*existing += ", " + value;
		} else {
			r_request.headers.insert(name, value);
		}
	}

	const String connection = r_request.get_header("connection").to_lower();
	if (request_line[2] == "HTTP/1.0") {
		r_request.keep_alive = connection.contains("keep-alive");
	} else {
		r_request.keep_alive = !connection.contains("close");
	}

	// Chunked bodies aren't needed by MCP clients, which send JSON with its
	// length.
	if (r_request.headers.has("transfer-encoding")) {
		r_status = 411;
		return PARSE_ERROR;
	}
	int64_t length = 0;
	const String length_text = r_request.get_header("content-length");
	if (!length_text.is_empty()) {
		if (!length_text.is_valid_int() || length_text.to_int() < 0) {
			r_status = 400;
			return PARSE_ERROR;
		}
		length = length_text.to_int();
	}
	if (length > MAX_BODY_SIZE) {
		r_status = 413;
		return PARSE_ERROR;
	}
	if (size < header_end + length) {
		return PARSE_INCOMPLETE;
	}
	r_request.body = p_buffer.slice(header_end, header_end + length);
	r_consumed = header_end + length;
	return PARSE_OK;
}

bool is_local_host(const String &p_host) {
	String host = p_host.strip_edges().to_lower();
	if (host.begins_with("[")) {
		const int end = host.find_char(']');
		host = end == -1 ? String() : host.substr(1, end - 1);
	} else if (host.count(":") == 1) {
		host = host.get_slicec(':', 0);
	}
	return host == "127.0.0.1" || host == "localhost" || host == "::1";
}

bool is_local_origin(const String &p_origin) {
	const String origin = p_origin.strip_edges().to_lower();
	String rest;
	if (origin.begins_with("http://")) {
		rest = origin.substr(7);
	} else if (origin.begins_with("https://")) {
		rest = origin.substr(8);
	} else {
		// Including "null", which any web page can send from a sandboxed frame.
		return false;
	}
	return is_local_host(rest.get_slicec('/', 0));
}

String status_text(int p_status) {
	switch (p_status) {
		case 200:
			return "OK";
		case 202:
			return "Accepted";
		case 204:
			return "No Content";
		case 400:
			return "Bad Request";
		case 403:
			return "Forbidden";
		case 404:
			return "Not Found";
		case 405:
			return "Method Not Allowed";
		case 411:
			return "Length Required";
		case 413:
			return "Content Too Large";
		case 431:
			return "Request Header Fields Too Large";
		case 503:
			return "Service Unavailable";
		default:
			return "Internal Server Error";
	}
}

PackedByteArray serialize(const EretheaMcpHttpResponse &p_response, bool p_close) {
	String head = vformat("HTTP/1.1 %d %s\r\n", p_response.status, status_text(p_response.status));
	if (!p_response.content_type.is_empty()) {
		head += "Content-Type: " + p_response.content_type + "\r\n";
	}
	head += vformat("Content-Length: %d\r\n", p_response.body.length());
	head += "Cache-Control: no-store\r\n";
	if (p_close) {
		head += "Connection: close\r\n";
	}
	for (const String &header : p_response.headers) {
		head += header + "\r\n";
	}
	head += "\r\n";

	const CharString head_utf8 = head.utf8();
	PackedByteArray bytes;
	bytes.resize(head_utf8.length() + p_response.body.length());
	memcpy(bytes.ptrw(), head_utf8.get_data(), head_utf8.length());
	if (p_response.body.length() > 0) {
		memcpy(bytes.ptrw() + head_utf8.length(), p_response.body.get_data(), p_response.body.length());
	}
	return bytes;
}

} // namespace EretheaMcpHttp

String EretheaMcpServer::_new_session_id() const {
	uint8_t bytes[16] = {};
	if (OS::get_singleton()->get_entropy(bytes, sizeof(bytes)) != OK) {
		// Not a secret, only an identifier: the time is enough then.
		const uint64_t ticks = OS::get_singleton()->get_ticks_usec();
		memcpy(bytes, &ticks, sizeof(ticks));
	}
	return String::hex_encode_buffer(bytes, sizeof(bytes));
}

EretheaMcpHttpResponse EretheaMcpServer::handle_request(const EretheaMcpHttpRequest &p_request) {
	EretheaMcpHttpResponse response;

	const String host = p_request.get_header("host");
	const String origin = p_request.get_header("origin");
	if ((!host.is_empty() && !EretheaMcpHttp::is_local_host(host)) || (!origin.is_empty() && !EretheaMcpHttp::is_local_origin(origin))) {
		response.set_json(403, _error_body("The Erethea MCP server only answers requests from this computer."));
		return response;
	}
	if (!origin.is_empty()) {
		// Local web tools, such as the MCP Inspector.
		response.headers.push_back("Access-Control-Allow-Origin: " + origin);
		response.headers.push_back("Access-Control-Expose-Headers: Mcp-Session-Id");
		response.headers.push_back("Vary: Origin");
	}
	if (p_request.method == "OPTIONS") {
		response.status = 204;
		response.headers.push_back("Access-Control-Allow-Methods: GET, POST, DELETE, OPTIONS");
		response.headers.push_back("Access-Control-Allow-Headers: Content-Type, Accept, Mcp-Session-Id, Mcp-Protocol-Version, Last-Event-ID");
		return response;
	}

	if (p_request.path == "/status") {
		if (p_request.method != "GET") {
			response.set_json(405, _error_body("Use GET."));
			response.headers.push_back("Allow: GET");
			return response;
		}
		Dictionary status = status_callable.is_valid() ? Dictionary(status_callable.call()) : Dictionary();
		status["mcp"] = vformat("http://127.0.0.1:%d%s", port, ENDPOINT);
		response.set_json(200, status);
		return response;
	}

	if (p_request.path != ENDPOINT) {
		response.set_json(404, _error_body(vformat("Unknown path. MCP is at %s.", ENDPOINT)));
		return response;
	}

	const String session_id = p_request.get_header("mcp-session-id");
	if (p_request.method == "DELETE") {
		response.status = sessions.erase(session_id) ? 204 : 404;
		return response;
	}
	if (p_request.method != "POST") {
		// No server-sent events: the server never starts a message.
		response.set_json(405, _error_body("Use POST to send MCP messages."));
		response.headers.push_back("Allow: POST, DELETE, OPTIONS");
		return response;
	}

	JSON json;
	const String text = String::utf8((const char *)p_request.body.ptr(), p_request.body.size());
	if (json.parse(text) != OK) {
		response.set_json(400, EretheaMcpProtocol::make_error(Variant(), PARSE_ERROR_CODE, vformat("Invalid JSON at line %d: %s", json.get_error_line(), json.get_error_message())));
		return response;
	}
	const Variant message = json.get_data();
	const bool initialize = message.get_type() == Variant::DICTIONARY && String(Dictionary(message).get("method", "")) == "initialize";

	EretheaMcpProtocol::Session session;
	if (!initialize && !session_id.is_empty()) {
		const EretheaMcpProtocol::Session *known = sessions.getptr(session_id);
		if (!known) {
			response.set_json(404, EretheaMcpProtocol::make_error(Variant(), -32001, "Unknown MCP session: start again with initialize."));
			return response;
		}
		session = *known;
	}
	// Without a session id, simple clients are still answered.

	const EretheaMcpProtocol::Reply reply = protocol.handle(message, session);

	if (initialize) {
		while (sessions.size() >= MAX_SESSIONS) {
			sessions.remove(sessions.begin());
		}
		const String new_id = _new_session_id();
		sessions.insert(new_id, session);
		response.headers.push_back("Mcp-Session-Id: " + new_id);
	}

	if (reply.pending) {
		response.pending = true;
		response.pending_id = reply.pending_id;
		response.request_id = reply.request_id;
		response.session = session;
		return response;
	}
	if (reply.response.get_type() == Variant::NIL) {
		// Notifications and the client's responses.
		response.status = 202;
		return response;
	}
	response.set_json(200, reply.response);
	return response;
}

void EretheaMcpServer::_send(Client &p_client, const EretheaMcpHttpResponse &p_response) {
	const PackedByteArray bytes = EretheaMcpHttp::serialize(p_response, p_client.close_after_response);
	if (p_client.peer->put_data(bytes.ptr(), bytes.size()) != OK || p_client.close_after_response) {
		p_client.done = true;
	}
}

void EretheaMcpServer::_read(Client &p_client) {
	p_client.peer->poll();
	if (p_client.peer->get_status() != StreamPeerTCP::STATUS_CONNECTED) {
		p_client.done = true;
		return;
	}
	int available = p_client.peer->get_available_bytes();
	while (available > 0 && p_client.buffer.size() <= EretheaMcpHttp::MAX_HEADER_SIZE + EretheaMcpHttp::MAX_BODY_SIZE) {
		const int old_size = p_client.buffer.size();
		const int chunk = MIN(available, 64 * 1024);
		p_client.buffer.resize(old_size + chunk);
		int received = 0;
		const Error error = p_client.peer->get_partial_data(p_client.buffer.ptrw() + old_size, chunk, received);
		p_client.buffer.resize(old_size + received);
		if (error != OK) {
			p_client.done = true;
			return;
		}
		if (received == 0) {
			break;
		}
		p_client.last_activity_msec = OS::get_singleton()->get_ticks_msec();
		available -= received;
	}
}

void EretheaMcpServer::_process_buffer(Client &p_client) {
	while (!p_client.waiting && !p_client.done && !p_client.buffer.is_empty()) {
		EretheaMcpHttpRequest request;
		int consumed = 0;
		int status = 0;
		const EretheaMcpHttp::ParseStatus parsed = EretheaMcpHttp::parse_request(p_client.buffer, request, consumed, status);
		if (parsed == EretheaMcpHttp::PARSE_INCOMPLETE) {
			return;
		}
		if (parsed == EretheaMcpHttp::PARSE_ERROR) {
			EretheaMcpHttpResponse response;
			response.set_json(status, _error_body(EretheaMcpHttp::status_text(status)));
			p_client.buffer.clear();
			p_client.close_after_response = true;
			_send(p_client, response);
			return;
		}
		p_client.buffer = p_client.buffer.slice(consumed);
		p_client.close_after_response = !request.keep_alive;

		const EretheaMcpHttpResponse response = handle_request(request);
		if (response.pending) {
			p_client.waiting = true;
			p_client.pending_id = response.pending_id;
			p_client.request_id = response.request_id;
			p_client.session = response.session;
			p_client.response_headers = response.headers;
			p_client.deadline_msec = OS::get_singleton()->get_ticks_msec() + CALL_TIMEOUT_MSEC;
			return;
		}
		_send(p_client, response);
	}
}

Error EretheaMcpServer::start(uint16_t p_port) {
	stop();
	if (tcp_server.is_null()) {
		tcp_server.instantiate();
	}
	// Only this computer can connect.
	const Error error = tcp_server->listen(p_port, IPAddress("127.0.0.1"));
	if (error == OK) {
		port = p_port;
	}
	return error;
}

void EretheaMcpServer::stop() {
	for (Client &client : clients) {
		client.peer->disconnect_from_host();
	}
	clients.clear();
	sessions.clear();
	if (tcp_server.is_valid()) {
		tcp_server->stop();
	}
	port = 0;
}

bool EretheaMcpServer::is_listening() const {
	return tcp_server.is_valid() && tcp_server->is_listening();
}

void EretheaMcpServer::poll() {
	if (!is_listening()) {
		return;
	}
	const uint64_t now = OS::get_singleton()->get_ticks_msec();

	while (tcp_server->is_connection_available()) {
		Ref<StreamPeerTCP> peer = tcp_server->take_connection();
		if (peer.is_null()) {
			break;
		}
		if (clients.size() >= MAX_CLIENTS) {
			peer->disconnect_from_host();
			continue;
		}
		peer->set_no_delay(true);
		Client client;
		client.peer = peer;
		client.last_activity_msec = now;
		clients.push_back(client);
	}

	for (Client &client : clients) {
		if (!client.done) {
			_read(client);
		}
		if (client.done) {
			continue;
		}
		if (client.waiting && now >= client.deadline_msec) {
			finish_call(client.pending_id, EretheaMcpToolResult::error("The tool didn't answer in time."));
		}
		_process_buffer(client);
		if (!client.waiting && now - client.last_activity_msec > IDLE_TIMEOUT_MSEC) {
			client.done = true;
		}
	}

	for (List<Client>::Element *E = clients.front(); E;) {
		List<Client>::Element *next = E->next();
		if (E->get().done) {
			E->get().peer->disconnect_from_host();
			clients.erase(E);
		}
		E = next;
	}
}

void EretheaMcpServer::finish_call(uint64_t p_pending_id, const EretheaMcpToolResult &p_result) {
	for (Client &client : clients) {
		if (!client.waiting || client.pending_id != p_pending_id) {
			continue;
		}
		EretheaMcpHttpResponse response;
		response.set_json(200, EretheaMcpProtocol::tool_response(client.request_id, p_result, client.session));
		response.headers = client.response_headers;
		client.waiting = false;
		client.last_activity_msec = OS::get_singleton()->get_ticks_msec();
		if (!client.done) {
			_send(client, response);
		}
		return;
	}
	// The client left: nobody to answer.
}

EretheaMcpServer::~EretheaMcpServer() {
	stop();
}
