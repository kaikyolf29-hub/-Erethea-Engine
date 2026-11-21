/**************************************************************************/
/*  erethea_vulkan_requirements.h                                         */
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
#include "core/templates/local_vector.h"

#include <drivers/vulkan/godot_vulkan.h>

// Erethea: Lets modules enable what a library they load needs from the
// Vulkan instance and device that Godot creates, such as an upscaler's
// runtime. What they request is optional: extensions the driver lacks are
// left out, and if the device can't be created with the features they add,
// Godot creates it without them. Modules find out through is_device_patched()
// and their own checks.
class EretheaVulkanRequirements {
	static LocalVector<EretheaVulkanRequirements *> list;
	static bool device_features_refused;

	VkPhysicalDevice patched_device = VK_NULL_HANDLE;

public:
	static void add(EretheaVulkanRequirements *p_requirements);
	static void remove(EretheaVulkanRequirements *p_requirements);
	static const LocalVector<EretheaVulkanRequirements *> &get_list() { return list; }

	// Whether the requirements should patch the features of the device.
	static bool should_patch_device_features() { return !list.is_empty() && !device_features_refused; }
	// The device couldn't be created with the patched features.
	static void set_device_features_refused() { device_features_refused = true; }

	// The instance extensions to enable, and the lowest Vulkan version to
	// create the instance for (raised only as far as the loader supports).
	virtual void get_instance_requirements(LocalVector<CharString> &r_extensions, uint32_t &r_min_api_version) {}

	// The device extensions to enable on p_physical_device.
	virtual void get_device_extensions(VkInstance p_instance, VkPhysicalDevice p_physical_device, LocalVector<CharString> &r_extensions) {}

	// Patches the chain of feature structures given to vkCreateDevice, which
	// starts with a VkPhysicalDeviceFeatures2. Returns the new start of the
	// chain, or p_features.
	virtual void *patch_device_features(VkInstance p_instance, VkPhysicalDevice p_physical_device, void *p_features) { return p_features; }

	// Called by the Vulkan driver once the device is created.
	void set_patched_device(VkPhysicalDevice p_physical_device) { patched_device = p_physical_device; }
	// Whether the device on p_physical_device was created with these requirements.
	bool is_device_patched(VkPhysicalDevice p_physical_device) const { return patched_device != VK_NULL_HANDLE && patched_device == p_physical_device; }

	virtual ~EretheaVulkanRequirements();
};
