/**************************************************************************/
/*  xess_backend_vulkan.cpp                                               */
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

#if defined(WINDOWS_ENABLED) && defined(VULKAN_ENABLED)

#include "drivers/vulkan/erethea_vulkan_requirements.h"
#include "drivers/vulkan/rendering_device_driver_vulkan.h"
#include "servers/rendering/rendering_device.h"

#include <thirdparty/xess/xess_vk.h>

namespace {

struct Functions {
	decltype(&xessVKCreateContext) create_context = nullptr;
	decltype(&xessVKInit) init = nullptr;
	decltype(&xessVKExecute) execute = nullptr;
};

Functions functions;

// Enables what XeSS needs on the Vulkan instance and device.
class XessVulkanRequirements : public EretheaVulkanRequirements {
public:
	decltype(&xessVKGetRequiredInstanceExtensions) get_required_instance_extensions = nullptr;
	decltype(&xessVKGetRequiredDeviceExtensions) get_required_device_extensions = nullptr;
	decltype(&xessVKGetRequiredDeviceFeatures) get_required_device_features = nullptr;

	// Why the device can't run XeSS, found while it was created.
	String refused;

	virtual void get_instance_requirements(LocalVector<CharString> &r_extensions, uint32_t &r_min_api_version) override {
		uint32_t count = 0;
		const char *const *names = nullptr;
		uint32_t api_version = 0;
		const xess_result_t result = get_required_instance_extensions(&count, &names, &api_version);
		if (result < XESS_RESULT_SUCCESS) {
			refused = vformat("XeSS didn't tell which Vulkan instance extensions it needs (error %d)", int(result));
			return;
		}
		for (uint32_t i = 0; i < count; i++) {
			r_extensions.push_back(CharString(names[i]));
		}
		r_min_api_version = api_version;
		print_verbose(vformat("XeSS: needs Vulkan %d.%d and %d instance extensions.", VK_API_VERSION_MAJOR(api_version), VK_API_VERSION_MINOR(api_version), count));
	}

	virtual void get_device_extensions(VkInstance p_instance, VkPhysicalDevice p_physical_device, LocalVector<CharString> &r_extensions) override {
		uint32_t count = 0;
		const char *const *names = nullptr;
		const xess_result_t result = get_required_device_extensions(p_instance, p_physical_device, &count, &names);
		if (result < XESS_RESULT_SUCCESS) {
			refused = vformat("the graphics card isn't supported (error %d)", int(result));
			return;
		}
		for (uint32_t i = 0; i < count; i++) {
			r_extensions.push_back(CharString(names[i]));
		}
	}

	virtual void *patch_device_features(VkInstance p_instance, VkPhysicalDevice p_physical_device, void *p_features) override {
		if (!refused.is_empty()) {
			return p_features;
		}

		// XeSS can't run without all of its extensions, and then its features
		// could keep the device from being created, so they're left out.
		LocalVector<CharString> required;
		get_device_extensions(p_instance, p_physical_device, required);
		uint32_t count = 0;
		vkEnumerateDeviceExtensionProperties(p_physical_device, nullptr, &count, nullptr);
		LocalVector<VkExtensionProperties> available;
		available.resize(count);
		vkEnumerateDeviceExtensionProperties(p_physical_device, nullptr, &count, available.ptr());
		for (const CharString &extension : required) {
			bool found = false;
			for (const VkExtensionProperties &properties : available) {
				if (strcmp(extension.get_data(), properties.extensionName) == 0) {
					found = true;
					break;
				}
			}
			if (!found) {
				refused = vformat("the graphics driver lacks the Vulkan extension %s", String::utf8(extension.get_data()));
				return p_features;
			}
		}

		void *features = p_features;
		const xess_result_t result = get_required_device_features(p_instance, p_physical_device, &features);
		if (result < XESS_RESULT_SUCCESS || features == nullptr) {
			refused = vformat("XeSS didn't tell which Vulkan features it needs (error %d)", int(result));
			return p_features;
		}
		return features;
	}
};

XessVulkanRequirements *requirements = nullptr;

// Arguments of one upscale, read when the rendering graph records it.
struct DispatchArgs {
	xess_context_handle_t context = nullptr;
	xess_vk_execute_params_t params = {};
};

void dispatch_callback(RenderingDeviceDriver *p_driver, RenderingDeviceDriver::CommandBufferID p_command_buffer, void *p_userdata) {
	DispatchArgs *args = (DispatchArgs *)p_userdata;
	const xess_result_t result = functions.execute(args->context, RenderingDeviceDriverVulkan::command_buffer_get_vk_handle(p_command_buffer), &args->params);
	if (result < XESS_RESULT_SUCCESS) {
		ERR_PRINT_ONCE(vformat("XeSS couldn't upscale (error %d).", int(result)));
	}
	memdelete(args);
}

xess_vk_image_view_info make_image_view_info(RID p_texture) {
	RenderingDevice *rd = RenderingDevice::get_singleton();
	const RD::TextureFormat format = rd->texture_get_format(p_texture);
	xess_vk_image_view_info info = {};
	info.imageView = (VkImageView)rd->get_driver_resource(RD::DRIVER_RESOURCE_TEXTURE_VIEW, p_texture);
	info.image = (VkImage)rd->get_driver_resource(RD::DRIVER_RESOURCE_TEXTURE, p_texture);
	info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	info.subresourceRange.levelCount = 1;
	info.subresourceRange.layerCount = 1;
	info.format = (VkFormat)rd->get_driver_resource(RD::DRIVER_RESOURCE_TEXTURE_DATA_FORMAT, p_texture);
	info.width = format.width;
	info.height = format.height;
	return info;
}

} // namespace

bool XessBackends::load_vulkan(void *p_library) {
	Functions loaded;
	if (!load_function(p_library, "xessVKCreateContext", loaded.create_context) ||
			!load_function(p_library, "xessVKInit", loaded.init) ||
			!load_function(p_library, "xessVKExecute", loaded.execute)) {
		return false;
	}
	functions = loaded;
	return true;
}

xess_result_t XessBackends::create_context_vulkan(xess_context_handle_t *r_context) {
	RenderingDevice *rd = RenderingDevice::get_singleton();
	VkInstance instance = (VkInstance)rd->get_driver_resource(RD::DRIVER_RESOURCE_TOPMOST_OBJECT);
	VkPhysicalDevice physical_device = (VkPhysicalDevice)rd->get_driver_resource(RD::DRIVER_RESOURCE_PHYSICAL_DEVICE);
	VkDevice device = (VkDevice)rd->get_driver_resource(RD::DRIVER_RESOURCE_LOGICAL_DEVICE);
	return functions.create_context(instance, physical_device, device, r_context);
}

xess_result_t XessBackends::init_vulkan(xess_context_handle_t p_context, const Size2i &p_output_size, xess_quality_settings_t p_quality, uint32_t p_flags) {
	xess_vk_init_params_t params = {};
	params.outputResolution.x = p_output_size.width;
	params.outputResolution.y = p_output_size.height;
	params.qualitySetting = p_quality;
	params.initFlags = p_flags;
	return functions.init(p_context, &params);
}

void XessBackends::add_dispatch_vulkan(xess_context_handle_t p_context, const Dispatch &p_dispatch) {
	DispatchArgs *args = memnew(DispatchArgs);
	args->context = p_context;
	xess_vk_execute_params_t &params = args->params;
	params.colorTexture = make_image_view_info(p_dispatch.color);
	params.velocityTexture = make_image_view_info(p_dispatch.velocity);
	params.depthTexture = make_image_view_info(p_dispatch.depth);
	params.outputTexture = make_image_view_info(p_dispatch.output);
	params.jitterOffsetX = p_dispatch.jitter.x;
	params.jitterOffsetY = p_dispatch.jitter.y;
	params.exposureScale = 1.0f;
	params.resetHistory = p_dispatch.reset ? 1 : 0;
	params.inputWidth = p_dispatch.input_size.width;
	params.inputHeight = p_dispatch.input_size.height;

	add_callback(&dispatch_callback, args, p_dispatch);
}

void XessBackends::add_requirements_vulkan(void *p_library) {
	if (requirements != nullptr) {
		return;
	}
	XessVulkanRequirements *loaded = memnew(XessVulkanRequirements);
	if (!load_function(p_library, "xessVKGetRequiredInstanceExtensions", loaded->get_required_instance_extensions) ||
			!load_function(p_library, "xessVKGetRequiredDeviceExtensions", loaded->get_required_device_extensions) ||
			!load_function(p_library, "xessVKGetRequiredDeviceFeatures", loaded->get_required_device_features)) {
		memdelete(loaded);
		return;
	}
	requirements = loaded;
	EretheaVulkanRequirements::add(requirements);
}

void XessBackends::remove_requirements_vulkan() {
	if (requirements != nullptr) {
		memdelete(requirements);
		requirements = nullptr;
	}
}

bool XessBackends::check_device_vulkan(String &r_reason) {
	if (requirements == nullptr) {
		r_reason = "it has to be chosen before Erethea starts, to set up the Vulkan device for it. Restart to use it";
		return false;
	}
	if (!requirements->refused.is_empty()) {
		r_reason = requirements->refused;
		return false;
	}
	RenderingDevice *rd = RenderingDevice::get_singleton();
	VkPhysicalDevice physical_device = (VkPhysicalDevice)rd->get_driver_resource(RD::DRIVER_RESOURCE_PHYSICAL_DEVICE);
	if (!requirements->is_device_patched(physical_device)) {
		r_reason = "the graphics driver didn't accept the Vulkan features it needs";
		return false;
	}
	return true;
}

#endif // defined(WINDOWS_ENABLED) && defined(VULKAN_ENABLED)
