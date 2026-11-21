/**************************************************************************/
/*  erethea_texture_streaming.cpp                                         */
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

#include "erethea_texture_streaming.h"

#include "core/config/project_settings.h"

#ifdef TOOLS_ENABLED
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/image.h"
#include "core/io/image_loader.h"
#include "core/string/print_string.h"
#include "editor/file_system/editor_paths.h"
#include "editor/import/resource_importer_texture.h"
#endif

void EretheaTextureStreaming::register_project_settings() {
	GLOBAL_DEF_BASIC(PropertyInfo(Variant::BOOL, PROJECT_SETTING), false);
}

bool EretheaTextureStreaming::is_enabled() {
	return GLOBAL_GET(PROJECT_SETTING) && GLOBAL_GET(GODOT_STREAMING_SETTING);
}

#ifdef TOOLS_ENABLED

static constexpr const char *TEXTURE_IMPORTER = "texture";
static constexpr const char *STREAMED_IMPORTER = "streamed_texture_2d";

String EretheaTextureStreaming::get_queue_path() {
	return EditorPaths::get_singleton()->get_project_settings_dir().path_join("erethea_texture_streaming.cfg");
}

PackedStringArray EretheaTextureStreaming::get_queued_textures() {
	Ref<ConfigFile> queue;
	queue.instantiate();
	if (queue->load(get_queue_path()) != OK) {
		return PackedStringArray();
	}
	return queue->get_value("queue", "textures", PackedStringArray());
}

void EretheaTextureStreaming::queue_texture(const String &p_path) {
	if (!is_enabled()) {
		return;
	}

	PackedStringArray textures = get_queued_textures();
	if (textures.has(p_path)) {
		return;
	}
	textures.push_back(p_path);

	Ref<ConfigFile> queue;
	queue.instantiate();
	queue->set_value("queue", "textures", textures);
	ERR_FAIL_COND_MSG(queue->save(get_queue_path()) != OK, vformat("Can't save the texture streaming queue to \"%s\".", get_queue_path()));

	print_line(vformat(TTR("%s: Texture used in 3D. Its mipmaps will stream the next time the editor starts."), p_path));
}

String EretheaTextureStreaming::get_conversion_blocker(const Ref<ConfigFile> &p_import, const List<ResourceImporter::ImportOption> &p_texture_options) {
	ERR_FAIL_COND_V(p_import.is_null(), "no import settings");

	if (int(p_import->get_value("params", "compress/mode", ResourceImporterTexture::COMPRESS_LOSSLESS)) != ResourceImporterTexture::COMPRESS_VRAM_COMPRESSED) {
		return TTR("it isn't VRAM compressed");
	}
	if (!bool(p_import->get_value("params", "mipmaps/generate", false))) {
		return TTR("it doesn't generate mipmaps");
	}

	// Options that the streamed importer has too, that it handles in its own
	// way, or that don't matter for a VRAM compressed texture with mipmaps.
	// Every other option must be at its default value.
	static const char *handled_options[] = {
		"compress/mode",
		"compress/high_quality",
		"compress/lossy_quality",
		"compress/uastc_level",
		"compress/rdo_quality_loss",
		"compress/hdr_compression",
		"compress/normal_map",
		"compress/channel_pack",
		"mipmaps/generate",
		"mipmaps/alpha_test_threshold",
		"roughness/mode",
		"roughness/src_normal",
		"process/fix_alpha_border",
		"detect_3d/compress_to",
	};

	for (const ResourceImporter::ImportOption &option : p_texture_options) {
		const String &name = option.option.name;

		bool handled = false;
		for (const char *handled_option : handled_options) {
			if (name == handled_option) {
				handled = true;
				break;
			}
		}
		if (handled || !p_import->has_section_key("params", name)) {
			continue;
		}

		const Variant is_default = Variant::evaluate(Variant::OP_EQUAL, p_import->get_value("params", name), option.default_value);
		if (!is_default.booleanize()) {
			return vformat(TTR("its import option \"%s\" isn't the default, and the streamed importer doesn't have it"), name);
		}
	}

	return String();
}

bool EretheaTextureStreaming::fixes_alpha_border(const Ref<ConfigFile> &p_import) {
	ERR_FAIL_COND_V(p_import.is_null(), true);
	return p_import->get_value("params", "process/fix_alpha_border", true);
}

Ref<ConfigFile> EretheaTextureStreaming::make_streamed_import(const Ref<ConfigFile> &p_import, const List<ResourceImporter::ImportOption> &p_streamed_options, const Dictionary &p_streamed_defaults) {
	ERR_FAIL_COND_V(p_import.is_null(), Ref<ConfigFile>());

	Ref<ConfigFile> streamed;
	streamed.instantiate();
	streamed->set_value("remap", "importer", STREAMED_IMPORTER);
	streamed->set_value("remap", "type", "StreamedTexture2D");
	// The UID keeps scenes and materials pointing at the texture. Scenes save
	// imported textures as Texture2D, so they load either class.
	for (const char *key : { "uid", "generator_parameters" }) {
		if (p_import->has_section_key("remap", key)) {
			streamed->set_value("remap", key, p_import->get_value("remap", key));
		}
	}
	if (p_import->has_section_key("deps", "source_file")) {
		streamed->set_value("deps", "source_file", p_import->get_value("deps", "source_file"));
	}

	for (const ResourceImporter::ImportOption &option : p_streamed_options) {
		const String &name = option.option.name;
		Variant value = p_streamed_defaults.get(name, option.default_value);
		if (p_import->has_section_key("params", name)) {
			value = p_import->get_value("params", name);
		}
		streamed->set_value("params", name, value);
	}

	return streamed;
}

static bool _source_has_alpha(const String &p_path) {
	Ref<Image> image;
	image.instantiate();
	if (ImageLoader::load_image(p_path, image) != OK) {
		return true; // Can't tell, so leave the texture as it is.
	}
	return image->detect_alpha() != Image::ALPHA_NONE;
}

static void _remove_imported_files(const Ref<ConfigFile> &p_import) {
	const PackedStringArray dest_files = p_import->get_value("deps", "dest_files", PackedStringArray());
	for (const String &file : dest_files) {
		// Only the files the import made, which the streamed import replaces.
		if (file.begins_with("res://.godot/imported/") && FileAccess::exists(file)) {
			DirAccess::remove_absolute(ProjectSettings::get_singleton()->globalize_path(file));
		}
	}
}

void EretheaTextureStreaming::convert_queued_textures() {
	const String queue_path = get_queue_path();
	if (!FileAccess::exists(queue_path) || !is_enabled()) {
		return; // When streaming is off, the queue waits until it's on again.
	}

	Ref<ResourceImporter> texture_importer = ResourceFormatImporter::get_singleton()->get_importer_by_name(TEXTURE_IMPORTER);
	Ref<ResourceImporter> streamed_importer = ResourceFormatImporter::get_singleton()->get_importer_by_name(STREAMED_IMPORTER);
	ERR_FAIL_COND(texture_importer.is_null() || streamed_importer.is_null());

	const String defaults_setting = String("importer_defaults/") + STREAMED_IMPORTER;
	const Dictionary streamed_defaults = ProjectSettings::get_singleton()->has_setting(defaults_setting) ? Dictionary(GLOBAL_GET(defaults_setting)) : Dictionary();

	for (const String &path : get_queued_textures()) {
		Ref<ConfigFile> import;
		import.instantiate();
		if (import->load(path + ".import") != OK || String(import->get_value("remap", "importer", "")) != TEXTURE_IMPORTER) {
			continue; // Deleted, or already imported another way.
		}

		List<ResourceImporter::ImportOption> texture_options;
		texture_importer->get_import_options(path, &texture_options);
		String blocker = get_conversion_blocker(import, texture_options);
		if (blocker.is_empty() && fixes_alpha_border(import) && _source_has_alpha(path)) {
			blocker = TTR("it has transparency, and the streamed importer doesn't fix transparent borders");
		}
		if (!blocker.is_empty()) {
			print_line(vformat(TTR("%s: Its mipmaps won't stream: %s."), path, blocker));
			continue;
		}

		List<ResourceImporter::ImportOption> streamed_options;
		streamed_importer->get_import_options(path, &streamed_options);
		const Ref<ConfigFile> streamed = make_streamed_import(import, streamed_options, streamed_defaults);
		if (streamed->save(path + ".import") != OK) {
			ERR_PRINT(vformat("Can't save \"%s.import\".", path));
			continue;
		}
		// The file system scan sees the changed .import file and imports the
		// texture again, now as a StreamedTexture2D.
		_remove_imported_files(import);
		print_line(vformat(TTR("%s: Its mipmaps now stream (imported as \"Texture2D Streamed\")."), path));
	}

	DirAccess::remove_absolute(ProjectSettings::get_singleton()->globalize_path(queue_path));
}

#endif // TOOLS_ENABLED
