/**************************************************************************/
/*  erethea_texture_streaming.h                                           */
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

#include "core/string/ustring.h"

#ifdef TOOLS_ENABLED
#include "core/io/config_file.h"
#include "core/io/resource_importer.h"
#include "core/templates/list.h"
#endif

// Streams the mipmaps of the textures that the editor sees used in 3D.
//
// Godot's StreamedTexture2D only learns which mipmaps it needs from the 3D
// renderer, so a texture shown in 2D or in the UI would stay at a low
// resolution. Textures are imported with Godot's "texture" importer as usual.
// When the editor detects one used in a 3D material, it is queued, and the next
// time the editor starts, before anything is loaded, its import switches to
// Godot's "streamed_texture_2d" importer. Switching while the editor runs would
// replace a loaded resource with one of another class, which Godot only does by
// clearing the undo history.
class EretheaTextureStreaming {
public:
	static constexpr const char *PROJECT_SETTING = "rendering/erethea/stream_3d_textures";
	static constexpr const char *GODOT_STREAMING_SETTING = "rendering/textures/streaming/enabled";

	static void register_project_settings();
	// This module's setting and Godot's texture streaming are both on.
	static bool is_enabled();

#ifdef TOOLS_ENABLED
	static String get_queue_path();
	static PackedStringArray get_queued_textures();

	// ResourceImporterTexture::texture_3d_detected_callback.
	static void queue_texture(const String &p_path);
	// Runs when the editor starts, before the first file system scan.
	static void convert_queued_textures();

	// Why a texture imported with these "texture" importer settings can't
	// stream its mipmaps without looking different, or an empty string if it
	// can. The streamed importer has fewer options, so only textures that use
	// none of the missing ones are switched.
	static String get_conversion_blocker(const Ref<ConfigFile> &p_import, const List<ResourceImporter::ImportOption> &p_texture_options);
	// The texture importer fixes transparent borders and the streamed importer
	// doesn't, which only makes a difference if the image has transparency.
	static bool fixes_alpha_border(const Ref<ConfigFile> &p_import);
	// The .import file for the streamed importer, keeping the UID and the
	// options both importers share.
	static Ref<ConfigFile> make_streamed_import(const Ref<ConfigFile> &p_import, const List<ResourceImporter::ImportOption> &p_streamed_options, const Dictionary &p_streamed_defaults);
#endif
};
