/**************************************************************************/
/*  erethea_fornalha_link.h                                               */
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

#include "scene/main/node.h"

#include "modules/modules_enabled.gen.h" // For websocket.

#ifdef MODULE_WEBSOCKET_ENABLED
#include "modules/websocket/websocket_peer.h"
#endif

class HTTPRequest;
class Timer;

// Live link with the Fornalha, in projects made with it. The Fornalha says when
// it wrote new files in the project, and the editor imports them. It is asked
// every 2 seconds over HTTP, like its bridge add-on does, and when its answer
// names a WebSocket, the Fornalha pushes the news through it instead. When what
// the scenes are built from changed, such as a new character, the scenes are
// built again by themselves.
class EretheaFornalhaLink : public Node {
	GDCLASS(EretheaFornalhaLink, Node);

public:
	enum Mode {
		MODE_OFF,
		MODE_WEBSOCKET,
		MODE_HTTP,
	};

	static constexpr const char *SETTING_PORT = "erethea/fornalha/port";
	static constexpr const char *PORT_ENVIRONMENT = "ERETHEA_FORNALHA_PORT";
	static constexpr int DEFAULT_PORT = 11480;
	static constexpr const char *BRIDGE_PATH = "/fornalha/ponte";
	static constexpr double POLL_INTERVAL = 2.0;
	static constexpr double WEBSOCKET_RETRY_INTERVAL = 30.0;
	static constexpr double BUILD_DELAY = 1.0;
	static constexpr int MAX_IMPORT_WAITS = 30;

private:
	Mode mode = MODE_OFF;
	bool started = false;
	int port = DEFAULT_PORT;

#ifdef MODULE_WEBSOCKET_ENABLED
	Ref<WebSocketPeer> websocket;
	bool websocket_open = false;
#endif
	HTTPRequest *http = nullptr;
	double poll_wait = 0.0;
	double websocket_wait = 0.0;
	int64_t bridge_version = -1;

	Timer *build_timer = nullptr;
	String checked_signature;
	int import_waits = 0;
	// What the last "ola" told the Fornalha: whether the editor builds the scenes.
	bool told_builds_scenes = false;

	void _start();
	void _settings_changed();
	void _set_live_link(bool p_enabled);
	void _start_websocket(const String &p_path);
	void _process_websocket();
	void _process_http(double p_delta);
	void _http_completed(int p_result, int p_code, const PackedStringArray &p_headers, const PackedByteArray &p_body);
	void _bridge_changed(int64_t p_version, const PackedStringArray &p_files);
	void _send(const Dictionary &p_message);
	void _send_hello();

	void _filesystem_changed();
	void _build_if_needed();

protected:
	void _notification(int p_what);

public:
	// The first message to the Fornalha over the WebSocket.
	static Dictionary make_hello(const String &p_project_folder, bool p_builds_scenes);
	// A message from the Fornalha: {"versao": 3, "arquivos": [...]}, with
	// "tipo": "ponte" when it comes through the WebSocket. Over HTTP,
	// "websocket": "/path" says where its WebSocket is.
	static bool parse_bridge_message(const String &p_text, int64_t &r_version, PackedStringArray &r_files, String &r_websocket_path);

	Mode get_mode() const { return mode; }

	EretheaFornalhaLink();
};
