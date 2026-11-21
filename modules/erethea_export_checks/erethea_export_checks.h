/**************************************************************************/
/*  erethea_export_checks.h                                               */
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

#include "core/math/aabb.h"
#include "core/math/vector2.h"
#include "core/templates/hash_map.h"
#include "core/templates/hash_set.h"
#include "core/templates/vector.h"
#include "core/variant/dictionary.h"
#include "core/variant/variant.h"

class EditorExportPreset;
class Node;
class Texture2D;

// What to look at before a game is exported: how heavy the models are, how
// big the textures are, whether the models have a believable size, and, in
// projects made with the Fornalha (the game factory of Plano 1), whether every
// asset has a license receipt in ai/recibos.jsonl.
struct EretheaExportFinding {
	enum Level {
		LEVEL_WARNING,
		// Something that shouldn't ship, such as an asset made with a model
		// that has no commercial license in a game that will be sold.
		LEVEL_ERROR,
	};

	Level level = LEVEL_WARNING;
	// "polygons", "texture_size", "scale" or "license".
	String check;
	String file;
	String message;

	Dictionary to_dictionary() const;
};

struct EretheaExportCheckSettings {
	int max_texture_size = 4096;
	int max_character_triangles = 100000;
	int max_model_triangles = 500000;
	// Height in meters of a character or an enemy.
	Vector2 character_height = Vector2(0.3, 4.0);
	// Largest side in meters of any other model.
	Vector2 model_size = Vector2(0.005, 5000.0);
	// Models in these folders are characters.
	PackedStringArray character_folders = { "res://Characters", "res://Enemies" };
	bool license_receipts = true;

	static EretheaExportCheckSettings from_project_settings();
};

// The license receipt of an asset, as the Fornalha writes it.
struct EretheaLicenseReceipt {
	String file; // res:// path.
	String sha256;
	String model_id;
	String model_name;
	String license;
	// "sim", "nao" or "condicional".
	String commercial_use;
};

class EretheaExportChecks {
public:
	enum RunOnExport {
		RUN_ON_EXPORT_NEVER,
		RUN_ON_EXPORT_FORNALHA_PROJECTS,
		RUN_ON_EXPORT_ALL_PROJECTS,
	};

	static constexpr const char *SETTING_RUN_ON_EXPORT = "editor/erethea_export_checks/run_on_export";
	static constexpr const char *SETTING_MAX_TEXTURE_SIZE = "editor/erethea_export_checks/max_texture_size";
	static constexpr const char *SETTING_MAX_CHARACTER_TRIANGLES = "editor/erethea_export_checks/max_character_triangles";
	static constexpr const char *SETTING_MAX_MODEL_TRIANGLES = "editor/erethea_export_checks/max_model_triangles";
	static constexpr const char *SETTING_CHARACTER_HEIGHT = "editor/erethea_export_checks/character_height";
	static constexpr const char *SETTING_MODEL_SIZE = "editor/erethea_export_checks/model_size";
	static constexpr const char *SETTING_CHARACTER_FOLDERS = "editor/erethea_export_checks/character_folders";
	static constexpr const char *SETTING_LICENSE_RECEIPTS = "editor/erethea_export_checks/license_receipts";

	// Files the Fornalha keeps next to project.godot.
	static constexpr const char *GAME_PROJECT_FILE = "res://game.project.json";
	static constexpr const char *RECEIPTS_FILE = "res://ai/recibos.jsonl";

	struct Report {
		Vector<EretheaExportFinding> findings;
		int models = 0;
		int textures = 0;
		int licensed_assets = 0;
		bool fornalha_project = false;
		bool commercial = false;

		int count(EretheaExportFinding::Level p_level) const;
		Dictionary to_dictionary() const;
	};

	static void register_project_settings();
	static bool is_fornalha_project();
	static bool should_run_on_export();

	// res:// files that a preset exports (every resource of the project
	// without a preset), as the editor's file system knows them.
	static Vector<String> get_project_files();
	static Vector<String> get_files_to_export(const Ref<EditorExportPreset> &p_preset);

	static Report check_files(const Vector<String> &p_files, const EretheaExportCheckSettings &p_settings);

	// The parts of check_files(), separate for the tests.
	static int count_triangles(const Node *p_root);
	static AABB get_model_bounds(const Node *p_root);
	static void check_model(const Node *p_root, const String &p_path, const EretheaExportCheckSettings &p_settings, Vector<EretheaExportFinding> &r_findings);
	static void check_texture(const Ref<Texture2D> &p_texture, const String &p_path, const String &p_name, const EretheaExportCheckSettings &p_settings, Vector<EretheaExportFinding> &r_findings);
	static bool is_character(const String &p_path, const EretheaExportCheckSettings &p_settings);

	// One receipt per file, the newest one. Unreadable lines are skipped.
	static HashMap<String, EretheaLicenseReceipt> parse_receipts(const String &p_jsonl);
	static bool needs_receipt(const String &p_path);
	static bool is_derived_from_receipted_model(const String &p_path, const HashMap<String, EretheaLicenseReceipt> &p_receipts);
	// p_sha256 is the file's current hash, or empty to skip that comparison.
	static void check_license(const String &p_path, const String &p_sha256, bool p_commercial, const HashMap<String, EretheaLicenseReceipt> &p_receipts, Vector<EretheaExportFinding> &r_findings);
};
