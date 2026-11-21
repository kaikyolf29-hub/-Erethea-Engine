/**************************************************************************/
/*  erethea_upscale_export_plugin.cpp                                     */
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

#include "erethea_upscale_export_plugin.h"

#include "../amd_fsr_upscaler.h"
#include "../erethea_upscaler.h"

#include "core/io/file_access.h"
#include "core/os/os.h"

void EretheaUpscaleExportPlugin::_export_begin(const HashSet<String> &p_features, bool p_debug, const String &p_path, int p_flags) {
	if (!p_features.has("windows") || EretheaUpscaler::get_project_mode() != EretheaUpscaler::MODE_AMD_FSR) {
		return;
	}

	// Only the runtime of the game's rendering driver: each one is large. If
	// the game falls back to the other driver, it uses FSR 2.2.
	const String driver = EditorExportPlatform::get_project_setting(get_export_preset(), "rendering/rendering_device/driver.windows");
	const Vector<String> libraries = AmdFsrUpscaler::get_runtime_libraries(driver);
	if (libraries.is_empty()) {
		return;
	}

	const String folder = OS::get_singleton()->get_executable_path().get_base_dir();
	for (const String &library : libraries) {
		const String path = folder.path_join(library);
		if (!FileAccess::exists(path)) {
			const String message = vformat(TTR("\"%s\" isn't next to the editor's executable, so the exported game will use FSR 2.2 instead of AMD FSR."), library);
			Ref<EditorExportPlatform> platform = get_export_platform();
			if (platform.is_valid()) {
				platform->add_message(EditorExportPlatform::EXPORT_MESSAGE_WARNING, TTR("AMD FSR"), message);
			} else {
				WARN_PRINT(message);
			}
			return;
		}
	}
	for (const String &library : libraries) {
		add_shared_object(folder.path_join(library), Vector<String>());
	}
}
