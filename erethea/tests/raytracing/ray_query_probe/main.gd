## Erethea: proves that the rendering device builds acceleration structures and
## traces rays from a compute shader (ray queries). Expected output:
## RTPROBE result=[-1.0, 5.0, 5.0, -1.0]. Run with lavapipe as in HANDOFF.md.
extends Node

const SHADER := """
#version 460
#extension GL_EXT_ray_query : require
layout(local_size_x = 4) in;
layout(set = 0, binding = 0) uniform accelerationStructureEXT tlas;
layout(set = 0, binding = 1, std430) buffer Out { float result[]; };
void main() {
	uint i = gl_GlobalInvocationID.x;
	vec3 origin = vec3(float(i) * 6.0 - 9.0, 0.0, 5.0);
	rayQueryEXT q;
	rayQueryInitializeEXT(q, tlas, gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsOpaqueEXT, 0xFF, origin, 0.001, vec3(0.0, 0.0, -1.0), 100.0);
	while (rayQueryProceedEXT(q)) {
	}
	result[i] = rayQueryGetIntersectionTypeEXT(q, true) == gl_RayQueryCommittedIntersectionTriangleEXT ? rayQueryGetIntersectionTEXT(q, true) : -1.0;
}
"""

func _ready() -> void:
	var rd := RenderingServer.get_rendering_device()
	var verts := PackedFloat32Array([-10, -10, 0, 10, -10, 0, 0, 10, 0]).to_byte_array()
	var inds := PackedInt32Array([0, 1, 2]).to_byte_array()
	var as_input := RenderingDevice.BUFFER_CREATION_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT
	var vb := rd.vertex_buffer_create(verts.size(), verts, as_input)
	var ib := rd.index_buffer_create(3, RenderingDevice.INDEX_BUFFER_FORMAT_UINT32, inds, false, as_input)
	var geom := RDAccelerationStructureGeometry.new()
	geom.flags = RenderingDevice.ACCELERATION_STRUCTURE_GEOMETRY_OPAQUE_BIT
	geom.vertex_buffer = vb
	geom.vertex_format = RenderingDevice.DATA_FORMAT_R32G32B32_SFLOAT
	geom.vertex_stride = 12
	geom.vertex_count = 3
	geom.index_buffer = ib
	geom.index_count = 3
	var blas := rd.blas_create([geom], RenderingDevice.ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT)
	print("RTPROBE blas valid=", blas.is_valid(), " build=", rd.blas_build(blas))
	var tlas := rd.tlas_create(1, 0)
	var inst := RDAccelerationStructureInstance.new()
	inst.transform = Transform3D()
	inst.blas = blas
	inst.mask = 0xFF
	inst.hit_sbt_range = 1 << 32 # count 1, offset 0: ray queries use no table, but the TLAS asks for one.
	print("RTPROBE tlas valid=", tlas.is_valid(), " build=", rd.tlas_build(tlas, [inst]))

	var src := RDShaderSource.new()
	src.language = RenderingDevice.SHADER_LANGUAGE_GLSL
	src.source_compute = SHADER
	var spirv := rd.shader_compile_spirv_from_source(src)
	if spirv.compile_error_compute != "":
		print("RTPROBE compile error: ", spirv.compile_error_compute)
		get_tree().quit()
		return
	var shader := rd.shader_create_from_spirv(spirv)
	var pipeline := rd.compute_pipeline_create(shader)
	var out := rd.storage_buffer_create(16)
	var u0 := RDUniform.new()
	u0.uniform_type = RenderingDevice.UNIFORM_TYPE_ACCELERATION_STRUCTURE
	u0.binding = 0
	u0.add_id(tlas)
	var u1 := RDUniform.new()
	u1.uniform_type = RenderingDevice.UNIFORM_TYPE_STORAGE_BUFFER
	u1.binding = 1
	u1.add_id(out)
	var uset := rd.uniform_set_create([u0, u1], shader, 0)
	var cl := rd.compute_list_begin()
	rd.compute_list_bind_compute_pipeline(cl, pipeline)
	rd.compute_list_bind_uniform_set(cl, uset, 0)
	rd.compute_list_dispatch(cl, 1, 1, 1)
	rd.compute_list_end()
	var result := rd.buffer_get_data(out).to_float32_array()
	print("RTPROBE result=", result, " expected=[-1, 5, 5, -1]")
	get_tree().quit()
