/**************************************************************************/
/*  xess_backend_d3d12.cpp                                                */
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

#include "xess_backends.h"

#if defined(WINDOWS_ENABLED) && defined(D3D12_ENABLED)

#include "servers/rendering/rendering_device.h"

// The driver's header brings the DirectX headers Godot builds with, which
// Intel's header then uses.
#include "drivers/d3d12/rendering_device_driver_d3d12.h"

#include <thirdparty/xess/xess_d3d12.h>

namespace {

struct Functions {
	decltype(&xessD3D12CreateContext) create_context = nullptr;
	decltype(&xessD3D12Init) init = nullptr;
	decltype(&xessD3D12Execute) execute = nullptr;
};

Functions functions;

// Arguments of one upscale, read when the rendering graph records it.
struct DispatchArgs {
	xess_context_handle_t context = nullptr;
	xess_d3d12_execute_params_t params = {};
};

void dispatch_callback(RenderingDeviceDriver *p_driver, RenderingDeviceDriver::CommandBufferID p_command_buffer, void *p_userdata) {
	DispatchArgs *args = (DispatchArgs *)p_userdata;
	// XeSS binds its own pipelines and descriptor heap, so Godot forgets the
	// ones it bound.
	ID3D12GraphicsCommandList *command_list = (ID3D12GraphicsCommandList *)RenderingDeviceDriverD3D12::command_buffer_begin_native_commands(p_command_buffer);
	const xess_result_t result = functions.execute(args->context, command_list, &args->params);
	if (result < XESS_RESULT_SUCCESS) {
		ERR_PRINT_ONCE(vformat("XeSS couldn't upscale (error %d).", int(result)));
	}
	memdelete(args);
}

} // namespace

bool XessBackends::load_d3d12(void *p_library) {
	Functions loaded;
	if (!load_function(p_library, "xessD3D12CreateContext", loaded.create_context) ||
			!load_function(p_library, "xessD3D12Init", loaded.init) ||
			!load_function(p_library, "xessD3D12Execute", loaded.execute)) {
		return false;
	}
	functions = loaded;
	return true;
}

xess_result_t XessBackends::create_context_d3d12(xess_context_handle_t *r_context) {
	RenderingDevice *rd = RenderingDevice::get_singleton();
	ID3D12Device *device = (ID3D12Device *)rd->get_driver_resource(RD::DRIVER_RESOURCE_LOGICAL_DEVICE);
	return functions.create_context(device, r_context);
}

xess_result_t XessBackends::init_d3d12(xess_context_handle_t p_context, const Size2i &p_output_size, xess_quality_settings_t p_quality, uint32_t p_flags) {
	xess_d3d12_init_params_t params = {};
	params.outputResolution.x = p_output_size.width;
	params.outputResolution.y = p_output_size.height;
	params.qualitySetting = p_quality;
	params.initFlags = p_flags;
	return functions.init(p_context, &params);
}

void XessBackends::add_dispatch_d3d12(xess_context_handle_t p_context, const Dispatch &p_dispatch) {
	RenderingDevice *rd = RenderingDevice::get_singleton();

	DispatchArgs *args = memnew(DispatchArgs);
	args->context = p_context;
	xess_d3d12_execute_params_t &params = args->params;
	params.pColorTexture = (ID3D12Resource *)rd->get_driver_resource(RD::DRIVER_RESOURCE_TEXTURE, p_dispatch.color);
	params.pVelocityTexture = (ID3D12Resource *)rd->get_driver_resource(RD::DRIVER_RESOURCE_TEXTURE, p_dispatch.velocity);
	params.pDepthTexture = (ID3D12Resource *)rd->get_driver_resource(RD::DRIVER_RESOURCE_TEXTURE, p_dispatch.depth);
	params.pOutputTexture = (ID3D12Resource *)rd->get_driver_resource(RD::DRIVER_RESOURCE_TEXTURE, p_dispatch.output);
	params.jitterOffsetX = p_dispatch.jitter.x;
	params.jitterOffsetY = p_dispatch.jitter.y;
	params.exposureScale = 1.0f;
	params.resetHistory = p_dispatch.reset ? 1 : 0;
	params.inputWidth = p_dispatch.input_size.width;
	params.inputHeight = p_dispatch.input_size.height;

	add_callback(&dispatch_callback, args, p_dispatch);
}

#endif // defined(WINDOWS_ENABLED) && defined(D3D12_ENABLED)
