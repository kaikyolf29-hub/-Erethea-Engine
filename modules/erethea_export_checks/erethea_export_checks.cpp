/**************************************************************************/
/*  erethea_export_checks.cpp                                             */
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

#include "erethea_export_checks.h"

#include "core/config/project_settings.h"
#include "core/io/file_access.h"
#include "core/io/json.h"
#include "core/io/resource_loader.h"
#include "core/object/class_db.h"
#include "editor/export/editor_export_preset.h"
#include "editor/file_system/editor_file_system.h"
#include "scene/3d/mesh_instance_3d.h"
#include "scene/resources/material.h"
#include "scene/resources/mesh.h"
#include "scene/resources/packed_scene.h"
#include "scene/resources/texture.h"

// The folders and file types that the Fornalha gives a license receipt to
// (crates/fornalha-core/src/recibos.rs). Code, documents and work files don't
// need one.
static const char *RECEIPT_FOLDERS[] = { "World", "Characters", "Enemies", "Items", "Weapons", "Animations", "Audio", "VFX", "Cutscenes", "UI" };
static const char *RECEIPT_EXTENSIONS[] = { "png", "jpg", "jpeg", "webp", "tga", "exr", "hdr", "bmp", "glb", "gltf", "fbx", "obj", "bvh", "wav", "ogg", "mp3", "flac", "opus", "mp4", "webm", "mov", "mkv", "vrm", "usdz" };

static String _setting_hint(const char *p_setting) {
	return vformat("(Project Settings: %s)", p_setting);
}

static String _thousands(int64_t p_value) {
	const String digits = String::num_int64(p_value);
	String result;
	for (int i = 0; i < digits.length(); i++) {
		if (i > 0 && (digits.length() - i) % 3 == 0) {
			result += ",";
		}
		result += digits[i];
	}
	return result;
}

Dictionary EretheaExportFinding::to_dictionary() const {
	Dictionary result;
	result["level"] = level == LEVEL_ERROR ? "error" : "warning";
	result["check"] = check;
	result["file"] = file;
	result["message"] = message;
	return result;
}

EretheaExportCheckSettings EretheaExportCheckSettings::from_project_settings() {
	EretheaExportCheckSettings settings;
	ProjectSettings *project = ProjectSettings::get_singleton();
	settings.max_texture_size = MAX(1, int(project->get_setting(EretheaExportChecks::SETTING_MAX_TEXTURE_SIZE, settings.max_texture_size)));
	settings.max_character_triangles = MAX(1, int(project->get_setting(EretheaExportChecks::SETTING_MAX_CHARACTER_TRIANGLES, settings.max_character_triangles)));
	settings.max_model_triangles = MAX(1, int(project->get_setting(EretheaExportChecks::SETTING_MAX_MODEL_TRIANGLES, settings.max_model_triangles)));
	settings.character_height = project->get_setting(EretheaExportChecks::SETTING_CHARACTER_HEIGHT, settings.character_height);
	settings.model_size = project->get_setting(EretheaExportChecks::SETTING_MODEL_SIZE, settings.model_size);
	settings.character_folders = project->get_setting(EretheaExportChecks::SETTING_CHARACTER_FOLDERS, settings.character_folders);
	settings.license_receipts = project->get_setting(EretheaExportChecks::SETTING_LICENSE_RECEIPTS, settings.license_receipts);
	return settings;
}

int EretheaExportChecks::Report::count(EretheaExportFinding::Level p_level) const {
	int total = 0;
	for (const EretheaExportFinding &finding : findings) {
		total += finding.level == p_level;
	}
	return total;
}

Dictionary EretheaExportChecks::Report::to_dictionary() const {
	Array list;
	for (const EretheaExportFinding &finding : findings) {
		list.push_back(finding.to_dictionary());
	}
	Dictionary checked;
	checked["models"] = models;
	checked["textures"] = textures;
	checked["licensed_assets"] = licensed_assets;

	Dictionary result;
	result["errors"] = count(EretheaExportFinding::LEVEL_ERROR);
	result["warnings"] = count(EretheaExportFinding::LEVEL_WARNING);
	result["checked"] = checked;
	result["fornalha_project"] = fornalha_project;
	result["commercial"] = commercial;
	result["findings"] = list;
	return result;
}

void EretheaExportChecks::register_project_settings() {
	const EretheaExportCheckSettings defaults;
	GLOBAL_DEF(PropertyInfo(Variant::INT, SETTING_RUN_ON_EXPORT, PROPERTY_HINT_ENUM, "Never,Fornalha Projects,All Projects"), RUN_ON_EXPORT_FORNALHA_PROJECTS);
	GLOBAL_DEF(PropertyInfo(Variant::INT, SETTING_MAX_TEXTURE_SIZE, PROPERTY_HINT_RANGE, "256,16384,1,or_greater,suffix:px"), defaults.max_texture_size);
	GLOBAL_DEF(PropertyInfo(Variant::INT, SETTING_MAX_CHARACTER_TRIANGLES, PROPERTY_HINT_RANGE, "100,10000000,1,or_greater"), defaults.max_character_triangles);
	GLOBAL_DEF(PropertyInfo(Variant::INT, SETTING_MAX_MODEL_TRIANGLES, PROPERTY_HINT_RANGE, "100,100000000,1,or_greater"), defaults.max_model_triangles);
	GLOBAL_DEF(PropertyInfo(Variant::VECTOR2, SETTING_CHARACTER_HEIGHT, PROPERTY_HINT_NONE, "suffix:m"), defaults.character_height);
	GLOBAL_DEF(PropertyInfo(Variant::VECTOR2, SETTING_MODEL_SIZE, PROPERTY_HINT_NONE, "suffix:m"), defaults.model_size);
	GLOBAL_DEF(PropertyInfo(Variant::PACKED_STRING_ARRAY, SETTING_CHARACTER_FOLDERS, PROPERTY_HINT_TYPE_STRING, vformat("%d/%d:", Variant::STRING, PROPERTY_HINT_DIR)), defaults.character_folders);
	GLOBAL_DEF(SETTING_LICENSE_RECEIPTS, defaults.license_receipts);
}

bool EretheaExportChecks::is_fornalha_project() {
	return FileAccess::exists(GAME_PROJECT_FILE);
}

bool EretheaExportChecks::should_run_on_export() {
	switch (int(GLOBAL_GET(SETTING_RUN_ON_EXPORT))) {
		case RUN_ON_EXPORT_FORNALHA_PROJECTS:
			return is_fornalha_project();
		case RUN_ON_EXPORT_ALL_PROJECTS:
			return true;
		default:
			return false;
	}
}

static void _find_files(EditorFileSystemDirectory *p_dir, const Ref<EditorExportPreset> &p_preset, EditorExportPreset::FileExportMode p_mode, HashSet<String> &r_files) {
	for (int i = 0; i < p_dir->get_subdir_count(); i++) {
		EditorFileSystemDirectory *subdir = p_dir->get_subdir(i);
		_find_files(subdir, p_preset, p_preset.is_valid() ? p_preset->get_file_export_mode(subdir->get_path(), p_mode) : p_mode, r_files);
	}
	for (int i = 0; i < p_dir->get_file_count(); i++) {
		if (p_dir->get_file_type(i) == "TextFile") {
			continue;
		}
		const String path = p_dir->get_file_path(i);
		if (p_preset.is_null() || p_preset->get_file_export_mode(path, p_mode) != EditorExportPreset::MODE_FILE_REMOVE) {
			r_files.insert(path);
		}
	}
}

static void _find_dependencies(const String &p_path, HashSet<String> &r_files) {
	if (r_files.has(p_path)) {
		return;
	}
	r_files.insert(p_path);
	int index = -1;
	EditorFileSystemDirectory *dir = EditorFileSystem::get_singleton()->find_file(p_path, &index);
	if (!dir) {
		return;
	}
	for (const String &dependency : dir->get_file_deps(index)) {
		_find_dependencies(dependency, r_files);
	}
}

static Vector<String> _sorted(const HashSet<String> &p_files) {
	Vector<String> files;
	for (const String &file : p_files) {
		files.push_back(file);
	}
	files.sort();
	return files;
}

Vector<String> EretheaExportChecks::get_project_files() {
	return get_files_to_export(Ref<EditorExportPreset>());
}

Vector<String> EretheaExportChecks::get_files_to_export(const Ref<EditorExportPreset> &p_preset) {
	EditorFileSystem *file_system = EditorFileSystem::get_singleton();
	if (!file_system || !file_system->get_filesystem()) {
		return Vector<String>();
	}
	// The same choice as EditorExportPlatform::export_project_files().
	HashSet<String> files;
	const EditorExportPreset::ExportFilter filter = p_preset.is_valid() ? p_preset->get_export_filter() : EditorExportPreset::EXPORT_ALL_RESOURCES;
	switch (filter) {
		case EditorExportPreset::EXPORT_ALL_RESOURCES: {
			_find_files(file_system->get_filesystem(), Ref<EditorExportPreset>(), EditorExportPreset::MODE_FILE_NOT_CUSTOMIZED, files);
		} break;
		case EditorExportPreset::EXCLUDE_SELECTED_RESOURCES: {
			_find_files(file_system->get_filesystem(), Ref<EditorExportPreset>(), EditorExportPreset::MODE_FILE_NOT_CUSTOMIZED, files);
			for (const String &file : p_preset->get_files_to_export()) {
				files.erase(file);
			}
		} break;
		case EditorExportPreset::EXPORT_CUSTOMIZED: {
			_find_files(file_system->get_filesystem(), p_preset, p_preset->get_file_export_mode("res://"), files);
		} break;
		default: {
			const bool scenes_only = filter == EditorExportPreset::EXPORT_SELECTED_SCENES;
			for (const String &file : p_preset->get_files_to_export()) {
				if (!scenes_only || ResourceLoader::get_resource_type(file) == "PackedScene") {
					_find_dependencies(file, files);
				}
			}
		} break;
	}
	return _sorted(files);
}

bool EretheaExportChecks::is_character(const String &p_path, const EretheaExportCheckSettings &p_settings) {
	for (const String &folder : p_settings.character_folders) {
		const String prefix = folder.trim_suffix("/") + "/";
		if (p_path.begins_with(prefix)) {
			return true;
		}
	}
	return false;
}

int EretheaExportChecks::count_triangles(const Node *p_root) {
	int64_t triangles = 0;
	TypedArray<Node> meshes = const_cast<Node *>(p_root)->find_children("*", "MeshInstance3D", true, false);
	if (Object::cast_to<MeshInstance3D>(p_root)) {
		meshes.push_back(const_cast<Node *>(p_root));
	}
	for (const Variant &node : meshes) {
		const MeshInstance3D *instance = Object::cast_to<MeshInstance3D>(node);
		const Ref<Mesh> mesh = instance ? instance->get_mesh() : Ref<Mesh>();
		if (mesh.is_null()) {
			continue;
		}
		for (int surface = 0; surface < mesh->get_surface_count(); surface++) {
			if (mesh->surface_get_primitive_type(surface) != Mesh::PRIMITIVE_TRIANGLES) {
				continue;
			}
			// The full detail mesh; the LODs Godot makes on import come on top.
			const int indices = mesh->surface_get_array_index_len(surface);
			triangles += (indices > 0 ? indices : mesh->surface_get_array_len(surface)) / 3;
		}
	}
	return int(MIN(triangles, int64_t(INT32_MAX)));
}

// The node's transform relative to the model's root, which works before the
// model is in a scene tree.
static Transform3D _transform_from_root(const Node *p_root, const Node *p_node) {
	Transform3D transform;
	for (const Node *node = p_node; node && node != p_root; node = node->get_parent()) {
		const Node3D *spatial = Object::cast_to<Node3D>(node);
		if (spatial) {
			transform = spatial->get_transform() * transform;
		}
	}
	return transform;
}

AABB EretheaExportChecks::get_model_bounds(const Node *p_root) {
	AABB bounds;
	bool first = true;
	TypedArray<Node> meshes = const_cast<Node *>(p_root)->find_children("*", "MeshInstance3D", true, false);
	if (Object::cast_to<MeshInstance3D>(p_root)) {
		meshes.push_back(const_cast<Node *>(p_root));
	}
	for (const Variant &node : meshes) {
		const MeshInstance3D *instance = Object::cast_to<MeshInstance3D>(node);
		const Ref<Mesh> mesh = instance ? instance->get_mesh() : Ref<Mesh>();
		if (mesh.is_null()) {
			continue;
		}
		const AABB box = _transform_from_root(p_root, instance).xform(mesh->get_aabb());
		if (first) {
			bounds = box;
			first = false;
		} else {
			bounds.merge_with(box);
		}
	}
	return bounds;
}

static String _meters(real_t p_value) {
	return p_value >= 10.0 ? String::num(p_value, 0) : String::num(p_value, p_value >= 1.0 ? 2 : 3);
}

void EretheaExportChecks::check_model(const Node *p_root, const String &p_path, const EretheaExportCheckSettings &p_settings, Vector<EretheaExportFinding> &r_findings) {
	const bool character = is_character(p_path, p_settings);

	const int triangles = count_triangles(p_root);
	const int max_triangles = character ? p_settings.max_character_triangles : p_settings.max_model_triangles;
	if (triangles > max_triangles) {
		EretheaExportFinding finding;
		finding.check = "polygons";
		finding.file = p_path;
		finding.message = vformat("%s triangles, more than the %s allowed for %s. Use a lighter version, or turn on LOD generation in the import settings. %s",
				_thousands(triangles), _thousands(max_triangles), character ? "a character" : "a model",
				_setting_hint(character ? SETTING_MAX_CHARACTER_TRIANGLES : SETTING_MAX_MODEL_TRIANGLES));
		r_findings.push_back(finding);
	}

	const AABB bounds = get_model_bounds(p_root);
	if (bounds.get_longest_axis_size() > 0) {
		EretheaExportFinding finding;
		finding.check = "scale";
		finding.file = p_path;
		if (character) {
			const real_t height = bounds.size.y;
			if (height < p_settings.character_height.x || height > p_settings.character_height.y) {
				finding.message = vformat("The character is %s m tall, outside %s to %s m: it was probably exported in other units, such as centimeters instead of meters. Fix the scale in Blender, or with Root Scale in the import settings. %s",
						_meters(height), _meters(p_settings.character_height.x), _meters(p_settings.character_height.y), _setting_hint(SETTING_CHARACTER_HEIGHT));
				r_findings.push_back(finding);
			}
		} else {
			const real_t size = bounds.get_longest_axis_size();
			if (size < p_settings.model_size.x || size > p_settings.model_size.y) {
				finding.message = vformat("The model is %s m across, outside %s to %s m: it was probably exported in other units. Fix the scale in Blender, or with Root Scale in the import settings. %s",
						_meters(size), _meters(p_settings.model_size.x), _meters(p_settings.model_size.y), _setting_hint(SETTING_MODEL_SIZE));
				r_findings.push_back(finding);
			}
		}
	}

	// Textures inside the model file. Textures that were extracted to their
	// own files are checked as files.
	HashSet<const Texture2D *> seen;
	TypedArray<Node> meshes = const_cast<Node *>(p_root)->find_children("*", "MeshInstance3D", true, false);
	if (Object::cast_to<MeshInstance3D>(p_root)) {
		meshes.push_back(const_cast<Node *>(p_root));
	}
	for (const Variant &node : meshes) {
		const MeshInstance3D *instance = Object::cast_to<MeshInstance3D>(node);
		const Ref<Mesh> mesh = instance ? instance->get_mesh() : Ref<Mesh>();
		if (mesh.is_null()) {
			continue;
		}
		Vector<Ref<Material>> materials;
		materials.push_back(instance->get_material_override());
		for (int surface = 0; surface < mesh->get_surface_count(); surface++) {
			materials.push_back(mesh->surface_get_material(surface));
			materials.push_back(instance->get_surface_override_material(surface));
		}
		for (const Ref<Material> &material : materials) {
			const Ref<BaseMaterial3D> base = material;
			if (base.is_null()) {
				continue;
			}
			for (int parameter = 0; parameter < BaseMaterial3D::TEXTURE_MAX; parameter++) {
				const Ref<Texture2D> texture = base->get_texture(BaseMaterial3D::TextureParam(parameter));
				if (texture.is_null() || seen.has(texture.ptr()) || texture->get_path().is_resource_file()) {
					continue;
				}
				seen.insert(texture.ptr());
				const String name = texture->get_name().is_empty() ? base->get_name() : texture->get_name();
				check_texture(texture, p_path, name, p_settings, r_findings);
			}
		}
	}
}

void EretheaExportChecks::check_texture(const Ref<Texture2D> &p_texture, const String &p_path, const String &p_name, const EretheaExportCheckSettings &p_settings, Vector<EretheaExportFinding> &r_findings) {
	if (p_texture.is_null()) {
		return;
	}
	const int width = p_texture->get_width();
	const int height = p_texture->get_height();
	if (width <= p_settings.max_texture_size && height <= p_settings.max_texture_size) {
		return;
	}
	EretheaExportFinding finding;
	finding.check = "texture_size";
	finding.file = p_path;
	const String what = p_name.is_empty() ? String("The texture") : vformat("The texture \"%s\"", p_name);
	finding.message = vformat("%s is %dx%d, bigger than %d px. Lower it with Size Limit in the import settings, or export it smaller. %s",
			what, width, height, p_settings.max_texture_size, _setting_hint(SETTING_MAX_TEXTURE_SIZE));
	r_findings.push_back(finding);
}

HashMap<String, EretheaLicenseReceipt> EretheaExportChecks::parse_receipts(const String &p_jsonl) {
	HashMap<String, EretheaLicenseReceipt> receipts;
	Ref<JSON> json;
	json.instantiate();
	for (const String &line : p_jsonl.split("\n", false)) {
		// Like the Fornalha, skip lines that can't be read, quietly.
		if (line.strip_edges().is_empty() || json->parse(line) != OK || json->get_data().get_type() != Variant::DICTIONARY) {
			continue;
		}
		const Dictionary entry = json->get_data();
		const String file = String(entry.get("arquivo", "")).strip_edges().replace_char('\\', '/');
		if (file.is_empty()) {
			continue;
		}
		const Variant model_variant = entry.get("modelo", Dictionary());
		const Dictionary model = model_variant.get_type() == Variant::DICTIONARY ? Dictionary(model_variant) : Dictionary();

		EretheaLicenseReceipt receipt;
		receipt.file = file.begins_with("res://") ? file : "res://" + file;
		receipt.sha256 = String(entry.get("sha256", "")).to_lower();
		receipt.model_id = model.get("id", "");
		receipt.model_name = model.get("nome", receipt.model_id);
		receipt.license = model.get("licenca", "");
		receipt.commercial_use = String(model.get("uso_comercial", "")).to_lower();
		// Later lines are newer.
		receipts[receipt.file] = receipt;
	}
	return receipts;
}

bool EretheaExportChecks::needs_receipt(const String &p_path) {
	if (!p_path.begins_with("res://")) {
		return false;
	}
	const String relative = p_path.trim_prefix("res://");
	const String folder = relative.get_slicec('/', 0);
	if (!relative.contains_char('/')) {
		return false;
	}
	bool asset_folder = false;
	for (const char *candidate : RECEIPT_FOLDERS) {
		asset_folder = asset_folder || folder == candidate;
	}
	if (!asset_folder) {
		return false;
	}
	const String extension = p_path.get_extension().to_lower();
	for (const char *candidate : RECEIPT_EXTENSIONS) {
		if (extension == candidate) {
			return true;
		}
	}
	return false;
}

bool EretheaExportChecks::is_derived_from_receipted_model(const String &p_path, const HashMap<String, EretheaLicenseReceipt> &p_receipts) {
	// Godot extracts the textures of "folder/name.glb" as "folder/name_*.png".
	const String extension = p_path.get_extension().to_lower();
	if (extension != "png" && extension != "jpg" && extension != "jpeg" && extension != "webp") {
		return false;
	}
	const String folder = p_path.get_base_dir();
	const String file = p_path.get_file();
	for (const KeyValue<String, EretheaLicenseReceipt> &entry : p_receipts) {
		if (entry.key.get_extension().to_lower() == "glb" && entry.key.get_base_dir() == folder && file.begins_with(entry.key.get_file().get_basename() + "_")) {
			return true;
		}
	}
	return false;
}

void EretheaExportChecks::check_license(const String &p_path, const String &p_sha256, bool p_commercial, const HashMap<String, EretheaLicenseReceipt> &p_receipts, Vector<EretheaExportFinding> &r_findings) {
	EretheaExportFinding finding;
	finding.check = "license";
	finding.file = p_path;

	const EretheaLicenseReceipt *receipt = p_receipts.getptr(p_path);
	if (!receipt) {
		if (is_derived_from_receipted_model(p_path, p_receipts)) {
			return;
		}
		finding.level = p_commercial ? EretheaExportFinding::LEVEL_ERROR : EretheaExportFinding::LEVEL_WARNING;
		finding.message = vformat("No license receipt in %s. Register it in the Fornalha (register_receipt, or \"autoral\" if the studio made it).", String(RECEIPTS_FILE).trim_prefix("res://"));
		r_findings.push_back(finding);
		return;
	}

	if (receipt->commercial_use == "nao") {
		if (p_commercial) {
			finding.level = EretheaExportFinding::LEVEL_ERROR;
			finding.message = vformat("Made with %s, whose license (%s) doesn't allow selling it. Replace it before selling the game.", receipt->model_name, receipt->license);
			r_findings.push_back(finding);
		}
	} else if (receipt->commercial_use == "condicional") {
		if (p_commercial) {
			finding.message = vformat("Made with %s, whose license (%s) has conditions, such as accepting it or a revenue limit. Check it in the Fornalha before selling the game.", receipt->model_name, receipt->license);
			r_findings.push_back(finding);
		}
	} else if (receipt->commercial_use != "sim") {
		finding.message = vformat("The receipt doesn't say whether %s allows commercial use.", receipt->model_name);
		r_findings.push_back(finding);
	}

	if (!p_sha256.is_empty() && !receipt->sha256.is_empty() && p_sha256 != receipt->sha256) {
		EretheaExportFinding changed;
		changed.check = "license";
		changed.file = p_path;
		changed.message = "Changed after its license receipt was made (edited by hand or cleaned up in Blender). The receipt still points at the original.";
		r_findings.push_back(changed);
	}
}

EretheaExportChecks::Report EretheaExportChecks::check_files(const Vector<String> &p_files, const EretheaExportCheckSettings &p_settings) {
	Report report;
	report.fornalha_project = is_fornalha_project();

	// The Fornalha makes projects commercial unless told otherwise.
	report.commercial = report.fornalha_project;
	if (report.fornalha_project) {
		const Variant game_project = JSON::parse_string(FileAccess::get_file_as_string(GAME_PROJECT_FILE));
		if (game_project.get_type() == Variant::DICTIONARY) {
			report.commercial = Dictionary(game_project).get("comercial", true);
		}
	}
	const bool check_receipts = p_settings.license_receipts && (report.fornalha_project || FileAccess::exists(RECEIPTS_FILE));
	const HashMap<String, EretheaLicenseReceipt> receipts = check_receipts ? parse_receipts(FileAccess::get_file_as_string(RECEIPTS_FILE)) : HashMap<String, EretheaLicenseReceipt>();

	EditorFileSystem *file_system = EditorFileSystem::get_singleton();
	for (const String &path : p_files) {
		if (check_receipts && needs_receipt(path)) {
			report.licensed_assets++;
			check_license(path, FileAccess::get_sha256(path), report.commercial, receipts, report.findings);
		}

		if (!FileAccess::exists(path + ".import")) {
			continue;
		}
		const String type = file_system ? String(file_system->get_file_type(path)) : ResourceLoader::get_resource_type(path);
		if (type == "PackedScene") {
			const Ref<PackedScene> scene = ResourceLoader::load(path);
			Node *root = scene.is_valid() ? scene->instantiate() : nullptr;
			if (root) {
				report.models++;
				check_model(root, path, p_settings, report.findings);
				memdelete(root);
			}
		} else if (ClassDB::is_parent_class(type, "Mesh")) {
			const Ref<Mesh> mesh = ResourceLoader::load(path);
			if (mesh.is_valid()) {
				report.models++;
				MeshInstance3D *instance = memnew(MeshInstance3D);
				instance->set_mesh(mesh);
				check_model(instance, path, p_settings, report.findings);
				memdelete(instance);
			}
		} else if (ClassDB::is_parent_class(type, "Texture2D")) {
			const Ref<Texture2D> texture = ResourceLoader::load(path);
			if (texture.is_valid()) {
				report.textures++;
				check_texture(texture, path, String(), p_settings, report.findings);
			}
		}
	}

	// Errors first, then by file.
	struct FindingOrder {
		bool operator()(const EretheaExportFinding &p_a, const EretheaExportFinding &p_b) const {
			if (p_a.level != p_b.level) {
				return p_a.level > p_b.level;
			}
			if (p_a.file != p_b.file) {
				return p_a.file < p_b.file;
			}
			return p_a.check < p_b.check;
		}
	};
	report.findings.sort_custom<FindingOrder>();
	return report;
}
