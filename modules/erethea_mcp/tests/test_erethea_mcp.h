/**************************************************************************/
/*  test_erethea_mcp.h                                                    */
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

#include "../editor/erethea_mcp_scene_tools.h"
#include "../erethea_mcp_protocol.h"
#include "../erethea_mcp_server.h"
#include "../erethea_mcp_values.h"

#include "core/io/json.h"
#include "scene/3d/node_3d.h"
#include "scene/resources/3d/primitive_meshes.h"
#include "tests/test_macros.h"

namespace TestEretheaMcp {

static PackedByteArray _bytes(const String &p_text) {
	const CharString utf8 = p_text.utf8();
	PackedByteArray bytes;
	bytes.resize(utf8.length());
	memcpy(bytes.ptrw(), utf8.get_data(), utf8.length());
	return bytes;
}

static EretheaMcpHttp::ParseStatus _parse(const String &p_text, EretheaMcpHttpRequest &r_request, int &r_status) {
	int consumed = 0;
	return EretheaMcpHttp::parse_request(_bytes(p_text), r_request, consumed, r_status);
}

class FakeTools : public EretheaMcpToolHandler {
public:
	bool last_can_wait = false;

	virtual Vector<EretheaMcpTool> get_tools() const override {
		Vector<EretheaMcpTool> tools;
		EretheaMcpTool echo;
		echo.name = "echo";
		echo.title = "Echo";
		echo.read_only = true;
		tools.push_back(echo);
		EretheaMcpTool wait;
		wait.name = "wait";
		wait.title = "Wait";
		tools.push_back(wait);
		return tools;
	}

	virtual bool call_tool(const String &p_name, const Dictionary &p_arguments, bool p_can_wait, EretheaMcpToolResult &r_result, uint64_t &r_pending_id) override {
		last_can_wait = p_can_wait;
		if (p_name == "wait") {
			if (p_can_wait) {
				r_pending_id = 7;
				return false;
			}
			r_result = EretheaMcpToolResult::error("can't wait");
			return true;
		}
		Dictionary result;
		result["echo"] = p_arguments.get("text", "");
		r_result = EretheaMcpToolResult::object(result);
		return true;
	}
};

static Dictionary _request(const Variant &p_id, const String &p_method, const Dictionary &p_params = Dictionary()) {
	Dictionary request;
	request["jsonrpc"] = "2.0";
	if (p_id.get_type() != Variant::NIL) {
		request["id"] = p_id;
	}
	request["method"] = p_method;
	request["params"] = p_params;
	return request;
}

TEST_CASE("[Modules][EretheaMcp] HTTP requests") {
	const String body = R"({"jsonrpc": "2.0", "id": 1, "method": "ping"})";
	const String first = "POST /mcp?x=1 HTTP/1.1\r\nHost: 127.0.0.1:11481\r\nContent-Type: application/json\r\nContent-Length: " + itos(body.utf8().length()) + "\r\n\r\n" + body;
	const String second = "GET /status HTTP/1.1\r\nHost: localhost\r\n\r\n";

	EretheaMcpHttpRequest request;
	int consumed = 0;
	int status = 0;
	const PackedByteArray both = _bytes(first + second);
	REQUIRE(EretheaMcpHttp::parse_request(both, request, consumed, status) == EretheaMcpHttp::PARSE_OK);
	CHECK(request.method == "POST");
	CHECK(request.path == "/mcp");
	CHECK(request.get_header("HOST") == "127.0.0.1:11481");
	CHECK(request.get_header("content-type") == "application/json");
	CHECK(String::utf8((const char *)request.body.ptr(), request.body.size()) == body);
	CHECK(request.keep_alive);
	CHECK(consumed == first.utf8().length());

	// The next request on the same connection.
	REQUIRE(EretheaMcpHttp::parse_request(both.slice(consumed), request, consumed, status) == EretheaMcpHttp::PARSE_OK);
	CHECK(request.method == "GET");
	CHECK(request.path == "/status");
	CHECK(request.body.is_empty());

	// Not all there yet.
	CHECK(_parse(first.substr(0, 20), request, status) == EretheaMcpHttp::PARSE_INCOMPLETE);
	CHECK(_parse(first.substr(0, first.length() - 5), request, status) == EretheaMcpHttp::PARSE_INCOMPLETE);

	CHECK(_parse("GET /mcp HTTP/1.1\r\nConnection: close\r\n\r\n", request, status) == EretheaMcpHttp::PARSE_OK);
	CHECK_FALSE(request.keep_alive);
	CHECK(_parse("GET /mcp HTTP/1.0\r\n\r\n", request, status) == EretheaMcpHttp::PARSE_OK);
	CHECK_FALSE(request.keep_alive);

	CHECK(_parse("NONSENSE\r\n\r\n", request, status) == EretheaMcpHttp::PARSE_ERROR);
	CHECK(status == 400);
	CHECK(_parse("POST /mcp HTTP/1.1\r\nTransfer-Encoding: chunked\r\n\r\n", request, status) == EretheaMcpHttp::PARSE_ERROR);
	CHECK(status == 411);
	CHECK(_parse("POST /mcp HTTP/1.1\r\nContent-Length: 999999999999\r\n\r\n", request, status) == EretheaMcpHttp::PARSE_ERROR);
	CHECK(status == 413);
	CHECK(_parse("POST /mcp HTTP/1.1\r\nContent-Length: -1\r\n\r\n", request, status) == EretheaMcpHttp::PARSE_ERROR);
	CHECK(status == 400);
}

TEST_CASE("[Modules][EretheaMcp] Only requests for and from this computer") {
	CHECK(EretheaMcpHttp::is_local_host("127.0.0.1:11481"));
	CHECK(EretheaMcpHttp::is_local_host("localhost"));
	CHECK(EretheaMcpHttp::is_local_host("LOCALHOST:80"));
	CHECK(EretheaMcpHttp::is_local_host("[::1]:11481"));
	CHECK_FALSE(EretheaMcpHttp::is_local_host("example.com"));
	CHECK_FALSE(EretheaMcpHttp::is_local_host("127.0.0.1.example.com"));
	CHECK_FALSE(EretheaMcpHttp::is_local_host("localhost.example.com:80"));
	CHECK_FALSE(EretheaMcpHttp::is_local_host("[::2]:80"));

	CHECK(EretheaMcpHttp::is_local_origin("http://localhost:6274"));
	CHECK(EretheaMcpHttp::is_local_origin("https://127.0.0.1"));
	CHECK_FALSE(EretheaMcpHttp::is_local_origin("https://example.com"));
	CHECK_FALSE(EretheaMcpHttp::is_local_origin("http://localhost.example.com"));
	CHECK_FALSE(EretheaMcpHttp::is_local_origin("null"));
	CHECK_FALSE(EretheaMcpHttp::is_local_origin("file://"));
}

TEST_CASE("[Modules][EretheaMcp] MCP messages") {
	FakeTools tools;
	EretheaMcpProtocol protocol;
	protocol.setup(&tools, "erethea", "Erethea Engine", "test", "Instructions.");
	EretheaMcpProtocol::Session session;

	Dictionary client_info;
	client_info["name"] = "claude-code";
	Dictionary params;
	params["protocolVersion"] = "2025-03-26";
	params["clientInfo"] = client_info;
	EretheaMcpProtocol::Reply reply = protocol.handle(_request(1, "initialize", params), session);
	Dictionary response = reply.response;
	Dictionary result = response["result"];
	CHECK(result["protocolVersion"] == "2025-03-26");
	CHECK(Dictionary(result["serverInfo"])["name"] == "erethea");
	CHECK(Dictionary(result["capabilities"]).has("tools"));
	CHECK(result["instructions"] == "Instructions.");
	CHECK(session.protocol_version == "2025-03-26");
	CHECK(session.client_name == "claude-code");

	// An unknown version gets the newest.
	EretheaMcpProtocol::Session other;
	params["protocolVersion"] = "1999-01-01";
	reply = protocol.handle(_request(1, "initialize", params), other);
	CHECK(Dictionary(Dictionary(reply.response)["result"])["protocolVersion"] == EretheaMcpProtocol::VERSIONS[0]);

	// Notifications and the client's responses get nothing back.
	CHECK(protocol.handle(_request(Variant(), "notifications/initialized"), session).response.get_type() == Variant::NIL);
	Dictionary client_response;
	client_response["jsonrpc"] = "2.0";
	client_response["id"] = 5;
	client_response["result"] = Dictionary();
	CHECK(protocol.handle(client_response, session).response.get_type() == Variant::NIL);

	// JSON numbers arrive as floats; whole ids go back as integers.
	response = protocol.handle(_request(3.0, "ping"), session).response;
	CHECK(response["id"].get_type() == Variant::INT);
	CHECK(int(response["id"]) == 3);
	CHECK(response.has("result"));

	response = protocol.handle(_request(4, "tools/list"), session).response;
	const Array listed = Dictionary(response["result"])["tools"];
	REQUIRE(listed.size() == 2);
	CHECK(Dictionary(listed[0])["name"] == "echo");
	CHECK(bool(Dictionary(Dictionary(listed[0])["annotations"])["readOnlyHint"]));

	Dictionary arguments;
	arguments["text"] = "hello";
	Dictionary call;
	call["name"] = "echo";
	call["arguments"] = arguments;
	response = protocol.handle(_request(5, "tools/call", call), session).response;
	result = response["result"];
	CHECK_FALSE(bool(result["isError"]));
	CHECK(String(Dictionary(Array(result["content"])[0])["text"]).contains("hello"));
	// Structured content came with 2025-06-18.
	CHECK_FALSE(result.has("structuredContent"));
	EretheaMcpProtocol::Session newer;
	newer.protocol_version = "2025-06-18";
	result = Dictionary(protocol.handle(_request(5, "tools/call", call), newer).response)["result"];
	CHECK(Dictionary(result["structuredContent"])["echo"] == "hello");

	call["name"] = "missing";
	response = protocol.handle(_request(6, "tools/call", call), session).response;
	CHECK(int(Dictionary(response["error"])["code"]) == -32602);

	call["name"] = "wait";
	reply = protocol.handle(_request(7, "tools/call", call), session);
	CHECK(reply.pending);
	CHECK(reply.pending_id == 7);
	CHECK(int(reply.request_id) == 7);
	CHECK(reply.response.get_type() == Variant::NIL);

	// In a batch, nothing can wait.
	Array batch;
	batch.push_back(_request(8, "ping"));
	batch.push_back(_request(9, "tools/call", call));
	reply = protocol.handle(batch, session);
	CHECK_FALSE(reply.pending);
	const Array responses = reply.response;
	REQUIRE(responses.size() == 2);
	CHECK(bool(Dictionary(Dictionary(responses[1])["result"])["isError"]));
	CHECK_FALSE(tools.last_can_wait);

	response = protocol.handle(_request(10, "resources/list"), session).response;
	CHECK(int(Dictionary(response["error"])["code"]) == -32601);
	response = protocol.handle("nonsense", session).response;
	CHECK(int(Dictionary(response["error"])["code"]) == -32600);
}

static EretheaMcpHttpRequest _http(const String &p_method, const String &p_path, const String &p_body = String()) {
	EretheaMcpHttpRequest request;
	request.method = p_method;
	request.path = p_path;
	request.headers.insert("host", "127.0.0.1:11481");
	request.body = _bytes(p_body);
	return request;
}

static Dictionary _json(const EretheaMcpHttpResponse &p_response) {
	return JSON::parse_string(String::utf8(p_response.body.get_data(), p_response.body.length()));
}

static String _header(const EretheaMcpHttpResponse &p_response, const String &p_name) {
	for (const String &header : p_response.headers) {
		if (header.to_lower().begins_with(p_name.to_lower() + ":")) {
			return header.substr(p_name.length() + 1).strip_edges();
		}
	}
	return String();
}

TEST_CASE("[Modules][EretheaMcp] HTTP transport") {
	FakeTools tools;
	EretheaMcpServer server;
	server.get_protocol().setup(&tools, "erethea", "Erethea Engine", "test", "Instructions.");

	const String initialize = JSON::stringify(_request(1, "initialize"));
	EretheaMcpHttpRequest request = _http("POST", "/mcp", initialize);
	request.headers["host"] = "example.com";
	CHECK(server.handle_request(request).status == 403);
	request = _http("POST", "/mcp", initialize);
	request.headers.insert("origin", "https://example.com");
	CHECK(server.handle_request(request).status == 403);

	CHECK(server.handle_request(_http("GET", "/mcp")).status == 405);
	CHECK(server.handle_request(_http("GET", "/elsewhere")).status == 404);

	EretheaMcpHttpResponse response = server.handle_request(_http("POST", "/mcp", "{nope"));
	CHECK(response.status == 400);
	CHECK(int(Dictionary(_json(response)["error"])["code"]) == -32700);

	response = server.handle_request(_http("POST", "/mcp", initialize));
	CHECK(response.status == 200);
	CHECK(response.content_type == "application/json");
	const String session = _header(response, "Mcp-Session-Id");
	CHECK(session.length() == 32);
	CHECK(Dictionary(_json(response)["result"]).has("protocolVersion"));

	request = _http("POST", "/mcp", JSON::stringify(_request(2, "ping")));
	request.headers.insert("mcp-session-id", "unknown");
	CHECK(server.handle_request(request).status == 404);
	request.headers["mcp-session-id"] = session;
	CHECK(server.handle_request(request).status == 200);

	request.body = _bytes(JSON::stringify(_request(Variant(), "notifications/initialized")));
	CHECK(server.handle_request(request).status == 202);

	Dictionary call;
	call["name"] = "wait";
	request.body = _bytes(JSON::stringify(_request(3, "tools/call", call)));
	response = server.handle_request(request);
	CHECK(response.pending);
	CHECK(response.pending_id == 7);

	request = _http("DELETE", "/mcp");
	request.headers.insert("mcp-session-id", session);
	CHECK(server.handle_request(request).status == 204);
	CHECK(server.handle_request(request).status == 404);

	request = _http("OPTIONS", "/mcp");
	request.headers.insert("origin", "http://localhost:6274");
	response = server.handle_request(request);
	CHECK(response.status == 204);
	CHECK(_header(response, "Access-Control-Allow-Origin") == "http://localhost:6274");

	response = server.handle_request(_http("GET", "/status"));
	CHECK(response.status == 200);
	CHECK(_json(response).has("mcp"));
}

static Variant _from_json(const Variant &p_json, const PropertyInfo &p_property, bool &r_ok) {
	Variant value;
	String error;
	r_ok = EretheaMcpValues::from_json(p_json, p_property, value, error);
	return value;
}

TEST_CASE("[Modules][EretheaMcp] Values from JSON") {
	bool ok = false;
	const PropertyInfo vector3(Variant::VECTOR3, "position");
	CHECK(_from_json(JSON::parse_string("[1, 2, 3]"), vector3, ok) == Variant(Vector3(1, 2, 3)));
	CHECK(ok);
	CHECK(_from_json(JSON::parse_string(R"({"x": 1, "y": 2, "z": 3})"), vector3, ok) == Variant(Vector3(1, 2, 3)));
	CHECK(ok);
	CHECK(_from_json("Vector3(1, 2, 3)", vector3, ok) == Variant(Vector3(1, 2, 3)));
	CHECK(ok);
	_from_json(JSON::parse_string("[1, 2]"), vector3, ok);
	CHECK_FALSE(ok);
	_from_json(JSON::parse_string(R"({"x": 1, "y": 2, "z": 3, "q": 4})"), vector3, ok);
	CHECK_FALSE(ok);
	// Only literals of the expected type.
	_from_json("Color(1, 0, 0, 1)", vector3, ok);
	CHECK_FALSE(ok);
	_from_json("Object(Node, \"name\": \"x\")", PropertyInfo(Variant::OBJECT, "x"), ok);
	CHECK_FALSE(ok);

	const PropertyInfo vector2i(Variant::VECTOR2I, "size");
	CHECK(_from_json(JSON::parse_string("[1, 2]"), vector2i, ok) == Variant(Vector2i(1, 2)));
	_from_json(JSON::parse_string("[1.5, 2]"), vector2i, ok);
	CHECK_FALSE(ok);

	const PropertyInfo color(Variant::COLOR, "color");
	CHECK(_from_json("#ff0000", color, ok) == Variant(Color(1, 0, 0)));
	CHECK(ok);
	CHECK(_from_json("red", color, ok) == Variant(Color::named("red")));
	CHECK(_from_json(JSON::parse_string("[1, 0, 0]"), color, ok) == Variant(Color(1, 0, 0, 1)));
	CHECK(_from_json(JSON::parse_string("[1, 0, 0, 0.5]"), color, ok) == Variant(Color(1, 0, 0, 0.5)));

	const PropertyInfo integer(Variant::INT, "count");
	const Variant three = _from_json(3.0, integer, ok);
	CHECK(three.get_type() == Variant::INT);
	CHECK(int(three) == 3);
	_from_json(3.5, integer, ok);
	CHECK_FALSE(ok);
	CHECK(int(_from_json("Center", PropertyInfo(Variant::INT, "align", PROPERTY_HINT_ENUM, "Left,Center,Right"), ok)) == 1);
	CHECK(int(_from_json("enabled", PropertyInfo(Variant::INT, "mode", PROPERTY_HINT_ENUM, "Disabled:0,Enabled:5"), ok)) == 5);
	_from_json("Sideways", PropertyInfo(Variant::INT, "align", PROPERTY_HINT_ENUM, "Left,Center,Right"), ok);
	CHECK_FALSE(ok);

	CHECK(bool(_from_json(true, PropertyInfo(Variant::BOOL, "visible"), ok)));
	_from_json(1.0, PropertyInfo(Variant::BOOL, "visible"), ok);
	CHECK_FALSE(ok);
	CHECK(_from_json("Player", PropertyInfo(Variant::STRING_NAME, "group"), ok).get_type() == Variant::STRING_NAME);
	CHECK(_from_json("../Player", PropertyInfo(Variant::NODE_PATH, "target"), ok).get_type() == Variant::NODE_PATH);

	const PropertyInfo mesh(Variant::OBJECT, "mesh", PROPERTY_HINT_RESOURCE_TYPE, "Mesh");
	const Ref<BoxMesh> box = _from_json(JSON::parse_string(R"({"type": "BoxMesh", "properties": {"size": [2, 1, 1]}})"), mesh, ok);
	REQUIRE(ok);
	REQUIRE(box.is_valid());
	CHECK(box->get_size() == Vector3(2, 1, 1));
	_from_json(JSON::parse_string(R"({"type": "StandardMaterial3D"})"), mesh, ok);
	CHECK_FALSE(ok);
	_from_json(JSON::parse_string(R"({"type": "Node3D"})"), mesh, ok);
	CHECK_FALSE(ok);
	_from_json(JSON::parse_string(R"({"type": "BoxMesh", "properties": {"nope": 1}})"), mesh, ok);
	CHECK_FALSE(ok);
	_from_json("not a path", mesh, ok);
	CHECK_FALSE(ok);
	CHECK(_from_json(Variant(), mesh, ok).get_type() == Variant::OBJECT);
	CHECK(ok);

	const Array numbers = _from_json(JSON::parse_string("[1, 2.0]"), PropertyInfo(Variant::ARRAY, "numbers", PROPERTY_HINT_ARRAY_TYPE, "int"), ok);
	REQUIRE(ok);
	CHECK(numbers.is_typed());
	CHECK(numbers.get_typed_builtin() == Variant::INT);
	_from_json(JSON::parse_string("[1.5]"), PropertyInfo(Variant::ARRAY, "numbers", PROPERTY_HINT_ARRAY_TYPE, "int"), ok);
	CHECK_FALSE(ok);

	const PackedVector3Array points = _from_json(JSON::parse_string("[[1, 2, 3], [4, 5, 6]]"), PropertyInfo(Variant::PACKED_VECTOR3_ARRAY, "points"), ok);
	REQUIRE(ok);
	REQUIRE(points.size() == 2);
	CHECK(points[1] == Vector3(4, 5, 6));
}

TEST_CASE("[Modules][EretheaMcp] Values to JSON") {
	CHECK(EretheaMcpValues::to_json(Vector3(1, 2, 3)) == "Vector3(1, 2, 3)");
	CHECK(EretheaMcpValues::to_json(5) == Variant(5));
	CHECK(EretheaMcpValues::to_json(StringName("x")).get_type() == Variant::STRING);
	CHECK(EretheaMcpValues::to_json(Math::INF).get_type() == Variant::STRING);
	Ref<BoxMesh> box;
	box.instantiate();
	CHECK(Dictionary(EretheaMcpValues::to_json(box))["type"] == "BoxMesh");
	PackedInt32Array few;
	few.push_back(1);
	CHECK(EretheaMcpValues::to_json(few).get_type() == Variant::ARRAY);
	PackedInt32Array many;
	many.resize(1000);
	CHECK(EretheaMcpValues::to_json(many).get_type() == Variant::STRING);

	// What comes out goes back in.
	const Transform3D transform(Basis(Vector3(0, 1, 0), 0.5), Vector3(1, 2, 3));
	Variant value;
	String error;
	REQUIRE(EretheaMcpValues::from_json(EretheaMcpValues::to_json(transform), PropertyInfo(Variant::TRANSFORM3D, "transform"), value, error));
	CHECK(Transform3D(value).is_equal_approx(transform));
}

TEST_CASE("[Modules][EretheaMcp] Setting properties is all or nothing") {
	Node3D *node = memnew(Node3D);
	Dictionary properties;
	properties["position"] = JSON::parse_string("[1, 2, 3]");
	properties["visible"] = false;
	String error;
	CHECK(EretheaMcpValues::set_properties(node, properties, error));
	CHECK(node->get_position() == Vector3(1, 2, 3));
	CHECK_FALSE(node->is_visible());

	properties["position"] = JSON::parse_string("[4, 5, 6]");
	properties["nope"] = 1;
	CHECK_FALSE(EretheaMcpValues::set_properties(node, properties, error));
	CHECK(error.contains("nope"));
	CHECK(node->get_position() == Vector3(1, 2, 3));
	memdelete(node);
}

TEST_CASE("[Modules][EretheaMcp] File paths stay in the project") {
	String path;
	String error;
	CHECK(EretheaMcpSceneTools::project_path("levels/forest.tscn", path, error));
	CHECK(path == "res://levels/forest.tscn");
	CHECK(EretheaMcpSceneTools::project_path("res://a/../b.tscn", path, error));
	CHECK(path == "res://b.tscn");
	CHECK_FALSE(EretheaMcpSceneTools::project_path("res://../outside.gd", path, error));
	CHECK_FALSE(EretheaMcpSceneTools::project_path("/etc/passwd", path, error));
	CHECK_FALSE(EretheaMcpSceneTools::project_path("", path, error));
	CHECK_FALSE(EretheaMcpSceneTools::project_path(5, path, error));
}

} // namespace TestEretheaMcp
