/**************************************************************************/
/*  amd_fsr_upscaler.cpp                                                  */
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

#include "amd_fsr_upscaler.h"

#if defined(WINDOWS_ENABLED) && defined(VULKAN_ENABLED) && defined(RD_ENABLED)
#define ERETHEA_AMD_FSR_ENABLED
#endif

#ifdef ERETHEA_AMD_FSR_ENABLED

#include "api/include/vk/ffx_api_vk.h"
#include "erethea_upscale_inputs.glsl.gen.h"
#include "upscalers/include/ffx_upscale.h"

#include "core/io/file_access.h"
#include "core/os/os.h"
#include "drivers/vulkan/rendering_device_driver_vulkan.h"
#include "servers/rendering/renderer_rd/storage_rd/material_storage.h"
#include "servers/rendering/renderer_rd/storage_rd/render_data_rd.h"
#include "servers/rendering/renderer_rd/storage_rd/render_scene_buffers_rd.h"
#include "servers/rendering/renderer_rd/storage_rd/render_scene_data_rd.h"
#include "servers/rendering/renderer_rd/uniform_set_cache_rd.h"
#include "servers/rendering/rendering_device.h"

namespace {

const char *const LOADER_LIBRARY = AmdFsrUpscaler::RUNTIME_LIBRARIES[0];

const StringName &buffers_name() {
	static const StringName name = "erethea_amd_fsr";
	return name;
}

const StringName &depth_name() {
	static const StringName name = "depth";
	return name;
}

const StringName &velocity_name() {
	static const StringName name = "velocity";
	return name;
}

const StringName &reactive_name() {
	static const StringName name = "reactive";
	return name;
}

struct PushConstant {
	float reprojection[16];
	int32_t size[2];
	float pad[2];
};

// Everything AMD FSR needs once the runtime is loaded. One upscaler owns it.
struct State {
	const AmdFsrUpscaler *owner = nullptr;
	void *library = nullptr;
	bool library_tried = false;
	bool failed = false;

	PfnFfxCreateContext create_context = nullptr;
	PfnFfxDestroyContext destroy_context = nullptr;
	PfnFfxQuery query = nullptr;
	PfnFfxDispatch dispatch = nullptr;

	EretheaUpscaleInputsShaderRD shader;
	RID shader_version;
	RID pipeline;

	// FSR contexts waiting for the frames that use them to finish on the GPU.
	struct PendingContext {
		ffxContext context = nullptr;
		uint64_t frame = 0;
	};
	LocalVector<PendingContext> pending;

	String provider;
};

State *state = nullptr;

// Arguments of one upscale, read when the rendering graph records it.
struct DispatchArgs {
	ffxContext context = nullptr;
	PfnFfxDispatch dispatch = nullptr;
	ffxDispatchDescUpscale desc = {};
};

void destroy_pending_contexts(bool p_all_finished) {
	if (state == nullptr || state->pending.is_empty()) {
		return;
	}
	RenderingDevice *rd = RenderingDevice::get_singleton();
	const uint64_t frame = rd->get_frames_drawn();
	const uint64_t delay = rd->get_frame_delay() + 1;
	for (uint32_t i = 0; i < state->pending.size();) {
		State::PendingContext &pending = state->pending[i];
		if (p_all_finished || frame > pending.frame + delay) {
			state->destroy_context(&pending.context, nullptr);
			state->pending.remove_at_unordered(i);
		} else {
			i++;
		}
	}
}

void message_callback(uint32_t p_type, const wchar_t *p_message) {
	if (p_type == FFX_API_MESSAGE_TYPE_ERROR) {
		ERR_PRINT(vformat("AMD FSR: %s", String(p_message)));
	} else {
		WARN_PRINT(vformat("AMD FSR: %s", String(p_message)));
	}
}

bool load_runtime() {
	if (state->library_tried) {
		return state->library != nullptr;
	}
	state->library_tried = true;

	const String path = OS::get_singleton()->get_executable_path().get_base_dir().path_join(LOADER_LIBRARY);
	if (!FileAccess::exists(path)) {
		WARN_PRINT(vformat("AMD FSR isn't available: %s isn't next to the executable. Using FSR 2.2.", LOADER_LIBRARY));
		return false;
	}

	void *library = nullptr;
	if (OS::get_singleton()->open_dynamic_library(path, library) != OK) {
		WARN_PRINT(vformat("AMD FSR isn't available: %s couldn't be loaded. Using FSR 2.2.", LOADER_LIBRARY));
		return false;
	}

	void *create_context = nullptr;
	void *destroy_context = nullptr;
	void *query = nullptr;
	void *dispatch = nullptr;
	OS *os = OS::get_singleton();
	if (os->get_dynamic_library_symbol_handle(library, "ffxCreateContext", create_context) != OK ||
			os->get_dynamic_library_symbol_handle(library, "ffxDestroyContext", destroy_context) != OK ||
			os->get_dynamic_library_symbol_handle(library, "ffxQuery", query) != OK ||
			os->get_dynamic_library_symbol_handle(library, "ffxDispatch", dispatch) != OK) {
		os->close_dynamic_library(library);
		WARN_PRINT(vformat("AMD FSR isn't available: %s is missing functions. Using FSR 2.2.", LOADER_LIBRARY));
		return false;
	}

	state->library = library;
	state->create_context = (PfnFfxCreateContext)create_context;
	state->destroy_context = (PfnFfxDestroyContext)destroy_context;
	state->query = (PfnFfxQuery)query;
	state->dispatch = (PfnFfxDispatch)dispatch;
	return true;
}

ffxContext create_context(const Size2i &p_internal_size, const Size2i &p_target_size) {
	RenderingDevice *rd = RenderingDevice::get_singleton();

	ffxCreateBackendVKDesc backend = {};
	backend.header.type = FFX_API_CREATE_CONTEXT_DESC_TYPE_BACKEND_VK;
	backend.deviceContext.vkDevice = (VkDevice)rd->get_driver_resource(RD::DRIVER_RESOURCE_LOGICAL_DEVICE);
	backend.deviceContext.vkPhysicalDevice = (VkPhysicalDevice)rd->get_driver_resource(RD::DRIVER_RESOURCE_PHYSICAL_DEVICE);
	backend.deviceContext.vkDeviceProcAddr = vkGetDeviceProcAddr;

	ffxCreateContextDescUpscaleVersion version = {};
	version.header.type = FFX_API_CREATE_CONTEXT_DESC_TYPE_UPSCALE_VERSION;
	version.header.pNext = &backend.header;
	version.version = FFX_UPSCALER_VERSION;

	ffxCreateContextDescUpscale desc = {};
	desc.header.type = FFX_API_CREATE_CONTEXT_DESC_TYPE_UPSCALE;
	desc.header.pNext = &version.header;
	// Godot's 3D buffers hold linear HDR color and reversed depth. FSR finds
	// the exposure itself, like Godot's FSR 2.2 without auto exposure.
	desc.flags = FFX_UPSCALE_ENABLE_HIGH_DYNAMIC_RANGE | FFX_UPSCALE_ENABLE_DEPTH_INVERTED | FFX_UPSCALE_ENABLE_AUTO_EXPOSURE;
	desc.maxRenderSize.width = p_internal_size.width;
	desc.maxRenderSize.height = p_internal_size.height;
	desc.maxUpscaleSize.width = p_target_size.width;
	desc.maxUpscaleSize.height = p_target_size.height;
	desc.fpMessage = message_callback;

	ffxContext context = nullptr;
	const ffxReturnCode_t result = state->create_context(&context, &desc.header, nullptr);
	if (result != FFX_API_RETURN_OK || context == nullptr) {
		ERR_PRINT(vformat("AMD FSR couldn't start (error %d). Using FSR 2.2.", int(result)));
		state->failed = true;
		return nullptr;
	}

	// Say once which upscaler the runtime chose for this graphics card.
	ffxQueryGetProviderVersion provider = {};
	provider.header.type = FFX_API_QUERY_DESC_TYPE_GET_PROVIDER_VERSION;
	if (state->query(&context, &provider.header) == FFX_API_RETURN_OK && provider.versionName != nullptr) {
		const String name = String::utf8(provider.versionName);
		if (name != state->provider) {
			state->provider = name;
			print_line(vformat("Erethea: upscaling with AMD FSR %s.", name));
		}
	}
	return context;
}

uint32_t surface_format(RD::DataFormat p_format) {
	switch (p_format) {
		case RD::DATA_FORMAT_R16G16B16A16_SFLOAT:
			return FFX_API_SURFACE_FORMAT_R16G16B16A16_FLOAT;
		case RD::DATA_FORMAT_R32G32B32A32_SFLOAT:
			return FFX_API_SURFACE_FORMAT_R32G32B32A32_FLOAT;
		case RD::DATA_FORMAT_B10G11R11_UFLOAT_PACK32:
			return FFX_API_SURFACE_FORMAT_R11G11B10_FLOAT;
		case RD::DATA_FORMAT_A2B10G10R10_UNORM_PACK32:
			return FFX_API_SURFACE_FORMAT_R10G10B10A2_UNORM;
		case RD::DATA_FORMAT_R8G8B8A8_UNORM:
			return FFX_API_SURFACE_FORMAT_R8G8B8A8_UNORM;
		case RD::DATA_FORMAT_R16G16_SFLOAT:
			return FFX_API_SURFACE_FORMAT_R16G16_FLOAT;
		case RD::DATA_FORMAT_R32_SFLOAT:
			return FFX_API_SURFACE_FORMAT_R32_FLOAT;
		case RD::DATA_FORMAT_R8_UNORM:
			return FFX_API_SURFACE_FORMAT_R8_UNORM;
		default:
			return FFX_API_SURFACE_FORMAT_UNKNOWN;
	}
}

// Describes a 2D texture of the rendering device for FSR. p_state must match
// the layout that the rendering graph gives the texture for the callback.
FfxApiResource make_resource(RID p_texture, uint32_t p_state) {
	RenderingDevice *rd = RenderingDevice::get_singleton();
	const RD::TextureFormat format = rd->texture_get_format(p_texture);

	FfxApiResource resource = {};
	resource.resource = (void *)(uintptr_t)rd->get_driver_resource(RD::DRIVER_RESOURCE_TEXTURE, p_texture);
	resource.description.type = FFX_API_RESOURCE_TYPE_TEXTURE2D;
	resource.description.format = surface_format(format.format);
	resource.description.width = format.width;
	resource.description.height = format.height;
	resource.description.depth = 1;
	resource.description.mipCount = 1;
	resource.description.flags = FFX_API_RESOURCE_FLAGS_NONE;
	resource.description.usage = p_state == FFX_API_RESOURCE_STATE_UNORDERED_ACCESS ? FFX_API_RESOURCE_USAGE_UAV : FFX_API_RESOURCE_USAGE_READ_ONLY;
	resource.state = p_state;
	return resource;
}

void dispatch_callback(RenderingDeviceDriver *p_driver, RenderingDeviceDriver::CommandBufferID p_command_buffer, void *p_userdata) {
	DispatchArgs *args = (DispatchArgs *)p_userdata;
	args->desc.commandList = RenderingDeviceDriverVulkan::command_buffer_get_vk_handle(p_command_buffer);
	const ffxReturnCode_t result = args->dispatch(&args->context, &args->desc.header);
	if (result != FFX_API_RETURN_OK) {
		ERR_PRINT_ONCE(vformat("AMD FSR couldn't upscale (error %d).", int(result)));
	}
	memdelete(args);
}

} // namespace

// The FSR context of one set of render buffers. Rebuilt when they change
// size, since FSR keeps its history at that size.
class EretheaAmdFsrBuffers : public RenderBufferCustomDataRD {
	GDCLASS(EretheaAmdFsrBuffers, RenderBufferCustomDataRD);

public:
	ffxContext context = nullptr;
	bool reset = true;

	void release() {
		if (context != nullptr && state != nullptr) {
			State::PendingContext pending;
			pending.context = context;
			pending.frame = RenderingDevice::get_singleton()->get_frames_drawn();
			state->pending.push_back(pending);
		}
		context = nullptr;
	}

	virtual void configure(RenderSceneBuffersRD *p_render_buffers) override { release(); }
	virtual void free_data() override { release(); }

	~EretheaAmdFsrBuffers() { release(); }
};

bool AmdFsrUpscaler::upscale(const Ref<RenderSceneBuffersRD> &p_render_buffers, RenderDataRD *p_render_data, double p_time_step) {
	if (state == nullptr) {
		state = memnew(State);
		state->owner = this;
	}
	destroy_pending_contexts(false);
	if (state->owner != this || state->failed) {
		return false;
	}

	if (OS::get_singleton()->get_current_rendering_driver_name() != "vulkan") {
		WARN_PRINT_ONCE("AMD FSR needs the Vulkan rendering driver. Using FSR 2.2.");
		return false;
	}

	Ref<RenderSceneBuffersRD> rb = p_render_buffers;
	ERR_FAIL_COND_V(rb.is_null() || p_render_data == nullptr || p_render_data->scene_data == nullptr, false);
	if (rb->get_view_count() != 1) {
		WARN_PRINT_ONCE("AMD FSR doesn't support more than one view (XR) yet. Using FSR 2.2.");
		return false;
	}
	if (!rb->has_velocity_buffer(false) || !rb->has_upscaled_texture()) {
		return false;
	}

	const RID color = rb->get_internal_texture(0);
	const RID output = rb->get_upscaled_texture(0);
	RenderingDevice *rd = RenderingDevice::get_singleton();
	if (surface_format(rd->texture_get_format(color).format) == FFX_API_SURFACE_FORMAT_UNKNOWN || surface_format(rd->texture_get_format(output).format) == FFX_API_SURFACE_FORMAT_UNKNOWN) {
		WARN_PRINT_ONCE("AMD FSR doesn't support the format of the 3D buffers. Using FSR 2.2.");
		return false;
	}

	if (!load_runtime()) {
		return false;
	}

	Ref<EretheaAmdFsrBuffers> buffers;
	if (rb->has_custom_data(buffers_name())) {
		buffers = rb->get_custom_data(buffers_name());
	} else {
		buffers.instantiate();
		rb->set_custom_data(buffers_name(), buffers);
	}

	const Size2i internal_size = rb->get_internal_size();
	const Size2i target_size = rb->get_target_size();
	if (buffers->context == nullptr) {
		buffers->context = create_context(internal_size, target_size);
		if (buffers->context == nullptr) {
			return false;
		}
		buffers->reset = true;
	}

	if (state->shader_version.is_null()) {
		state->shader.initialize(Vector<String>({ "" }));
		state->shader_version = state->shader.version_create();
		state->pipeline = rd->compute_pipeline_create(state->shader.version_get_shader(state->shader_version, 0));
	}

	// FSR reads the depth, motion vectors and reactive mask from textures of
	// its own formats. Copying them also fills in the motion of the sky, which
	// Godot leaves to the upscaler, as its FSR 2.2 does.
	const uint32_t usage = RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_STORAGE_BIT;
	if (!rb->has_texture(buffers_name(), depth_name())) {
		rb->create_texture(buffers_name(), depth_name(), RD::DATA_FORMAT_R32_SFLOAT, usage, RD::TEXTURE_SAMPLES_1, internal_size, 1);
		rb->create_texture(buffers_name(), velocity_name(), RD::DATA_FORMAT_R16G16_SFLOAT, usage, RD::TEXTURE_SAMPLES_1, internal_size, 1);
		rb->create_texture(buffers_name(), reactive_name(), RD::DATA_FORMAT_R8_UNORM, usage, RD::TEXTURE_SAMPLES_1, internal_size, 1);
	}
	const RID depth = rb->get_texture(buffers_name(), depth_name());
	const RID velocity = rb->get_texture(buffers_name(), velocity_name());
	const RID reactive = rb->get_texture(buffers_name(), reactive_name());

	const RenderSceneDataRD *scene_data = p_render_data->scene_data;

	PushConstant push_constant = {};
	push_constant.size[0] = internal_size.width;
	push_constant.size[1] = internal_size.height;
	{
		// The same reprojection as Godot's FSR 2.2.
		Projection correction;
		correction.set_depth_correction(true, true, false);
		const Projection reprojection = (correction * scene_data->prev_cam_projection) * scene_data->prev_cam_transform.affine_inverse() * scene_data->cam_transform * (correction * scene_data->cam_projection).inverse();
		RendererRD::MaterialStorage::store_camera(reprojection, push_constant.reprojection);
	}

	UniformSetCacheRD *uniform_set_cache = UniformSetCacheRD::get_singleton();
	RendererRD::MaterialStorage *material_storage = RendererRD::MaterialStorage::get_singleton();
	const RID nearest_sampler = material_storage->sampler_rd_get_default(RSE::CANVAS_ITEM_TEXTURE_FILTER_NEAREST, RSE::CANVAS_ITEM_TEXTURE_REPEAT_DISABLED);
	const RID shader = state->shader.version_get_shader(state->shader_version, 0);

	RD::Uniform u_color(RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE, 0, Vector<RID>({ nearest_sampler, color }));
	RD::Uniform u_depth(RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE, 1, Vector<RID>({ nearest_sampler, rb->get_depth_texture(0) }));
	RD::Uniform u_velocity(RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE, 2, Vector<RID>({ nearest_sampler, rb->get_velocity_buffer(false, 0) }));
	RD::Uniform u_dest_depth(RD::UNIFORM_TYPE_IMAGE, 0, Vector<RID>({ depth }));
	RD::Uniform u_dest_velocity(RD::UNIFORM_TYPE_IMAGE, 1, Vector<RID>({ velocity }));
	RD::Uniform u_dest_reactive(RD::UNIFORM_TYPE_IMAGE, 2, Vector<RID>({ reactive }));

	rd->draw_command_begin_label("AMD FSR");
	RD::ComputeListID compute_list = rd->compute_list_begin();
	rd->compute_list_bind_compute_pipeline(compute_list, state->pipeline);
	rd->compute_list_bind_uniform_set(compute_list, uniform_set_cache->get_cache(shader, 0, u_color, u_depth, u_velocity), 0);
	rd->compute_list_bind_uniform_set(compute_list, uniform_set_cache->get_cache(shader, 1, u_dest_depth, u_dest_velocity, u_dest_reactive), 1);
	rd->compute_list_set_push_constant(compute_list, &push_constant, sizeof(PushConstant));
	rd->compute_list_dispatch_threads(compute_list, internal_size.width, internal_size.height, 1);
	rd->compute_list_end();

	DispatchArgs *args = memnew(DispatchArgs);
	args->context = buffers->context;
	args->dispatch = state->dispatch;

	ffxDispatchDescUpscale &desc = args->desc;
	desc.header.type = FFX_API_DISPATCH_DESC_TYPE_UPSCALE;
	desc.color = make_resource(color, FFX_API_RESOURCE_STATE_COMPUTE_READ);
	desc.depth = make_resource(depth, FFX_API_RESOURCE_STATE_COMPUTE_READ);
	desc.motionVectors = make_resource(velocity, FFX_API_RESOURCE_STATE_COMPUTE_READ);
	desc.reactive = make_resource(reactive, FFX_API_RESOURCE_STATE_COMPUTE_READ);
	desc.output = make_resource(output, FFX_API_RESOURCE_STATE_UNORDERED_ACCESS);

	// The same conventions as Godot's FSR 2.2: jitter in pixels, motion
	// vectors in UV units pointing to the previous frame.
	const Vector2 jitter = scene_data->taa_jitter * Vector2(internal_size) * 0.5f;
	desc.jitterOffset.x = jitter.x;
	desc.jitterOffset.y = jitter.y;
	desc.motionVectorScale.x = float(internal_size.width);
	desc.motionVectorScale.y = float(internal_size.height);
	desc.renderSize.width = internal_size.width;
	desc.renderSize.height = internal_size.height;
	desc.upscaleSize.width = target_size.width;
	desc.upscaleSize.height = target_size.height;

	const float sharpness = CLAMP(1.0f - (rb->get_fsr_sharpness() / 2.0f), 0.0f, 1.0f);
	desc.enableSharpening = sharpness > 1e-6f;
	desc.sharpness = sharpness;
	desc.frameTimeDelta = float(p_time_step * 1000.0);
	desc.preExposure = 1.0f;
	desc.reset = buffers->reset;
	buffers->reset = false;

	const real_t fov = scene_data->cam_projection.get_fov();
	const real_t aspect = scene_data->cam_projection.get_aspect();
	desc.cameraNear = scene_data->z_near;
	desc.cameraFar = scene_data->z_far;
	desc.cameraFovAngleVertical = Math::deg_to_rad(scene_data->cam_projection.get_fovy(fov, 1.0 / aspect));
	desc.viewSpaceToMetersFactor = 1.0f;

	RD::CallbackResource resources[5];
	resources[0].rid = color;
	resources[0].usage = RD::CALLBACK_RESOURCE_USAGE_TEXTURE_SAMPLE;
	resources[1].rid = depth;
	resources[1].usage = RD::CALLBACK_RESOURCE_USAGE_TEXTURE_SAMPLE;
	resources[2].rid = velocity;
	resources[2].usage = RD::CALLBACK_RESOURCE_USAGE_TEXTURE_SAMPLE;
	resources[3].rid = reactive;
	resources[3].usage = RD::CALLBACK_RESOURCE_USAGE_TEXTURE_SAMPLE;
	resources[4].rid = output;
	resources[4].usage = RD::CALLBACK_RESOURCE_USAGE_STORAGE_IMAGE_READ_WRITE;
	rd->driver_callback_add(&dispatch_callback, args, VectorView<RD::CallbackResource>(resources, 5));
	rd->draw_command_end_label();
	return true;
}

void AmdFsrUpscaler::release(const Ref<RenderSceneBuffersRD> &p_render_buffers) {
	if (state == nullptr || state->owner != this || p_render_buffers.is_null() || !p_render_buffers->has_custom_data(buffers_name())) {
		return;
	}
	Ref<EretheaAmdFsrBuffers> buffers = p_render_buffers->get_custom_data(buffers_name());
	if (buffers.is_valid()) {
		buffers->release();
	}
	destroy_pending_contexts(false);
}

void AmdFsrUpscaler::finish() {
	if (state == nullptr || state->owner != this) {
		return;
	}
	// Contexts that the GPU may still use leak instead: the program is
	// closing. The runtime stays loaded for the same reason.
	destroy_pending_contexts(false);
	if (state->shader_version.is_valid()) {
		state->shader.version_free(state->shader_version);
	}
	memdelete(state);
	state = nullptr;
}

bool AmdFsrUpscaler::is_supported_by_build() {
	return true;
}

#else // ERETHEA_AMD_FSR_ENABLED

bool AmdFsrUpscaler::upscale(const Ref<RenderSceneBuffersRD> &p_render_buffers, RenderDataRD *p_render_data, double p_time_step) {
	WARN_PRINT_ONCE("AMD FSR is only available on Windows with Vulkan. Using FSR 2.2.");
	return false;
}

void AmdFsrUpscaler::release(const Ref<RenderSceneBuffersRD> &p_render_buffers) {}

void AmdFsrUpscaler::finish() {}

bool AmdFsrUpscaler::is_supported_by_build() {
	return false;
}

#endif // ERETHEA_AMD_FSR_ENABLED

AmdFsrUpscaler::~AmdFsrUpscaler() {
	finish();
}
