/**************************************************************************/
/*  test_erethea_streaming.h                                              */
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

#ifdef TOOLS_ENABLED

#include "../erethea_texture_streaming.h"

#include "editor/import/resource_importer_streamed_texture.h"
#include "editor/import/resource_importer_texture.h"
#include "tests/test_macros.h"

namespace TestEretheaStreaming {

// A texture that Godot detected as used in 3D: VRAM compressed, with mipmaps.
static Ref<ConfigFile> _detected_3d_import() {
	Ref<ConfigFile> import;
	import.instantiate();
	import->set_value("remap", "importer", "texture");
	import->set_value("remap", "type", "CompressedTexture2D");
	import->set_value("remap", "uid", "uid://b2wqk5n7xn0yv");
	import->set_value("remap", "path", "res://.godot/imported/brick.png-0123.s3tc.ctex");
	import->set_value("deps", "source_file", "res://brick.png");
	import->set_value("deps", "dest_files", PackedStringArray({ "res://.godot/imported/brick.png-0123.s3tc.ctex" }));
	import->set_value("params", "compress/mode", ResourceImporterTexture::COMPRESS_VRAM_COMPRESSED);
	import->set_value("params", "compress/high_quality", false);
	import->set_value("params", "compress/lossy_quality", 0.7);
	import->set_value("params", "compress/normal_map", 0);
	import->set_value("params", "mipmaps/generate", true);
	import->set_value("params", "mipmaps/limit", -1);
	import->set_value("params", "roughness/mode", 0);
	import->set_value("params", "process/fix_alpha_border", true);
	import->set_value("params", "process/size_limit", 0);
	import->set_value("params", "detect_3d/compress_to", 0);
	return import;
}

static List<ResourceImporter::ImportOption> _texture_options(const String &p_path = "res://brick.png") {
	Ref<ResourceImporterTexture> importer;
	importer.instantiate();
	List<ResourceImporter::ImportOption> options;
	importer->get_import_options(p_path, &options);
	return options;
}

TEST_CASE("[Modules][EretheaStreaming] Textures detected as used in 3D can stream") {
	CHECK(EretheaTextureStreaming::get_conversion_blocker(_detected_3d_import(), _texture_options()).is_empty());
}

TEST_CASE("[Modules][EretheaStreaming] Textures that aren't VRAM compressed with mipmaps keep their importer") {
	Ref<ConfigFile> lossless = _detected_3d_import();
	lossless->set_value("params", "compress/mode", ResourceImporterTexture::COMPRESS_LOSSLESS);
	CHECK_FALSE(EretheaTextureStreaming::get_conversion_blocker(lossless, _texture_options()).is_empty());

	Ref<ConfigFile> basis = _detected_3d_import();
	basis->set_value("params", "compress/mode", ResourceImporterTexture::COMPRESS_BASIS_UNIVERSAL);
	CHECK_FALSE(EretheaTextureStreaming::get_conversion_blocker(basis, _texture_options()).is_empty());

	Ref<ConfigFile> no_mipmaps = _detected_3d_import();
	no_mipmaps->set_value("params", "mipmaps/generate", false);
	CHECK_FALSE(EretheaTextureStreaming::get_conversion_blocker(no_mipmaps, _texture_options()).is_empty());
}

TEST_CASE("[Modules][EretheaStreaming] Options the streamed importer lacks must be at their default") {
	const char *changed_options[] = {
		"process/normal_map_invert_y",
		"process/premult_alpha",
		"mipmaps/preserve_alpha_test_coverage",
	};
	for (const char *option : changed_options) {
		Ref<ConfigFile> import = _detected_3d_import();
		import->set_value("params", option, true);
		const String blocker = EretheaTextureStreaming::get_conversion_blocker(import, _texture_options());
		CHECK_MESSAGE(blocker.contains(option), option);
	}

	Ref<ConfigFile> size_limit = _detected_3d_import();
	size_limit->set_value("params", "process/size_limit", 1024);
	CHECK(EretheaTextureStreaming::get_conversion_blocker(size_limit, _texture_options()).contains("process/size_limit"));

	Ref<ConfigFile> channel_remap = _detected_3d_import();
	channel_remap->set_value("params", "process/channel_remap/red", ResourceImporterTexture::REMAP_INV_R);
	CHECK(EretheaTextureStreaming::get_conversion_blocker(channel_remap, _texture_options()).contains("process/channel_remap/red"));
}

TEST_CASE("[Modules][EretheaStreaming] A default value saved with another number type still counts as the default") {
	Ref<ConfigFile> svg = _detected_3d_import();
	svg->set_value("params", "svg/scale", 1); // An int, where the option's default is 1.0.
	CHECK(EretheaTextureStreaming::get_conversion_blocker(svg, _texture_options("res://brick.svg")).is_empty());

	svg->set_value("params", "svg/scale", 2.0);
	CHECK(EretheaTextureStreaming::get_conversion_blocker(svg, _texture_options("res://brick.svg")).contains("svg/scale"));
}

TEST_CASE("[Modules][EretheaStreaming] Fixing transparent borders is only checked when it's on") {
	Ref<ConfigFile> import = _detected_3d_import();
	CHECK(EretheaTextureStreaming::fixes_alpha_border(import));
	import->set_value("params", "process/fix_alpha_border", false);
	CHECK_FALSE(EretheaTextureStreaming::fixes_alpha_border(import));
}

TEST_CASE("[Modules][EretheaStreaming] The streamed import keeps the UID and the shared options") {
	Ref<ConfigFile> import = _detected_3d_import();
	import->set_value("params", "compress/high_quality", true);
	import->set_value("params", "compress/normal_map", 1);
	import->set_value("params", "roughness/mode", 4);
	import->set_value("params", "roughness/src_normal", "res://brick_normal.png");
	Dictionary generator_parameters;
	generator_parameters["md5"] = "0123456789abcdef";
	import->set_value("remap", "generator_parameters", generator_parameters);

	Ref<ResourceImporterStreamedTexture> streamed_importer;
	streamed_importer.instantiate();
	List<ResourceImporter::ImportOption> streamed_options;
	streamed_importer->get_import_options("res://brick.png", &streamed_options);

	Dictionary defaults;
	defaults["streaming/max_lod_override"] = 2;
	const Ref<ConfigFile> streamed = EretheaTextureStreaming::make_streamed_import(import, streamed_options, defaults);
	REQUIRE(streamed.is_valid());

	CHECK(String(streamed->get_value("remap", "importer")) == "streamed_texture_2d");
	CHECK(String(streamed->get_value("remap", "type")) == "StreamedTexture2D");
	CHECK(String(streamed->get_value("remap", "uid")) == "uid://b2wqk5n7xn0yv");
	CHECK(Dictionary(streamed->get_value("remap", "generator_parameters")) == generator_parameters);
	CHECK_FALSE(streamed->has_section_key("remap", "path"));
	CHECK(String(streamed->get_value("deps", "source_file")) == "res://brick.png");
	CHECK_FALSE(streamed->has_section_key("deps", "dest_files"));

	CHECK(bool(streamed->get_value("params", "compress/high_quality")));
	CHECK(int(streamed->get_value("params", "compress/normal_map")) == 1);
	CHECK(int(streamed->get_value("params", "roughness/mode")) == 4);
	CHECK(String(streamed->get_value("params", "roughness/src_normal")) == "res://brick_normal.png");
	CHECK(int(streamed->get_value("params", "streaming/max_lod_override")) == 2);
	CHECK(int(streamed->get_value("params", "streaming/min_lod_override")) == 0);
	// Only the streamed importer's options.
	CHECK_FALSE(streamed->has_section_key("params", "compress/mode"));
	CHECK_FALSE(streamed->has_section_key("params", "mipmaps/generate"));
	CHECK(streamed->get_section_keys("params").size() == streamed_options.size());
}

} // namespace TestEretheaStreaming

#endif // TOOLS_ENABLED
