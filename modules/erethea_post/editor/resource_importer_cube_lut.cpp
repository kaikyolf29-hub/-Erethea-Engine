/**************************************************************************/
/*  resource_importer_cube_lut.cpp                                        */
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

#include "resource_importer_cube_lut.h"

#include "../cube_lut.h"

#include "core/io/file_access.h"
#include "editor/import/resource_importer_layered_texture.h"
#include "editor/import/resource_importer_texture.h"
#include "scene/resources/compressed_texture.h"

String ResourceImporterCubeLUT::get_importer_name() const {
	return "cube_lut";
}

String ResourceImporterCubeLUT::get_visible_name() const {
	return "Color Correction LUT";
}

void ResourceImporterCubeLUT::get_recognized_extensions(List<String> *p_extensions) const {
	p_extensions->push_back("cube");
}

String ResourceImporterCubeLUT::get_save_extension() const {
	return "ctex3d";
}

String ResourceImporterCubeLUT::get_resource_type() const {
	return "CompressedTexture3D";
}

int ResourceImporterCubeLUT::get_preset_count() const {
	return 0;
}

String ResourceImporterCubeLUT::get_preset_name(int p_idx) const {
	return String();
}

void ResourceImporterCubeLUT::get_import_options(const String &p_path, List<ImportOption> *r_options, int p_preset) const {
}

bool ResourceImporterCubeLUT::get_option_visibility(const String &p_path, const String &p_option, const HashMap<StringName, Variant> &p_options) const {
	return true;
}

Error ResourceImporterCubeLUT::import(ResourceUID::ID p_source_id, const String &p_source_file, const String &p_save_path, const HashMap<StringName, Variant> &p_options, List<String> *r_platform_variants, List<String> *r_gen_files, Variant *r_metadata) {
	Error err = OK;
	const String text = FileAccess::get_file_as_string(p_source_file, &err);
	ERR_FAIL_COND_V_MSG(err != OK, err, vformat("Can't read the color lookup table \"%s\".", p_source_file));

	CubeLUT lut;
	String parse_error;
	err = lut.parse(text, &parse_error);
	ERR_FAIL_COND_V_MSG(err != OK, err, vformat("Can't import the color lookup table \"%s\". %s", p_source_file, parse_error));

	bool clipped = false;
	const Vector<Ref<Image>> slices = lut.make_texture_slices(&clipped);
	if (clipped) {
		WARN_PRINT(vformat("The color lookup table \"%s\" has outputs outside the 0-1 range. They were clipped, since Environment color correction works on 0-1 colors.", p_source_file));
	}

	// Same layout as a 3D texture saved by ResourceImporterLayeredTexture,
	// with lossless compression and no mipmaps, as color correction needs.
	Ref<FileAccess> f = FileAccess::open(p_save_path + ".ctex3d", FileAccess::WRITE, &err);
	ERR_FAIL_COND_V_MSG(f.is_null(), err, vformat("Can't write \"%s.ctex3d\".", p_save_path));

	f->store_8('G');
	f->store_8('S');
	f->store_8('T');
	f->store_8('L');

	f->store_32(CompressedTexture3D::FORMAT_VERSION);
	f->store_32(slices.size()); // Depth.
	f->store_32(ResourceImporterLayeredTexture::MODE_3D);
	f->store_32(0);

	f->store_32(0);
	f->store_32(0); // Mipmaps.
	f->store_32(0);
	f->store_32(0);

	for (const Ref<Image> &slice : slices) {
		ResourceImporterTexture::save_to_ctex_format(f, slice, ResourceImporterTexture::COMPRESS_LOSSLESS, Image::USED_CHANNELS_RGB, Image::COMPRESS_S3TC, Image::COMPRESS_PROFILE_AUTOMATIC, 1.0, Image::BasisUniversalPackerParams(), Image::BPTC_DETECT);
	}

	return OK;
}
