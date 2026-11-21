/**************************************************************************/
/*  amd_fsr_backends.h                                                    */
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

// AMD's headers only build on Windows.
#ifdef WINDOWS_ENABLED

#include "servers/rendering/rendering_device_driver.h"

#include <thirdparty/amd-fsr-api/api/include/ffx_api.h>

// The rendering driver parts of AmdFsrUpscaler. AMD's Vulkan and Direct3D 12
// headers can't be included together, so each driver has its own file, built
// only with that driver.
namespace AmdFsrBackends {

// Creates an FSR context. The driver's description is linked after
// p_chain_end, the last description of the chain that starts at p_desc.
ffxReturnCode_t create_context_vulkan(PfnFfxCreateContext p_create_context, ffxContext *r_context, ffxCreateContextDescHeader *p_desc, ffxApiHeader *p_chain_end);
// The command buffer for FSR to record into, inside a driver callback.
void *get_command_list_vulkan(RenderingDeviceDriver::CommandBufferID p_command_buffer);
// Whether the device has a memory type that is device-local without being
// visible to the CPU. AMD's Vulkan backend puts its textures only there, so
// devices with shared memory only (software renderers) can't run FSR.
bool has_device_only_memory_vulkan();

ffxReturnCode_t create_context_d3d12(PfnFfxCreateContext p_create_context, ffxContext *r_context, ffxCreateContextDescHeader *p_desc, ffxApiHeader *p_chain_end);
void *get_command_list_d3d12(RenderingDeviceDriver::CommandBufferID p_command_buffer);
// Whether the device has enhanced barriers, which FSR needs on Direct3D 12.
bool has_enhanced_barriers_d3d12();

} // namespace AmdFsrBackends

#endif // WINDOWS_ENABLED
