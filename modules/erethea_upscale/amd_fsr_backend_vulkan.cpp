/**************************************************************************/
/*  amd_fsr_backend_vulkan.cpp                                            */
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

#if defined(WINDOWS_ENABLED) && defined(VULKAN_ENABLED)

#include "drivers/vulkan/rendering_device_driver_vulkan.h"
#include "servers/rendering/rendering_device.h"

#include <thirdparty/amd-fsr-api/api/include/vk/ffx_api_vk.h>

ffxReturnCode_t AmdFsrBackends::create_context_vulkan(PfnFfxCreateContext p_create_context, ffxContext *r_context, ffxCreateContextDescHeader *p_desc, ffxApiHeader *p_chain_end) {
	RenderingDevice *rd = RenderingDevice::get_singleton();
	ffxCreateBackendVKDesc backend = {};
	backend.header.type = FFX_API_CREATE_CONTEXT_DESC_TYPE_BACKEND_VK;
	backend.deviceContext.vkDevice = (VkDevice)rd->get_driver_resource(RD::DRIVER_RESOURCE_LOGICAL_DEVICE);
	backend.deviceContext.vkPhysicalDevice = (VkPhysicalDevice)rd->get_driver_resource(RD::DRIVER_RESOURCE_PHYSICAL_DEVICE);
	backend.deviceContext.vkDeviceProcAddr = vkGetDeviceProcAddr;

	p_chain_end->pNext = &backend.header;
	const ffxReturnCode_t result = p_create_context(r_context, p_desc, nullptr);
	p_chain_end->pNext = nullptr;
	return result;
}

void *AmdFsrBackends::get_command_list_vulkan(RenderingDeviceDriver::CommandBufferID p_command_buffer) {
	return RenderingDeviceDriverVulkan::command_buffer_get_vk_handle(p_command_buffer);
}

bool AmdFsrBackends::has_device_only_memory_vulkan() {
	RenderingDevice *rd = RenderingDevice::get_singleton();
	VkPhysicalDevice physical_device = (VkPhysicalDevice)rd->get_driver_resource(RD::DRIVER_RESOURCE_PHYSICAL_DEVICE);
	VkPhysicalDeviceMemoryProperties properties = {};
	vkGetPhysicalDeviceMemoryProperties(physical_device, &properties);
	for (uint32_t i = 0; i < properties.memoryTypeCount; i++) {
		const VkMemoryPropertyFlags flags = properties.memoryTypes[i].propertyFlags;
		if ((flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) && !(flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)) {
			return true;
		}
	}
	return false;
}

#endif // defined(WINDOWS_ENABLED) && defined(VULKAN_ENABLED)
