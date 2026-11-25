/**************************************************************************/
/*  erethea_gltf_materials.cpp                                            */
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

#include "erethea_gltf_materials.h"

#include "core/config/project_settings.h"
#include "scene/resources/image_texture.h"
#include "scene/resources/material.h"

#include "modules/gltf/gltf_state.h"

namespace {

const char *CLEARCOAT = "KHR_materials_clearcoat";
const char *SHEEN = "KHR_materials_sheen";
const char *ANISOTROPY = "KHR_materials_anisotropy";
const char *IOR = "KHR_materials_ior";
const char *SPECULAR = "KHR_materials_specular";

float read_float(const Dictionary &p_dict, const String &p_key, float p_default) {
	return p_dict.has(p_key) ? float(p_dict[p_key]) : p_default;
}

Color read_color(const Dictionary &p_dict, const String &p_key, const Color &p_default) {
	if (!p_dict.has(p_key)) {
		return p_default;
	}
	const Array color = p_dict[p_key];
	if (color.size() < 3) {
		return p_default;
	}
	return Color(color[0], color[1], color[2]);
}

int read_texture_index(const Dictionary &p_dict, const String &p_key) {
	if (!p_dict.has(p_key)) {
		return -1;
	}
	const Dictionary texture_info = p_dict[p_key];
	return texture_info.has("index") ? int(texture_info["index"]) : -1;
}

} // namespace

void EretheaGLTFMaterials::register_project_settings() {
	GLOBAL_DEF(SETTING_ENABLED, false);
}

bool EretheaGLTFMaterials::is_enabled() {
	return GLOBAL_GET(SETTING_ENABLED);
}

float EretheaGLTFMaterials::clearcoat_roughness_from_gltf(float p_roughness) {
	const float alpha = p_roughness * p_roughness;
	// Below about 0.03, the peak of the coat's highlight overflows in half
	// precision (16-bit floats top out at 65504) and the highlight can get lost.
	// 0.05 looks as sharp.
	return CLAMP((alpha - 0.001f) / 0.099f, 0.05f, 1.0f);
}

float EretheaGLTFMaterials::specular_from_gltf(float p_ior, float p_specular_factor, const Color &p_specular_color) {
	if (p_ior < 1.0f) {
		// Not a valid index of refraction: glTF's default.
		p_ior = 1.5f;
	}
	const float reflectance = Math::pow((p_ior - 1.0f) / (p_ior + 1.0f), 2.0f);
	// Godot's specular has no color: the average of the three channels.
	const float colored = (MIN(reflectance * p_specular_color.r, 1.0f) + MIN(reflectance * p_specular_color.g, 1.0f) + MIN(reflectance * p_specular_color.b, 1.0f)) / 3.0f;
	return CLAMP(Math::sqrt(colored * p_specular_factor / 0.16f), 0.0f, 1.0f);
}

Vector<String> EretheaGLTFMaterials::apply_extensions(const Ref<BaseMaterial3D> &p_material, const Dictionary &p_extensions, const Vector<Ref<Texture2D>> &p_textures) {
	Vector<String> skipped;
	ERR_FAIL_COND_V(p_material.is_null(), skipped);
	const auto texture = [&p_textures](int p_index) -> Ref<Texture2D> {
		return p_index >= 0 && p_index < p_textures.size() ? p_textures[p_index] : Ref<Texture2D>();
	};

	if (p_extensions.has(CLEARCOAT)) {
		const Dictionary clearcoat = p_extensions[CLEARCOAT];
		const float factor = read_float(clearcoat, "clearcoatFactor", 0.0f);
		if (factor > 0.0f) {
			p_material->set_feature(BaseMaterial3D::FEATURE_CLEARCOAT, true);
			p_material->set_clearcoat(MIN(factor, 1.0f));
			p_material->set_clearcoat_roughness(clearcoat_roughness_from_gltf(read_float(clearcoat, "clearcoatRoughnessFactor", 0.0f)));
			// Godot reads the factor from the red channel and the roughness
			// from the green one of a single texture, like glTF does from its two.
			const int factor_texture = read_texture_index(clearcoat, "clearcoatTexture");
			const int roughness_texture = read_texture_index(clearcoat, "clearcoatRoughnessTexture");
			if (factor_texture >= 0 && factor_texture == roughness_texture) {
				p_material->set_texture(BaseMaterial3D::TEXTURE_CLEARCOAT, texture(factor_texture));
			} else if (factor_texture >= 0 || roughness_texture >= 0) {
				skipped.push_back("clearcoat textures that aren't the same texture");
			}
			if (clearcoat.has("clearcoatNormalTexture")) {
				skipped.push_back("the clearcoat normal map");
			}
		}
	}

	if (p_extensions.has(SHEEN)) {
		const Dictionary sheen = p_extensions[SHEEN];
		const Color color = read_color(sheen, "sheenColorFactor", Color(0, 0, 0));
		const float strength = MAX(color.r, MAX(color.g, color.b));
		if (strength > 0.0f) {
			// Godot's rim is its sheen: light at grazing angles, as on velvet.
			// Its width comes from the material's roughness, not the sheen's.
			p_material->set_feature(BaseMaterial3D::FEATURE_RIM, true);
			p_material->set_rim(MIN(strength, 1.0f));
			// A white sheen keeps the light's color, a colored one takes the albedo's.
			p_material->set_rim_tint(color.get_s());
		}
		if (read_texture_index(sheen, "sheenColorTexture") >= 0 || read_texture_index(sheen, "sheenRoughnessTexture") >= 0) {
			skipped.push_back("the sheen textures");
		}
	}

	if (p_extensions.has(ANISOTROPY)) {
		const Dictionary anisotropy = p_extensions[ANISOTROPY];
		const float strength = read_float(anisotropy, "anisotropyStrength", 0.0f);
		if (strength > 0.0f) {
			p_material->set_feature(BaseMaterial3D::FEATURE_ANISOTROPY, true);
			p_material->set_anisotropy(MIN(strength, 1.0f));
			const float rotation = read_float(anisotropy, "anisotropyRotation", 0.0f);
			if (!Math::is_zero_approx(rotation)) {
				// Godot has no rotation, but a flow map with the direction in
				// tangent space: one pixel of it.
				Ref<Image> flow = Image::create_empty(1, 1, false, Image::FORMAT_RGBA8);
				flow->set_pixel(0, 0, Color(Math::cos(rotation) * 0.5f + 0.5f, Math::sin(rotation) * 0.5f + 0.5f, 0.0f, 1.0f));
				p_material->set_texture(BaseMaterial3D::TEXTURE_FLOWMAP, ImageTexture::create_from_image(flow));
			}
		}
		if (read_texture_index(anisotropy, "anisotropyTexture") >= 0) {
			skipped.push_back("the anisotropy texture");
		}
	}

	if (p_extensions.has(IOR) || p_extensions.has(SPECULAR)) {
		const Dictionary ior = p_extensions.has(IOR) ? Dictionary(p_extensions[IOR]) : Dictionary();
		const Dictionary specular = p_extensions.has(SPECULAR) ? Dictionary(p_extensions[SPECULAR]) : Dictionary();
		p_material->set_specular(specular_from_gltf(read_float(ior, "ior", 1.5f), read_float(specular, "specularFactor", 1.0f), read_color(specular, "specularColorFactor", Color(1, 1, 1))));
		if (read_texture_index(specular, "specularTexture") >= 0 || read_texture_index(specular, "specularColorTexture") >= 0) {
			skipped.push_back("the specular textures");
		}
	}
	return skipped;
}

Error EretheaGLTFMaterials::import_preflight(Ref<GLTFState> p_state, const Vector<String> &p_extensions) {
	for (const String &extension : get_supported_extensions()) {
		if (p_extensions.has(extension)) {
			return OK;
		}
	}
	return ERR_SKIP;
}

Vector<String> EretheaGLTFMaterials::get_supported_extensions() {
	// Off, files that require them fail to import as in Godot.
	if (!is_enabled()) {
		return Vector<String>();
	}
	return { CLEARCOAT, SHEEN, ANISOTROPY, IOR, SPECULAR };
}

Error EretheaGLTFMaterials::import_post_parse(Ref<GLTFState> p_state) {
	const Dictionary json = p_state->get_json();
	if (!json.has("materials")) {
		return OK;
	}
	// The texture of each glTF texture index.
	Vector<Ref<Texture2D>> textures;
	const Vector<Ref<Texture2D>> &images = p_state->get_images();
	for (const Ref<GLTFTexture> &gltf_texture : p_state->get_textures()) {
		const int image = gltf_texture.is_valid() ? gltf_texture->get_src_image() : -1;
		textures.push_back(image >= 0 && image < images.size() ? images[image] : Ref<Texture2D>());
	}

	const Array materials = json["materials"];
	const Vector<Ref<Material>> &parsed = p_state->get_materials();
	for (int i = 0; i < materials.size() && i < parsed.size(); i++) {
		const Dictionary material = materials[i];
		const Ref<BaseMaterial3D> base_material = parsed[i];
		if (!material.has("extensions") || base_material.is_null()) {
			continue;
		}
		for (const String &what : apply_extensions(base_material, material["extensions"], textures)) {
			WARN_PRINT(vformat("glTF: Erethea can't import %s of the material \"%s\" in \"%s\" yet.", what, base_material->get_name(), p_state->get_filename()));
		}
	}
	return OK;
}
