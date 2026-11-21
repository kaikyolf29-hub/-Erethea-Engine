#[compute]

#version 450

#VERSION_DEFINES

// Copies the depth, motion vectors and reactive mask that AMD FSR reads into
// textures of its own formats, and fills in the motion vectors that Godot
// leaves invalid (the sky) from the depth and the camera's motion.

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

layout(set = 0, binding = 0) uniform sampler2D source_color;
layout(set = 0, binding = 1) uniform sampler2D source_depth;
layout(set = 0, binding = 2) uniform sampler2D source_velocity;

layout(r32f, set = 1, binding = 0) uniform restrict writeonly image2D dest_depth;
layout(rg16f, set = 1, binding = 1) uniform restrict writeonly image2D dest_velocity;
layout(r8, set = 1, binding = 2) uniform restrict writeonly image2D dest_reactive;

layout(push_constant, std430) uniform Params {
	mat4 reprojection;
	ivec2 size;
	vec2 pad;
}
params;

void main() {
	ivec2 pos = ivec2(gl_GlobalInvocationID.xy);
	if (any(greaterThanEqual(pos, params.size))) {
		return;
	}

	float depth = texelFetch(source_depth, pos, 0).r;
	vec2 velocity = texelFetch(source_velocity, pos, 0).xy;
	// Same as Godot's FSR 2.2 (see motion_vector_inc.glsl).
	if (all(lessThanEqual(velocity, vec2(-1.0)))) {
		vec2 uv = (vec2(pos) + 0.5) / vec2(params.size);
		vec4 previous = params.reprojection * vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
		velocity = 0.5 + (previous.xy / previous.w) * 0.5 - uv;
	}

	imageStore(dest_depth, pos, vec4(depth));
	imageStore(dest_velocity, pos, vec4(velocity, 0.0, 0.0));
	// Godot's FSR 2.2 reads the reactive mask from the alpha of the color.
	imageStore(dest_reactive, pos, vec4(texelFetch(source_color, pos, 0).a));
}
