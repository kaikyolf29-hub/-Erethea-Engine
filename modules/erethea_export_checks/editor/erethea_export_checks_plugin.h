/**************************************************************************/
/*  erethea_export_checks_plugin.h                                        */
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

#include "../erethea_export_checks.h"

#include "editor/export/editor_export_plugin.h"
#include "editor/plugins/editor_plugin.h"
#include "scene/gui/dialogs.h"

class Label;
class Tree;

// Reports the checks in the export log.
class EretheaExportChecksExportPlugin : public EditorExportPlugin {
	GDCLASS(EretheaExportChecksExportPlugin, EditorExportPlugin);

protected:
	virtual void _export_begin(const HashSet<String> &p_features, bool p_debug, const String &p_path, int p_flags) override;

public:
	virtual String get_name() const override { return "EretheaExportChecks"; }
};

// Project > Tools > Check Project for Export...
class EretheaExportChecksDialog : public AcceptDialog {
	GDCLASS(EretheaExportChecksDialog, AcceptDialog);

	Label *summary = nullptr;
	Tree *tree = nullptr;

	void _item_activated();

protected:
	virtual void custom_action(const String &p_action) override;

public:
	void run_checks();

	EretheaExportChecksDialog();
};

class EretheaExportChecksPlugin : public EditorPlugin {
	GDCLASS(EretheaExportChecksPlugin, EditorPlugin);

	Ref<EretheaExportChecksExportPlugin> export_plugin;
	EretheaExportChecksDialog *dialog = nullptr;

	void _show_dialog();

protected:
	void _notification(int p_what);

public:
	virtual String get_plugin_name() const override { return "EretheaExportChecks"; }

	EretheaExportChecksPlugin();
};
