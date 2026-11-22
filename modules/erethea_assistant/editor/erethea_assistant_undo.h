/**************************************************************************/
/*  erethea_assistant_undo.h                                              */
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

#include "core/templates/hash_map.h"
#include "core/templates/local_vector.h"

#include "modules/erethea_mcp/erethea_mcp_protocol.h"

// Remembers what the tools of one answer of the assistant changed, so that
// "Undo last answer" puts it back: scene changes go back through the scenes'
// undo history, and files get their old content back (or are deleted, when
// the answer created them). When the person changed something after the
// answer, nothing is undone, so that their work isn't lost.
class EretheaAssistantUndo {
public:
	// Text files larger than this aren't kept, when a tool might change many
	// files (build_fornalha_scenes).
	static constexpr int64_t MAX_FILE_SIZE = 8 * 1024 * 1024;
	static constexpr int MAX_SCAN_FILES = 4000;

	// Why a file changed, which decides how it goes back.
	enum Reason {
		// A scene saved by save_scene, run_game or run_test: if the answer
		// changed it through its history, it's saved again once that history
		// goes back; otherwise the save kept the person's own changes.
		REASON_SAVED,
		// A scene written by build_fornalha_scenes: it gets its old file back
		// and, if open, is reloaded.
		REASON_REBUILT,
		// A script or text file: it gets its old content back.
		REASON_WRITTEN,
		// A scene that create_scene made: it's closed and deleted.
		REASON_CREATED,
	};

private:
	struct FileState {
		String path;
		Reason reason = REASON_WRITTEN;
		bool existed = false;
		Vector<uint8_t> before;
		// After the answer, to see whether the person changed it since.
		bool exists_after = false;
		String hash_after;
	};

	struct SceneState {
		int history_id = 0;
		String scene_path;
		uint64_t version_before = 0;
		uint64_t version_after = 0;
	};

	// Candidates for the tool being run, until it finishes.
	struct Candidate {
		String path;
		Reason reason = REASON_WRITTEN;
		bool existed = false;
		Vector<uint8_t> content;
	};
	LocalVector<Candidate> candidates;

	LocalVector<FileState> files;
	LocalVector<SceneState> scenes;
	bool recording = false;
	bool finished = false;

	// The answer before, which stays the one to undo when the current one
	// changes nothing (such as a question that only reads).
	LocalVector<FileState> previous_files;
	LocalVector<SceneState> previous_scenes;
	bool previous_finished = false;

	static bool _read(const String &p_path, Vector<uint8_t> &r_content);
	static String _hash(const Vector<uint8_t> &p_content);
	static bool _write(const String &p_path, const Vector<uint8_t> &p_content);
	void _add_candidate(const String &p_path, Reason p_reason);
	void _add_scene_history();
	FileState *_find_file(const String &p_path);
	SceneState *_find_scene(const String &p_path);
	bool _check_unchanged(String &r_reason) const;
	void _remove_file(const String &p_path);
	void _reload_script(const String &p_path);

public:
	// Tools that change the scenes being edited, and need their history kept.
	static bool changes_scene(const String &p_tool);

	void begin_answer();
	void before_tool(const String &p_tool, const Dictionary &p_arguments);
	void after_tool(const String &p_tool, const Dictionary &p_arguments, const EretheaMcpToolResult &p_result);
	void end_answer();

	bool can_undo() const;
	// Undoes the last answer that changed something. Returns false, with why in `r_message`, when it
	// can't; on success, `r_message` says what went back.
	bool undo(String &r_message);
	void clear();
};
