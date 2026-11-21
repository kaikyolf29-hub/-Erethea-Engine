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

#if defined(WINDOWS_ENABLED) && defined(RD_ENABLED) && (defined(D3D12_ENABLED) || defined(VULKAN_ENABLED))
#define ERETHEA_XESS_ENABLED
#endif

Vector<String> XessUpscaler::get_runtime_libraries(const String &p_rendering_driver) {
	Vector<String> libraries;
	if (p_rendering_driver == "d3d12" || p_rendering_driver == "vulkan") {
		libraries.push_back(LIBRARY);
	}
	return libraries;
}

bool XessUpscaler::is_supported_by_build(const String &p_rendering_driver) {
#ifdef ERETHEA_XESS_ENABLED
#ifdef D3D12_ENABLED
	if (p_rendering_driver == "d3d12") {
		return true;
	}
#endif
#ifdef VULKAN_ENABLED
	if (p_rendering_driver == "vulkan") {
		return true;
	}
#endif
#endif
	return false;
}

#ifdef ERETHEA_XESS_ENABLED

#include "erethea_upscale_inputs.h"
#include "xess_backends.h"

#include "core/io/file_access.h"
#include "core/os/os.h"
#include "servers/rendering/renderer_rd/storage_rd/render_data_rd.h"
#include "servers/rendering/renderer_rd/storage_rd/render_scene_buffers_rd.h"
#include "servers/rendering/renderer_rd/storage_rd/render_scene_data_rd.h"
#include "servers/rendering/rendering_device.h"

namespace {

const StringName &buffers_name() {
	static const StringName name = "erethea_xess";
	return name;
}

enum class Backend {
	NONE,
	D3D12,
	VULKAN,
};

// libxess.dll is loaded at run time, so its functions are called through
// pointers of the same types as Intel's declarations. These ones don't
// depend on the rendering driver.
struct Functions {
	decltype(&xessGetVersion) get_version = nullptr;
	decltype(&xessDestroyContext) destroy_context = nullptr;
	decltype(&xessGetOptimalInputResolution) get_optimal_input_resolution = nullptr;
	decltype(&xessSetVelocityScale) set_velocity_scale = nullptr;
	decltype(&xessSetLoggingCallback) set_logging_callback = nullptr;
};

// Everything XeSS needs once the runtime is loaded. One upscaler owns it.
struct State {
	const XessUpscaler *owner = nullptr;
	bool driver_checked = false;
	Backend backend = Backend::NONE;
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

String get_library_path() {
	return OS::get_singleton()->get_executable_path().get_base_dir().path_join(XessUpscaler::LIBRARY);
}

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

const char *backend_name() {
	return state->backend == Backend::VULKAN ? "Vulkan" : "Direct3D 12";
}

bool check_driver() {
	if (state->driver_checked) {
		return state->backend != Backend::NONE;
	}
	state->driver_checked = true;
	const String driver = OS::get_singleton()->get_current_rendering_driver_name();
	if (!XessUpscaler::is_supported_by_build(driver)) {
		WARN_PRINT(vformat("XeSS needs the Direct3D 12 or Vulkan rendering driver (this one is \"%s\"). Using FSR 2.2.", driver));
		return false;
	}
#ifdef VULKAN_ENABLED
	if (driver == "vulkan") {
		String reason;
		if (!XessBackends::check_device_vulkan(reason)) {
			WARN_PRINT(vformat("XeSS can't run on Vulkan: %s. Using FSR 2.2.", reason));
			return false;
		}
		state->backend = Backend::VULKAN;
	}
#endif
#ifdef D3D12_ENABLED
	if (driver == "d3d12") {
		state->backend = Backend::D3D12;
	}
#endif
	return state->backend != Backend::NONE;
}

bool load_runtime() {
	if (state->library_tried) {
		return state->library != nullptr;
	}
	state->library_tried = true;

	const String path = get_library_path();
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
	bool found = XessBackends::load_function(library, "xessGetVersion", functions.get_version) &&
			XessBackends::load_function(library, "xessDestroyContext", functions.destroy_context) &&
			XessBackends::load_function(library, "xessGetOptimalInputResolution", functions.get_optimal_input_resolution) &&
			XessBackends::load_function(library, "xessSetVelocityScale", functions.set_velocity_scale) &&
			XessBackends::load_function(library, "xessSetLoggingCallback", functions.set_logging_callback);
#ifdef D3D12_ENABLED
	if (state->backend == Backend::D3D12) {
		found = found && XessBackends::load_d3d12(library);
	}
#endif
#ifdef VULKAN_ENABLED
	if (state->backend == Backend::VULKAN) {
		found = found && XessBackends::load_vulkan(library);
	}
#endif
	if (!found) {
		os->close_dynamic_library(library);
		WARN_PRINT(vformat("XeSS isn't available: %s is missing functions. Using FSR 2.2.", XessUpscaler::LIBRARY));
		return false;
	}

	state->library = library;
	state->functions = functions;
	return true;
}

xess_result_t create_backend_context(xess_context_handle_t *r_context) {
#ifdef D3D12_ENABLED
	if (state->backend == Backend::D3D12) {
		return XessBackends::create_context_d3d12(r_context);
	}
#endif
#ifdef VULKAN_ENABLED
	if (state->backend == Backend::VULKAN) {
		return XessBackends::create_context_vulkan(r_context);
	}
#endif
	return XESS_RESULT_ERROR_NOT_IMPLEMENTED;
}

xess_result_t init_backend_context(xess_context_handle_t p_context, const Size2i &p_output_size, xess_quality_settings_t p_quality, uint32_t p_flags) {
#ifdef D3D12_ENABLED
	if (state->backend == Backend::D3D12) {
		return XessBackends::init_d3d12(p_context, p_output_size, p_quality, p_flags);
	}
#endif
#ifdef VULKAN_ENABLED
	if (state->backend == Backend::VULKAN) {
		return XessBackends::init_vulkan(p_context, p_output_size, p_quality, p_flags);
	}
#endif
	return XESS_RESULT_ERROR_NOT_IMPLEMENTED;
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
	xess_context_handle_t context = nullptr;
	// Positive results are warnings, such as an old graphics driver.
	xess_result_t result = create_backend_context(&context);
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

	// Godot's 3D buffers hold linear HDR color, reversed depth and motion
	// vectors at the internal resolution. XeSS finds the exposure itself,
	// like Godot's FSR 2.2 without auto exposure.
	result = init_backend_context(context, p_target_size, quality, XESS_INIT_FLAG_INVERTED_DEPTH | XESS_INIT_FLAG_ENABLE_AUTOEXPOSURE);
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
		print_line(vformat("Erethea: upscaling with XeSS %d.%d.%d (%s).", version.major, version.minor, version.patch, backend_name()));
	}
	return context;
}

} // namespace

// XeSS doesn't change the textures' layouts: it reads the inputs as shader
// resources and writes the output as a storage image, which are the layouts
// the rendering graph gives them for these usages. It doesn't synchronize
// with other work on them either; the rendering graph does.
void XessBackends::add_callback(RenderingDeviceDriver::DriverCallback p_callback, void *p_userdata, const Dispatch &p_dispatch) {
	RD::CallbackResource resources[4];
	resources[0].rid = p_dispatch.color;
	resources[0].usage = RD::CALLBACK_RESOURCE_USAGE_TEXTURE_SAMPLE;
	resources[1].rid = p_dispatch.velocity;
	resources[1].usage = RD::CALLBACK_RESOURCE_USAGE_TEXTURE_SAMPLE;
	resources[2].rid = p_dispatch.depth;
	resources[2].usage = RD::CALLBACK_RESOURCE_USAGE_TEXTURE_SAMPLE;
	resources[3].rid = p_dispatch.output;
	resources[3].usage = RD::CALLBACK_RESOURCE_USAGE_STORAGE_IMAGE_READ_WRITE;
	RenderingDevice::get_singleton()->driver_callback_add(p_callback, p_userdata, VectorView<RD::CallbackResource>(resources, 4));
}

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

void XessUpscaler::prepare_rendering_driver() {
#ifdef VULKAN_ENABLED
	const String path = get_library_path();
	if (!FileAccess::exists(path)) {
		return;
	}
	void *library = nullptr;
	if (OS::get_singleton()->open_dynamic_library(path, library) != OK) {
		return;
	}
	// The runtime stays loaded, as Vulkan devices may keep using it.
	XessBackends::add_requirements_vulkan(library);
#endif
}

void XessUpscaler::finish_rendering_driver() {
#ifdef VULKAN_ENABLED
	XessBackends::remove_requirements_vulkan();
#endif
}

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
	rd->draw_command_begin_label("XeSS");
	// XeSS reads the depth and motion vectors from typed textures, with the
	// motion of the sky filled in. Godot's reactive mask is left out: XeSS
	// only recommends its mask where it clearly helps.
	const EretheaUpscaleInputs::Textures inputs = state->inputs.prepare(rb, p_render_data, buffers_name());

	XessBackends::Dispatch dispatch;
	dispatch.color = rb->get_internal_texture(0);
	dispatch.velocity = inputs.velocity;
	dispatch.depth = inputs.depth;
	dispatch.output = rb->get_upscaled_texture(0);
	dispatch.input_size = internal_size;
	// The same jitter as Godot's FSR 2.2: in pixels, in the convention XeSS
	// shares with FSR.
	dispatch.jitter = p_render_data->scene_data->taa_jitter * Vector2(internal_size) * 0.5f;
	dispatch.reset = buffers->reset;
	buffers->reset = false;

#ifdef D3D12_ENABLED
	if (state->backend == Backend::D3D12) {
		XessBackends::add_dispatch_d3d12(buffers->context, dispatch);
	}
#endif
#ifdef VULKAN_ENABLED
	if (state->backend == Backend::VULKAN) {
		XessBackends::add_dispatch_vulkan(buffers->context, dispatch);
	}
#endif
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

void XessUpscaler::prepare_rendering_driver() {}

void XessUpscaler::finish_rendering_driver() {}

bool XessUpscaler::upscale(const Ref<RenderSceneBuffersRD> &p_render_buffers, RenderDataRD *p_render_data, double p_time_step) {
	WARN_PRINT_ONCE("XeSS is only available on Windows, with the Direct3D 12 or Vulkan rendering driver. Using FSR 2.2.");
	return false;
}

void XessUpscaler::release(const Ref<RenderSceneBuffersRD> &p_render_buffers) {}

void XessUpscaler::finish() {}

#endif // ERETHEA_XESS_ENABLED

XessUpscaler::~XessUpscaler() {
	finish();
}
