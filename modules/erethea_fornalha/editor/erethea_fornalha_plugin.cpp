/**************************************************************************/
/*  erethea_fornalha_plugin.cpp                                           */
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

#include "erethea_fornalha_plugin.h"

#include "erethea_fornalha_link.h"

#include "core/config/project_settings.h"
#include "core/object/callable_mp.h"
#include "editor/editor_node.h"
#include "editor/editor_string_names.h"
#include "editor/settings/editor_settings.h"
#include "editor/themes/editor_scale.h"
#include "scene/animation/animation_mixer.h"
#include "scene/gui/box_container.h"
#include "scene/gui/label.h"
#include "scene/gui/tree.h"

int EretheaFornalhaAnimationLoops::loop_animations(Node *p_scene, const PackedStringArray &p_names, const Dictionary &p_animation_settings) {
	int looped = 0;
	AnimationMixer *mixer = Object::cast_to<AnimationMixer>(p_scene);
	if (mixer) {
		LocalVector<StringName> animations;
		mixer->get_animation_list(&animations);
		for (const StringName &name : animations) {
			if (!p_names.has(name)) {
				continue;
			}
			// What the person set in the import settings wins.
			const Variant settings_value = p_animation_settings.get(name, Variant());
			const Dictionary settings = settings_value.get_type() == Variant::DICTIONARY ? Dictionary(settings_value) : Dictionary();
			if (settings.has("settings/loop_mode") || bool(settings.get("save_to_file/enabled", false))) {
				continue;
			}
			const Ref<Animation> animation = mixer->get_animation(name);
			if (animation.is_valid() && animation->get_loop_mode() == Animation::LOOP_NONE) {
				animation->set_loop_mode(Animation::LOOP_LINEAR);
				looped++;
			}
		}
	}
	for (int i = 0; i < p_scene->get_child_count(); i++) {
		looped += loop_animations(p_scene->get_child(i), p_names, p_animation_settings);
	}
	return looped;
}

void EretheaFornalhaAnimationLoops::post_process(Node *p_scene, const HashMap<StringName, Variant> &p_options) {
	if (!p_scene || !EretheaFornalhaImport::is_fornalha_project()) {
		return;
	}
	const PackedStringArray names = GLOBAL_GET(EretheaFornalhaImport::SETTING_LOOPING_ANIMATIONS);
	if (names.is_empty()) {
		return;
	}
	Dictionary animation_settings;
	if (p_options.has("_subresources") && p_options["_subresources"].get_type() == Variant::DICTIONARY) {
		const Variant animations = Dictionary(p_options["_subresources"]).get("animations", Variant());
		if (animations.get_type() == Variant::DICTIONARY) {
			animation_settings = animations;
		}
	}
	loop_animations(p_scene, names, animation_settings);
}

void EretheaFornalhaBuildDialog::build(bool p_force) {
	const EretheaFornalhaImport::Report report = EretheaFornalhaImport::build(p_force);

	tree->clear();
	TreeItem *root = tree->create_item();
	for (const EretheaFornalhaImport::FileResult &file : report.files) {
		TreeItem *item = tree->create_item(root);
		StringName icon = SNAME("StatusSuccess");
		String result = TTR("Unchanged");
		switch (file.action) {
			case EretheaFornalhaImport::ACTION_CREATED:
				result = TTR("Created");
				break;
			case EretheaFornalhaImport::ACTION_UPDATED:
				result = TTR("Updated");
				break;
			case EretheaFornalhaImport::ACTION_REPLACED:
				result = TTR("Replaced");
				break;
			case EretheaFornalhaImport::ACTION_KEPT_EDITED:
			case EretheaFornalhaImport::ACTION_KEPT_FOREIGN:
			case EretheaFornalhaImport::ACTION_KEPT_UNSAVED:
				result = TTR("Kept");
				icon = SNAME("StatusWarning");
				break;
			case EretheaFornalhaImport::ACTION_FAILED:
				result = TTR("Failed");
				icon = SNAME("StatusError");
				break;
			default:
				break;
		}
		item->set_icon(0, get_editor_theme_icon(icon));
		item->set_text(0, result);
		item->set_text(1, file.file.trim_prefix("res://"));
		item->set_tooltip_text(1, file.file);
		item->set_metadata(1, file.file);
		item->set_text(2, file.message);
		item->set_tooltip_text(2, file.message);
	}
	for (const String &warning : report.warnings) {
		TreeItem *item = tree->create_item(root);
		item->set_icon(0, get_editor_theme_icon(SNAME("StatusWarning")));
		item->set_text(0, TTR("Warning"));
		item->set_text(2, warning);
		item->set_tooltip_text(2, warning);
	}

	String text = report.get_summary();
	if (report.error.is_empty()) {
		text += " " + TTR("Double-click a scene to open it.");
		if (report.count_kept() > 0) {
			text += "\n" + TTR("Kept scenes were changed by hand or made elsewhere. \"Replace Kept Scenes\" builds them again and keeps a copy of the old ones in res://.erethea/backup/.");
		}
	}
	summary->set_text(text);
}

void EretheaFornalhaBuildDialog::_item_activated() {
	TreeItem *item = tree->get_selected();
	if (!item) {
		return;
	}
	const String file = item->get_metadata(1);
	if (!file.is_empty() && FileAccess::exists(file)) {
		EditorNode::get_singleton()->open_scene(file);
		hide();
	}
}

void EretheaFornalhaBuildDialog::custom_action(const String &p_action) {
	if (p_action == "build_again") {
		build(false);
	} else if (p_action == "replace_kept") {
		build(true);
	}
}

EretheaFornalhaBuildDialog::EretheaFornalhaBuildDialog() {
	set_title(TTR("Build Scenes from game.project.json"));
	set_ok_button_text(TTR("Close"));
	add_button(TTR("Build Again"), true, "build_again");
	add_button(TTR("Replace Kept Scenes"), true, "replace_kept");

	VBoxContainer *box = memnew(VBoxContainer);
	add_child(box);

	summary = memnew(Label);
	summary->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	box->add_child(summary);

	tree = memnew(Tree);
	tree->set_columns(3);
	tree->set_column_titles_visible(true);
	tree->set_column_title(0, TTR("Result"));
	tree->set_column_title(1, TTR("Scene"));
	tree->set_column_title(2, TTR("Details"));
	tree->set_column_expand(0, false);
	tree->set_column_custom_minimum_width(0, 120 * EDSCALE);
	tree->set_column_expand_ratio(1, 1);
	tree->set_column_expand_ratio(2, 2);
	tree->set_hide_root(true);
	tree->set_select_mode(Tree::SELECT_ROW);
	tree->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	tree->set_custom_minimum_size(Size2(900, 320) * EDSCALE);
	tree->connect("item_activated", callable_mp(this, &EretheaFornalhaBuildDialog::_item_activated));
	box->add_child(tree);
}

void EretheaFornalhaPlugin::_show_dialog() {
	dialog->popup_centered_clamped(Size2(1000, 480) * EDSCALE);
	dialog->build(false);
}

void EretheaFornalhaPlugin::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			add_scene_post_import_plugin(animation_loops);
		} break;

		case NOTIFICATION_EXIT_TREE: {
			remove_scene_post_import_plugin(animation_loops);
		} break;
	}
}

EretheaFornalhaPlugin::EretheaFornalhaPlugin() {
	animation_loops.instantiate();
	EDITOR_DEF(EretheaFornalhaLink::SETTING_PORT, EretheaFornalhaLink::DEFAULT_PORT);
	EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::INT, EretheaFornalhaLink::SETTING_PORT, PROPERTY_HINT_RANGE, "1024,65535,1"));

	// Godot projects see no new menu and don't look for the Fornalha.
	if (EretheaFornalhaImport::is_fornalha_project()) {
		dialog = memnew(EretheaFornalhaBuildDialog);
		EditorNode::get_singleton()->get_gui_base()->add_child(dialog);
		add_tool_menu_item(TTR("Build Scenes from game.project.json..."), callable_mp(this, &EretheaFornalhaPlugin::_show_dialog));
		link = memnew(EretheaFornalhaLink);
		add_child(link);
	}
}
