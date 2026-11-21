/**************************************************************************/
/*  erethea_export_checks_plugin.cpp                                      */
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

#include "erethea_export_checks_plugin.h"

#include "core/object/callable_mp.h"
#include "editor/docks/filesystem_dock.h"
#include "editor/editor_node.h"
#include "editor/editor_string_names.h"
#include "editor/export/editor_export_platform.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/box_container.h"
#include "scene/gui/label.h"
#include "scene/gui/tree.h"

void EretheaExportChecksExportPlugin::_export_begin(const HashSet<String> &p_features, bool p_debug, const String &p_path, int p_flags) {
	if (!EretheaExportChecks::should_run_on_export()) {
		return;
	}
	const Ref<EditorExportPlatform> platform = get_export_platform();
	if (platform.is_null()) {
		return;
	}

	const Vector<String> files = EretheaExportChecks::get_files_to_export(get_export_preset());
	const EretheaExportChecks::Report report = EretheaExportChecks::check_files(files, EretheaExportCheckSettings::from_project_settings());
	const String category = TTR("Erethea Checks");
	for (const EretheaExportFinding &finding : report.findings) {
		const EditorExportPlatform::ExportMessageType type = finding.level == EretheaExportFinding::LEVEL_ERROR ? EditorExportPlatform::EXPORT_MESSAGE_ERROR : EditorExportPlatform::EXPORT_MESSAGE_WARNING;
		platform->add_message(type, category, vformat("%s: %s", finding.file, finding.message));
	}
	platform->add_message(EditorExportPlatform::EXPORT_MESSAGE_INFO, category,
			vformat(TTR("Checked %d models, %d textures and %d license receipts: %d errors, %d warnings."),
					report.models, report.textures, report.licensed_assets, report.count(EretheaExportFinding::LEVEL_ERROR), report.count(EretheaExportFinding::LEVEL_WARNING)));
}

static String _check_title(const String &p_check) {
	if (p_check == "polygons") {
		return TTR("Polygons");
	} else if (p_check == "texture_size") {
		return TTR("Texture Size");
	} else if (p_check == "scale") {
		return TTR("Scale");
	} else if (p_check == "license") {
		return TTR("License");
	}
	return p_check;
}

void EretheaExportChecksDialog::run_checks() {
	const EretheaExportChecks::Report report = EretheaExportChecks::check_files(EretheaExportChecks::get_project_files(), EretheaExportCheckSettings::from_project_settings());

	tree->clear();
	TreeItem *root = tree->create_item();
	for (const EretheaExportFinding &finding : report.findings) {
		TreeItem *item = tree->create_item(root);
		item->set_icon(0, get_editor_theme_icon(finding.level == EretheaExportFinding::LEVEL_ERROR ? SNAME("StatusError") : SNAME("StatusWarning")));
		item->set_text(0, _check_title(finding.check));
		item->set_text(1, finding.file.trim_prefix("res://"));
		item->set_tooltip_text(1, finding.file);
		item->set_metadata(1, finding.file);
		item->set_text(2, finding.message);
		item->set_tooltip_text(2, finding.message);
	}

	const int errors = report.count(EretheaExportFinding::LEVEL_ERROR);
	const int warnings = report.count(EretheaExportFinding::LEVEL_WARNING);
	String text = vformat(TTR("Checked %d models, %d textures and %d license receipts."), report.models, report.textures, report.licensed_assets);
	if (errors == 0 && warnings == 0) {
		text += " " + TTR("Nothing to fix.");
	} else {
		text += " " + vformat(TTR("%d errors and %d warnings. Double-click a line to find the file."), errors, warnings);
	}
	if (!report.fornalha_project) {
		text += "\n" + TTR("License receipts are checked in projects made with the Fornalha (with a game.project.json).");
	}
	summary->set_text(text);
}

void EretheaExportChecksDialog::_item_activated() {
	TreeItem *item = tree->get_selected();
	if (!item) {
		return;
	}
	const String file = item->get_metadata(1);
	if (!file.is_empty() && FileSystemDock::get_singleton()) {
		FileSystemDock::get_singleton()->navigate_to_path(file);
	}
}

void EretheaExportChecksDialog::custom_action(const String &p_action) {
	if (p_action == "check_again") {
		run_checks();
	}
}

EretheaExportChecksDialog::EretheaExportChecksDialog() {
	set_title(TTR("Check Project for Export"));
	set_ok_button_text(TTR("Close"));
	add_button(TTR("Check Again"), true, "check_again");

	VBoxContainer *box = memnew(VBoxContainer);
	add_child(box);

	summary = memnew(Label);
	summary->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	box->add_child(summary);

	tree = memnew(Tree);
	tree->set_columns(3);
	tree->set_column_titles_visible(true);
	tree->set_column_title(0, TTR("Check"));
	tree->set_column_title(1, TTR("File"));
	tree->set_column_title(2, TTR("Problem"));
	tree->set_column_expand(0, false);
	tree->set_column_custom_minimum_width(0, 140 * EDSCALE);
	tree->set_column_expand_ratio(1, 1);
	tree->set_column_expand_ratio(2, 3);
	tree->set_hide_root(true);
	tree->set_select_mode(Tree::SELECT_ROW);
	tree->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	tree->set_custom_minimum_size(Size2(900, 360) * EDSCALE);
	tree->connect("item_activated", callable_mp(this, &EretheaExportChecksDialog::_item_activated));
	box->add_child(tree);
}

void EretheaExportChecksPlugin::_show_dialog() {
	dialog->popup_centered_clamped(Size2(1000, 520) * EDSCALE);
	dialog->run_checks();
}

void EretheaExportChecksPlugin::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			add_export_plugin(export_plugin);
		} break;

		case NOTIFICATION_EXIT_TREE: {
			remove_export_plugin(export_plugin);
		} break;
	}
}

EretheaExportChecksPlugin::EretheaExportChecksPlugin() {
	export_plugin.instantiate();

	dialog = memnew(EretheaExportChecksDialog);
	EditorNode::get_singleton()->get_gui_base()->add_child(dialog);

	add_tool_menu_item(TTR("Check Project for Export..."), callable_mp(this, &EretheaExportChecksPlugin::_show_dialog));
}
