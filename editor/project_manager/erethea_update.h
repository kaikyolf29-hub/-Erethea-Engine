/**************************************************************************/
/*  erethea_update.h                                                      */
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
#include "core/variant/array.h"

// Erethea: Updates of the editor, from the releases of the Erethea repository
// on GitHub.
//
// A release is tagged "v<version>" (for example "v0.2.0" or "v0.2.0-beta.1")
// and carries the Windows installer as an asset. Release builds are made with
// BUILD_NAME=erethea-<version>, which is how the editor knows its own version.
// Development builds have another build name and never offer updates.

class EretheaUpdate {
public:
	static constexpr const char *RELEASES_URL = "https://api.github.com/repos/kaikyolf29-hub/-Erethea-Engine/releases?per_page=30";
	static constexpr const char *RELEASES_PAGE_URL = "https://github.com/kaikyolf29-hub/-Erethea-Engine/releases";
	static constexpr const char *INSTALLER_ASSET = "erethea-setup-x86_64.exe";

	struct Release {
		String version;
		String page_url;
		String installer_url;
		String installer_sha256; // Empty when GitHub didn't give it.
		bool prerelease = false;
	};

	// Version of this build, or an empty string for a development build.
	static String get_current_version();
	static String get_version_from_build_name(const String &p_build_name);

	// Returns < 0, 0 or > 0, like strcmp(). Prereleases ("1.0.0-beta.2") come
	// before the release ("1.0.0"). Returns 0 for versions that can't be read.
	static int compare_versions(const String &p_a, const String &p_b);
	static bool is_valid_version(const String &p_version);
	static bool is_prerelease(const String &p_version);

	// Picks the newest release that is newer than p_current, from the JSON
	// array returned by RELEASES_URL. p_patch_only keeps the major and minor
	// version of p_current.
	static bool find_newer_release(const Array &p_releases, const String &p_current, bool p_stable_only, bool p_patch_only, Release &r_release);

	// True when this editor was installed by the Erethea installer on Windows,
	// so that an update can be installed over it.
	static bool can_install_update();
	static String get_installer_download_path(const String &p_version);
	static Error launch_installer(const String &p_path);
};
