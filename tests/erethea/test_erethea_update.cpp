/**************************************************************************/
/*  test_erethea_update.cpp                                               */
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

#include "tests/test_macros.h"

TEST_FORCE_LINK(test_erethea_update)

#ifdef TOOLS_ENABLED

#include "core/io/json.h"
#include "editor/project_manager/erethea_update.h"

namespace TestEretheaUpdate {

TEST_CASE("[EretheaUpdate] Version of a build") {
	CHECK(EretheaUpdate::get_version_from_build_name("erethea-0.2.0") == "0.2.0");
	CHECK(EretheaUpdate::get_version_from_build_name("erethea-1.0.0-beta.3") == "1.0.0-beta.3");
	CHECK(EretheaUpdate::get_version_from_build_name("custom_build").is_empty());
	CHECK(EretheaUpdate::get_version_from_build_name("gh").is_empty());
	CHECK(EretheaUpdate::get_version_from_build_name("erethea-").is_empty());
	CHECK(EretheaUpdate::get_version_from_build_name("erethea-main").is_empty());
}

TEST_CASE("[EretheaUpdate] Version comparison") {
	CHECK(EretheaUpdate::compare_versions("0.1.0", "0.2.0") < 0);
	CHECK(EretheaUpdate::compare_versions("0.10.0", "0.9.0") > 0);
	CHECK(EretheaUpdate::compare_versions("1.0.0", "1.0.0") == 0);
	CHECK(EretheaUpdate::compare_versions("1.0", "1.0.0") == 0);
	CHECK(EretheaUpdate::compare_versions("v1.2.3", "1.2.3") == 0);
	CHECK(EretheaUpdate::compare_versions("1.0.1", "1.0.0") > 0);

	// Prereleases come before the release, and are compared like SemVer.
	CHECK(EretheaUpdate::compare_versions("1.0.0-beta.1", "1.0.0") < 0);
	CHECK(EretheaUpdate::compare_versions("1.0.0", "1.0.0-rc.1") > 0);
	CHECK(EretheaUpdate::compare_versions("1.0.0-beta.2", "1.0.0-beta.10") < 0);
	CHECK(EretheaUpdate::compare_versions("1.0.0-alpha.1", "1.0.0-beta.1") < 0);
	CHECK(EretheaUpdate::compare_versions("1.0.0-beta", "1.0.0-beta.1") < 0);
	CHECK(EretheaUpdate::compare_versions("1.0.0-rc.1", "0.9.0") > 0);

	CHECK(EretheaUpdate::is_valid_version("0.1.0"));
	CHECK(EretheaUpdate::is_valid_version("1.0.0-beta.1"));
	CHECK_FALSE(EretheaUpdate::is_valid_version(""));
	CHECK_FALSE(EretheaUpdate::is_valid_version("main"));
	CHECK_FALSE(EretheaUpdate::is_valid_version("1..0"));
	CHECK_FALSE(EretheaUpdate::is_valid_version("1.0.0.0"));
	CHECK_FALSE(EretheaUpdate::is_valid_version("1.0.0-"));
	CHECK(EretheaUpdate::is_prerelease("1.0.0-rc.1"));
	CHECK_FALSE(EretheaUpdate::is_prerelease("1.0.0"));
}

// A shortened answer of the GitHub API, newest release first.
static Array get_releases() {
	const String json = R"([
		{"tag_name": "v0.4.0-beta.1", "draft": false, "prerelease": true, "html_url": "https://example.com/v0.4.0-beta.1", "assets": []},
		{"tag_name": "v0.5.0", "draft": true, "prerelease": false, "html_url": "https://example.com/v0.5.0", "assets": []},
		{"tag_name": "v0.3.1", "draft": false, "prerelease": false, "html_url": "https://example.com/v0.3.1", "assets": [
			{"name": "erethea-setup-x86_64.exe", "browser_download_url": "https://example.com/v0.3.1/erethea-setup-x86_64.exe", "digest": "sha256:ABCDEF0123"},
			{"name": "other.zip", "browser_download_url": "https://example.com/v0.3.1/other.zip"}
		]},
		{"tag_name": "nightly", "draft": false, "prerelease": true, "html_url": "https://example.com/nightly", "assets": []},
		{"tag_name": "v0.3.0", "draft": false, "prerelease": false, "html_url": "https://example.com/v0.3.0", "assets": []},
		{"tag_name": "v0.2.0", "draft": false, "prerelease": false, "html_url": "https://example.com/v0.2.0", "assets": []}
	])";
	return JSON::parse_string(json);
}

TEST_CASE("[EretheaUpdate] Newest stable release") {
	EretheaUpdate::Release release;
	REQUIRE(EretheaUpdate::find_newer_release(get_releases(), "0.2.0", true, false, release));
	CHECK(release.version == "0.3.1");
	CHECK_FALSE(release.prerelease);
	CHECK(release.page_url == "https://example.com/v0.3.1");
	CHECK(release.installer_url == "https://example.com/v0.3.1/erethea-setup-x86_64.exe");
	CHECK(release.installer_sha256 == "abcdef0123");
}

TEST_CASE("[EretheaUpdate] Newest release, prereleases included") {
	EretheaUpdate::Release release;
	REQUIRE(EretheaUpdate::find_newer_release(get_releases(), "0.3.1", false, false, release));
	CHECK(release.version == "0.4.0-beta.1");
	CHECK(release.prerelease);
	CHECK(release.installer_url.is_empty());

	// Drafts are never offered.
	CHECK_FALSE(EretheaUpdate::find_newer_release(get_releases(), "0.4.0", false, false, release));
}

TEST_CASE("[EretheaUpdate] Prereleases setting") {
	// Off: stable modes skip prereleases, as before.
	CHECK(EretheaUpdate::is_stable_only(true, false));
	CHECK_FALSE(EretheaUpdate::is_stable_only(false, false));
	// On: prereleases are offered even in the stable modes.
	CHECK_FALSE(EretheaUpdate::is_stable_only(true, true));
	CHECK_FALSE(EretheaUpdate::is_stable_only(false, true));

	// A stable editor on 0.3.1 sees nothing with the setting off, and the beta with it on.
	EretheaUpdate::Release release;
	CHECK_FALSE(EretheaUpdate::find_newer_release(get_releases(), "0.3.1", EretheaUpdate::is_stable_only(true, false), false, release));
	REQUIRE(EretheaUpdate::find_newer_release(get_releases(), "0.3.1", EretheaUpdate::is_stable_only(true, true), false, release));
	CHECK(release.version == "0.4.0-beta.1");
}

TEST_CASE("[EretheaUpdate] Patch releases only") {
	EretheaUpdate::Release release;
	REQUIRE(EretheaUpdate::find_newer_release(get_releases(), "0.3.0", true, true, release));
	CHECK(release.version == "0.3.1");
	CHECK_FALSE(EretheaUpdate::find_newer_release(get_releases(), "0.2.0", true, true, release));
}

TEST_CASE("[EretheaUpdate] No update") {
	EretheaUpdate::Release release;
	CHECK_FALSE(EretheaUpdate::find_newer_release(get_releases(), "0.3.1", true, false, release));
	CHECK_FALSE(EretheaUpdate::find_newer_release(get_releases(), "1.0.0", false, false, release));
	CHECK_FALSE(EretheaUpdate::find_newer_release(Array(), "0.1.0", false, false, release));
	// Development builds don't know their version.
	CHECK_FALSE(EretheaUpdate::find_newer_release(get_releases(), "", false, false, release));
}

} // namespace TestEretheaUpdate

#endif // TOOLS_ENABLED
