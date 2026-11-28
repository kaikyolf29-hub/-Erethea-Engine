/**************************************************************************/
/*  erethea_mcp_scene_tools.cpp                                           */
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

#include "erethea_mcp_scene_tools.h"

#include "../erethea_mcp_values.h"

#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "core/io/resource_uid.h"
#include "core/object/class_db.h"
#include "core/object/editor_language.h"
#include "core/object/script_language.h"
#include "editor/docks/scene_tree_dock.h"
#include "editor/editor_data.h"
#include "editor/editor_node.h"
#include "editor/editor_undo_redo_manager.h"
#include "editor/file_system/editor_file_system.h"
#include "editor/script/script_editor_plugin.h"
#include "scene/resources/packed_scene.h"

namespace EretheaMcpSceneTools {

static constexpr int DEFAULT_TREE_DEPTH = 12;
static constexpr int MAX_TREE_NODES = 2000;

static const char *action_source = "MCP";

void set_action_source(const char *p_source) {
	action_source = p_source;
}

const char *get_action_source() {
	return action_source;
}

static EretheaMcpToolResult _error(const String &p_message) {
	return EretheaMcpToolResult::error(p_message);
}

static String _string(const Dictionary &p_arguments, const String &p_name, const String &p_default = String()) {
	const Variant value = p_arguments.get(p_name, Variant());
	return value.get_type() == Variant::STRING ? String(value) : p_default;
}

bool project_path(const Variant &p_path, String &r_path, String &r_error) {
	if (p_path.get_type() != Variant::STRING || String(p_path).strip_edges().is_empty()) {
		r_error = "Missing the file path, such as \"res://levels/forest.tscn\".";
		return false;
	}
	String path = String(p_path).strip_edges().replace_char('\\', '/');
	if (path.begins_with("uid://")) {
		path = ResourceUID::ensure_path(path);
	}
	if (!path.begins_with("res://")) {
		path = path.is_absolute_path() ? ProjectSettings::get_singleton()->localize_path(path) : "res://" + path;
	}
	path = path.simplify_path();
	if (!path.begins_with("res://") || path.contains("..")) {
		r_error = vformat("\"%s\" isn't inside the project (res://).", String(p_path));
		return false;
	}
	r_path = path;
	return true;
}

static bool _scene_file_path(const Variant &p_path, String &r_path, String &r_error) {
	if (!project_path(p_path, r_path, r_error)) {
		return false;
	}
	const String extension = r_path.get_extension().to_lower();
	if (extension != "tscn" && extension != "scn") {
		r_error = vformat("\"%s\" isn't a scene file: use .tscn (or .scn).", r_path);
		return false;
	}
	return true;
}

// A scene to instance: a .tscn or .scn, or a 3D model that the editor imports
// as a scene (.glb, .gltf, .fbx, .blend and the like), as the FileSystem dock
// instances it when it's dragged to the scene.
static bool _instanceable_scene_path(const Variant &p_path, String &r_path, String &r_error) {
	if (!project_path(p_path, r_path, r_error)) {
		return false;
	}
	if (!ResourceLoader::exists(r_path)) {
		if (FileAccess::exists(r_path)) {
			r_error = vformat("%s isn't imported yet. Call rescan_files, wait until get_editor_state says it's neither scanning nor importing, and try again.", r_path);
		} else {
			r_error = vformat("%s doesn't exist.", r_path);
		}
		return false;
	}
	const String type = ResourceLoader::get_resource_type(r_path);
	if (!ClassDB::is_parent_class(type, "PackedScene")) {
		r_error = vformat("%s isn't a scene, nor a model imported as a scene: the editor loads it as %s.", r_path, type.is_empty() ? String("nothing") : type);
		if (ClassDB::is_parent_class(type, "Mesh")) {
			r_error += vformat(" Add a MeshInstance3D with {\"mesh\": \"%s\"} instead.", r_path);
		}
		return false;
	}
	return true;
}

static Node *_edited_scene(String &r_error) {
	Node *root = EditorNode::get_singleton()->get_edited_scene();
	if (!root) {
		r_error = "No scene is open. Open one with open_scene or create one with create_scene.";
	}
	return root;
}

// The nodes that are saved with the scene: its own, and those of instanced
// scenes with editable children.
static bool _is_in_scene(const Node *p_root, const Node *p_node) {
	if (p_node == p_root || p_node->get_owner() == p_root) {
		return true;
	}
	Node *owner = p_node->get_owner();
	return owner && p_root->is_editable_instance(owner) && _is_in_scene(p_root, owner);
}

static String _path_of(const Node *p_root, const Node *p_node) {
	return String(p_root->get_path_to(p_node));
}

static Node *_find_node(const Variant &p_path, String &r_error) {
	Node *root = _edited_scene(r_error);
	if (!root) {
		return nullptr;
	}
	String path = p_path.get_type() == Variant::STRING ? String(p_path).strip_edges() : String();
	const String root_name = root->get_name();
	if (path.is_empty() || path == "." || path == root_name) {
		return root;
	}
	if (path.begins_with(root_name + "/")) {
		path = path.substr(root_name.length() + 1);
	}
	Node *node = root->get_node_or_null(NodePath(path));
	if (!node || !root->is_ancestor_of(node)) {
		r_error = vformat("No node \"%s\" in the scene being edited (%s). get_scene_tree lists the paths.", path, root->get_scene_file_path());
		return nullptr;
	}
	if (!_is_in_scene(root, node)) {
		r_error = vformat("\"%s\" is inside the instanced scene %s. Open that scene to change it.", path, node->get_owner() ? node->get_owner()->get_scene_file_path() : String());
		return nullptr;
	}
	return node;
}

static Node *_instantiate_node(const String &p_type, String &r_error) {
	if (p_type.is_empty()) {
		r_error = "Missing the node type, such as \"Node3D\".";
		return nullptr;
	}
	if (ScriptServer::is_global_class(p_type)) {
		const StringName native = ScriptServer::get_global_class_native_base(p_type);
		const Ref<Script> script = ResourceLoader::load(ScriptServer::get_global_class_path(p_type));
		if (script.is_null() || !ClassDB::is_parent_class(native, "Node") || ScriptServer::is_global_class_abstract(p_type)) {
			r_error = vformat("The script class \"%s\" can't be used as a node.", p_type);
			return nullptr;
		}
		Node *node = Object::cast_to<Node>(ClassDB::instantiate(native));
		ERR_FAIL_NULL_V(node, nullptr);
		node->set_script(script);
		return node;
	}
	if (!ClassDB::class_exists(p_type) || !ClassDB::is_parent_class(p_type, "Node")) {
		r_error = vformat("\"%s\" isn't a node type.", p_type);
		return nullptr;
	}
	const ClassDB::APIType api = ClassDB::get_api_type(p_type);
	if (!ClassDB::can_instantiate(p_type) || !ClassDB::is_class_exposed(p_type) || api == ClassDB::API_EDITOR || api == ClassDB::API_EDITOR_EXTENSION) {
		r_error = vformat("A %s can't be added to a scene.", p_type);
		return nullptr;
	}
	Node *node = Object::cast_to<Node>(ClassDB::instantiate(p_type));
	if (!node) {
		r_error = vformat("A %s couldn't be created.", p_type);
	}
	return node;
}

static Dictionary _describe(const Node *p_root, Node *p_node, int p_depth, int p_max_depth, int &r_count, bool &r_truncated) {
	Dictionary description;
	description["name"] = String(p_node->get_name());
	description["type"] = p_node->get_class();
	description["path"] = _path_of(p_root, p_node);
	const Ref<Script> script = p_node->get_script();
	if (script.is_valid()) {
		description["script"] = script->get_path();
	}
	if (p_node != p_root && !p_node->get_scene_file_path().is_empty()) {
		description["scene"] = p_node->get_scene_file_path();
	}
	r_count++;

	Array children;
	const bool show_children = p_node == p_root || p_node->get_scene_file_path().is_empty() || p_root->is_editable_instance(p_node);
	for (int i = 0; show_children && i < p_node->get_child_count(false); i++) {
		Node *child = p_node->get_child(i, false);
		if (!_is_in_scene(p_root, child)) {
			continue;
		}
		if (p_depth >= p_max_depth || r_count >= MAX_TREE_NODES) {
			r_truncated = true;
			break;
		}
		children.push_back(_describe(p_root, child, p_depth + 1, p_max_depth, r_count, r_truncated));
	}
	if (!children.is_empty()) {
		description["children"] = children;
	}
	return description;
}

EretheaMcpToolResult get_scene_tree(const Dictionary &p_arguments) {
	String error;
	Node *root = _edited_scene(error);
	if (!root) {
		return _error(error);
	}
	const int max_depth = MAX(1, int(p_arguments.get("max_depth", DEFAULT_TREE_DEPTH)));
	int count = 0;
	bool truncated = false;
	Dictionary result;
	result["scene"] = root->get_scene_file_path();
	result["root"] = _describe(root, root, 1, max_depth, count, truncated);
	result["node_count"] = count;
	result["truncated"] = truncated;
	return EretheaMcpToolResult::object(result);
}

EretheaMcpToolResult get_node_properties(const Dictionary &p_arguments) {
	String error;
	Node *node = _find_node(p_arguments.get("node", "."), error);
	if (!node) {
		return _error(error);
	}
	Dictionary values;
	const Variant names = p_arguments.get("properties", Array());
	if (names.get_type() == Variant::ARRAY && !Array(names).is_empty()) {
		for (const Variant &name : Array(names)) {
			PropertyInfo property;
			if (!EretheaMcpValues::find_property(node, String(name), property)) {
				return _error(vformat("%s has no property \"%s\".", node->get_class(), name));
			}
			values[name] = EretheaMcpValues::to_json(node->get(String(name)));
		}
	} else {
		List<PropertyInfo> properties;
		node->get_property_list(&properties);
		for (const PropertyInfo &property : properties) {
			if ((property.usage & (PROPERTY_USAGE_CATEGORY | PROPERTY_USAGE_GROUP | PROPERTY_USAGE_SUBGROUP)) || !(property.usage & PROPERTY_USAGE_EDITOR)) {
				continue;
			}
			values[property.name] = EretheaMcpValues::to_json(node->get(property.name));
		}
	}
	Dictionary result;
	result["node"] = _path_of(EditorNode::get_singleton()->get_edited_scene(), node);
	result["type"] = node->get_class();
	result["properties"] = values;
	return EretheaMcpToolResult::object(result);
}

EretheaMcpToolResult open_scene(const Dictionary &p_arguments) {
	String path;
	String error;
	if (!project_path(p_arguments.get("path", Variant()), path, error)) {
		return _error(error);
	}
	if (!ResourceLoader::exists(path)) {
		return _error(vformat("%s doesn't exist.", path));
	}
	if (!ClassDB::is_parent_class(ResourceLoader::get_resource_type(path), "PackedScene")) {
		return _error(vformat("%s isn't a scene.", path));
	}
	const Error result_error = EditorNode::get_singleton()->open_scene(path);
	Node *root = EditorNode::get_singleton()->get_edited_scene();
	if (result_error != OK || !root || root->get_scene_file_path() != path) {
		return _error(vformat("%s couldn't be opened (%s). get_errors may say why.", path, error_names[result_error]));
	}
	Dictionary result;
	result["scene"] = path;
	result["root"] = String(root->get_name());
	result["root_type"] = root->get_class();
	return EretheaMcpToolResult::object(result);
}

EretheaMcpToolResult create_scene(const Dictionary &p_arguments) {
	String path;
	String error;
	if (!_scene_file_path(p_arguments.get("path", Variant()), path, error)) {
		return _error(error);
	}
	if (FileAccess::exists(path)) {
		return _error(vformat("%s already exists. Open it with open_scene.", path));
	}
	Node *root = _instantiate_node(_string(p_arguments, "root_type", "Node3D"), error);
	if (!root) {
		return _error(error);
	}
	String name = _string(p_arguments, "root_name").validate_node_name();
	if (name.is_empty()) {
		name = Node::adjust_name_casing(path.get_file().get_basename()).validate_node_name();
	}
	root->set_name(name.is_empty() ? root->get_class() : name);

	Ref<PackedScene> scene;
	scene.instantiate();
	Error result_error = scene->pack(root);
	memdelete(root);
	if (result_error == OK) {
		result_error = DirAccess::make_dir_recursive_absolute(ProjectSettings::get_singleton()->globalize_path(path.get_base_dir()));
	}
	if (result_error == OK) {
		result_error = ResourceSaver::save(scene, path);
	}
	if (result_error != OK) {
		return _error(vformat("%s couldn't be saved (%s).", path, error_names[result_error]));
	}
	EditorFileSystem::get_singleton()->update_file(path);
	return open_scene(Dictionary{ { "path", path } });
}

EretheaMcpToolResult create_node(const Dictionary &p_arguments) {
	String error;
	Node *root = _edited_scene(error);
	if (!root) {
		return _error(error);
	}
	Node *parent = _find_node(p_arguments.get("parent", "."), error);
	if (!parent) {
		return _error(error);
	}
	const String type = _string(p_arguments, "type");
	const Variant scene_argument = p_arguments.get("scene", Variant());
	if (type.is_empty() == (scene_argument.get_type() == Variant::NIL)) {
		return _error("Give either \"type\" (a node class) or \"scene\" (a scene to instance).");
	}

	Node *child = nullptr;
	String name = _string(p_arguments, "name").validate_node_name();
	if (!type.is_empty()) {
		child = _instantiate_node(type, error);
		if (!child) {
			return _error(error);
		}
		if (name.is_empty()) {
			name = Node::adjust_name_casing(type).validate_node_name();
		}
	} else {
		String path;
		if (!_instanceable_scene_path(scene_argument, path, error)) {
			return _error(error);
		}
		if (path == root->get_scene_file_path()) {
			return _error("A scene can't be instanced inside itself.");
		}
		const Ref<PackedScene> packed = ResourceLoader::load(path);
		if (packed.is_null()) {
			return _error(vformat("%s couldn't be loaded as a scene. get_errors may say why.", path));
		}
		child = packed->instantiate(PackedScene::GEN_EDIT_STATE_INSTANCE);
		if (!child) {
			return _error(vformat("%s couldn't be instanced. get_errors may say why.", path));
		}
		child->set_scene_file_path(path);
		if (name.is_empty()) {
			name = Node::adjust_name_casing(path.get_file().get_basename()).validate_node_name();
		}
	}

	const Variant properties = p_arguments.get("properties", Dictionary());
	if (properties.get_type() != Variant::DICTIONARY) {
		memdelete(child);
		return _error("\"properties\" must be an object.");
	}
	if (!EretheaMcpValues::set_properties(child, properties, error)) {
		memdelete(child);
		return _error(error);
	}
	child->set_name(parent->prevalidate_child_name(child, name.is_empty() ? child->get_class() : name));

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(vformat("%s: Create %s", String(action_source), child->get_name()), UndoRedo::MERGE_DISABLE, root);
	undo_redo->add_do_method(parent, "add_child", child, true);
	undo_redo->add_do_method(child, "set_owner", root);
	undo_redo->add_do_reference(child);
	undo_redo->add_undo_method(parent, "remove_child", child);
	undo_redo->commit_action();

	Dictionary result;
	result["node"] = _path_of(root, child);
	result["name"] = String(child->get_name());
	result["type"] = child->get_class();
	return EretheaMcpToolResult::object(result);
}

EretheaMcpToolResult set_node_properties(const Dictionary &p_arguments) {
	String error;
	Node *node = _find_node(p_arguments.get("node", Variant()), error);
	if (!node) {
		return _error(error);
	}
	const Variant properties_variant = p_arguments.get("properties", Variant());
	if (properties_variant.get_type() != Variant::DICTIONARY || Dictionary(properties_variant).is_empty()) {
		return _error("\"properties\" must be an object with at least one property, such as {\"visible\": true}.");
	}
	const Dictionary properties = properties_variant;

	// Everything is converted first, so that a mistake changes nothing.
	LocalVector<Pair<StringName, Variant>> values;
	PackedStringArray names;
	for (const KeyValue<Variant, Variant> &entry : properties) {
		const String name = entry.key;
		PropertyInfo property;
		if (!EretheaMcpValues::find_property(node, name, property)) {
			return _error(vformat("%s has no property \"%s\". get_node_properties lists them.", node->get_class(), name));
		}
		Variant value;
		if (!EretheaMcpValues::from_json(entry.value, property, value, error)) {
			return _error(vformat("\"%s\" %s.", name, error));
		}
		values.push_back(Pair<StringName, Variant>(name, value));
		names.push_back(name);
	}

	Node *root = EditorNode::get_singleton()->get_edited_scene();
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(vformat("%s: Set %s on %s", String(action_source), String(", ").join(names), node->get_name()), UndoRedo::MERGE_DISABLE, root);
	for (const Pair<StringName, Variant> &value : values) {
		undo_redo->add_do_property(node, value.first, value.second);
		undo_redo->add_undo_property(node, value.first, node->get(value.first));
	}
	undo_redo->commit_action();

	Dictionary new_values;
	for (const Pair<StringName, Variant> &value : values) {
		new_values[value.first] = EretheaMcpValues::to_json(node->get(value.first));
	}
	Dictionary result;
	result["node"] = _path_of(root, node);
	result["properties"] = new_values;
	return EretheaMcpToolResult::object(result);
}

EretheaMcpToolResult delete_node(const Dictionary &p_arguments) {
	String error;
	Node *node = _find_node(p_arguments.get("node", Variant()), error);
	if (!node) {
		return _error(error);
	}
	Node *root = EditorNode::get_singleton()->get_edited_scene();
	if (node == root) {
		return _error("The scene's root can't be deleted. Create another scene with create_scene instead.");
	}
	if (node->get_owner() != root) {
		return _error(vformat("\"%s\" belongs to an instanced scene and can't be deleted here.", _path_of(root, node)));
	}
	const String path = _path_of(root, node);

	// Nothing may keep showing the node.
	EditorNode::get_singleton()->get_editor_selection()->clear();
	EditorNode::get_singleton()->push_item(nullptr);

	List<Node *> owned;
	node->get_owned_by(node->get_owner(), &owned);
	Array owners;
	for (Node *owned_node : owned) {
		owners.push_back(owned_node);
	}

	Node *parent = node->get_parent();
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(vformat("%s: Delete %s", String(action_source), node->get_name()), UndoRedo::MERGE_DISABLE, root);
	undo_redo->add_do_method(parent, "remove_child", node);
	undo_redo->add_undo_method(parent, "add_child", node, true);
	undo_redo->add_undo_method(parent, "move_child", node, node->get_index(false));
	undo_redo->add_undo_method(SceneTreeDock::get_singleton(), "_set_owners", root, owners);
	undo_redo->add_undo_reference(node);
	undo_redo->commit_action();

	Dictionary result;
	result["deleted"] = path;
	return EretheaMcpToolResult::object(result);
}

static ScriptLanguage *_language_for(const String &p_path) {
	const String extension = p_path.get_extension().to_lower();
	for (int i = 0; i < ScriptServer::get_language_count(); i++) {
		ScriptLanguage *language = ScriptServer::get_language(i);
		if (language->get_extension() == extension) {
			return language;
		}
	}
	return nullptr;
}

EretheaMcpToolResult write_script(const Dictionary &p_arguments) {
	String path;
	String error;
	if (!project_path(p_arguments.get("path", Variant()), path, error)) {
		return _error(error);
	}
	ScriptLanguage *language = _language_for(path);
	if (!language) {
		return _error(vformat("%s isn't a script file: GDScript files end in .gd.", path));
	}
	const Variant source = p_arguments.get("source", Variant());
	if (source.get_type() != Variant::STRING) {
		return _error("Missing \"source\", the code of the script.");
	}

	List<EditorLanguage::ScriptError> errors;
	List<EditorLanguage::Warning> warnings;
	language->get_editor_language()->validate(source, path, &errors, &warnings, nullptr, nullptr);

	Error result_error = DirAccess::make_dir_recursive_absolute(ProjectSettings::get_singleton()->globalize_path(path.get_base_dir()));
	Ref<FileAccess> file;
	if (result_error == OK) {
		file = FileAccess::open(path, FileAccess::WRITE, &result_error);
	}
	if (file.is_null()) {
		return _error(vformat("%s couldn't be written (%s).", path, error_names[result_error]));
	}
	file->store_string(source);
	file.unref();

	// Scripts already loaded, and open in the script editor, take the new code.
	if (ResourceCache::has(path)) {
		Ref<Script> script = ResourceCache::get_ref(path);
		if (script.is_valid()) {
			script->set_source_code(source);
			script->reload(true);
		}
	}
	ScriptEditor::get_singleton()->reload_scripts();
	EditorFileSystem::get_singleton()->update_file(path);

	Array error_list;
	for (const EditorLanguage::ScriptError &script_error : errors) {
		Dictionary entry;
		entry["line"] = script_error.start_line;
		entry["column"] = script_error.start_column;
		entry["message"] = script_error.message;
		if (!script_error.path.is_empty() && script_error.path != path) {
			entry["file"] = script_error.path;
		}
		error_list.push_back(entry);
	}
	Array warning_list;
	for (const EditorLanguage::Warning &warning : warnings) {
		Dictionary entry;
		entry["line"] = warning.start_line;
		entry["code"] = warning.string_code;
		entry["message"] = warning.message;
		warning_list.push_back(entry);
	}
	Dictionary result;
	result["path"] = path;
	result["valid"] = errors.is_empty();
	result["errors"] = error_list;
	result["warnings"] = warning_list;
	EretheaMcpToolResult tool_result = EretheaMcpToolResult::object(result);
	// Saved, but the agent has to fix it.
	tool_result.is_error = !errors.is_empty();
	return tool_result;
}

EretheaMcpToolResult attach_script(const Dictionary &p_arguments) {
	String error;
	Node *node = _find_node(p_arguments.get("node", Variant()), error);
	if (!node) {
		return _error(error);
	}
	String path;
	if (!project_path(p_arguments.get("path", Variant()), path, error)) {
		return _error(error);
	}
	if (!ResourceLoader::exists(path)) {
		return _error(vformat("%s doesn't exist. Write it first with write_script.", path));
	}
	const Ref<Script> script = ResourceLoader::load(path);
	if (script.is_null()) {
		return _error(vformat("%s isn't a script.", path));
	}
	ScriptLanguage *language = _language_for(path);
	if (language && script->has_source_code()) {
		List<EditorLanguage::ScriptError> errors;
		language->get_editor_language()->validate(script->get_source_code(), path, &errors, nullptr, nullptr, nullptr);
		if (!errors.is_empty()) {
			return _error(vformat("%s has errors. write_script shows them.", path));
		}
	}
	const StringName base = script->get_instance_base_type();
	if (base != StringName() && !ClassDB::is_parent_class(node->get_class_name(), base)) {
		return _error(vformat("%s extends %s, but \"%s\" is a %s.", path, base, node->get_name(), node->get_class()));
	}

	Node *root = EditorNode::get_singleton()->get_edited_scene();
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(vformat("%s: Attach %s to %s", String(action_source), path.get_file(), node->get_name()), UndoRedo::MERGE_DISABLE, root);
	undo_redo->add_do_method(node, "set_script", script);
	undo_redo->add_undo_method(node, "set_script", node->get_script());
	undo_redo->commit_action();

	Dictionary result;
	result["node"] = _path_of(root, node);
	result["script"] = path;
	return EretheaMcpToolResult::object(result);
}

EretheaMcpToolResult save_scene(const Dictionary &p_arguments) {
	String error;
	Node *root = _edited_scene(error);
	if (!root) {
		return _error(error);
	}
	String path = root->get_scene_file_path();
	if (p_arguments.has("path")) {
		if (!_scene_file_path(p_arguments["path"], path, error)) {
			return _error(error);
		}
		DirAccess::make_dir_recursive_absolute(ProjectSettings::get_singleton()->globalize_path(path.get_base_dir()));
	}
	if (path.is_empty()) {
		return _error("The scene has no file yet: pass \"path\".");
	}
	// Without the preview, which needs a rendered viewport.
	EditorNode::get_singleton()->save_scene_to_path(path, false);
	const int history = EditorNode::get_editor_data().get_current_edited_scene_history_id();
	if (!FileAccess::exists(path) || EditorUndoRedoManager::get_singleton()->is_history_unsaved(history)) {
		return _error(vformat("%s couldn't be saved. get_errors may say why.", path));
	}
	Dictionary result;
	result["scene"] = path;
	return EretheaMcpToolResult::object(result);
}

EretheaMcpToolResult rescan_files(const Dictionary &p_arguments) {
	EditorFileSystem::get_singleton()->scan_changes();
	Dictionary result;
	result["scanning"] = true;
	return EretheaMcpToolResult::object(result);
}

} // namespace EretheaMcpSceneTools
