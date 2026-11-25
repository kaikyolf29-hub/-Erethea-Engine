/**************************************************************************/
/*  erethea_dense_geometry_import_plugin.cpp                              */
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

#include "erethea_dense_geometry_import_plugin.h"

#include "../erethea_dense_geometry.h"

#include "core/config/project_settings.h"

void EretheaDenseGeometryImportPlugin::get_import_options(const String &p_path, List<ResourceImporter::ImportOption> *r_options) {
	r_options->push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::INT, EretheaDenseGeometry::IMPORT_OPTION, PROPERTY_HINT_RANGE, "0,1048576,1,or_greater"), GLOBAL_GET(EretheaDenseGeometry::PROJECT_SETTING)));
}

void EretheaDenseGeometryImportPlugin::pre_process(Node *p_scene, const HashMap<StringName, Variant> &p_options) {
	const Variant *cluster_triangles = p_options.getptr(EretheaDenseGeometry::IMPORT_OPTION);
	if (cluster_triangles == nullptr || int(*cluster_triangles) <= 0) {
		return;
	}
	const int split = EretheaDenseGeometry::split_scene(p_scene, *cluster_triangles);
	if (split > 0) {
		print_verbose(vformat("Erethea dense geometry: split %d mesh(es) into clusters of at most %d triangles.", split, MAX(int(*cluster_triangles), EretheaDenseGeometry::MIN_CLUSTER_TRIANGLES)));
	}
}

void EretheaDenseGeometryEditorPlugin::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			add_scene_post_import_plugin(import_plugin);
		} break;

		case NOTIFICATION_EXIT_TREE: {
			remove_scene_post_import_plugin(import_plugin);
		} break;
	}
}

EretheaDenseGeometryEditorPlugin::EretheaDenseGeometryEditorPlugin() {
	import_plugin.instantiate();
}
