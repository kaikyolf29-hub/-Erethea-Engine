/**************************************************************************/
/*  erethea_dense_geometry.h                                              */
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

#include "core/templates/vector.h"
#include "scene/resources/3d/importer_mesh.h"

class Node;

// Dense geometry, for models of millions of triangles that have no LODs made
// by hand. When the editor imports a scene, a mesh with many triangles is
// split into clusters of nearby triangles, each its own mesh instance. Godot
// then culls each cluster on its own (frustum and occlusion) and generates its
// LODs and picks one per cluster by its own distance, so the parts near the
// camera keep their detail while the far ones are simplified. Godot locks the
// open borders when it simplifies, so the clusters don't crack apart.
class EretheaDenseGeometry {
public:
	// The project setting that gives the default of the import option below:
	// the most triangles in a cluster, or 0 to keep the meshes whole.
	static constexpr const char *PROJECT_SETTING = "rendering/erethea/dense_geometry/cluster_triangles";
	// The scene import option.
	static constexpr const char *IMPORT_OPTION = "erethea/dense_geometry/cluster_triangles";
	static constexpr int MIN_CLUSTER_TRIANGLES = 1024;

	static void register_project_settings();

	static int64_t count_triangles(const Ref<ImporterMesh> &p_mesh);
	// Meshes with blend shapes, or that aren't made only of triangles, are
	// kept whole, as are skinned ones (see split_scene()).
	static bool can_split(const Ref<ImporterMesh> &p_mesh);

	// Splits the mesh into clusters of at most p_cluster_triangles triangles,
	// by the median of their centers along the longest axis, again and
	// again. Each cluster keeps the surfaces (and their materials and vertex
	// attributes) that it has triangles of. Returns no clusters when the mesh
	// can't be split, or has fewer than twice that many triangles.
	static Vector<Ref<ImporterMesh>> split_mesh(const Ref<ImporterMesh> &p_mesh, int p_cluster_triangles);

	// Splits the meshes of the scene's ImporterMeshInstance3D nodes (not the
	// skinned ones): the node keeps the first cluster, and the others become
	// its children. Returns how many meshes were split.
	static int split_scene(Node *p_scene, int p_cluster_triangles);
};
