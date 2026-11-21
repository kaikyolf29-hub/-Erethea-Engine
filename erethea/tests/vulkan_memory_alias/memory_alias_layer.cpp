/**************************************************************************/
/*  memory_alias_layer.cpp                                                */
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

// A Vulkan layer for Erethea's CI only, never shipped with the engine.
//
// Software renderers like Mesa's lavapipe have one memory type, which is
// device-local and visible to the CPU. AMD's FSR Vulkan backend only places
// its textures in a device-local type that the CPU can't see, like the ones
// graphics cards have, so it can't start on them. This layer adds such a type
// as an alias of the device-local one: memory requirements that allow the real
// type allow the alias too, and allocations of the alias get the real type.
// Nothing else changes, so the CI can run AMD FSR on lavapipe.
//
// Built by .github/workflows/erethea_amd_fsr.yml with MSVC, against the Vulkan
// headers in thirdparty/vulkan.

// The layer exports its own vkGetInstanceProcAddr and vkGetDeviceProcAddr.
#define VK_NO_PROTOTYPES
#include <vulkan/vk_layer.h>
#include <vulkan/vulkan.h>

#include <cstring>
#include <mutex>
#include <unordered_map>

#ifdef _WIN32
#define LAYER_EXPORT extern "C" __declspec(dllexport)
#else
#define LAYER_EXPORT extern "C" __attribute__((visibility("default")))
#endif

namespace {

struct Alias {
	bool active = false;
	uint32_t alias_index = 0;
	uint32_t source_index = 0;
};

struct InstanceData {
	VkInstance instance = VK_NULL_HANDLE;
	PFN_vkGetInstanceProcAddr get_instance_proc_addr = nullptr;
	PFN_vkDestroyInstance destroy_instance = nullptr;
	PFN_vkGetPhysicalDeviceMemoryProperties get_memory_properties = nullptr;
	PFN_vkGetPhysicalDeviceMemoryProperties2 get_memory_properties_2 = nullptr;
	PFN_vkGetPhysicalDeviceMemoryProperties2 get_memory_properties_2_khr = nullptr;
};

struct DeviceData {
	VkPhysicalDevice physical_device = VK_NULL_HANDLE;
	PFN_vkGetDeviceProcAddr get_device_proc_addr = nullptr;
	PFN_vkDestroyDevice destroy_device = nullptr;
	PFN_vkAllocateMemory allocate_memory = nullptr;
	PFN_vkGetBufferMemoryRequirements get_buffer_requirements = nullptr;
	PFN_vkGetImageMemoryRequirements get_image_requirements = nullptr;
	PFN_vkGetBufferMemoryRequirements2 get_buffer_requirements_2 = nullptr;
	PFN_vkGetBufferMemoryRequirements2 get_buffer_requirements_2_khr = nullptr;
	PFN_vkGetImageMemoryRequirements2 get_image_requirements_2 = nullptr;
	PFN_vkGetImageMemoryRequirements2 get_image_requirements_2_khr = nullptr;
	PFN_vkGetDeviceBufferMemoryRequirements get_device_buffer_requirements = nullptr;
	PFN_vkGetDeviceBufferMemoryRequirements get_device_buffer_requirements_khr = nullptr;
	PFN_vkGetDeviceImageMemoryRequirements get_device_image_requirements = nullptr;
	PFN_vkGetDeviceImageMemoryRequirements get_device_image_requirements_khr = nullptr;
};

std::mutex mutex;
// Dispatchable handles start with the loader's dispatch table, which is the
// same for an instance and its physical devices, and for a device and its
// queues and command buffers.
std::unordered_map<void *, InstanceData> instances;
std::unordered_map<void *, DeviceData> devices;
std::unordered_map<VkPhysicalDevice, Alias> aliases;

void *key(const void *p_handle) {
	return *(void **)p_handle;
}

InstanceData get_instance(const void *p_handle) {
	std::lock_guard<std::mutex> lock(mutex);
	return instances[key(p_handle)];
}

DeviceData get_device(const void *p_handle) {
	std::lock_guard<std::mutex> lock(mutex);
	return devices[key(p_handle)];
}

Alias get_alias(VkPhysicalDevice p_physical_device) {
	std::lock_guard<std::mutex> lock(mutex);
	return aliases[p_physical_device];
}

// Adds the alias to memory properties from the driver, and remembers it.
void add_alias(VkPhysicalDevice p_physical_device, VkPhysicalDeviceMemoryProperties *r_properties) {
	Alias alias;
	bool has_device_only = false;
	for (uint32_t i = 0; i < r_properties->memoryTypeCount; i++) {
		const VkMemoryPropertyFlags flags = r_properties->memoryTypes[i].propertyFlags;
		if ((flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) && !(flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)) {
			has_device_only = true;
		}
	}
	if (!has_device_only && r_properties->memoryTypeCount < VK_MAX_MEMORY_TYPES) {
		for (uint32_t i = 0; i < r_properties->memoryTypeCount; i++) {
			if (r_properties->memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) {
				alias.active = true;
				alias.source_index = i;
				alias.alias_index = r_properties->memoryTypeCount;
				break;
			}
		}
	}
	if (alias.active) {
		VkMemoryType &type = r_properties->memoryTypes[alias.alias_index];
		type.propertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		type.heapIndex = r_properties->memoryTypes[alias.source_index].heapIndex;
		r_properties->memoryTypeCount++;
	}
	std::lock_guard<std::mutex> lock(mutex);
	aliases[p_physical_device] = alias;
}

void allow_alias(VkDevice p_device, VkMemoryRequirements *r_requirements) {
	const Alias alias = get_alias(get_device(p_device).physical_device);
	if (alias.active && (r_requirements->memoryTypeBits & (1u << alias.source_index))) {
		r_requirements->memoryTypeBits |= 1u << alias.alias_index;
	}
}

VKAPI_ATTR void VKAPI_CALL get_memory_properties(VkPhysicalDevice p_physical_device, VkPhysicalDeviceMemoryProperties *r_properties) {
	get_instance(p_physical_device).get_memory_properties(p_physical_device, r_properties);
	add_alias(p_physical_device, r_properties);
}

VKAPI_ATTR void VKAPI_CALL get_memory_properties_2(VkPhysicalDevice p_physical_device, VkPhysicalDeviceMemoryProperties2 *r_properties) {
	get_instance(p_physical_device).get_memory_properties_2(p_physical_device, r_properties);
	add_alias(p_physical_device, &r_properties->memoryProperties);
}

VKAPI_ATTR void VKAPI_CALL get_memory_properties_2_khr(VkPhysicalDevice p_physical_device, VkPhysicalDeviceMemoryProperties2 *r_properties) {
	get_instance(p_physical_device).get_memory_properties_2_khr(p_physical_device, r_properties);
	add_alias(p_physical_device, &r_properties->memoryProperties);
}

VKAPI_ATTR VkResult VKAPI_CALL allocate_memory(VkDevice p_device, const VkMemoryAllocateInfo *p_info, const VkAllocationCallbacks *p_allocator, VkDeviceMemory *r_memory) {
	const DeviceData device = get_device(p_device);
	const Alias alias = get_alias(device.physical_device);
	if (alias.active && p_info->memoryTypeIndex == alias.alias_index) {
		VkMemoryAllocateInfo info = *p_info;
		info.memoryTypeIndex = alias.source_index;
		return device.allocate_memory(p_device, &info, p_allocator, r_memory);
	}
	return device.allocate_memory(p_device, p_info, p_allocator, r_memory);
}

VKAPI_ATTR void VKAPI_CALL get_buffer_requirements(VkDevice p_device, VkBuffer p_buffer, VkMemoryRequirements *r_requirements) {
	get_device(p_device).get_buffer_requirements(p_device, p_buffer, r_requirements);
	allow_alias(p_device, r_requirements);
}

VKAPI_ATTR void VKAPI_CALL get_image_requirements(VkDevice p_device, VkImage p_image, VkMemoryRequirements *r_requirements) {
	get_device(p_device).get_image_requirements(p_device, p_image, r_requirements);
	allow_alias(p_device, r_requirements);
}

VKAPI_ATTR void VKAPI_CALL get_buffer_requirements_2(VkDevice p_device, const VkBufferMemoryRequirementsInfo2 *p_info, VkMemoryRequirements2 *r_requirements) {
	get_device(p_device).get_buffer_requirements_2(p_device, p_info, r_requirements);
	allow_alias(p_device, &r_requirements->memoryRequirements);
}

VKAPI_ATTR void VKAPI_CALL get_buffer_requirements_2_khr(VkDevice p_device, const VkBufferMemoryRequirementsInfo2 *p_info, VkMemoryRequirements2 *r_requirements) {
	get_device(p_device).get_buffer_requirements_2_khr(p_device, p_info, r_requirements);
	allow_alias(p_device, &r_requirements->memoryRequirements);
}

VKAPI_ATTR void VKAPI_CALL get_image_requirements_2(VkDevice p_device, const VkImageMemoryRequirementsInfo2 *p_info, VkMemoryRequirements2 *r_requirements) {
	get_device(p_device).get_image_requirements_2(p_device, p_info, r_requirements);
	allow_alias(p_device, &r_requirements->memoryRequirements);
}

VKAPI_ATTR void VKAPI_CALL get_image_requirements_2_khr(VkDevice p_device, const VkImageMemoryRequirementsInfo2 *p_info, VkMemoryRequirements2 *r_requirements) {
	get_device(p_device).get_image_requirements_2_khr(p_device, p_info, r_requirements);
	allow_alias(p_device, &r_requirements->memoryRequirements);
}

VKAPI_ATTR void VKAPI_CALL get_device_buffer_requirements(VkDevice p_device, const VkDeviceBufferMemoryRequirements *p_info, VkMemoryRequirements2 *r_requirements) {
	get_device(p_device).get_device_buffer_requirements(p_device, p_info, r_requirements);
	allow_alias(p_device, &r_requirements->memoryRequirements);
}

VKAPI_ATTR void VKAPI_CALL get_device_buffer_requirements_khr(VkDevice p_device, const VkDeviceBufferMemoryRequirements *p_info, VkMemoryRequirements2 *r_requirements) {
	get_device(p_device).get_device_buffer_requirements_khr(p_device, p_info, r_requirements);
	allow_alias(p_device, &r_requirements->memoryRequirements);
}

VKAPI_ATTR void VKAPI_CALL get_device_image_requirements(VkDevice p_device, const VkDeviceImageMemoryRequirements *p_info, VkMemoryRequirements2 *r_requirements) {
	get_device(p_device).get_device_image_requirements(p_device, p_info, r_requirements);
	allow_alias(p_device, &r_requirements->memoryRequirements);
}

VKAPI_ATTR void VKAPI_CALL get_device_image_requirements_khr(VkDevice p_device, const VkDeviceImageMemoryRequirements *p_info, VkMemoryRequirements2 *r_requirements) {
	get_device(p_device).get_device_image_requirements_khr(p_device, p_info, r_requirements);
	allow_alias(p_device, &r_requirements->memoryRequirements);
}

VKAPI_ATTR VkResult VKAPI_CALL create_instance(const VkInstanceCreateInfo *p_info, const VkAllocationCallbacks *p_allocator, VkInstance *r_instance) {
	VkLayerInstanceCreateInfo *chain = (VkLayerInstanceCreateInfo *)p_info->pNext;
	while (chain && !(chain->sType == VK_STRUCTURE_TYPE_LOADER_INSTANCE_CREATE_INFO && chain->function == VK_LAYER_LINK_INFO)) {
		chain = (VkLayerInstanceCreateInfo *)chain->pNext;
	}
	if (chain == nullptr) {
		return VK_ERROR_INITIALIZATION_FAILED;
	}
	const PFN_vkGetInstanceProcAddr next_get_instance_proc_addr = chain->u.pLayerInfo->pfnNextGetInstanceProcAddr;
	chain->u.pLayerInfo = chain->u.pLayerInfo->pNext;

	const PFN_vkCreateInstance next_create_instance = (PFN_vkCreateInstance)next_get_instance_proc_addr(VK_NULL_HANDLE, "vkCreateInstance");
	const VkResult result = next_create_instance(p_info, p_allocator, r_instance);
	if (result != VK_SUCCESS) {
		return result;
	}

	InstanceData data;
	data.instance = *r_instance;
	data.get_instance_proc_addr = next_get_instance_proc_addr;
	data.destroy_instance = (PFN_vkDestroyInstance)next_get_instance_proc_addr(*r_instance, "vkDestroyInstance");
	data.get_memory_properties = (PFN_vkGetPhysicalDeviceMemoryProperties)next_get_instance_proc_addr(*r_instance, "vkGetPhysicalDeviceMemoryProperties");
	data.get_memory_properties_2 = (PFN_vkGetPhysicalDeviceMemoryProperties2)next_get_instance_proc_addr(*r_instance, "vkGetPhysicalDeviceMemoryProperties2");
	data.get_memory_properties_2_khr = (PFN_vkGetPhysicalDeviceMemoryProperties2)next_get_instance_proc_addr(*r_instance, "vkGetPhysicalDeviceMemoryProperties2KHR");
	std::lock_guard<std::mutex> lock(mutex);
	instances[key(*r_instance)] = data;
	return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL destroy_instance(VkInstance p_instance, const VkAllocationCallbacks *p_allocator) {
	const InstanceData data = get_instance(p_instance);
	{
		std::lock_guard<std::mutex> lock(mutex);
		instances.erase(key(p_instance));
	}
	data.destroy_instance(p_instance, p_allocator);
}

VKAPI_ATTR VkResult VKAPI_CALL create_device(VkPhysicalDevice p_physical_device, const VkDeviceCreateInfo *p_info, const VkAllocationCallbacks *p_allocator, VkDevice *r_device) {
	VkLayerDeviceCreateInfo *chain = (VkLayerDeviceCreateInfo *)p_info->pNext;
	while (chain && !(chain->sType == VK_STRUCTURE_TYPE_LOADER_DEVICE_CREATE_INFO && chain->function == VK_LAYER_LINK_INFO)) {
		chain = (VkLayerDeviceCreateInfo *)chain->pNext;
	}
	if (chain == nullptr) {
		return VK_ERROR_INITIALIZATION_FAILED;
	}
	const PFN_vkGetInstanceProcAddr next_get_instance_proc_addr = chain->u.pLayerInfo->pfnNextGetInstanceProcAddr;
	const PFN_vkGetDeviceProcAddr next_get_device_proc_addr = chain->u.pLayerInfo->pfnNextGetDeviceProcAddr;
	chain->u.pLayerInfo = chain->u.pLayerInfo->pNext;

	const InstanceData instance = get_instance(p_physical_device);
	const PFN_vkCreateDevice next_create_device = (PFN_vkCreateDevice)next_get_instance_proc_addr(instance.instance, "vkCreateDevice");
	const VkResult result = next_create_device(p_physical_device, p_info, p_allocator, r_device);
	if (result != VK_SUCCESS) {
		return result;
	}

	// Learn the alias before the program does, in case it never asks.
	if (instance.get_memory_properties) {
		VkPhysicalDeviceMemoryProperties properties = {};
		instance.get_memory_properties(p_physical_device, &properties);
		add_alias(p_physical_device, &properties);
	}

	const VkDevice device = *r_device;
	DeviceData data;
	data.physical_device = p_physical_device;
	data.get_device_proc_addr = next_get_device_proc_addr;
	data.destroy_device = (PFN_vkDestroyDevice)next_get_device_proc_addr(device, "vkDestroyDevice");
	data.allocate_memory = (PFN_vkAllocateMemory)next_get_device_proc_addr(device, "vkAllocateMemory");
	data.get_buffer_requirements = (PFN_vkGetBufferMemoryRequirements)next_get_device_proc_addr(device, "vkGetBufferMemoryRequirements");
	data.get_image_requirements = (PFN_vkGetImageMemoryRequirements)next_get_device_proc_addr(device, "vkGetImageMemoryRequirements");
	data.get_buffer_requirements_2 = (PFN_vkGetBufferMemoryRequirements2)next_get_device_proc_addr(device, "vkGetBufferMemoryRequirements2");
	data.get_buffer_requirements_2_khr = (PFN_vkGetBufferMemoryRequirements2)next_get_device_proc_addr(device, "vkGetBufferMemoryRequirements2KHR");
	data.get_image_requirements_2 = (PFN_vkGetImageMemoryRequirements2)next_get_device_proc_addr(device, "vkGetImageMemoryRequirements2");
	data.get_image_requirements_2_khr = (PFN_vkGetImageMemoryRequirements2)next_get_device_proc_addr(device, "vkGetImageMemoryRequirements2KHR");
	data.get_device_buffer_requirements = (PFN_vkGetDeviceBufferMemoryRequirements)next_get_device_proc_addr(device, "vkGetDeviceBufferMemoryRequirements");
	data.get_device_buffer_requirements_khr = (PFN_vkGetDeviceBufferMemoryRequirements)next_get_device_proc_addr(device, "vkGetDeviceBufferMemoryRequirementsKHR");
	data.get_device_image_requirements = (PFN_vkGetDeviceImageMemoryRequirements)next_get_device_proc_addr(device, "vkGetDeviceImageMemoryRequirements");
	data.get_device_image_requirements_khr = (PFN_vkGetDeviceImageMemoryRequirements)next_get_device_proc_addr(device, "vkGetDeviceImageMemoryRequirementsKHR");
	std::lock_guard<std::mutex> lock(mutex);
	devices[key(device)] = data;
	return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL destroy_device(VkDevice p_device, const VkAllocationCallbacks *p_allocator) {
	const DeviceData data = get_device(p_device);
	{
		std::lock_guard<std::mutex> lock(mutex);
		devices.erase(key(p_device));
	}
	data.destroy_device(p_device, p_allocator);
}

// Finds a device function this layer replaces. A function the driver doesn't
// have stays missing.
bool find_device_function(const DeviceData &p_device, const char *p_name, PFN_vkVoidFunction &r_function) {
	struct Entry {
		const char *name;
		PFN_vkVoidFunction function;
		PFN_vkVoidFunction next;
	};
	const Entry entries[] = {
		{ "vkDestroyDevice", (PFN_vkVoidFunction)destroy_device, (PFN_vkVoidFunction)p_device.destroy_device },
		{ "vkAllocateMemory", (PFN_vkVoidFunction)allocate_memory, (PFN_vkVoidFunction)p_device.allocate_memory },
		{ "vkGetBufferMemoryRequirements", (PFN_vkVoidFunction)get_buffer_requirements, (PFN_vkVoidFunction)p_device.get_buffer_requirements },
		{ "vkGetImageMemoryRequirements", (PFN_vkVoidFunction)get_image_requirements, (PFN_vkVoidFunction)p_device.get_image_requirements },
		{ "vkGetBufferMemoryRequirements2", (PFN_vkVoidFunction)get_buffer_requirements_2, (PFN_vkVoidFunction)p_device.get_buffer_requirements_2 },
		{ "vkGetBufferMemoryRequirements2KHR", (PFN_vkVoidFunction)get_buffer_requirements_2_khr, (PFN_vkVoidFunction)p_device.get_buffer_requirements_2_khr },
		{ "vkGetImageMemoryRequirements2", (PFN_vkVoidFunction)get_image_requirements_2, (PFN_vkVoidFunction)p_device.get_image_requirements_2 },
		{ "vkGetImageMemoryRequirements2KHR", (PFN_vkVoidFunction)get_image_requirements_2_khr, (PFN_vkVoidFunction)p_device.get_image_requirements_2_khr },
		{ "vkGetDeviceBufferMemoryRequirements", (PFN_vkVoidFunction)get_device_buffer_requirements, (PFN_vkVoidFunction)p_device.get_device_buffer_requirements },
		{ "vkGetDeviceBufferMemoryRequirementsKHR", (PFN_vkVoidFunction)get_device_buffer_requirements_khr, (PFN_vkVoidFunction)p_device.get_device_buffer_requirements_khr },
		{ "vkGetDeviceImageMemoryRequirements", (PFN_vkVoidFunction)get_device_image_requirements, (PFN_vkVoidFunction)p_device.get_device_image_requirements },
		{ "vkGetDeviceImageMemoryRequirementsKHR", (PFN_vkVoidFunction)get_device_image_requirements_khr, (PFN_vkVoidFunction)p_device.get_device_image_requirements_khr },
	};
	for (const Entry &entry : entries) {
		if (strcmp(p_name, entry.name) == 0) {
			r_function = entry.next ? entry.function : nullptr;
			return true;
		}
	}
	return false;
}

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL get_device_proc_addr(VkDevice p_device, const char *p_name) {
	if (strcmp(p_name, "vkGetDeviceProcAddr") == 0) {
		return (PFN_vkVoidFunction)get_device_proc_addr;
	}
	const DeviceData device = get_device(p_device);
	PFN_vkVoidFunction function = nullptr;
	if (find_device_function(device, p_name, function)) {
		return function;
	}
	return device.get_device_proc_addr ? device.get_device_proc_addr(p_device, p_name) : nullptr;
}

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL get_instance_proc_addr(VkInstance p_instance, const char *p_name) {
	if (strcmp(p_name, "vkGetInstanceProcAddr") == 0) {
		return (PFN_vkVoidFunction)get_instance_proc_addr;
	}
	if (strcmp(p_name, "vkCreateInstance") == 0) {
		return (PFN_vkVoidFunction)create_instance;
	}
	if (p_instance == VK_NULL_HANDLE) {
		return nullptr;
	}
	const InstanceData instance = get_instance(p_instance);
	if (strcmp(p_name, "vkDestroyInstance") == 0) {
		return (PFN_vkVoidFunction)destroy_instance;
	}
	if (strcmp(p_name, "vkCreateDevice") == 0) {
		return (PFN_vkVoidFunction)create_device;
	}
	if (strcmp(p_name, "vkGetDeviceProcAddr") == 0) {
		return (PFN_vkVoidFunction)get_device_proc_addr;
	}
	if (strcmp(p_name, "vkGetPhysicalDeviceMemoryProperties") == 0) {
		return instance.get_memory_properties ? (PFN_vkVoidFunction)get_memory_properties : nullptr;
	}
	if (strcmp(p_name, "vkGetPhysicalDeviceMemoryProperties2") == 0) {
		return instance.get_memory_properties_2 ? (PFN_vkVoidFunction)get_memory_properties_2 : nullptr;
	}
	if (strcmp(p_name, "vkGetPhysicalDeviceMemoryProperties2KHR") == 0) {
		return instance.get_memory_properties_2_khr ? (PFN_vkVoidFunction)get_memory_properties_2_khr : nullptr;
	}
	return instance.get_instance_proc_addr ? instance.get_instance_proc_addr(p_instance, p_name) : nullptr;
}

} // namespace

LAYER_EXPORT VKAPI_ATTR VkResult VKAPI_CALL vkNegotiateLoaderLayerInterfaceVersion(VkNegotiateLayerInterface *r_interface) {
	if (r_interface->sType != LAYER_NEGOTIATE_INTERFACE_STRUCT) {
		return VK_ERROR_INITIALIZATION_FAILED;
	}
	if (r_interface->loaderLayerInterfaceVersion > 2) {
		r_interface->loaderLayerInterfaceVersion = 2;
	}
	r_interface->pfnGetInstanceProcAddr = get_instance_proc_addr;
	r_interface->pfnGetDeviceProcAddr = get_device_proc_addr;
	r_interface->pfnGetPhysicalDeviceProcAddr = nullptr;
	return VK_SUCCESS;
}

LAYER_EXPORT VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetInstanceProcAddr(VkInstance p_instance, const char *p_name) {
	return get_instance_proc_addr(p_instance, p_name);
}

LAYER_EXPORT VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetDeviceProcAddr(VkDevice p_device, const char *p_name) {
	return get_device_proc_addr(p_device, p_name);
}
