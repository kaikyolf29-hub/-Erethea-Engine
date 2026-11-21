/**************************************************************************/
/*  amd_fsr_backend_d3d12.cpp                                             */
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

#include "amd_fsr_backends.h"

#if defined(WINDOWS_ENABLED) && defined(D3D12_ENABLED)

// The driver's header brings the DirectX headers Godot builds with, which
// AMD's header then uses.
#include "drivers/d3d12/rendering_device_driver_d3d12.h"
#include "servers/rendering/rendering_device.h"

#include <thirdparty/amd-fsr-api/api/include/dx12/ffx_api_dx12.h>

ffxReturnCode_t AmdFsrBackends::create_context_d3d12(PfnFfxCreateContext p_create_context, ffxContext *r_context, ffxCreateContextDescHeader *p_desc, ffxApiHeader *p_chain_end) {
	ffxCreateBackendDX12Desc backend = {};
	backend.header.type = FFX_API_CREATE_CONTEXT_DESC_TYPE_BACKEND_DX12;
	backend.device = (ID3D12Device *)RenderingDevice::get_singleton()->get_driver_resource(RD::DRIVER_RESOURCE_LOGICAL_DEVICE);

	p_chain_end->pNext = &backend.header;
	const ffxReturnCode_t result = p_create_context(r_context, p_desc, nullptr);
	p_chain_end->pNext = nullptr;
	return result;
}

void *AmdFsrBackends::get_command_list_d3d12(RenderingDeviceDriver::CommandBufferID p_command_buffer) {
	return RenderingDeviceDriverD3D12::command_buffer_begin_native_commands(p_command_buffer);
}

bool AmdFsrBackends::has_enhanced_barriers_d3d12() {
	ID3D12Device *device = (ID3D12Device *)RenderingDevice::get_singleton()->get_driver_resource(RD::DRIVER_RESOURCE_LOGICAL_DEVICE);
	D3D12_FEATURE_DATA_D3D12_OPTIONS12 options12 = {};
	return device != nullptr && SUCCEEDED(device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS12, &options12, sizeof(options12))) && options12.EnhancedBarriersSupported;
}

#endif // defined(WINDOWS_ENABLED) && defined(D3D12_ENABLED)
