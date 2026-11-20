/**************************************************************************/
/*  erethea_update.cpp                                                    */
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

#include "erethea_update.h"

#include "core/io/file_access.h"
#include "core/os/os.h"
#include "core/variant/dictionary.h"
#include "core/version.h"

static const String BUILD_NAME_PREFIX = "erethea-";

String EretheaUpdate::get_version_from_build_name(const String &p_build_name) {
	if (!p_build_name.begins_with(BUILD_NAME_PREFIX)) {
		return String();
	}
	const String version = p_build_name.trim_prefix(BUILD_NAME_PREFIX);
	return is_valid_version(version) ? version : String();
}

String EretheaUpdate::get_current_version() {
	return get_version_from_build_name(GODOT_VERSION_BUILD);
}

// Splits "1.2.3-beta.4" into its numbers and its prerelease part.
static bool _parse_version(const String &p_version, int r_numbers[3], String &r_prerelease) {
	String version = p_version.strip_edges().trim_prefix("v");
	const int dash = version.find_char('-');
	r_prerelease = dash >= 0 ? version.substr(dash + 1) : String();
	if (dash >= 0) {
		version = version.substr(0, dash);
	}
	if (dash >= 0 && r_prerelease.is_empty()) {
		return false;
	}

	const PackedStringArray parts = version.split(".");
	if (parts.size() < 1 || parts.size() > 3) {
		return false;
	}
	for (int i = 0; i < 3; i++) {
		r_numbers[i] = 0;
		if (i < parts.size()) {
			if (parts[i].is_empty() || !parts[i].is_valid_int()) {
				return false;
			}
			r_numbers[i] = parts[i].to_int();
			if (r_numbers[i] < 0) {
				return false;
			}
		}
	}
	return true;
}

bool EretheaUpdate::is_valid_version(const String &p_version) {
	int numbers[3];
	String prerelease;
	return _parse_version(p_version, numbers, prerelease);
}

bool EretheaUpdate::is_prerelease(const String &p_version) {
	int numbers[3];
	String prerelease;
	return _parse_version(p_version, numbers, prerelease) && !prerelease.is_empty();
}

// Compares prerelease parts like SemVer: identifiers separated by dots,
// numeric ones compared as numbers and before alphanumeric ones.
static int _compare_prerelease(const String &p_a, const String &p_b) {
	const PackedStringArray a = p_a.split(".");
	const PackedStringArray b = p_b.split(".");
	for (int i = 0; i < MIN(a.size(), b.size()); i++) {
		const bool a_number = a[i].is_valid_int();
		const bool b_number = b[i].is_valid_int();
		if (a_number && b_number) {
			const int64_t diff = a[i].to_int() - b[i].to_int();
			if (diff != 0) {
				return diff < 0 ? -1 : 1;
			}
		} else if (a_number != b_number) {
			return a_number ? -1 : 1;
		} else {
			const int cmp = a[i].casecmp_to(b[i]);
			if (cmp != 0) {
				return cmp;
			}
		}
	}
	return a.size() == b.size() ? 0 : (a.size() < b.size() ? -1 : 1);
}

int EretheaUpdate::compare_versions(const String &p_a, const String &p_b) {
	int a[3];
	int b[3];
	String a_prerelease;
	String b_prerelease;
	if (!_parse_version(p_a, a, a_prerelease) || !_parse_version(p_b, b, b_prerelease)) {
		return 0;
	}
	for (int i = 0; i < 3; i++) {
		if (a[i] != b[i]) {
			return a[i] < b[i] ? -1 : 1;
		}
	}
	if (a_prerelease.is_empty() || b_prerelease.is_empty()) {
		// A release comes after its prereleases.
		return a_prerelease.is_empty() == b_prerelease.is_empty() ? 0 : (a_prerelease.is_empty() ? 1 : -1);
	}
	return _compare_prerelease(a_prerelease, b_prerelease);
}

bool EretheaUpdate::find_newer_release(const Array &p_releases, const String &p_current, bool p_stable_only, bool p_patch_only, Release &r_release) {
	int current[3];
	String current_prerelease;
	if (!_parse_version(p_current, current, current_prerelease)) {
		return false;
	}

	bool found = false;
	for (const Variant &item : p_releases) {
		if (item.get_type() != Variant::DICTIONARY) {
			continue;
		}
		const Dictionary release = item;
		if (bool(release.get("draft", false))) {
			continue;
		}
		const String tag = release.get("tag_name", "");
		if (!tag.begins_with("v") || !is_valid_version(tag)) {
			continue;
		}
		const String version = tag.trim_prefix("v");
		const bool prerelease = bool(release.get("prerelease", false)) || is_prerelease(version);
		if (p_stable_only && prerelease) {
			continue;
		}
		if (p_patch_only) {
			int numbers[3];
			String version_prerelease;
			_parse_version(version, numbers, version_prerelease);
			if (numbers[0] != current[0] || numbers[1] != current[1]) {
				continue;
			}
		}
		if (compare_versions(version, p_current) <= 0) {
			continue;
		}
		if (found && compare_versions(version, r_release.version) <= 0) {
			continue;
		}

		found = true;
		r_release = Release();
		r_release.version = version;
		r_release.prerelease = prerelease;
		r_release.page_url = release.get("html_url", "");
		const Array assets = release.get("assets", Array());
		for (const Variant &asset_item : assets) {
			if (asset_item.get_type() != Variant::DICTIONARY) {
				continue;
			}
			const Dictionary asset = asset_item;
			if (String(asset.get("name", "")) == INSTALLER_ASSET) {
				r_release.installer_url = asset.get("browser_download_url", "");
				const String digest = asset.get("digest", "");
				if (digest.begins_with("sha256:")) {
					r_release.installer_sha256 = digest.trim_prefix("sha256:").to_lower();
				}
				break;
			}
		}
	}
	return found;
}

bool EretheaUpdate::can_install_update() {
#ifdef WINDOWS_ENABLED
	// The installer leaves its uninstaller next to the executable.
	const String dir = OS::get_singleton()->get_executable_path().get_base_dir();
	return !get_current_version().is_empty() && FileAccess::exists(dir.path_join("unins000.exe"));
#else
	return false;
#endif
}

String EretheaUpdate::get_installer_download_path(const String &p_version) {
	return OS::get_singleton()->get_temp_path().path_join("erethea-setup-" + p_version + ".exe");
}

Error EretheaUpdate::launch_installer(const String &p_path) {
	// Installs over the current installation and starts the editor again
	// (see "update" in erethea/installer/erethea.iss).
	List<String> args;
	args.push_back("/SILENT");
	args.push_back("/SUPPRESSMSGBOXES");
	args.push_back("/NORESTART");
	args.push_back("/CLOSEAPPLICATIONS");
	args.push_back("/update=1");
	return OS::get_singleton()->create_process(p_path, args);
}
