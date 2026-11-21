/**************************************************************************/
/*  dlss_upscaler.cpp                                                     */
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

#include "dlss_upscaler.h"

#if defined(WINDOWS_ENABLED) && defined(RD_ENABLED) && defined(D3D12_ENABLED)
#define ERETHEA_DLSS_ENABLED
#endif

Vector<String> DlssUpscaler::get_runtime_libraries(const String &p_rendering_driver) {
	Vector<String> libraries;
	if (p_rendering_driver == "d3d12") {
		libraries.push_back(LIBRARY);
		libraries.push_back("sl.common.dll");
		libraries.push_back("sl.dlss.dll");
		libraries.push_back("nvngx_dlss.dll");
	}
	return libraries;
}

bool DlssUpscaler::is_supported_by_build(const String &p_rendering_driver) {
#ifdef ERETHEA_DLSS_ENABLED
	return p_rendering_driver == "d3d12";
#else
	return false;
#endif
}

#ifdef ERETHEA_DLSS_ENABLED

#include "erethea_upscale_inputs.h"

#include "core/config/project_settings.h"
#include "core/io/file_access.h"
#include "core/object/callable_mp.h"
#include "core/os/os.h"
#include "core/version.h"
#include "servers/rendering/renderer_rd/storage_rd/render_data_rd.h"
#include "servers/rendering/renderer_rd/storage_rd/render_scene_buffers_rd.h"
#include "servers/rendering/renderer_rd/storage_rd/render_scene_data_rd.h"
#include "servers/rendering/rendering_device.h"
#include "servers/rendering/rendering_server.h"

// The driver's header brings the Windows and DirectX headers Godot builds with.
#include "drivers/d3d12/rendering_device_driver_d3d12.h"

// NVIDIA's headers, as they are.
GODOT_GCC_WARNING_PUSH
GODOT_GCC_WARNING_IGNORE("-Wdeprecated-declarations")
GODOT_GCC_WARNING_IGNORE("-Wnon-virtual-dtor")
GODOT_GCC_WARNING_IGNORE("-Wshadow")
GODOT_CLANG_WARNING_PUSH
GODOT_CLANG_WARNING_IGNORE("-Wdeprecated-declarations")
GODOT_CLANG_WARNING_IGNORE("-Wnon-virtual-dtor")
GODOT_CLANG_WARNING_IGNORE("-Wshadow")
GODOT_MSVC_WARNING_PUSH_AND_IGNORE(4458 4996)
#include <thirdparty/streamline/sl.h>
#include <thirdparty/streamline/sl_dlss.h>
GODOT_MSVC_WARNING_POP
GODOT_CLANG_WARNING_POP
GODOT_GCC_WARNING_POP

#include <softpub.h>
#include <wintrust.h>

namespace {

const StringName &buffers_name() {
	static const StringName name = "erethea_dlss";
	return name;
}

// sl.interposer.dll is loaded at run time, so its functions are called
// through pointers of the same types as NVIDIA's declarations.
struct Functions {
	decltype(&slInit) init = nullptr;
	decltype(&slShutdown) shutdown = nullptr;
	decltype(&slIsFeatureSupported) is_feature_supported = nullptr;
	decltype(&slSetD3DDevice) set_d3d_device = nullptr;
	decltype(&slGetFeatureFunction) get_feature_function = nullptr;
	decltype(&slGetNewFrameToken) get_new_frame_token = nullptr;
	decltype(&slSetConstants) set_constants = nullptr;
	decltype(&slSetTagForFrame) set_tag_for_frame = nullptr;
	decltype(&slEvaluateFeature) evaluate_feature = nullptr;
	decltype(&slFreeResources) free_resources = nullptr;
	PFun_slDLSSGetOptimalSettings *get_optimal_settings = nullptr;
	PFun_slDLSSSetOptions *set_options = nullptr;
};

// Streamline, started before the rendering device is created and shut down
// after the upscaler is freed.
struct Runtime {
	void *library = nullptr;
	Functions functions;
	// Why DLSS can't run, found while starting Streamline.
	String failure;
	// What slInit was given; kept alive while Streamline runs.
	Char16String plugin_path;
	CharString project_id;
	CharString engine_version;
};

Runtime *runtime = nullptr;

// Everything DLSS needs on the rendering thread. One upscaler owns it.
struct State {
	const DlssUpscaler *owner = nullptr;
	bool checked = false;
	bool supported = false;
	bool announced = false;
	uint32_t next_viewport = 1;

	EretheaUpscaleInputs inputs;

	// Viewports whose DLSS resources wait for the frames that use them to
	// finish on the GPU.
	struct PendingViewport {
		uint32_t viewport = 0;
		uint64_t frame = 0;
	};
	LocalVector<PendingViewport> pending;
};

State *state = nullptr;

template <typename T>
bool load_function(void *p_library, const char *p_name, T &r_function) {
	void *symbol = nullptr;
	if (OS::get_singleton()->get_dynamic_library_symbol_handle(p_library, p_name, symbol) != OK) {
		return false;
	}
	r_function = (T)symbol;
	return true;
}

String result_name(sl::Result p_result) {
	switch (p_result) {
		case sl::Result::eErrorNoSupportedAdapterFound:
		case sl::Result::eErrorAdapterNotSupported:
			return "the graphics card doesn't support DLSS, it needs an NVIDIA RTX card";
		case sl::Result::eErrorFeatureNotSupported:
			return "this computer doesn't support DLSS, it needs an NVIDIA RTX card and a recent NVIDIA driver";
		case sl::Result::eErrorDriverOutOfDate:
			return "the NVIDIA graphics driver is too old";
		case sl::Result::eErrorOSOutOfDate:
			return "Windows is too old";
		case sl::Result::eErrorNoPlugins:
			// Streamline's files are all there (prepare_rendering_driver()
			// checks), but none of its plugins supports this computer: it
			// found no NVIDIA graphics card.
			return "this computer doesn't support DLSS, it needs an NVIDIA RTX card and a recent NVIDIA driver";
		case sl::Result::eErrorFeatureMissing:
		case sl::Result::eErrorFeatureFailedToLoad:
		case sl::Result::eErrorFeatureMissingDependency:
			return "sl.common.dll, sl.dlss.dll or nvngx_dlss.dll is missing or didn't load";
		default:
			return vformat("Streamline error %d", int(p_result));
	}
}

// Whether a file carries a valid code signature, as NVIDIA asks to check
// before loading sl.interposer.dll.
bool is_signed(const String &p_path) {
	HMODULE wintrust = LoadLibraryW(L"wintrust.dll");
	if (wintrust == nullptr) {
		return false;
	}
	typedef LONG(WINAPI * PFN_WinVerifyTrust)(HWND, GUID *, LPVOID);
	PFN_WinVerifyTrust verify = (PFN_WinVerifyTrust)(void *)GetProcAddress(wintrust, "WinVerifyTrust");
	bool valid = false;
	if (verify != nullptr) {
		const Char16String path = p_path.replace("/", "\\").utf16();
		WINTRUST_FILE_INFO file = {};
		file.cbStruct = sizeof(file);
		file.pcwszFilePath = (LPCWSTR)path.get_data();
		WINTRUST_DATA data = {};
		data.cbStruct = sizeof(data);
		data.dwUIChoice = WTD_UI_NONE;
		data.fdwRevocationChecks = WTD_REVOKE_NONE;
		data.dwUnionChoice = WTD_CHOICE_FILE;
		data.pFile = &file;
		data.dwStateAction = WTD_STATEACTION_VERIFY;
		data.dwProvFlags = WTD_CACHE_ONLY_URL_RETRIEVAL;
		GUID action = WINTRUST_ACTION_GENERIC_VERIFY_V2;
		valid = verify(nullptr, &action, &data) == ERROR_SUCCESS;
		data.dwStateAction = WTD_STATEACTION_CLOSE;
		verify(nullptr, &action, &data);
	}
	FreeLibrary(wintrust);
	return valid;
}

// Called by Streamline, possibly from other threads.
void log_callback(sl::LogType p_type, const char *p_message) {
	const String message = String::utf8(p_message).strip_edges();
	if (p_type == sl::LogType::eError) {
		ERR_PRINT(vformat("DLSS: %s", message));
	} else if (p_type == sl::LogType::eWarn) {
		print_verbose(vformat("DLSS warning: %s", message));
	} else {
		print_verbose(vformat("DLSS: %s", message));
	}
}

// NVIDIA asks engines without an application ID for a GUID per project.
CharString make_project_id() {
	const String name = GLOBAL_GET("application/config/name");
	const String hash = (String("erethea:") + name).md5_text();
	return vformat("%s-%s-%s-%s-%s", hash.substr(0, 8), hash.substr(8, 4), hash.substr(12, 4), hash.substr(16, 4), hash.substr(20, 12)).utf8();
}

void free_pending_viewports(bool p_all_finished) {
	if (state == nullptr || state->pending.is_empty() || runtime == nullptr) {
		return;
	}
	RenderingDevice *rd = RenderingDevice::get_singleton();
	const uint64_t frame = rd->get_frames_drawn();
	const uint64_t delay = rd->get_frame_delay() + 1;
	for (uint32_t i = 0; i < state->pending.size();) {
		const State::PendingViewport &pending = state->pending[i];
		if (p_all_finished || frame > pending.frame + delay) {
			runtime->functions.free_resources(sl::kFeatureDLSS, sl::ViewportHandle(pending.viewport));
			state->pending.remove_at_unordered(i);
		} else {
			i++;
		}
	}
}

// Hands Godot's device to Streamline and asks whether it can run DLSS. Only
// done once, on the first frame that upscales with DLSS.
bool check_support() {
	if (state->checked) {
		return state->supported;
	}
	state->checked = true;

	const String driver = OS::get_singleton()->get_current_rendering_driver_name();
	if (driver != "d3d12") {
		WARN_PRINT(vformat("DLSS needs the Direct3D 12 rendering driver in Erethea for now (this one is \"%s\"). Using FSR 2.2.", driver));
		return false;
	}
	if (runtime == nullptr) {
		WARN_PRINT("DLSS has to be chosen before Erethea starts, to start NVIDIA's Streamline before the graphics device. Restart to use it. Using FSR 2.2.");
		return false;
	}
	if (!runtime->failure.is_empty()) {
		WARN_PRINT(vformat("DLSS isn't available: %s. Using FSR 2.2.", runtime->failure));
		return false;
	}

	RenderingDevice *rd = RenderingDevice::get_singleton();
	ID3D12Device *device = (ID3D12Device *)rd->get_driver_resource(RD::DRIVER_RESOURCE_LOGICAL_DEVICE);
	const Functions &functions = runtime->functions;
	sl::Result result = functions.set_d3d_device(device);
	if (result != sl::Result::eOk) {
		WARN_PRINT(vformat("DLSS isn't available: %s. Using FSR 2.2.", result_name(result)));
		return false;
	}

	LUID luid;
#if defined(_MSC_VER) || !defined(_WIN32)
	luid = device->GetAdapterLuid();
#else
	device->GetAdapterLuid(&luid);
#endif
	sl::AdapterInfo adapter;
	adapter.deviceLUID = (uint8_t *)&luid;
	adapter.deviceLUIDSizeInBytes = sizeof(LUID);
	result = functions.is_feature_supported(sl::kFeatureDLSS, adapter);
	if (result != sl::Result::eOk) {
		WARN_PRINT(vformat("DLSS isn't available: %s. Using FSR 2.2.", result_name(result)));
		return false;
	}

	Functions &loaded = runtime->functions;
	if (loaded.get_feature_function(sl::kFeatureDLSS, "slDLSSGetOptimalSettings", (void *&)loaded.get_optimal_settings) != sl::Result::eOk ||
			loaded.get_feature_function(sl::kFeatureDLSS, "slDLSSSetOptions", (void *&)loaded.set_options) != sl::Result::eOk) {
		WARN_PRINT("DLSS isn't available: sl.dlss.dll is missing functions. Using FSR 2.2.");
		return false;
	}

	state->supported = true;
	return true;
}

// DLSS takes any input resolution within the range of a mode, so the mode
// is the first whose range holds the resolution Godot renders at.
bool choose_mode(const Size2i &p_internal_size, const Size2i &p_target_size, sl::DLSSMode &r_mode) {
	static const sl::DLSSMode modes[] = {
		sl::DLSSMode::eDLAA,
		sl::DLSSMode::eMaxQuality,
		sl::DLSSMode::eBalanced,
		sl::DLSSMode::eMaxPerformance,
		sl::DLSSMode::eUltraPerformance,
	};
	for (const sl::DLSSMode mode : modes) {
		sl::DLSSOptions options;
		options.mode = mode;
		options.outputWidth = p_target_size.width;
		options.outputHeight = p_target_size.height;
		sl::DLSSOptimalSettings settings;
		if (runtime->functions.get_optimal_settings(options, settings) != sl::Result::eOk) {
			continue;
		}
		if (uint32_t(p_internal_size.width) >= settings.renderWidthMin && uint32_t(p_internal_size.width) <= settings.renderWidthMax && uint32_t(p_internal_size.height) >= settings.renderHeightMin && uint32_t(p_internal_size.height) <= settings.renderHeightMax) {
			r_mode = mode;
			return true;
		}
	}
	return false;
}

// Streamline's matrices are row major for row vectors, so each row is a
// column of Godot's.
sl::float4x4 to_sl(const Projection &p_matrix) {
	sl::float4x4 matrix;
	for (int i = 0; i < 4; i++) {
		matrix.setRow(i, sl::float4(p_matrix.columns[i].x, p_matrix.columns[i].y, p_matrix.columns[i].z, p_matrix.columns[i].w));
	}
	return matrix;
}

sl::float3 to_sl(const Vector3 &p_vector) {
	return sl::float3(p_vector.x, p_vector.y, p_vector.z);
}

// Arguments of one upscale, read when the rendering graph records it.
struct DispatchArgs {
	sl::FrameToken *frame = nullptr;
	uint32_t viewport = 0;
	ID3D12Resource *color = nullptr;
	ID3D12Resource *depth = nullptr;
	ID3D12Resource *velocity = nullptr;
	ID3D12Resource *output = nullptr;
	Size2i input_size;
	Size2i output_size;
};

void dispatch_callback(RenderingDeviceDriver *p_driver, RenderingDeviceDriver::CommandBufferID p_command_buffer, void *p_userdata) {
	DispatchArgs *args = (DispatchArgs *)p_userdata;
	// DLSS binds its own pipelines and descriptor heap, so Godot forgets the
	// ones it bound.
	ID3D12GraphicsCommandList *command_list = (ID3D12GraphicsCommandList *)RenderingDeviceDriverD3D12::command_buffer_begin_native_commands(p_command_buffer);

	// The inputs are shader resources and the output an unordered access
	// view, the layouts the rendering graph gives them for these usages.
	const uint32_t read_state = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
	sl::Resource color(sl::ResourceType::eTex2d, args->color, read_state);
	sl::Resource depth(sl::ResourceType::eTex2d, args->depth, read_state);
	sl::Resource velocity(sl::ResourceType::eTex2d, args->velocity, read_state);
	sl::Resource output(sl::ResourceType::eTex2d, args->output, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	sl::Extent input_extent;
	input_extent.width = args->input_size.width;
	input_extent.height = args->input_size.height;
	sl::Extent output_extent;
	output_extent.width = args->output_size.width;
	output_extent.height = args->output_size.height;
	const sl::ResourceTag tags[] = {
		sl::ResourceTag(&color, sl::kBufferTypeScalingInputColor, sl::ResourceLifecycle::eValidUntilEvaluate, &input_extent),
		sl::ResourceTag(&depth, sl::kBufferTypeDepth, sl::ResourceLifecycle::eValidUntilEvaluate, &input_extent),
		sl::ResourceTag(&velocity, sl::kBufferTypeMotionVectors, sl::ResourceLifecycle::eValidUntilEvaluate, &input_extent),
		sl::ResourceTag(&output, sl::kBufferTypeScalingOutputColor, sl::ResourceLifecycle::eValidUntilEvaluate, &output_extent),
	};

	const Functions &functions = runtime->functions;
	const sl::ViewportHandle viewport(args->viewport);
	sl::Result result = functions.set_tag_for_frame(*args->frame, viewport, tags, 4, command_list);
	if (result == sl::Result::eOk) {
		const sl::BaseStructure *inputs[] = { &viewport };
		result = functions.evaluate_feature(sl::kFeatureDLSS, *args->frame, inputs, 1, command_list);
	}
	if (result != sl::Result::eOk) {
		ERR_PRINT_ONCE(vformat("DLSS couldn't upscale (%s).", result_name(result)));
	}
	memdelete(args);
}

void shutdown_runtime() {
	if (runtime == nullptr) {
		return;
	}
	if (runtime->library != nullptr && runtime->failure.is_empty()) {
		runtime->functions.shutdown();
	}
	// The library stays loaded: Streamline's threads may still be finishing.
	memdelete(runtime);
	runtime = nullptr;
}

} // namespace

// The DLSS viewport of one set of render buffers. DLSS keeps its history per
// viewport at the sizes it last saw, so a new one starts when they change.
class EretheaDlssBuffers : public RenderBufferCustomDataRD {
	GDCLASS(EretheaDlssBuffers, RenderBufferCustomDataRD);

public:
	uint32_t viewport = 0;
	bool reset = true;
	// DLSS can't upscale between the current sizes.
	bool unsupported_size = false;
	sl::DLSSMode mode = sl::DLSSMode::eOff;

	void release() {
		if (viewport != 0 && state != nullptr) {
			State::PendingViewport pending;
			pending.viewport = viewport;
			pending.frame = RenderingDevice::get_singleton()->get_frames_drawn();
			state->pending.push_back(pending);
		}
		viewport = 0;
	}

	virtual void configure(RenderSceneBuffersRD *p_render_buffers) override {
		release();
		unsupported_size = false;
	}
	virtual void free_data() override { release(); }

	~EretheaDlssBuffers() { release(); }
};

void DlssUpscaler::prepare_rendering_driver() {
	if (runtime != nullptr) {
		return;
	}
	runtime = memnew(Runtime);

	if (OS::get_singleton()->get_current_rendering_driver_name() != "d3d12") {
		// check_support() tells why later, when DLSS would upscale.
		runtime->failure = "it needs the Direct3D 12 rendering driver in Erethea for now";
		return;
	}

	const String folder = OS::get_singleton()->get_executable_path().get_base_dir();
	for (const String &library : get_runtime_libraries("d3d12")) {
		if (!FileAccess::exists(folder.path_join(library))) {
			runtime->failure = vformat("%s isn't next to the executable. Erethea doesn't include NVIDIA's DLSS files, see \"DLSS\" in Erethea's README", library);
			return;
		}
	}
	const String path = folder.path_join(LIBRARY);
	if (!is_signed(path)) {
		runtime->failure = vformat("%s isn't signed by NVIDIA", LIBRARY);
		return;
	}

	OS *os = OS::get_singleton();
	void *library = nullptr;
	if (os->open_dynamic_library(path, library) != OK) {
		runtime->failure = vformat("%s couldn't be loaded", LIBRARY);
		return;
	}
	Functions &functions = runtime->functions;
	if (!load_function(library, "slInit", functions.init) ||
			!load_function(library, "slShutdown", functions.shutdown) ||
			!load_function(library, "slIsFeatureSupported", functions.is_feature_supported) ||
			!load_function(library, "slSetD3DDevice", functions.set_d3d_device) ||
			!load_function(library, "slGetFeatureFunction", functions.get_feature_function) ||
			!load_function(library, "slGetNewFrameToken", functions.get_new_frame_token) ||
			!load_function(library, "slSetConstants", functions.set_constants) ||
			!load_function(library, "slSetTagForFrame", functions.set_tag_for_frame) ||
			!load_function(library, "slEvaluateFeature", functions.evaluate_feature) ||
			!load_function(library, "slFreeResources", functions.free_resources)) {
		os->close_dynamic_library(library);
		runtime->failure = vformat("%s is missing functions", LIBRARY);
		return;
	}
	runtime->library = library;

	runtime->plugin_path = folder.replace("/", "\\").utf16();
	runtime->project_id = make_project_id();
	runtime->engine_version = String(GODOT_VERSION_NUMBER).utf8();
	const wchar_t *paths[] = { (const wchar_t *)runtime->plugin_path.get_data() };
	const sl::Feature features[] = { sl::kFeatureDLSS };

	sl::Preferences preferences;
	preferences.logLevel = OS::get_singleton()->is_stdout_verbose() ? sl::LogLevel::eVerbose : sl::LogLevel::eDefault;
	preferences.logMessageCallback = log_callback;
	preferences.pathsToPlugins = paths;
	preferences.numPathsToPlugins = 1;
	// Erethea creates the device itself, and Streamline doesn't download
	// updates from the internet.
	preferences.flags = sl::PreferenceFlags::eDisableCLStateTracking | sl::PreferenceFlags::eUseManualHooking;
	preferences.featuresToLoad = features;
	preferences.numFeaturesToLoad = 1;
	preferences.engine = sl::EngineType::eCustom;
	preferences.engineVersion = runtime->engine_version.get_data();
	preferences.projectId = runtime->project_id.get_data();
	preferences.renderAPI = sl::RenderAPI::eD3D12;
	const sl::Result result = functions.init(preferences, sl::kSDKVersion);
	if (result != sl::Result::eOk) {
		runtime->failure = vformat("NVIDIA's Streamline couldn't start (%s)", result_name(result));
	}
}

void DlssUpscaler::finish_rendering_driver() {
	RenderingServer *rs = RenderingServer::get_singleton();
	if (rs != nullptr) {
		// After the upscaler frees its resources on the rendering thread.
		rs->call_on_render_thread(callable_mp_static(&shutdown_runtime));
	} else {
		shutdown_runtime();
	}
}

bool DlssUpscaler::upscale(const Ref<RenderSceneBuffersRD> &p_render_buffers, RenderDataRD *p_render_data, double p_time_step) {
	if (state == nullptr) {
		state = memnew(State);
		state->owner = this;
	}
	free_pending_viewports(false);
	if (state->owner != this || !check_support()) {
		return false;
	}

	Ref<RenderSceneBuffersRD> rb = p_render_buffers;
	ERR_FAIL_COND_V(rb.is_null() || p_render_data == nullptr || p_render_data->scene_data == nullptr, false);
	if (rb->get_view_count() != 1) {
		WARN_PRINT_ONCE("DLSS doesn't support more than one view (XR) yet. Using FSR 2.2.");
		return false;
	}
	if (!rb->has_velocity_buffer(false) || !rb->has_upscaled_texture()) {
		return false;
	}

	Ref<EretheaDlssBuffers> buffers;
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
	const Functions &functions = runtime->functions;
	if (buffers->viewport == 0) {
		sl::DLSSMode mode = sl::DLSSMode::eOff;
		if (!choose_mode(internal_size, target_size, mode)) {
			WARN_PRINT(vformat("DLSS can't upscale from %dx%d to %dx%d: the 3D resolution scale is too low. Using FSR 2.2.", internal_size.width, internal_size.height, target_size.width, target_size.height));
			buffers->unsupported_size = true;
			return false;
		}
		buffers->viewport = state->next_viewport++;
		buffers->mode = mode;
		buffers->reset = true;

		sl::DLSSOptions options;
		options.mode = mode;
		options.outputWidth = target_size.width;
		options.outputHeight = target_size.height;
		// Godot's 3D buffers hold linear HDR color. DLSS finds the exposure
		// itself, like Godot's FSR 2.2 without auto exposure.
		options.colorBuffersHDR = sl::Boolean::eTrue;
		options.useAutoExposure = sl::Boolean::eTrue;
		const sl::Result result = functions.set_options(sl::ViewportHandle(buffers->viewport), options);
		if (result != sl::Result::eOk) {
			WARN_PRINT(vformat("DLSS couldn't start (%s). Using FSR 2.2.", result_name(result)));
			buffers->release();
			buffers->unsupported_size = true;
			return false;
		}
		if (!state->announced) {
			state->announced = true;
			print_line("Erethea: upscaling with DLSS (Direct3D 12).");
		}
	}

	RenderingDevice *rd = RenderingDevice::get_singleton();
	sl::FrameToken *frame = nullptr;
	const uint32_t frame_index = uint32_t(rd->get_frames_drawn());
	if (functions.get_new_frame_token(frame, &frame_index) != sl::Result::eOk || frame == nullptr) {
		ERR_PRINT_ONCE("DLSS couldn't get a frame from Streamline. Using FSR 2.2.");
		return false;
	}

	// The camera of this frame and the last one, as Streamline wants them:
	// without jitter, and with the depth correction Godot renders with.
	const RenderSceneDataRD *scene_data = p_render_data->scene_data;
	Projection correction;
	correction.set_depth_correction(true);
	const Projection view_to_clip = correction * scene_data->cam_projection;
	const Projection prev_view_to_clip = correction * scene_data->prev_cam_projection;
	const Projection clip_to_prev_clip = prev_view_to_clip * Projection(scene_data->prev_cam_transform.affine_inverse() * scene_data->cam_transform) * view_to_clip.inverse();

	sl::Constants constants;
	constants.cameraViewToClip = to_sl(view_to_clip);
	constants.clipToCameraView = to_sl(view_to_clip.inverse());
	constants.clipToLensClip = to_sl(Projection());
	constants.clipToPrevClip = to_sl(clip_to_prev_clip);
	constants.prevClipToClip = to_sl(clip_to_prev_clip.inverse());
	// The same jitter as Godot's FSR 2.2, in pixels.
	const Vector2 jitter = scene_data->taa_jitter * Vector2(internal_size) * 0.5f;
	constants.jitterOffset = sl::float2(jitter.x, jitter.y);
	// The motion vectors are already in UV units.
	constants.mvecScale = sl::float2(1.0f, 1.0f);
	constants.cameraPinholeOffset = sl::float2(0.0f, 0.0f);
	const Basis &basis = scene_data->cam_transform.basis;
	constants.cameraPos = to_sl(scene_data->cam_transform.origin);
	constants.cameraUp = to_sl(basis.get_column(1).normalized());
	constants.cameraRight = to_sl(basis.get_column(0).normalized());
	constants.cameraFwd = to_sl(-basis.get_column(2).normalized());
	constants.cameraNear = scene_data->z_near;
	constants.cameraFar = scene_data->z_far;
	// Vertical, in radians.
	constants.cameraFOV = 2.0f * Math::atan(1.0f / Math::abs(scene_data->cam_projection.columns[1][1]));
	constants.cameraAspectRatio = scene_data->cam_projection.get_aspect();
	constants.depthInverted = sl::Boolean::eTrue;
	constants.cameraMotionIncluded = sl::Boolean::eTrue;
	constants.motionVectors3D = sl::Boolean::eFalse;
	constants.reset = buffers->reset ? sl::Boolean::eTrue : sl::Boolean::eFalse;
	constants.orthographicProjection = scene_data->cam_orthogonal ? sl::Boolean::eTrue : sl::Boolean::eFalse;
	buffers->reset = false;
	if (functions.set_constants(constants, *frame, sl::ViewportHandle(buffers->viewport)) != sl::Result::eOk) {
		ERR_PRINT_ONCE("DLSS didn't accept the camera of this frame. Using FSR 2.2.");
		return false;
	}

	rd->draw_command_begin_label("DLSS");
	// DLSS reads the depth and motion vectors from typed textures, with the
	// motion of the sky filled in.
	const EretheaUpscaleInputs::Textures inputs = state->inputs.prepare(rb, p_render_data, buffers_name());
	const RID color = rb->get_internal_texture(0);
	const RID output = rb->get_upscaled_texture(0);

	DispatchArgs *args = memnew(DispatchArgs);
	args->frame = frame;
	args->viewport = buffers->viewport;
	args->color = (ID3D12Resource *)rd->get_driver_resource(RD::DRIVER_RESOURCE_TEXTURE, color);
	args->depth = (ID3D12Resource *)rd->get_driver_resource(RD::DRIVER_RESOURCE_TEXTURE, inputs.depth);
	args->velocity = (ID3D12Resource *)rd->get_driver_resource(RD::DRIVER_RESOURCE_TEXTURE, inputs.velocity);
	args->output = (ID3D12Resource *)rd->get_driver_resource(RD::DRIVER_RESOURCE_TEXTURE, output);
	args->input_size = internal_size;
	args->output_size = target_size;

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

void DlssUpscaler::release(const Ref<RenderSceneBuffersRD> &p_render_buffers) {
	if (state == nullptr || state->owner != this || p_render_buffers.is_null() || !p_render_buffers->has_custom_data(buffers_name())) {
		return;
	}
	Ref<EretheaDlssBuffers> buffers = p_render_buffers->get_custom_data(buffers_name());
	if (buffers.is_valid()) {
		buffers->release();
	}
	free_pending_viewports(false);
}

void DlssUpscaler::finish() {
	if (state == nullptr || state->owner != this) {
		return;
	}
	// Viewports that the GPU may still use leak instead: the program is
	// closing.
	free_pending_viewports(false);
	state->inputs.finish();
	memdelete(state);
	state = nullptr;
}

#else // ERETHEA_DLSS_ENABLED

void DlssUpscaler::prepare_rendering_driver() {}

void DlssUpscaler::finish_rendering_driver() {}

bool DlssUpscaler::upscale(const Ref<RenderSceneBuffersRD> &p_render_buffers, RenderDataRD *p_render_data, double p_time_step) {
	WARN_PRINT_ONCE("DLSS is only available on Windows, with the Direct3D 12 rendering driver. Using FSR 2.2.");
	return false;
}

void DlssUpscaler::release(const Ref<RenderSceneBuffersRD> &p_render_buffers) {}

void DlssUpscaler::finish() {}

#endif // ERETHEA_DLSS_ENABLED

DlssUpscaler::~DlssUpscaler() {
	finish();
}
