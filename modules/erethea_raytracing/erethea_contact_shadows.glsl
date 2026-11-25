#[compute]

#version 450

#VERSION_DEFINES

// Contact shadows of the sun, in screen space. From the surface seen in each
// pixel, a short ray goes towards the light, and the depth buffer tells if
// something in front of the camera covers it: the small, sharp shadows where
// things touch (feet on the ground, a cup on a table) that the shadow map
// misses. The result is the screen space mask that the Forward+ renderer
// applies to the first directional light.

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

layout(set = 0, binding = 0) uniform sampler2D source_depth;
layout(r8, set = 1, binding = 0) uniform restrict writeonly image2D dest_mask;

layout(set = 2, binding = 0, std140) uniform Params {
	// From world space to clip space and back, with Godot's depth correction.
	mat4 view_projection;
	mat4 inv_view_projection;

	vec3 camera_position;
	// How long the rays are, in meters.
	float ray_length;

	// Towards the light.
	vec3 light_direction;
	// How thick what the depth buffer shows is taken to be, in meters.
	float thickness;

	ivec2 size;
	uint steps;
	uint frame;

	// How far from the surface the rays start, in meters, at 1 m from the
	// camera; it grows with the distance, like the precision of the depth.
	float normal_bias;
	float pad0;
	float pad1;
	float pad2;
}
params;

// Interleaved gradient noise (Jimenez, "Next Generation Post Processing in Call of Duty: Advanced Warfare", 2014).
float gradient_noise(vec2 position) {
	return fract(52.9829189 * fract(dot(position, vec2(0.06711056, 0.00583715))));
}

vec3 world_position(vec2 uv, float depth) {
	vec4 world = params.inv_view_projection * vec4(uv * 2.0 - 1.0, depth, 1.0);
	return world.xyz / world.w;
}

vec3 world_position_at(ivec2 pixel, float depth) {
	return world_position((vec2(pixel) + 0.5) / vec2(params.size), depth);
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

	vec3 position = world_position_at(pos, depth);
	float distance_to_camera = length(position - params.camera_position);

	// The surface normal, from the neighbors on the same surface: on each axis,
	// the one whose depth is closer, so that edges don't bend it.
	float depth_right = depth_at(pos + ivec2(1, 0));
	float depth_left = depth_at(pos - ivec2(1, 0));
	float depth_down = depth_at(pos + ivec2(0, 1));
	float depth_up = depth_at(pos - ivec2(0, 1));
	vec3 tangent_x = abs(depth_right - depth) < abs(depth_left - depth) ? world_position_at(pos + ivec2(1, 0), depth_right) - position : position - world_position_at(pos - ivec2(1, 0), depth_left);
	vec3 tangent_y = abs(depth_down - depth) < abs(depth_up - depth) ? world_position_at(pos + ivec2(0, 1), depth_down) - position : position - world_position_at(pos - ivec2(0, 1), depth_up);
	vec3 normal = normalize(cross(tangent_x, tangent_y));
	if (dot(normal, position - params.camera_position) > 0.0) {
		normal = -normal;
	}

	// A surface that faces away from the light gets no light from it anyway:
	// leave that to the shading (see erethea_ray_traced_shadows.glsl).
	if (dot(normal, params.light_direction) <= 0.0) {
		imageStore(dest_mask, pos, vec4(1.0));
		return;
	}

	vec3 origin = position + normal * params.normal_bias * max(distance_to_camera, 1.0);
	float jitter = gradient_noise(vec2(pos) + float(params.frame % 64u) * 5.588238);

	float lit = 1.0;
	for (uint i = 0u; i < params.steps; i++) {
		float t = (float(i) + jitter) / float(params.steps) * params.ray_length;
		vec3 sample_position = origin + params.light_direction * t;
		vec4 clip = params.view_projection * vec4(sample_position, 1.0);
		if (clip.w <= 0.0) {
			break;
		}
		vec2 uv = (clip.xy / clip.w) * 0.5 + 0.5;
		if (any(lessThan(uv, vec2(0.0))) || any(greaterThan(uv, vec2(1.0)))) {
			break;
		}
		float scene_depth = texelFetch(source_depth, clamp(ivec2(uv * vec2(params.size)), ivec2(0), params.size - 1), 0).r;
		if (scene_depth == 0.0) {
			continue;
		}
		// Along the same line of sight: the scene's surface covers the sample
		// when it is closer to the camera, by less than its thickness.
		float sample_distance = length(sample_position - params.camera_position);
		float scene_distance = length(world_position(uv, scene_depth) - params.camera_position);
		float in_front = sample_distance - scene_distance;
		if (in_front > 0.002 * max(sample_distance, 1.0) && in_front < params.thickness) {
			// Fade out near the end of the ray, so that its length doesn't
			// show as a hard edge.
			lit = smoothstep(0.6, 1.0, t / params.ray_length);
			break;
		}
	}

	imageStore(dest_mask, pos, vec4(lit));
}
