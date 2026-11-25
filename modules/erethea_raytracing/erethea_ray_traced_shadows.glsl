#[compute]

#version 460

#VERSION_DEFINES

#extension GL_EXT_ray_query : require

// Ray traced shadow of the sun. From the surface seen in each pixel, rays go
// towards the light, inside the cone of its angular size, and the fraction of
// them that reaches it is the light that falls there. The result is a screen
// space mask that the Forward+ renderer applies to the first directional light.

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

layout(set = 0, binding = 0) uniform sampler2D source_depth;
layout(set = 0, binding = 1) uniform accelerationStructureEXT scene_tlas;
layout(r8, set = 1, binding = 0) uniform restrict writeonly image2D dest_mask;

layout(push_constant, std430) uniform Params {
	// From clip space (with Godot's depth correction) to world space.
	mat4 inv_view_projection;

	// Towards the light.
	vec3 light_direction;
	// Cosine of the light's angular radius: 1.0 for hard shadows.
	float cos_cone;

	ivec2 size;
	uint frame;
	uint samples;

	// How far from the surface the rays start, in meters, at 1 m from the
	// camera; it grows with the distance, like the precision of the depth.
	float normal_bias;
	float max_distance;
	float pad0;
	float pad1;
}
params;

// Interleaved gradient noise (Jimenez, "Next Generation Post Processing in Call of Duty: Advanced Warfare", 2014).
float gradient_noise(vec2 position) {
	return fract(52.9829189 * fract(dot(position, vec2(0.06711056, 0.00583715))));
}

vec3 world_position(ivec2 pixel, float depth) {
	vec2 uv = (vec2(pixel) + 0.5) / vec2(params.size);
	vec4 world = params.inv_view_projection * vec4(uv * 2.0 - 1.0, depth, 1.0);
	return world.xyz / world.w;
}

float depth_at(ivec2 pixel) {
	return texelFetch(source_depth, clamp(pixel, ivec2(0), params.size - 1), 0).r;
}

void main() {
	ivec2 pos = ivec2(gl_GlobalInvocationID.xy);
	if (any(greaterThanEqual(pos, params.size))) {
		return;
	}

	// Godot's depth is reversed: 0 is the far plane, where only the sky is.
	float depth = depth_at(pos);
	if (depth == 0.0) {
		imageStore(dest_mask, pos, vec4(1.0));
		return;
	}

	vec3 position = world_position(pos, depth);
	// The near plane, at depth 1, is next to the camera.
	vec3 view_vector = position - world_position(pos, 1.0);
	float distance_to_camera = length(view_vector);

	// The surface normal, from the neighbors on the same surface: on each axis,
	// the one whose depth is closer, so that edges don't bend it.
	float depth_right = depth_at(pos + ivec2(1, 0));
	float depth_left = depth_at(pos - ivec2(1, 0));
	float depth_down = depth_at(pos + ivec2(0, 1));
	float depth_up = depth_at(pos - ivec2(0, 1));
	vec3 tangent_x = abs(depth_right - depth) < abs(depth_left - depth) ? world_position(pos + ivec2(1, 0), depth_right) - position : position - world_position(pos - ivec2(1, 0), depth_left);
	vec3 tangent_y = abs(depth_down - depth) < abs(depth_up - depth) ? world_position(pos + ivec2(0, 1), depth_down) - position : position - world_position(pos - ivec2(0, 1), depth_up);
	vec3 normal = normalize(cross(tangent_x, tangent_y));
	if (dot(normal, view_vector) > 0.0) {
		normal = -normal;
	}

	// A surface that faces away from the light gets no light from it anyway:
	// leave that to the shading. Shadowing it here would darken the facets of
	// low poly shapes past their edge, where the smooth normals of the shading
	// still face the light, in steps (the "shadow terminator" problem).
	if (dot(normal, params.light_direction) <= 0.0) {
		imageStore(dest_mask, pos, vec4(1.0));
		return;
	}

	vec3 origin = position + normal * params.normal_bias * max(distance_to_camera, 1.0);

	// A basis around the light direction, to spread the rays in its cone.
	vec3 up = abs(params.light_direction.y) < 0.99 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
	vec3 basis_x = normalize(cross(up, params.light_direction));
	vec3 basis_y = cross(params.light_direction, basis_x);
	float noise_a = gradient_noise(vec2(pos) + float(params.frame % 64u) * 5.588238);
	float noise_b = gradient_noise(vec2(pos.yx) + float(params.frame % 64u) * 3.711493);

	uint lit = 0u;
	for (uint i = 0u; i < params.samples; i++) {
		vec3 direction = params.light_direction;
		if (params.cos_cone < 1.0) {
			// Uniform on the cone, with the samples spread by the golden ratio.
			float u = fract(noise_a + float(i) * 0.618034);
			float v = fract(noise_b + float(i) * 0.754878);
			float cos_theta = mix(1.0, params.cos_cone, u);
			float sin_theta = sqrt(max(0.0, 1.0 - cos_theta * cos_theta));
			float phi = 6.2831853 * v;
			direction = normalize(basis_x * (cos(phi) * sin_theta) + basis_y * (sin(phi) * sin_theta) + params.light_direction * cos_theta);
		}

		rayQueryEXT query;
		rayQueryInitializeEXT(query, scene_tlas, gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsOpaqueEXT, 0xFF, origin, 0.0, direction, params.max_distance);
		while (rayQueryProceedEXT(query)) {
		}
		if (rayQueryGetIntersectionTypeEXT(query, true) == gl_RayQueryCommittedIntersectionNoneEXT) {
			lit++;
		}
	}

	imageStore(dest_mask, pos, vec4(float(lit) / float(params.samples)));
}
