/**************************************************************************/
/*  erethea_fornalha_link.cpp                                             */
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

#include "erethea_fornalha_link.h"

#include "../erethea_fornalha_import.h"

#include "core/config/project_settings.h"
#include "core/io/file_access.h"
#include "core/io/json.h"
#include "core/io/resource_loader.h"
#include "core/object/callable_mp.h"
#include "core/os/os.h"
#include "core/version.h"
#include "editor/editor_node.h"
#include "editor/file_system/editor_file_system.h"
#include "editor/gui/editor_toaster.h"
#include "editor/settings/editor_settings.h"
#include "scene/main/http_request.h"
#include "scene/main/timer.h"

Dictionary EretheaFornalhaLink::make_hello(const String &p_project_folder, bool p_builds_scenes) {
	Dictionary hello;
	hello["tipo"] = "ola";
	hello["programa"] = GODOT_VERSION_NAME;
	hello["versao"] = GODOT_VERSION_FULL_BUILD;
	hello["projeto"] = p_project_folder;
	// The Fornalha doesn't need to assemble the scenes itself.
	hello["monta_cenas"] = p_builds_scenes;
	return hello;
}

bool EretheaFornalhaLink::parse_bridge_message(const String &p_text, int64_t &r_version, PackedStringArray &r_files, String &r_websocket_path) {
	Ref<JSON> json;
	json.instantiate();
	if (json->parse(p_text) != OK || json->get_data().get_type() != Variant::DICTIONARY) {
		return false;
	}
	const Dictionary message = json->get_data();
	const Variant type = message.get("tipo", Variant());
	if (type.get_type() == Variant::STRING && String(type) != "ponte") {
		return false;
	}
	const Variant version = message.get("versao", Variant());
	if (!version.is_num()) {
		return false;
	}
	r_version = int64_t(version);
	r_files.clear();
	const Variant files = message.get("arquivos", Variant());
	if (files.get_type() == Variant::ARRAY) {
		for (const Variant &file : Array(files)) {
			if (file.get_type() == Variant::STRING) {
				r_files.push_back(file);
			}
		}
	}
	const Variant websocket_path = message.get("websocket", Variant());
	r_websocket_path = websocket_path.get_type() == Variant::STRING && String(websocket_path).begins_with("/") ? String(websocket_path) : String();
	return true;
}

void EretheaFornalhaLink::_start() {
	started = true;
	port = EDITOR_GET(SETTING_PORT);
	const String port_environment = OS::get_singleton()->get_environment(PORT_ENVIRONMENT);
	if (!port_environment.is_empty()) {
		if (port_environment.is_valid_int() && port_environment.to_int() > 0 && port_environment.to_int() <= 65535) {
			port = port_environment.to_int();
		} else {
			print_error(vformat("Erethea: %s must be a port number, not \"%s\".", PORT_ENVIRONMENT, port_environment));
		}
	}
	EditorFileSystem::get_singleton()->connect("filesystem_changed", callable_mp(this, &EretheaFornalhaLink::_filesystem_changed));
	ProjectSettings::get_singleton()->connect("settings_changed", callable_mp(this, &EretheaFornalhaLink::_settings_changed));
	_set_live_link(GLOBAL_GET(EretheaFornalhaImport::SETTING_LIVE_LINK));
	// What the Fornalha did while the editor was closed.
	build_timer->start();
}

void EretheaFornalhaLink::_settings_changed() {
	_set_live_link(GLOBAL_GET(EretheaFornalhaImport::SETTING_LIVE_LINK));
	build_timer->start();
}

void EretheaFornalhaLink::_set_live_link(bool p_enabled) {
	if (p_enabled == (mode != MODE_OFF)) {
		return;
	}
#ifdef MODULE_WEBSOCKET_ENABLED
	if (websocket.is_valid()) {
		websocket->close();
		websocket.unref();
	}
	websocket_open = false;
#endif
	http->cancel_request();
	mode = p_enabled ? MODE_HTTP : MODE_OFF;
	poll_wait = 0.0;
	websocket_wait = 0.0;
}

void EretheaFornalhaLink::_start_websocket(const String &p_path) {
#ifdef MODULE_WEBSOCKET_ENABLED
	websocket = Ref<WebSocketPeer>(WebSocketPeer::create());
	if (websocket.is_valid() && websocket->connect_to_url(vformat("ws://127.0.0.1:%d%s", port, p_path)) == OK) {
		http->cancel_request();
		mode = MODE_WEBSOCKET;
		websocket_open = false;
		return;
	}
	websocket.unref();
	websocket_wait = WEBSOCKET_RETRY_INTERVAL;
#endif
}

void EretheaFornalhaLink::_process_websocket() {
#ifdef MODULE_WEBSOCKET_ENABLED
	websocket->poll();
	switch (websocket->get_ready_state()) {
		case WebSocketPeer::STATE_OPEN: {
			if (!websocket_open) {
				websocket_open = true;
				print_line(vformat("Erethea: live link with the Fornalha open (%s).", websocket->get_requested_url()));
				_send(make_hello(ProjectSettings::get_singleton()->globalize_path("res://"), GLOBAL_GET(EretheaFornalhaImport::SETTING_BUILD_AUTOMATICALLY)));
			}
			while (websocket.is_valid() && websocket->get_available_packet_count() > 0) {
				const uint8_t *buffer = nullptr;
				int size = 0;
				if (websocket->get_packet(&buffer, size) != OK) {
					break;
				}
				if (!websocket->was_string_packet()) {
					continue;
				}
				int64_t version = 0;
				PackedStringArray files;
				String websocket_path;
				if (parse_bridge_message(String::utf8((const char *)buffer, size), version, files, websocket_path)) {
					_bridge_changed(version, files);
				}
			}
		} break;

		case WebSocketPeer::STATE_CLOSED: {
			if (websocket_open) {
				print_line("Erethea: the Fornalha closed the live link. Asking it over HTTP until it opens again.");
			}
			websocket.unref();
			websocket_open = false;
			mode = MODE_HTTP;
			poll_wait = 0.0;
			websocket_wait = WEBSOCKET_RETRY_INTERVAL;
		} break;

		default:
			break;
	}
#endif
}

void EretheaFornalhaLink::_process_http(double p_delta) {
	poll_wait -= p_delta;
	websocket_wait -= p_delta;
	if (poll_wait > 0.0) {
		return;
	}
	poll_wait = POLL_INTERVAL;
	if (http->get_http_client_status() == HTTPClient::STATUS_DISCONNECTED) {
		const String project_folder = ProjectSettings::get_singleton()->globalize_path("res://");
		http->request(vformat("http://127.0.0.1:%d%s?projeto=%s", port, BRIDGE_PATH, project_folder.uri_encode()));
	}
}

void EretheaFornalhaLink::_http_completed(int p_result, int p_code, const PackedStringArray &p_headers, const PackedByteArray &p_body) {
	if (mode != MODE_HTTP || p_result != HTTPRequest::RESULT_SUCCESS || p_code != 200) {
		return;
	}
	int64_t version = 0;
	PackedStringArray files;
	String websocket_path;
	if (!parse_bridge_message(String::utf8((const char *)p_body.ptr(), p_body.size()), version, files, websocket_path)) {
		return;
	}
	_bridge_changed(version, files);
	if (!websocket_path.is_empty() && websocket_wait <= 0.0) {
		_start_websocket(websocket_path);
	}
}

void EretheaFornalhaLink::_bridge_changed(int64_t p_version, const PackedStringArray &p_files) {
	if (bridge_version >= 0 && p_version != bridge_version) {
		print_line(vformat("Erethea: the Fornalha wrote new files (%s). Importing them.", p_files.is_empty() ? String("?") : String(", ").join(p_files)));
		EditorFileSystem::get_singleton()->scan_changes();
	}
	bridge_version = p_version;
}

void EretheaFornalhaLink::_send(const Dictionary &p_message) {
#ifdef MODULE_WEBSOCKET_ENABLED
	if (websocket.is_valid() && websocket_open) {
		websocket->send_text(JSON::stringify(p_message));
	}
#endif
}

void EretheaFornalhaLink::_filesystem_changed() {
	build_timer->start();
}

void EretheaFornalhaLink::_build_if_needed() {
	if (!bool(GLOBAL_GET(EretheaFornalhaImport::SETTING_BUILD_AUTOMATICALLY)) || !EretheaFornalhaImport::is_fornalha_project()) {
		return;
	}
	EditorFileSystem *file_system = EditorFileSystem::get_singleton();
	if (file_system->is_scanning() || file_system->is_importing()) {
		build_timer->start();
		return;
	}
	const EretheaFornalhaProject project = EretheaFornalhaProject::read();
	const String signature = project.get_signature();
	if (signature == checked_signature) {
		return;
	}
	if (!EretheaFornalhaImport::should_build_automatically(EretheaFornalhaImport::was_built_before(), EretheaFornalhaImport::get_built_signature(), signature, FileAccess::exists(EretheaFornalhaImport::MAIN_SCENE))) {
		checked_signature = signature;
		return;
	}
	// A model the editor hasn't imported yet would leave its scene empty.
	bool imported = project.world_model.is_empty() || ResourceLoader::exists(project.world_model);
	for (const EretheaFornalhaCharacter &character : project.characters) {
		imported = imported && ResourceLoader::exists(character.model);
	}
	if (!imported && import_waits < MAX_IMPORT_WAITS) {
		import_waits++;
		build_timer->start();
		return;
	}
	import_waits = 0;
	checked_signature = signature;

	const bool had_main_scene = FileAccess::exists(EretheaFornalhaImport::MAIN_SCENE);
	const bool built_before = EretheaFornalhaImport::was_built_before();
	const EretheaFornalhaImport::Report report = EretheaFornalhaImport::build(false);
	print_line(String(built_before ? "Erethea: scenes built again from game.project.json, because what they are built from changed. " : "Erethea: scenes of this new project built from game.project.json. ") + report.get_summary());
	for (const String &warning : report.warnings) {
		print_line("Erethea: " + warning);
	}
	const int changed = report.count(EretheaFornalhaImport::ACTION_CREATED) + report.count(EretheaFornalhaImport::ACTION_UPDATED);
	if (changed > 0 || report.count(EretheaFornalhaImport::ACTION_FAILED) > 0) {
		const EditorToaster::Severity severity = report.count(EretheaFornalhaImport::ACTION_FAILED) > 0 ? EditorToaster::SEVERITY_WARNING : EditorToaster::SEVERITY_INFO;
		EditorToaster::get_singleton()->popup_str(vformat(TTR("Built the scenes from game.project.json. %s"), report.get_summary()), severity);
	}
	Dictionary message = report.to_dictionary();
	message["tipo"] = "cenas";
	_send(message);

	// A new project shows its game right away.
	if (!had_main_scene && FileAccess::exists(EretheaFornalhaImport::MAIN_SCENE) && !EditorNode::get_singleton()->get_edited_scene()) {
		EditorNode::get_singleton()->open_scene(EretheaFornalhaImport::MAIN_SCENE);
	}
}

void EretheaFornalhaLink::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			set_process_internal(true);
		} break;

		case NOTIFICATION_INTERNAL_PROCESS: {
			if (!started) {
				if (EditorNode::get_singleton()->is_editor_ready()) {
					_start();
				}
				break;
			}
			if (mode == MODE_WEBSOCKET) {
				_process_websocket();
			} else if (mode == MODE_HTTP) {
				_process_http(get_process_delta_time());
			}
		} break;

		case NOTIFICATION_EXIT_TREE: {
			_set_live_link(false);
		} break;
	}
}

EretheaFornalhaLink::EretheaFornalhaLink() {
	http = memnew(HTTPRequest);
	http->set_timeout(1.5);
	http->connect("request_completed", callable_mp(this, &EretheaFornalhaLink::_http_completed));
	add_child(http);

	build_timer = memnew(Timer);
	build_timer->set_wait_time(BUILD_DELAY);
	build_timer->set_one_shot(true);
	build_timer->connect("timeout", callable_mp(this, &EretheaFornalhaLink::_build_if_needed));
	add_child(build_timer);
}
