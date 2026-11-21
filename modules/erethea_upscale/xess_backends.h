/**************************************************************************/
/*  xess_backends.h                                                       */
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

// Intel's runtime is only for Windows.
#ifdef WINDOWS_ENABLED

#include "core/math/vector2.h"
#include "core/math/vector2i.h"
#include "core/os/os.h"
#include "core/string/ustring.h"
#include "core/templates/rid.h"
#include "servers/rendering/rendering_device_driver.h"

#include <thirdparty/xess/xess.h>

// The rendering driver parts of XessUpscaler. Intel's Vulkan and Direct3D 12
// headers each bring their graphics API, so each driver has its own file,
// built only with that driver.
namespace XessBackends {

// The textures and settings of one upscale.
struct Dispatch {
	RID color;
	RID velocity;
	RID depth;
	RID output;
	Size2i input_size;
	Vector2 jitter;
	bool reset = false;
};

// Finds a function of libxess.dll.
template <typename T>
bool load_function(void *p_library, const char *p_name, T &r_function) {
	void *symbol = nullptr;
	if (OS::get_singleton()->get_dynamic_library_symbol_handle(p_library, p_name, symbol) != OK) {
		return false;
	}
	r_function = (T)symbol;
	return true;
}

// Records the driver callback that runs XeSS on the textures of p_dispatch.
void add_callback(RenderingDeviceDriver::DriverCallback p_callback, void *p_userdata, const Dispatch &p_dispatch);

// Each backend loads its functions, returning false when some are missing,
// creates and initializes contexts, and records upscales.
bool load_d3d12(void *p_library);
xess_result_t create_context_d3d12(xess_context_handle_t *r_context);
xess_result_t init_d3d12(xess_context_handle_t p_context, const Size2i &p_output_size, xess_quality_settings_t p_quality, uint32_t p_flags);
void add_dispatch_d3d12(xess_context_handle_t p_context, const Dispatch &p_dispatch);

bool load_vulkan(void *p_library);
xess_result_t create_context_vulkan(xess_context_handle_t *r_context);
xess_result_t init_vulkan(xess_context_handle_t p_context, const Size2i &p_output_size, xess_quality_settings_t p_quality, uint32_t p_flags);
void add_dispatch_vulkan(xess_context_handle_t p_context, const Dispatch &p_dispatch);

// XeSS needs extensions and features enabled when the Vulkan instance and
// device are created. Asks Godot for them; must be called before the
// rendering device is created.
void add_requirements_vulkan(void *p_library);
void remove_requirements_vulkan();
// Whether the Vulkan device was created with what XeSS needs. r_reason tells
// why not.
bool check_device_vulkan(String &r_reason);

} // namespace XessBackends

#endif // WINDOWS_ENABLED
