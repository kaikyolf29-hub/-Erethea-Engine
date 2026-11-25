/**************************************************************************/
/*  erethea_ray_traced_shadows.cpp                                        */
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

#include "erethea_ray_traced_shadows.h"

#include "core/config/engine.h"
#include "core/config/project_settings.h"
#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "scene/3d/camera_3d.h"
#include "scene/3d/world_environment.h"
#include "scene/main/scene_tree.h"
#include "scene/main/window.h"
#include "servers/rendering/rendering_server.h"

#ifdef RD_ENABLED
#include "erethea_ray_traced_shadows.glsl.gen.h"

#include "core/templates/hash_map.h"
#include "servers/rendering/renderer_compositor.h"
#include "servers/rendering/renderer_rd/renderer_scene_render_rd.h"
#include "servers/rendering/renderer_rd/storage_rd/light_storage.h"
#include "servers/rendering/renderer_rd/storage_rd/material_storage.h"
#include "servers/rendering/renderer_rd/storage_rd/mesh_storage.h"
#include "servers/rendering/renderer_rd/storage_rd/render_data_rd.h"
#include "servers/rendering/renderer_rd/storage_rd/render_scene_buffers_rd.h"
#include "servers/rendering/renderer_rd/storage_rd/render_scene_data_rd.h"
#include "servers/rendering/renderer_rd/uniform_set_cache_rd.h"
#include "servers/rendering/rendering_server_globals.h"

namespace {

struct PushConstant {
	float inv_view_projection[16];

	float light_direction[3];
	float cos_cone;

	int32_t size[2];
	uint32_t frame;
	uint32_t samples;

	float normal_bias;
	float max_distance;
	float pad[2];
};
static_assert(sizeof(PushConstant) <= 128, "Push constants are limited to 128 bytes.");

// The bottom level acceleration structure of a mesh, with the copies of its
// positions and indices that it was built from.
struct MeshBLAS {
	RID blas;
	LocalVector<RID> buffers;
	int surface_count = 0;
	AABB aabb;
	uint64_t last_used_frame = 0;
};

// The transforms of the instances of a MultiMesh, read from its buffer now and
// then: reading it back from the graphics card every frame would stall.
struct MultiMeshTransforms {
	LocalVector<Transform3D> transforms;
	int instance_count = 0;
	uint64_t refreshed_frame = 0;
};

// Shared by all the effects. Only touched on the rendering thread.
struct RenderResources {
	EretheaRayTracedShadowsShaderRD shader;
	RID version;
	RID pipeline;

	HashMap<RID, MeshBLAS> meshes;
	HashMap<RID, MultiMeshTransforms> multimeshes;
	RID tlas;
	uint32_t tlas_capacity = 0;
	LocalVector<RD::AccelerationStructureInstance> instances;
	uint64_t last_cleanup_frame = 0;
};

RenderResources *render_resources = nullptr;

// The names of the mask in the render buffers, which the Forward+ renderer
// reads (render_forward_clustered.cpp, "erethea_rt" and "shadow_mask").
const StringName &context_name() {
	static const StringName name = "erethea_rt";
	return name;
}

const StringName &mask_texture_name() {
	static const StringName name = "shadow_mask";
	return name;
}

// A mesh unused for this many frames gives its memory back.
constexpr uint64_t UNUSED_MESH_FRAMES = 600;
// The transforms of a MultiMesh are read again after this many frames, or when
// its number of instances changes.
constexpr uint64_t MULTIMESH_REFRESH_FRAMES = 60;

void free_mesh(MeshBLAS &p_mesh) {
	RD *rd = RD::get_singleton();
	if (p_mesh.blas.is_valid()) {
		rd->free_rid(p_mesh.blas);
	}
	for (const RID &buffer : p_mesh.buffers) {
		rd->free_rid(buffer);
	}
	p_mesh.blas = RID();
	p_mesh.buffers.clear();
}

// The acceleration structure of a static mesh, built the first time and when
// the mesh changes. The positions come from the mesh's data, read back once.
RID get_mesh_blas(RID p_mesh, uint64_t p_frame) {
	RendererRD::MeshStorage *mesh_storage = RendererRD::MeshStorage::get_singleton();
	if (!mesh_storage->owns_mesh(p_mesh)) {
		return RID();
	}
	const int surface_count = mesh_storage->mesh_get_surface_count(p_mesh);
	const AABB aabb = mesh_storage->mesh_get_aabb(p_mesh, RID());

	MeshBLAS *mesh = render_resources->meshes.getptr(p_mesh);
	if (mesh != nullptr && mesh->surface_count == surface_count && mesh->aabb == aabb) {
		mesh->last_used_frame = p_frame;
		return mesh->blas;
	}
	if (mesh == nullptr) {
		mesh = &render_resources->meshes.insert(p_mesh, MeshBLAS())->value;
	} else {
		free_mesh(*mesh);
	}
	mesh->surface_count = surface_count;
	mesh->aabb = aabb;
	mesh->last_used_frame = p_frame;

	RD *rd = RD::get_singleton();
	LocalVector<RD::AccelerationStructureGeometry> geometries;
	for (int surface = 0; surface < surface_count; surface++) {
		// Through a const pointer, so that the virtual (RID, int) overload is
		// chosen instead of the inline one that returns the internal surface.
		const RenderingServerTypes::SurfaceData data = static_cast<const RendererRD::MeshStorage *>(mesh_storage)->mesh_get_surface(p_mesh, surface);
		if (data.primitive != RSE::PRIMITIVE_TRIANGLES) {
			continue;
		}
		const Array arrays = RenderingServer::get_singleton()->mesh_create_arrays_from_surface_data(data);
		if (arrays.size() != RSE::ARRAY_MAX) {
			continue;
		}
		const PackedVector3Array vertices = arrays[RSE::ARRAY_VERTEX];
		const PackedInt32Array indices = arrays[RSE::ARRAY_INDEX];
		const uint32_t vertex_count = vertices.size();
		const uint32_t index_count = indices.is_empty() ? vertex_count - vertex_count % 3 : indices.size() - indices.size() % 3;
		if (vertex_count == 0 || index_count == 0) {
			continue;
		}

		// Positions as 32-bit floats, also in builds with double precision.
		LocalVector<float> positions;
		positions.resize(vertex_count * 3);
		for (uint32_t i = 0; i < vertex_count; i++) {
			positions[i * 3 + 0] = vertices[i].x;
			positions[i * 3 + 1] = vertices[i].y;
			positions[i * 3 + 2] = vertices[i].z;
		}
		LocalVector<uint32_t> triangle_indices;
		triangle_indices.resize(index_count);
		for (uint32_t i = 0; i < index_count; i++) {
			triangle_indices[i] = indices.is_empty() ? i : uint32_t(indices[i]);
		}

		const RID vertex_buffer = rd->vertex_buffer_create(positions.size() * sizeof(float), Span<uint8_t>((uint8_t *)positions.ptr(), positions.size() * sizeof(float)), RD::BUFFER_CREATION_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT);
		const RID index_buffer = rd->index_buffer_create(index_count, RD::INDEX_BUFFER_FORMAT_UINT32, Span<uint8_t>((uint8_t *)triangle_indices.ptr(), triangle_indices.size() * sizeof(uint32_t)), false, RD::BUFFER_CREATION_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT);
		mesh->buffers.push_back(vertex_buffer);
		mesh->buffers.push_back(index_buffer);

		RD::AccelerationStructureGeometry geometry;
		geometry.flags = RD::ACCELERATION_STRUCTURE_GEOMETRY_OPAQUE_BIT;
		geometry.vertex_buffer = vertex_buffer;
		geometry.vertex_stride = sizeof(float) * 3;
		geometry.vertex_count = vertex_count;
		geometry.vertex_format = RD::DATA_FORMAT_R32G32B32_SFLOAT;
		geometry.index_buffer = index_buffer;
		geometry.index_count = index_count;
		geometries.push_back(geometry);
	}

	if (!geometries.is_empty()) {
		mesh->blas = rd->blas_create(geometries, RD::ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT);
		if (mesh->blas.is_valid() && rd->blas_build(mesh->blas) != OK) {
			free_mesh(*mesh);
		}
	}
	return mesh->blas;
}

// The transforms of the visible instances of a 3D MultiMesh.
const LocalVector<Transform3D> *get_multimesh_transforms(RID p_multimesh, uint64_t p_frame) {
	RendererRD::MeshStorage *mesh_storage = RendererRD::MeshStorage::get_singleton();
	if (!mesh_storage->owns_multimesh(p_multimesh) || mesh_storage->multimesh_get_transform_format(p_multimesh) != RSE::MULTIMESH_TRANSFORM_3D) {
		return nullptr;
	}
	int count = mesh_storage->multimesh_get_instance_count(p_multimesh);
	const int visible = mesh_storage->multimesh_get_visible_instances(p_multimesh);
	if (visible >= 0) {
		count = MIN(count, visible);
	}

	MultiMeshTransforms *cache = render_resources->multimeshes.getptr(p_multimesh);
	if (cache == nullptr) {
		cache = &render_resources->multimeshes.insert(p_multimesh, MultiMeshTransforms())->value;
	} else if (cache->instance_count == count && p_frame - cache->refreshed_frame < MULTIMESH_REFRESH_FRAMES) {
		return &cache->transforms;
	}

	// 12 floats per transform, then the color and the custom data, if used.
	const int stride = 12 + (mesh_storage->multimesh_uses_colors(p_multimesh) ? 4 : 0) + (mesh_storage->multimesh_uses_custom_data(p_multimesh) ? 4 : 0);
	const Vector<float> buffer = mesh_storage->multimesh_get_buffer(p_multimesh);
	count = MIN(count, int(buffer.size() / stride));
	cache->transforms.resize(MAX(count, 0));
	const float *r = buffer.ptr();
	for (int i = 0; i < count; i++) {
		const float *t = r + i * stride;
		cache->transforms[i] = Transform3D(t[0], t[1], t[2], t[4], t[5], t[6], t[8], t[9], t[10], t[3], t[7], t[11]);
	}
	cache->instance_count = count;
	cache->refreshed_frame = p_frame;
	return &cache->transforms;
}

// Gives back the memory of the meshes that weren't used for a while.
void free_unused_meshes(uint64_t p_frame) {
	if (p_frame - render_resources->last_cleanup_frame < 60) {
		return;
	}
	render_resources->last_cleanup_frame = p_frame;
	LocalVector<RID> unused;
	for (KeyValue<RID, MeshBLAS> &E : render_resources->meshes) {
		if (p_frame - E.value.last_used_frame > UNUSED_MESH_FRAMES || !RendererRD::MeshStorage::get_singleton()->owns_mesh(E.key)) {
			free_mesh(E.value);
			unused.push_back(E.key);
		}
	}
	for (const RID &mesh : unused) {
		render_resources->meshes.erase(mesh);
	}
	LocalVector<RID> unused_multimeshes;
	for (const KeyValue<RID, MultiMeshTransforms> &E : render_resources->multimeshes) {
		if (p_frame - E.value.refreshed_frame > UNUSED_MESH_FRAMES || !RendererRD::MeshStorage::get_singleton()->owns_multimesh(E.key)) {
			unused_multimeshes.push_back(E.key);
		}
	}
	for (const RID &multimesh : unused_multimeshes) {
		render_resources->multimeshes.erase(multimesh);
	}
}

} // namespace
#endif // RD_ENABLED

void EretheaRayTracedShadows::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_max_distance", "distance"), &EretheaRayTracedShadows::set_max_distance);
	ClassDB::bind_method(D_METHOD("get_max_distance"), &EretheaRayTracedShadows::get_max_distance);
	ClassDB::bind_method(D_METHOD("set_light_size", "degrees"), &EretheaRayTracedShadows::set_light_size);
	ClassDB::bind_method(D_METHOD("get_light_size"), &EretheaRayTracedShadows::get_light_size);
	ClassDB::bind_method(D_METHOD("set_samples", "samples"), &EretheaRayTracedShadows::set_samples);
	ClassDB::bind_method(D_METHOD("get_samples"), &EretheaRayTracedShadows::get_samples);
	ClassDB::bind_method(D_METHOD("set_normal_bias", "bias"), &EretheaRayTracedShadows::set_normal_bias);
	ClassDB::bind_method(D_METHOD("get_normal_bias"), &EretheaRayTracedShadows::get_normal_bias);
	ClassDB::bind_static_method("EretheaRayTracedShadows", D_METHOD("is_supported"), &EretheaRayTracedShadows::is_supported);

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "max_distance", PROPERTY_HINT_RANGE, "1,1000,0.1,or_greater,suffix:m"), "set_max_distance", "get_max_distance");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "light_size", PROPERTY_HINT_RANGE, "0,10,0.01,degrees"), "set_light_size", "get_light_size");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "samples", PROPERTY_HINT_RANGE, "1,16,1"), "set_samples", "get_samples");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "normal_bias", PROPERTY_HINT_RANGE, "0,0.5,0.001,suffix:m"), "set_normal_bias", "get_normal_bias");
}

void EretheaRayTracedShadows::_validate_property(PropertyInfo &p_property) const {
	// The effect always runs after the depth pre-pass with the resolved depth,
	// so these CompositorEffect settings don't apply.
	if (p_property.name == "effect_callback_type" || p_property.name == "access_resolved_color" || p_property.name == "access_resolved_depth" || p_property.name == "needs_motion_vectors" || p_property.name == "needs_normal_roughness" || p_property.name == "needs_separate_specular") {
		p_property.usage = PROPERTY_USAGE_NONE;
	}
}

void EretheaRayTracedShadows::set_max_distance(float p_distance) {
	max_distance = MAX(p_distance, 1.0f);
}

float EretheaRayTracedShadows::get_max_distance() const {
	return max_distance;
}

void EretheaRayTracedShadows::set_light_size(float p_degrees) {
	light_size = CLAMP(p_degrees, 0.0f, 10.0f);
}

float EretheaRayTracedShadows::get_light_size() const {
	return light_size;
}

void EretheaRayTracedShadows::set_samples(int p_samples) {
	samples = CLAMP(p_samples, 1, MAX_SAMPLES);
}

int EretheaRayTracedShadows::get_samples() const {
	return samples;
}

void EretheaRayTracedShadows::set_normal_bias(float p_bias) {
	normal_bias = CLAMP(p_bias, 0.0f, 0.5f);
}

float EretheaRayTracedShadows::get_normal_bias() const {
	return normal_bias;
}

void EretheaRayTracedShadows::register_project_settings() {
	GLOBAL_DEF(PropertyInfo(Variant::INT, SUN_SHADOWS_SETTING, PROPERTY_HINT_ENUM, "Disabled,Automatic,Enabled"), SUN_SHADOWS_AUTOMATIC);
}

bool EretheaRayTracedShadows::is_added_to_scenes(int p_setting, bool p_supported, bool p_automatic_cinematic_quality) {
	if (!p_supported) {
		return false;
	}
	switch (p_setting) {
		case SUN_SHADOWS_ENABLED:
			return true;
		case SUN_SHADOWS_AUTOMATIC:
			return p_automatic_cinematic_quality;
		default:
			return false;
	}
}

void EretheaRayTracedShadows::_add_to_node(Node *p_node) {
	WorldEnvironment *environment = Object::cast_to<WorldEnvironment>(p_node);
	Camera3D *camera = Object::cast_to<Camera3D>(p_node);
	if (environment == nullptr && camera == nullptr) {
		return;
	}
	Ref<Compositor> compositor = environment != nullptr ? environment->get_compositor() : camera->get_compositor();
	if (compositor.is_null()) {
		// A camera without its own compositor uses the environment's.
		if (camera != nullptr) {
			return;
		}
		compositor.instantiate();
		environment->set_compositor(compositor);
	}
	TypedArray<CompositorEffect> effects = compositor->get_compositor_effects();
	for (int i = 0; i < effects.size(); i++) {
		if (Object::cast_to<EretheaRayTracedShadows>(effects[i])) {
			return;
		}
	}
	Ref<EretheaRayTracedShadows> effect;
	effect.instantiate();
	effects.push_back(effect);
	compositor->set_compositor_effects(effects);
}

void EretheaRayTracedShadows::_add_to_tree(Node *p_node) {
	_add_to_node(p_node);
	for (int i = 0; i < p_node->get_child_count(); i++) {
		_add_to_tree(p_node->get_child(i));
	}
}

void EretheaRayTracedShadows::add_to_running_scenes() {
	const int setting = GLOBAL_GET(SUN_SHADOWS_SETTING);
	if (setting == SUN_SHADOWS_DISABLED) {
		return;
	}
	RenderingServer *rs = RenderingServer::get_singleton();
	const bool supported = rs != nullptr && rs->get_current_rendering_method() == "forward_plus" && is_supported();
	// What the "Automatic" quality preset picks (see modules/erethea_quality),
	// read by name so that this module doesn't need that one.
	bool automatic_cinematic = false;
	Engine *engine = Engine::get_singleton();
	if (int(GLOBAL_GET("rendering/erethea/quality_preset")) == 4 && engine->has_singleton("EretheaQuality")) {
		automatic_cinematic = int(engine->get_singleton_object("EretheaQuality")->call("get_recommended_preset")) == 3;
	}
	if (!is_added_to_scenes(setting, supported, automatic_cinematic)) {
		return;
	}
	SceneTree *tree = SceneTree::get_singleton();
	if (tree == nullptr || tree->get_root() == nullptr) {
		return;
	}
	print_verbose("Erethea: ray traced shadows of the sun added to the scenes (rendering/erethea/ray_tracing/sun_shadows).");
	tree->connect("node_added", callable_mp_static(&EretheaRayTracedShadows::_add_to_node));
	_add_to_tree(tree->get_root());
}

bool EretheaRayTracedShadows::is_supported() {
#ifdef RD_ENABLED
	RD *rd = RD::get_singleton();
	return rd != nullptr && rd->has_feature(RD::SUPPORTS_RAY_QUERY);
#else
	return false;
#endif
}

void EretheaRayTracedShadows::_render(int p_effect_callback_type, const RenderData *p_render_data) {
#ifdef RD_ENABLED
	if (p_effect_callback_type != RSE::COMPOSITOR_EFFECT_CALLBACK_TYPE_PRE_OPAQUE) {
		return;
	}
	if (!is_supported()) {
		WARN_PRINT_ONCE("EretheaRayTracedShadows needs a graphics card with ray tracing (ray queries). The shadow map is used instead.");
		return;
	}

	const RenderDataRD *render_data = Object::cast_to<RenderDataRD>(p_render_data);
	ERR_FAIL_NULL(render_data);
	Ref<RenderSceneBuffersRD> rb = render_data->render_buffers;
	const RenderSceneDataRD *scene_data = render_data->scene_data;
	if (rb.is_null() || scene_data == nullptr || rb->get_view_count() != 1 || render_data->reflection_probe.is_valid()) {
		return;
	}
	// The Mobile renderer doesn't read the mask.
	if (!rb->get_can_be_storage()) {
		WARN_PRINT_ONCE("EretheaRayTracedShadows needs the Forward+ renderer. It does nothing with the current renderer.");
		return;
	}

	RendererSceneRenderRD *scene_render = RendererSceneRenderRD::get_singleton();
	ERR_FAIL_NULL(scene_render);
	// The geometry around the camera is gathered from the next view on, and
	// the renderer reads the mask while this effect keeps asking.
	scene_render->erethea_request_ray_tracing(max_distance);
	const PagedArray<RenderGeometryInstance *> *geometry = scene_render->erethea_get_ray_tracing_instances();

	// The direction to the sun: the first directional light with energy.
	RendererRD::LightStorage *light_storage = RendererRD::LightStorage::get_singleton();
	ERR_FAIL_NULL(light_storage);
	Vector3 light_direction;
	if (render_data->lights != nullptr) {
		for (uint32_t i = 0; i < render_data->lights->size(); i++) {
			const RID light_instance = (*render_data->lights)[i];
			const RID light = light_storage->light_instance_get_base_light(light_instance);
			if (light_storage->light_get_type(light) != RSE::LIGHT_DIRECTIONAL || light_storage->light_get_param(light, RSE::LIGHT_PARAM_ENERGY) <= 0.0f) {
				continue;
			}
			// A light shines along its -Z axis, so +Z points to the light.
			light_direction = light_storage->light_instance_get_base_transform(light_instance).basis.xform(Vector3(0, 0, 1)).normalized();
			break;
		}
	}

	RD *rd = RD::get_singleton();
	if (render_resources == nullptr) {
		render_resources = memnew(RenderResources);
		Vector<String> modes;
		modes.push_back("");
		render_resources->shader.initialize(modes);
		render_resources->version = render_resources->shader.version_create();
		render_resources->pipeline = rd->compute_pipeline_create(render_resources->shader.version_get_shader(render_resources->version, 0));
	}

	const Size2i size = rb->get_internal_size();
	if (!rb->has_texture(context_name(), mask_texture_name())) {
		rb->create_texture(context_name(), mask_texture_name(), RD::DATA_FORMAT_R8_UNORM, RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_STORAGE_BIT | RD::TEXTURE_USAGE_CAN_COPY_TO_BIT, RD::TEXTURE_SAMPLES_1, size);
	}
	const RID mask = rb->get_texture(context_name(), mask_texture_name());

	// The acceleration structure of the scene: one instance per static mesh
	// around the camera.
	const uint64_t frame = RSG::rasterizer->get_frame_number();
	LocalVector<RD::AccelerationStructureInstance> &instances = render_resources->instances;
	instances.clear();
	if (geometry != nullptr && !light_direction.is_zero_approx()) {
		RendererRD::MeshStorage *mesh_storage = RendererRD::MeshStorage::get_singleton();
		const Vector3 camera_position = scene_data->cam_transform.origin;
		RD::AccelerationStructureInstance as_instance;
		as_instance.mask = 0xFF;
		// Ray queries use no hit shader binding table, but the instance needs
		// a range: one hit group, at the start.
		as_instance.hit_sbt_range = RD::HitShaderBindingTableRange(uint64_t(1) << 32);
		as_instance.flags = RD::ACCELERATION_STRUCTURE_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT | RD::ACCELERATION_STRUCTURE_INSTANCE_FORCE_OPAQUE_BIT;

		for (uint64_t i = 0; i < geometry->size(); i++) {
			const RenderGeometryInstanceBase *instance = static_cast<const RenderGeometryInstanceBase *>((*geometry)[i]);
			if (instance == nullptr || instance->data == nullptr || instance->data->skeleton.is_valid()) {
				continue;
			}
			if (instance->data->base_type == RSE::INSTANCE_MESH) {
				as_instance.blas = get_mesh_blas(instance->data->base, frame);
				if (as_instance.blas.is_valid()) {
					as_instance.transform = instance->transform;
					instances.push_back(as_instance);
				}
			} else if (instance->data->base_type == RSE::INSTANCE_MULTIMESH) {
				// Each instance of a MultiMesh (trees, grass, rocks) shares the
				// structure of its mesh. Only those near the camera go in.
				const RID multimesh = instance->data->base;
				as_instance.blas = get_mesh_blas(mesh_storage->multimesh_get_mesh(multimesh), frame);
				const LocalVector<Transform3D> *transforms = as_instance.blas.is_valid() ? get_multimesh_transforms(multimesh, frame) : nullptr;
				if (transforms == nullptr) {
					continue;
				}
				for (const Transform3D &local : *transforms) {
					as_instance.transform = instance->transform * local;
					if (as_instance.transform.origin.distance_squared_to(camera_position) <= max_distance * max_distance) {
						instances.push_back(as_instance);
					}
				}
			}
		}
	}
	free_unused_meshes(frame);

	if (instances.is_empty()) {
		// Nothing to cast a shadow yet: the light falls everywhere.
		rd->texture_clear(mask, Color(1, 1, 1, 1), 0, 1, 0, 1);
		return;
	}

	if (render_resources->tlas.is_null() || render_resources->tlas_capacity < instances.size()) {
		if (render_resources->tlas.is_valid()) {
			rd->free_rid(render_resources->tlas);
		}
		render_resources->tlas_capacity = MAX(instances.size(), 64u) * 2;
		render_resources->tlas = rd->tlas_create(render_resources->tlas_capacity, RD::ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT);
	}
	if (render_resources->tlas.is_null() || rd->tlas_build(render_resources->tlas, instances) != OK) {
		rd->texture_clear(mask, Color(1, 1, 1, 1), 0, 1, 0, 1);
		return;
	}

	// From clip space, with Godot's depth correction, to world space.
	Projection correction;
	correction.set_depth_correction(scene_data->flip_y);
	const Projection view_projection = correction * scene_data->cam_projection * Projection(scene_data->cam_transform.affine_inverse());
	const Projection inv_view_projection = view_projection.inverse();

	PushConstant push_constant = {};
	RendererRD::MaterialStorage::store_camera(inv_view_projection, push_constant.inv_view_projection);
	push_constant.light_direction[0] = light_direction.x;
	push_constant.light_direction[1] = light_direction.y;
	push_constant.light_direction[2] = light_direction.z;
	push_constant.cos_cone = Math::cos(Math::deg_to_rad(light_size * 0.5f));
	push_constant.size[0] = size.width;
	push_constant.size[1] = size.height;
	push_constant.frame = uint32_t(frame);
	push_constant.samples = uint32_t(light_size > 0.0f ? samples : 1);
	push_constant.normal_bias = normal_bias;
	push_constant.max_distance = max_distance;

	UniformSetCacheRD *uniform_set_cache = UniformSetCacheRD::get_singleton();
	RendererRD::MaterialStorage *material_storage = RendererRD::MaterialStorage::get_singleton();
	ERR_FAIL_NULL(uniform_set_cache);
	ERR_FAIL_NULL(material_storage);
	const RID nearest_sampler = material_storage->sampler_rd_get_default(RSE::CANVAS_ITEM_TEXTURE_FILTER_NEAREST, RSE::CANVAS_ITEM_TEXTURE_REPEAT_DISABLED);
	const RID shader = render_resources->shader.version_get_shader(render_resources->version, 0);

	RD::Uniform u_depth(RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE, 0, Vector<RID>({ nearest_sampler, rb->get_depth_texture() }));
	RD::Uniform u_tlas(RD::UNIFORM_TYPE_ACCELERATION_STRUCTURE, 1, render_resources->tlas);
	RD::Uniform u_mask(RD::UNIFORM_TYPE_IMAGE, 0, mask);

	rd->draw_command_begin_label("Erethea Ray Traced Shadows");
	RD::ComputeListID compute_list = rd->compute_list_begin();
	rd->compute_list_bind_compute_pipeline(compute_list, render_resources->pipeline);
	rd->compute_list_bind_uniform_set(compute_list, uniform_set_cache->get_cache(shader, 0, u_depth, u_tlas), 0);
	rd->compute_list_bind_uniform_set(compute_list, uniform_set_cache->get_cache(shader, 1, u_mask), 1);
	rd->compute_list_set_push_constant(compute_list, &push_constant, sizeof(PushConstant));
	rd->compute_list_dispatch_threads(compute_list, size.width, size.height, 1);
	rd->compute_list_end();
	rd->draw_command_end_label();
#endif // RD_ENABLED
}

void EretheaRayTracedShadows::_free_render_resources() {
#ifdef RD_ENABLED
	if (render_resources != nullptr) {
		RD *rd = RD::get_singleton();
		for (KeyValue<RID, MeshBLAS> &E : render_resources->meshes) {
			free_mesh(E.value);
		}
		render_resources->meshes.clear();
		render_resources->multimeshes.clear();
		if (render_resources->tlas.is_valid()) {
			rd->free_rid(render_resources->tlas);
		}
		render_resources->shader.version_free(render_resources->version);
		memdelete(render_resources);
		render_resources = nullptr;
	}
#endif // RD_ENABLED
}

void EretheaRayTracedShadows::finish() {
	RenderingServer *rs = RenderingServer::get_singleton();
	if (rs != nullptr) {
		rs->call_on_render_thread(callable_mp_static(&EretheaRayTracedShadows::_free_render_resources));
	}
}

EretheaRayTracedShadows::EretheaRayTracedShadows() {
	set_access_resolved_depth(true);

	RenderingServer *rs = RenderingServer::get_singleton();
	if (rs != nullptr && get_rid().is_valid()) {
		// CompositorEffect calls the _render_callback() virtual method, which
		// only scripts and extensions can override, so point the effect at
		// this class's own callback instead.
		rs->compositor_effect_set_callback(get_rid(), RSE::COMPOSITOR_EFFECT_CALLBACK_TYPE_PRE_OPAQUE, callable_mp(this, &EretheaRayTracedShadows::_render));
	}
}
