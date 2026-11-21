/**************************************************************************/
/*  xess_upscaler.cpp                                                     */
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

#include "xess_upscaler.h"

#if defined(WINDOWS_ENABLED) && defined(RD_ENABLED) && defined(D3D12_ENABLED)
#define ERETHEA_XESS_ENABLED
#endif

Vector<String> XessUpscaler::get_runtime_libraries(const String &p_rendering_driver) {
	Vector<String> libraries;
	if (p_rendering_driver == "d3d12") {
		libraries.push_back(LIBRARY);
	}
	return libraries;
}

bool XessUpscaler::is_supported_by_build(const String &p_rendering_driver) {
#ifdef ERETHEA_XESS_ENABLED
	return p_rendering_driver == "d3d12";
#else
	return false;
#endif
}

#ifdef ERETHEA_XESS_ENABLED

#include "erethea_upscale_inputs.h"

#include "core/io/file_access.h"
#include "core/os/os.h"
#include "servers/rendering/renderer_rd/storage_rd/render_data_rd.h"
#include "servers/rendering/renderer_rd/storage_rd/render_scene_buffers_rd.h"
#include "servers/rendering/renderer_rd/storage_rd/render_scene_data_rd.h"
#include "servers/rendering/rendering_device.h"

// The driver's header brings the DirectX headers Godot builds with, which
// Intel's header then uses.
#include "drivers/d3d12/rendering_device_driver_d3d12.h"

#include <thirdparty/xess/xess_d3d12.h>

namespace {

const StringName &buffers_name() {
	static const StringName name = "erethea_xess";
	return name;
}

// libxess.dll is loaded at run time, so its functions are called through
// pointers of the same types as Intel's declarations.
struct Functions {
	decltype(&xessGetVersion) get_version = nullptr;
	decltype(&xessD3D12CreateContext) create_context = nullptr;
	decltype(&xessD3D12Init) init = nullptr;
	decltype(&xessD3D12Execute) execute = nullptr;
	decltype(&xessDestroyContext) destroy_context = nullptr;
	decltype(&xessGetOptimalInputResolution) get_optimal_input_resolution = nullptr;
	decltype(&xessSetVelocityScale) set_velocity_scale = nullptr;
	decltype(&xessSetLoggingCallback) set_logging_callback = nullptr;
};

// Everything XeSS needs once the runtime is loaded. One upscaler owns it.
struct State {
	const XessUpscaler *owner = nullptr;
	bool driver_checked = false;
	bool driver_supported = false;
	void *library = nullptr;
	bool library_tried = false;
	bool failed = false;
	bool announced = false;

	Functions functions;
	EretheaUpscaleInputs inputs;

	// XeSS contexts waiting for the frames that use them to finish on the GPU.
	struct PendingContext {
		xess_context_handle_t context = nullptr;
		uint64_t frame = 0;
	};
	LocalVector<PendingContext> pending;
};

State *state = nullptr;

// Arguments of one upscale, read when the rendering graph records it.
struct DispatchArgs {
	decltype(&xessD3D12Execute) execute = nullptr;
	xess_context_handle_t context = nullptr;
	xess_d3d12_execute_params_t params = {};
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
			state->functions.destroy_context(pending.context);
			state->pending.remove_at_unordered(i);
		} else {
			i++;
		}
	}
}

// Called by XeSS, possibly from other threads.
void log_callback(const char *p_message, xess_logging_level_t p_level) {
	if (p_level == XESS_LOGGING_LEVEL_ERROR) {
		ERR_PRINT(vformat("XeSS: %s", String::utf8(p_message)));
	} else if (p_level == XESS_LOGGING_LEVEL_WARNING) {
		WARN_PRINT(vformat("XeSS: %s", String::utf8(p_message)));
	} else {
		print_verbose(vformat("XeSS: %s", String::utf8(p_message)));
	}
}

String result_name(xess_result_t p_result) {
	switch (p_result) {
		case XESS_RESULT_ERROR_UNSUPPORTED_DEVICE:
			return "the graphics card isn't supported, it needs Shader Model 6.4";
		case XESS_RESULT_ERROR_UNSUPPORTED_DRIVER:
			return "the graphics driver isn't supported";
		case XESS_RESULT_ERROR_DEVICE_OUT_OF_MEMORY:
			return "not enough video memory";
		case XESS_RESULT_ERROR_CANT_LOAD_LIBRARY:
			return "a library couldn't be loaded";
		default:
			return vformat("error %d", int(p_result));
	}
}

bool check_driver() {
	if (state->driver_checked) {
		return state->driver_supported;
	}
	state->driver_checked = true;
	const String driver = OS::get_singleton()->get_current_rendering_driver_name();
	if (driver == "vulkan") {
		WARN_PRINT("XeSS doesn't support the Vulkan rendering driver in Erethea yet. Use the Direct3D 12 rendering driver. Using FSR 2.2.");
		return false;
	}
	if (!XessUpscaler::is_supported_by_build(driver)) {
		WARN_PRINT(vformat("XeSS needs the Direct3D 12 rendering driver (this one is \"%s\"). Using FSR 2.2.", driver));
		return false;
	}
	state->driver_supported = true;
	return true;
}

bool load_runtime() {
	if (state->library_tried) {
		return state->library != nullptr;
	}
	state->library_tried = true;

	const String path = OS::get_singleton()->get_executable_path().get_base_dir().path_join(XessUpscaler::LIBRARY);
	if (!FileAccess::exists(path)) {
		WARN_PRINT(vformat("XeSS isn't available: %s isn't next to the executable. Using FSR 2.2.", XessUpscaler::LIBRARY));
		return false;
	}

	OS *os = OS::get_singleton();
	void *library = nullptr;
	if (os->open_dynamic_library(path, library) != OK) {
		WARN_PRINT(vformat("XeSS isn't available: %s couldn't be loaded. Using FSR 2.2.", XessUpscaler::LIBRARY));
		return false;
	}

	Functions functions;
	bool found = true;
#define LOAD_FUNCTION(m_member, m_name) \
	{ \
		void *symbol = nullptr; \
		found = found && os->get_dynamic_library_symbol_handle(library, m_name, symbol) == OK; \
		functions.m_member = (decltype(functions.m_member))symbol; \
	}
	LOAD_FUNCTION(get_version, "xessGetVersion");
	LOAD_FUNCTION(create_context, "xessD3D12CreateContext");
	LOAD_FUNCTION(init, "xessD3D12Init");
	LOAD_FUNCTION(execute, "xessD3D12Execute");
	LOAD_FUNCTION(destroy_context, "xessDestroyContext");
	LOAD_FUNCTION(get_optimal_input_resolution, "xessGetOptimalInputResolution");
	LOAD_FUNCTION(set_velocity_scale, "xessSetVelocityScale");
	LOAD_FUNCTION(set_logging_callback, "xessSetLoggingCallback");
#undef LOAD_FUNCTION
	if (!found) {
		os->close_dynamic_library(library);
		WARN_PRINT(vformat("XeSS isn't available: %s is missing functions. Using FSR 2.2.", XessUpscaler::LIBRARY));
		return false;
	}

	state->library = library;
	state->functions = functions;
	return true;
}

// XeSS takes any input resolution within the range of a quality preset, so
// the preset is the first whose range holds the resolution Godot renders at.
bool choose_quality(xess_context_handle_t p_context, const Size2i &p_internal_size, const Size2i &p_target_size, xess_quality_settings_t &r_quality) {
	static const xess_quality_settings_t qualities[] = {
		XESS_QUALITY_SETTING_AA,
		XESS_QUALITY_SETTING_ULTRA_QUALITY_PLUS,
		XESS_QUALITY_SETTING_ULTRA_QUALITY,
		XESS_QUALITY_SETTING_QUALITY,
		XESS_QUALITY_SETTING_BALANCED,
		XESS_QUALITY_SETTING_PERFORMANCE,
		XESS_QUALITY_SETTING_ULTRA_PERFORMANCE,
	};
	const xess_2d_t output = { uint32_t(p_target_size.width), uint32_t(p_target_size.height) };
	for (const xess_quality_settings_t quality : qualities) {
		xess_2d_t optimal = {};
		xess_2d_t min = {};
		xess_2d_t max = {};
		if (state->functions.get_optimal_input_resolution(p_context, &output, quality, &optimal, &min, &max) != XESS_RESULT_SUCCESS) {
			continue;
		}
		if (uint32_t(p_internal_size.width) >= min.x && uint32_t(p_internal_size.width) <= max.x && uint32_t(p_internal_size.height) >= min.y && uint32_t(p_internal_size.height) <= max.y) {
			r_quality = quality;
			return true;
		}
	}
	return false;
}

// Returns nullptr when XeSS can't run. r_unsupported_size tells that it only
// can't upscale between these sizes.
xess_context_handle_t create_context(const Size2i &p_internal_size, const Size2i &p_target_size, bool &r_unsupported_size) {
	RenderingDevice *rd = RenderingDevice::get_singleton();
	ID3D12Device *device = (ID3D12Device *)rd->get_driver_resource(RD::DRIVER_RESOURCE_LOGICAL_DEVICE);

	xess_context_handle_t context = nullptr;
	// Positive results are warnings, such as an old graphics driver.
	xess_result_t result = state->functions.create_context(device, &context);
	if (result < XESS_RESULT_SUCCESS || context == nullptr) {
		ERR_PRINT(vformat("XeSS couldn't start (%s). Using FSR 2.2.", result_name(result)));
		state->failed = true;
		return nullptr;
	}
	state->functions.set_logging_callback(context, OS::get_singleton()->is_stdout_verbose() ? XESS_LOGGING_LEVEL_DEBUG : XESS_LOGGING_LEVEL_WARNING, log_callback);

	xess_quality_settings_t quality = XESS_QUALITY_SETTING_QUALITY;
	if (!choose_quality(context, p_internal_size, p_target_size, quality)) {
		// Godot renders at a scale XeSS doesn't support (below 1/3).
		WARN_PRINT(vformat("XeSS can't upscale from %dx%d to %dx%d: the 3D resolution scale is too low. Using FSR 2.2.", p_internal_size.width, p_internal_size.height, p_target_size.width, p_target_size.height));
		state->functions.destroy_context(context);
		r_unsupported_size = true;
		return nullptr;
	}

	xess_d3d12_init_params_t params = {};
	params.outputResolution.x = p_target_size.width;
	params.outputResolution.y = p_target_size.height;
	params.qualitySetting = quality;
	// Godot's 3D buffers hold linear HDR color, reversed depth and motion
	// vectors at the internal resolution. XeSS finds the exposure itself,
	// like Godot's FSR 2.2 without auto exposure.
	params.initFlags = XESS_INIT_FLAG_INVERTED_DEPTH | XESS_INIT_FLAG_ENABLE_AUTOEXPOSURE;
	result = state->functions.init(context, &params);
	if (result < XESS_RESULT_SUCCESS) {
		ERR_PRINT(vformat("XeSS couldn't start (%s). Using FSR 2.2.", result_name(result)));
		state->functions.destroy_context(context);
		state->failed = true;
		return nullptr;
	}

	// The motion vectors are in UV units; XeSS wants pixels.
	state->functions.set_velocity_scale(context, float(p_internal_size.width), float(p_internal_size.height));

	if (!state->announced) {
		state->announced = true;
		xess_version_t version = {};
		state->functions.get_version(&version);
		print_line(vformat("Erethea: upscaling with XeSS %d.%d.%d (Direct3D 12).", version.major, version.minor, version.patch));
	}
	return context;
}

void dispatch_callback(RenderingDeviceDriver *p_driver, RenderingDeviceDriver::CommandBufferID p_command_buffer, void *p_userdata) {
	DispatchArgs *args = (DispatchArgs *)p_userdata;
	// XeSS binds its own pipelines and descriptor heap, so Godot forgets the
	// ones it bound.
	ID3D12GraphicsCommandList *command_list = (ID3D12GraphicsCommandList *)RenderingDeviceDriverD3D12::command_buffer_begin_native_commands(p_command_buffer);
	const xess_result_t result = args->execute(args->context, command_list, &args->params);
	if (result < XESS_RESULT_SUCCESS) {
		ERR_PRINT_ONCE(vformat("XeSS couldn't upscale (%s).", result_name(result)));
	}
	memdelete(args);
}

} // namespace

// The XeSS context of one set of render buffers. Rebuilt when they change
// size, since XeSS keeps its history at that size.
class EretheaXessBuffers : public RenderBufferCustomDataRD {
	GDCLASS(EretheaXessBuffers, RenderBufferCustomDataRD);

public:
	xess_context_handle_t context = nullptr;
	bool reset = true;
	// XeSS can't upscale between the current sizes.
	bool unsupported_size = false;

	void release() {
		if (context != nullptr && state != nullptr) {
			State::PendingContext pending;
			pending.context = context;
			pending.frame = RenderingDevice::get_singleton()->get_frames_drawn();
			state->pending.push_back(pending);
		}
		context = nullptr;
	}

	virtual void configure(RenderSceneBuffersRD *p_render_buffers) override {
		release();
		unsupported_size = false;
	}
	virtual void free_data() override { release(); }

	~EretheaXessBuffers() { release(); }
};

bool XessUpscaler::upscale(const Ref<RenderSceneBuffersRD> &p_render_buffers, RenderDataRD *p_render_data, double p_time_step) {
	if (state == nullptr) {
		state = memnew(State);
		state->owner = this;
	}
	destroy_pending_contexts(false);
	if (state->owner != this || state->failed || !check_driver()) {
		return false;
	}

	Ref<RenderSceneBuffersRD> rb = p_render_buffers;
	ERR_FAIL_COND_V(rb.is_null() || p_render_data == nullptr || p_render_data->scene_data == nullptr, false);
	if (rb->get_view_count() != 1) {
		WARN_PRINT_ONCE("XeSS doesn't support more than one view (XR) yet. Using FSR 2.2.");
		return false;
	}
	if (!rb->has_velocity_buffer(false) || !rb->has_upscaled_texture()) {
		return false;
	}

	if (!load_runtime()) {
		return false;
	}

	Ref<EretheaXessBuffers> buffers;
	if (rb->has_custom_data(buffers_name())) {
		buffers = rb->get_custom_data(buffers_name());
	} else {
		buffers.instantiate();
		rb->set_custom_data(buffers_name(), buffers);
	}

	const Size2i internal_size = rb->get_internal_size();
	const Size2i target_size = rb->get_target_size();
	if (buffers->unsupported_size) {
		return false;
	}
	if (buffers->context == nullptr) {
		buffers->context = create_context(internal_size, target_size, buffers->unsupported_size);
		if (buffers->context == nullptr) {
			return false;
		}
		buffers->reset = true;
	}

	RenderingDevice *rd = RenderingDevice::get_singleton();
	const RID color = rb->get_internal_texture(0);
	const RID output = rb->get_upscaled_texture(0);

	rd->draw_command_begin_label("XeSS");
	// XeSS reads the depth and motion vectors from typed textures, with the
	// motion of the sky filled in. Godot's reactive mask is left out: XeSS
	// only recommends its mask where it clearly helps.
	const EretheaUpscaleInputs::Textures inputs = state->inputs.prepare(rb, p_render_data, buffers_name());

	DispatchArgs *args = memnew(DispatchArgs);
	args->execute = state->functions.execute;
	args->context = buffers->context;

	xess_d3d12_execute_params_t &params = args->params;
	params.pColorTexture = (ID3D12Resource *)rd->get_driver_resource(RD::DRIVER_RESOURCE_TEXTURE, color);
	params.pVelocityTexture = (ID3D12Resource *)rd->get_driver_resource(RD::DRIVER_RESOURCE_TEXTURE, inputs.velocity);
	params.pDepthTexture = (ID3D12Resource *)rd->get_driver_resource(RD::DRIVER_RESOURCE_TEXTURE, inputs.depth);
	params.pOutputTexture = (ID3D12Resource *)rd->get_driver_resource(RD::DRIVER_RESOURCE_TEXTURE, output);

	// The same jitter as Godot's FSR 2.2: in pixels, in the convention XeSS
	// shares with FSR.
	const Vector2 jitter = p_render_data->scene_data->taa_jitter * Vector2(internal_size) * 0.5f;
	params.jitterOffsetX = jitter.x;
	params.jitterOffsetY = jitter.y;
	params.exposureScale = 1.0f;
	params.resetHistory = buffers->reset ? 1 : 0;
	buffers->reset = false;
	params.inputWidth = internal_size.width;
	params.inputHeight = internal_size.height;

	// XeSS doesn't change the textures' states: it reads the inputs as
	// shader resources and writes the output as an unordered access view,
	// which are the layouts the rendering graph gives them for these usages.
	RD::CallbackResource resources[4];
	resources[0].rid = color;
	resources[0].usage = RD::CALLBACK_RESOURCE_USAGE_TEXTURE_SAMPLE;
	resources[1].rid = inputs.velocity;
	resources[1].usage = RD::CALLBACK_RESOURCE_USAGE_TEXTURE_SAMPLE;
	resources[2].rid = inputs.depth;
	resources[2].usage = RD::CALLBACK_RESOURCE_USAGE_TEXTURE_SAMPLE;
	resources[3].rid = output;
	resources[3].usage = RD::CALLBACK_RESOURCE_USAGE_STORAGE_IMAGE_READ_WRITE;
	rd->driver_callback_add(&dispatch_callback, args, VectorView<RD::CallbackResource>(resources, 4));
	rd->draw_command_end_label();
	return true;
}

void XessUpscaler::release(const Ref<RenderSceneBuffersRD> &p_render_buffers) {
	if (state == nullptr || state->owner != this || p_render_buffers.is_null() || !p_render_buffers->has_custom_data(buffers_name())) {
		return;
	}
	Ref<EretheaXessBuffers> buffers = p_render_buffers->get_custom_data(buffers_name());
	if (buffers.is_valid()) {
		buffers->release();
	}
	destroy_pending_contexts(false);
}

void XessUpscaler::finish() {
	if (state == nullptr || state->owner != this) {
		return;
	}
	// Contexts that the GPU may still use leak instead: the program is
	// closing. The runtime stays loaded for the same reason.
	destroy_pending_contexts(false);
	state->inputs.finish();
	memdelete(state);
	state = nullptr;
}

#else // ERETHEA_XESS_ENABLED

bool XessUpscaler::upscale(const Ref<RenderSceneBuffersRD> &p_render_buffers, RenderDataRD *p_render_data, double p_time_step) {
	WARN_PRINT_ONCE("XeSS is only available on Windows, with the Direct3D 12 rendering driver. Using FSR 2.2.");
	return false;
}

void XessUpscaler::release(const Ref<RenderSceneBuffersRD> &p_render_buffers) {}

void XessUpscaler::finish() {}

#endif // ERETHEA_XESS_ENABLED

XessUpscaler::~XessUpscaler() {
	finish();
}
