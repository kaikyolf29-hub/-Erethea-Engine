/**************************************************************************/
/*  erethea_dense_geometry_import_plugin.h                                */
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

#include "editor/import/3d/resource_importer_scene.h"
#include "editor/plugins/editor_plugin.h"

// Splits the dense meshes of the scenes that the editor imports, from the
// import option "erethea/dense_geometry/cluster_triangles" (see
// EretheaDenseGeometry).
class EretheaDenseGeometryImportPlugin : public EditorScenePostImportPlugin {
	GDCLASS(EretheaDenseGeometryImportPlugin, EditorScenePostImportPlugin);

public:
	virtual void get_import_options(const String &p_path, List<ResourceImporter::ImportOption> *r_options) override;
	virtual void pre_process(Node *p_scene, const HashMap<StringName, Variant> &p_options) override;
};

class EretheaDenseGeometryEditorPlugin : public EditorPlugin {
	GDCLASS(EretheaDenseGeometryEditorPlugin, EditorPlugin);

	Ref<EretheaDenseGeometryImportPlugin> import_plugin;

protected:
	void _notification(int p_what);

public:
	virtual String get_plugin_name() const override { return "EretheaDenseGeometry"; }

	EretheaDenseGeometryEditorPlugin();
};
