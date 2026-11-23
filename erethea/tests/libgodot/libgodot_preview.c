/* Host program that embeds the Erethea Engine with LibGodot and shows the
 * preview of a glTF asset (modules/erethea_preview). It is what the Fornalha
 * does from Rust, written in C so the test needs nothing but a C compiler.
 *
 *     libgodot_preview <library> [engine arguments...]
 *
 * for example:
 *
 *     libgodot_preview bin/liberethea.linuxbsd.editor.x86_64.so \
 *         --erethea-preview=cube.glb --erethea-preview-capture=capture.png
 *
 * The calls, in order (core/extension/libgodot.h and the GDExtension C API):
 *
 *   1. Load the library and find libgodot_create_godot_instance and
 *      libgodot_destroy_godot_instance.
 *   2. libgodot_create_godot_instance(argc, argv, init_function): the engine
 *      reads the arguments (argv[0] is the name of the host, as in main()) and
 *      calls init_function, a GDExtension entry point of the host, which gets
 *      the function that finds all of the API.
 *   3. With that function, call GodotInstance.start(), then
 *      GodotInstance.iteration() once per frame until it returns true (the
 *      preview asked to quit).
 *   4. libgodot_destroy_godot_instance(instance).
 *
 * The exit code is 0 when the instance ran until it closed, 1 for a problem
 * in the host (arguments, library) and 2 when the engine failed to start. It
 * can't say if the preview worked: the test reads the capture for that.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#include "core/extension/gdextension_interface.gen.h"

typedef GDExtensionObjectPtr (*CreateInstance)(int p_argc, char *p_argv[], GDExtensionInitializationFunction p_init_func);
typedef void (*DestroyInstance)(GDExtensionObjectPtr p_instance);

/* The hashes of the methods of GodotInstance, from the API dump of the engine
 * (erethea --dump-extension-api). The engine checks them: a method that
 * changed is refused, not called with the wrong arguments. */
#define HASH_START INT64_C(2240911060)
#define HASH_ITERATION INT64_C(2240911060)

static GDExtensionInterfaceGetProcAddress get_proc_address = NULL;

static void host_initialize(void *p_userdata, GDExtensionInitializationLevel p_level) {
	(void)p_userdata;
	(void)p_level;
}

static void host_deinitialize(void *p_userdata, GDExtensionInitializationLevel p_level) {
	(void)p_userdata;
	(void)p_level;
}

/* The entry point of the host as a GDExtension. The engine calls it while
 * libgodot_create_godot_instance() runs. */
static GDExtensionBool host_entry(GDExtensionInterfaceGetProcAddress p_get_proc_address, GDExtensionClassLibraryPtr p_library, GDExtensionInitialization *r_initialization) {
	(void)p_library;
	get_proc_address = p_get_proc_address;
	r_initialization->minimum_initialization_level = GDEXTENSION_INITIALIZATION_CORE;
	r_initialization->userdata = NULL;
	r_initialization->initialize = host_initialize;
	r_initialization->deinitialize = host_deinitialize;
	return 1;
}

static GDExtensionMethodBindPtr get_method(const char *p_method, int64_t p_hash) {
	GDExtensionInterfaceStringNameNewWithLatin1Chars string_name_new = (GDExtensionInterfaceStringNameNewWithLatin1Chars)get_proc_address("string_name_new_with_latin1_chars");
	GDExtensionInterfaceClassdbGetMethodBind get_method_bind = (GDExtensionInterfaceClassdbGetMethodBind)get_proc_address("classdb_get_method_bind");
	GDExtensionInterfaceVariantGetPtrDestructor get_destructor = (GDExtensionInterfaceVariantGetPtrDestructor)get_proc_address("variant_get_ptr_destructor");
	if (!string_name_new || !get_method_bind || !get_destructor) {
		return NULL;
	}
	/* A StringName is one pointer; leave room. */
	uint64_t class_name[2] = { 0, 0 };
	uint64_t method_name[2] = { 0, 0 };
	string_name_new(class_name, "GodotInstance", 0);
	string_name_new(method_name, p_method, 0);
	GDExtensionMethodBindPtr method = get_method_bind(class_name, method_name, p_hash);
	GDExtensionPtrDestructor destroy_string_name = get_destructor(GDEXTENSION_VARIANT_TYPE_STRING_NAME);
	destroy_string_name(class_name);
	destroy_string_name(method_name);
	return method;
}

/* Calls a method of GodotInstance without arguments that returns a bool. */
static int call_bool(GDExtensionMethodBindPtr p_method, GDExtensionObjectPtr p_instance) {
	static GDExtensionInterfaceObjectMethodBindPtrcall ptrcall = NULL;
	if (!ptrcall) {
		ptrcall = (GDExtensionInterfaceObjectMethodBindPtrcall)get_proc_address("object_method_bind_ptrcall");
	}
	uint64_t result = 0;
	ptrcall(p_method, p_instance, NULL, &result);
	return (result & 0xff) != 0;
}

#ifdef _WIN32
typedef HMODULE Library;
static Library open_library(const char *p_path) {
	return LoadLibraryA(p_path);
}
static void *find_symbol(Library p_library, const char *p_name) {
	return (void *)GetProcAddress(p_library, p_name);
}
#else
typedef void *Library;
static Library open_library(const char *p_path) {
	return dlopen(p_path, RTLD_NOW | RTLD_LOCAL);
}
static void *find_symbol(Library p_library, const char *p_name) {
	return dlsym(p_library, p_name);
}
#endif

int main(int p_argc, char *p_argv[]) {
	if (p_argc < 2) {
		fprintf(stderr, "Usage: %s <library> [engine arguments...]\n", p_argv[0]);
		return 1;
	}

	Library library = open_library(p_argv[1]);
	if (!library) {
		fprintf(stderr, "Can't load the library \"%s\".\n", p_argv[1]);
#ifndef _WIN32
		fprintf(stderr, "%s\n", dlerror());
#endif
		return 1;
	}
	CreateInstance create_instance = (CreateInstance)find_symbol(library, "libgodot_create_godot_instance");
	DestroyInstance destroy_instance = (DestroyInstance)find_symbol(library, "libgodot_destroy_godot_instance");
	if (!create_instance || !destroy_instance) {
		fprintf(stderr, "The library has no libgodot_create_godot_instance: was it built with library_type=shared_library?\n");
		return 1;
	}

	/* argv of the engine: the name of the host, then the engine's arguments. */
	int engine_argc = p_argc - 1;
	char **engine_argv = (char **)calloc((size_t)engine_argc + 1, sizeof(char *));
	engine_argv[0] = p_argv[0];
	for (int i = 2; i < p_argc; i++) {
		engine_argv[i - 1] = p_argv[i];
	}

	GDExtensionObjectPtr instance = create_instance(engine_argc, engine_argv, host_entry);
	if (!instance || !get_proc_address) {
		fprintf(stderr, "libgodot_create_godot_instance failed.\n");
		return 2;
	}

	GDExtensionMethodBindPtr start = get_method("start", HASH_START);
	GDExtensionMethodBindPtr iteration = get_method("iteration", HASH_ITERATION);
	if (!start || !iteration) {
		fprintf(stderr, "GodotInstance.start or GodotInstance.iteration not found: the hashes in this file don't match the engine.\n");
		destroy_instance(instance);
		return 2;
	}

	if (!call_bool(start, instance)) {
		fprintf(stderr, "GodotInstance.start() failed.\n");
		destroy_instance(instance);
		return 2;
	}
	printf("libgodot_preview: the instance started.\n");
	fflush(stdout);

	/* One iteration is one frame. It returns true when the engine wants to quit. */
	const char *timeout_text = getenv("ERETHEA_LIBGODOT_TIMEOUT");
	double timeout = timeout_text ? atof(timeout_text) : 120.0;
	time_t begin = time(NULL);
	long frames = 0;
	int quit = 0;
	while (!quit) {
		quit = call_bool(iteration, instance);
		frames++;
		if (difftime(time(NULL), begin) > timeout) {
			fprintf(stderr, "libgodot_preview: the instance didn't close in %.0f seconds, stopping it.\n", timeout);
			break;
		}
	}
	printf("libgodot_preview: the instance closed after %ld frames.\n", frames);
	fflush(stdout);

	destroy_instance(instance);
	return quit ? 0 : 3;
}
