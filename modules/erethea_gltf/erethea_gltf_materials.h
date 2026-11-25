/**************************************************************************/
/*  erethea_gltf_materials.h                                              */
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

#include "modules/gltf/extensions/gltf_document_extension.h"

class BaseMaterial3D;

// Erethea: Reads the glTF material extensions that Godot's importer skips
// (clearcoat, sheen, anisotropy, index of refraction and specular) into the
// closest StandardMaterial3D properties. Blender's exporter writes them from
// the Principled BSDF: the Fornalha uses a coat for the cornea of the eyes and
// anisotropy and sheen for hair. Off unless
// rendering/erethea/gltf/import_material_extensions is enabled.
class EretheaGLTFMaterials : public GLTFDocumentExtension {
	GDCLASS(EretheaGLTFMaterials, GLTFDocumentExtension);

protected:
	static void _bind_methods() {}

public:
	static constexpr const char *SETTING_ENABLED = "rendering/erethea/gltf/import_material_extensions";

	static void register_project_settings();
	static bool is_enabled();

	// glTF's perceptual clearcoat roughness to Godot's, which goes from a GGX
	// alpha of 0.001 to 0.1 and so can't be rougher than about 0.32 in glTF.
	// Never below 0.05.
	static float clearcoat_roughness_from_gltf(float p_roughness);
	// The specular of a dielectric: Godot's specular gives a reflectance at
	// normal incidence of 0.16 * specular^2, glTF's ((ior - 1) / (ior + 1))^2
	// times the specular factor and color.
	static float specular_from_gltf(float p_ior, float p_specular_factor, const Color &p_specular_color);
	// Sets the properties of p_material from the material's "extensions".
	// Returns what couldn't be carried over, such as textures Godot has no slot for.
	static Vector<String> apply_extensions(const Ref<BaseMaterial3D> &p_material, const Dictionary &p_extensions, const Vector<Ref<Texture2D>> &p_textures);

	Error import_preflight(Ref<GLTFState> p_state, const Vector<String> &p_extensions) override;
	Vector<String> get_supported_extensions() override;
	Error import_post_parse(Ref<GLTFState> p_state) override;
};
