/**************************************************************************/
/*  test_erethea_gltf_materials.h                                         */
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

#include "../erethea_gltf_materials.h"

#include "core/config/project_settings.h"
#include "scene/resources/material.h"
#include "tests/test_macros.h"

#include "modules/gltf/gltf_document.h"
#include "modules/gltf/gltf_state.h"

namespace TestEretheaGLTFMaterials {

// Materials as Blender's exporter writes them from the Principled BSDF: the
// Fornalha's eye (a coat for the cornea), its hair (anisotropy and sheen) and
// its skin (a lower specular), plus a few for the textures and the rotation.
const char *GLTF = R"({
	"asset": { "version": "2.0" },
	"extensionsUsed": [ "KHR_materials_clearcoat", "KHR_materials_sheen", "KHR_materials_anisotropy", "KHR_materials_ior", "KHR_materials_specular" ],
	"images": [ { "uri": "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR4nGP438DwHwAGgAJ/EEwb4QAAAABJRU5ErkJggg==" } ],
	"textures": [ { "source": 0 } ],
	"materials": [
		{ "name": "olho", "pbrMetallicRoughness": { "metallicFactor": 0.0, "roughnessFactor": 0.25 },
			"extensions": { "KHR_materials_clearcoat": { "clearcoatFactor": 1.0, "clearcoatRoughnessFactor": 0.02 }, "KHR_materials_ior": { "ior": 1.376 } } },
		{ "name": "cabelo", "pbrMetallicRoughness": { "metallicFactor": 0.0, "roughnessFactor": 0.32 },
			"extensions": { "KHR_materials_anisotropy": { "anisotropyStrength": 0.75 }, "KHR_materials_sheen": { "sheenColorFactor": [ 0.2, 0.2, 0.2 ], "sheenRoughnessFactor": 0.4 } } },
		{ "name": "pele", "extensions": { "KHR_materials_specular": { "specularFactor": 0.9 } } },
		{ "name": "verniz", "extensions": { "KHR_materials_clearcoat": { "clearcoatFactor": 0.5, "clearcoatRoughnessFactor": 0.3, "clearcoatTexture": { "index": 0 }, "clearcoatRoughnessTexture": { "index": 0 } } } },
		{ "name": "verniz_pela_metade", "extensions": { "KHR_materials_clearcoat": { "clearcoatFactor": 0.5, "clearcoatTexture": { "index": 0 } } } },
		{ "name": "escovado", "extensions": { "KHR_materials_anisotropy": { "anisotropyStrength": 0.5, "anisotropyRotation": 1.5707963 } } },
		{ "name": "liso" }
	],
	"scenes": [ { "nodes": [] } ],
	"scene": 0
})";

Vector<Ref<BaseMaterial3D>> import_materials(bool p_enabled) {
	ProjectSettings *project_settings = ProjectSettings::get_singleton();
	const Variant enabled_before = project_settings->get_setting(EretheaGLTFMaterials::SETTING_ENABLED);
	project_settings->set_setting(EretheaGLTFMaterials::SETTING_ENABLED, p_enabled);
	Ref<GLTFDocument> document;
	document.instantiate();
	Ref<GLTFState> state;
	state.instantiate();
	const CharString text = String(GLTF).utf8();
	PackedByteArray bytes;
	bytes.resize(text.length());
	memcpy(bytes.ptrw(), text.get_data(), text.length());
	ERR_PRINT_OFF;
	const Error err = document->append_from_buffer(bytes, "", state);
	ERR_PRINT_ON;
	project_settings->set_setting(EretheaGLTFMaterials::SETTING_ENABLED, enabled_before);
	CHECK(err == OK);
	Vector<Ref<BaseMaterial3D>> materials;
	for (const Ref<Material> &material : state->get_materials()) {
		materials.push_back(material);
	}
	return materials;
}

TEST_CASE("[Modules][EretheaGLTF] Conversions") {
	// Not so sharp that the highlight overflows in half precision.
	CHECK(EretheaGLTFMaterials::clearcoat_roughness_from_gltf(0.0) == doctest::Approx(0.05));
	// alpha 0.09: (0.09 - 0.001) / 0.099.
	CHECK(EretheaGLTFMaterials::clearcoat_roughness_from_gltf(0.3) == doctest::Approx(0.8990).epsilon(0.001));
	CHECK(EretheaGLTFMaterials::clearcoat_roughness_from_gltf(1.0) == doctest::Approx(1.0));
	// glTF's default dielectric is Godot's default specular.
	CHECK(EretheaGLTFMaterials::specular_from_gltf(1.5, 1.0, Color(1, 1, 1)) == doctest::Approx(0.5));
	CHECK(EretheaGLTFMaterials::specular_from_gltf(1.376, 1.0, Color(1, 1, 1)) == doctest::Approx(0.3956).epsilon(0.001));
	CHECK(EretheaGLTFMaterials::specular_from_gltf(1.5, 0.0, Color(1, 1, 1)) == doctest::Approx(0.0));
	CHECK(EretheaGLTFMaterials::specular_from_gltf(0.0, 1.0, Color(1, 1, 1)) == doctest::Approx(0.5));
	CHECK(EretheaGLTFMaterials::specular_from_gltf(10.0, 100.0, Color(1, 1, 1)) == doctest::Approx(1.0));
}

TEST_CASE("[Modules][EretheaGLTF] Material extensions are skipped as in Godot when disabled") {
	const Vector<Ref<BaseMaterial3D>> materials = import_materials(false);
	REQUIRE(materials.size() == 7);
	for (const Ref<BaseMaterial3D> &material : materials) {
		REQUIRE(material.is_valid());
		CHECK_FALSE(material->get_feature(BaseMaterial3D::FEATURE_CLEARCOAT));
		CHECK_FALSE(material->get_feature(BaseMaterial3D::FEATURE_ANISOTROPY));
		CHECK_FALSE(material->get_feature(BaseMaterial3D::FEATURE_RIM));
		CHECK(material->get_specular() == doctest::Approx(0.5));
	}
}

TEST_CASE("[Modules][EretheaGLTF] Material extensions are imported when enabled") {
	const Vector<Ref<BaseMaterial3D>> materials = import_materials(true);
	REQUIRE(materials.size() == 7);

	const Ref<BaseMaterial3D> eye = materials[0];
	CHECK(eye->get_feature(BaseMaterial3D::FEATURE_CLEARCOAT));
	CHECK(eye->get_clearcoat() == doctest::Approx(1.0));
	// As sharp as Godot's coat goes, for the catchlight.
	CHECK(eye->get_clearcoat_roughness() == doctest::Approx(0.05));
	CHECK(eye->get_specular() == doctest::Approx(0.3956).epsilon(0.001));
	CHECK(eye->get_roughness() == doctest::Approx(0.25));

	const Ref<BaseMaterial3D> hair = materials[1];
	CHECK(hair->get_feature(BaseMaterial3D::FEATURE_ANISOTROPY));
	CHECK(hair->get_anisotropy() == doctest::Approx(0.75));
	CHECK(hair->get_texture(BaseMaterial3D::TEXTURE_FLOWMAP).is_null());
	CHECK(hair->get_feature(BaseMaterial3D::FEATURE_RIM));
	CHECK(hair->get_rim() == doctest::Approx(0.2));
	CHECK(hair->get_rim_tint() == doctest::Approx(0.0));
	CHECK(hair->get_specular() == doctest::Approx(0.5));

	const Ref<BaseMaterial3D> skin = materials[2];
	CHECK(skin->get_specular() == doctest::Approx(0.4743).epsilon(0.001));
	CHECK_FALSE(skin->get_feature(BaseMaterial3D::FEATURE_CLEARCOAT));

	// One texture for the coat and its roughness, as Godot reads them.
	const Ref<BaseMaterial3D> varnish = materials[3];
	CHECK(varnish->get_clearcoat() == doctest::Approx(0.5));
	CHECK(varnish->get_clearcoat_roughness() == doctest::Approx(0.8990).epsilon(0.001));
	CHECK(varnish->get_texture(BaseMaterial3D::TEXTURE_CLEARCOAT).is_valid());
	// Godot would read the roughness from this texture too: left out.
	const Ref<BaseMaterial3D> half_varnish = materials[4];
	CHECK(half_varnish->get_feature(BaseMaterial3D::FEATURE_CLEARCOAT));
	CHECK(half_varnish->get_texture(BaseMaterial3D::TEXTURE_CLEARCOAT).is_null());

	// A quarter turn: the flow points along the bitangent.
	const Ref<BaseMaterial3D> brushed = materials[5];
	CHECK(brushed->get_anisotropy() == doctest::Approx(0.5));
	const Ref<Texture2D> flow = brushed->get_texture(BaseMaterial3D::TEXTURE_FLOWMAP);
	REQUIRE(flow.is_valid());
	const Color direction = flow->get_image()->get_pixel(0, 0);
	CHECK(direction.r == doctest::Approx(0.5).epsilon(0.01));
	CHECK(direction.g == doctest::Approx(1.0).epsilon(0.01));

	const Ref<BaseMaterial3D> plain = materials[6];
	CHECK_FALSE(plain->get_feature(BaseMaterial3D::FEATURE_CLEARCOAT));
	CHECK_FALSE(plain->get_feature(BaseMaterial3D::FEATURE_ANISOTROPY));
	CHECK_FALSE(plain->get_feature(BaseMaterial3D::FEATURE_RIM));
	CHECK(plain->get_specular() == doctest::Approx(0.5));
}

} // namespace TestEretheaGLTFMaterials
