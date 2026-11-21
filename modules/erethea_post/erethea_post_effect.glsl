#[compute]

#version 450

#VERSION_DEFINES

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

layout(set = 0, binding = 0) uniform sampler2D source_color;
layout(rgba16f, set = 1, binding = 0) uniform restrict writeonly image2D dest_color;

layout(push_constant, std430) uniform Params {
	ivec2 size;
	float vignette_intensity;
	float vignette_smoothness;

	float chromatic_aberration;
	float grain_intensity;
	float grain_size;
	uint frame;
}
params;

// Integer hash from Jarzynski and Olano, "Hash Functions for GPU Rendering" (2020).
uvec3 pcg3d(uvec3 v) {
	v = v * 1664525u + 1013904223u;
	v.x += v.y * v.z;
	v.y += v.z * v.x;
	v.z += v.x * v.y;
	v ^= v >> 16u;
	v.x += v.y * v.z;
	v.y += v.z * v.x;
	v.z += v.x * v.y;
	return v;
}

// Noise between -1 and 1 with a triangular distribution, closer to film grain
// than uniform noise.
float grain_value(ivec2 cell) {
	uvec3 h = pcg3d(uvec3(uvec2(cell + 32768), params.frame));
	return (float(h.x) + float(h.y)) * (1.0 / 4294967296.0) - 1.0;
}

float grain(vec2 pixel) {
	vec2 p = pixel / params.grain_size;
	vec2 cell = floor(p);
	vec2 f = p - cell;
	f = f * f * (3.0 - 2.0 * f);
	ivec2 c = ivec2(cell);
	float a = grain_value(c);
	float b = grain_value(c + ivec2(1, 0));
	float d = grain_value(c + ivec2(0, 1));
	float e = grain_value(c + ivec2(1, 1));
	return mix(mix(a, b, f.x), mix(d, e, f.x), f.y);
}

void main() {
	ivec2 pos = ivec2(gl_GlobalInvocationID.xy);
	if (any(greaterThanEqual(pos, params.size))) {
		return;
	}

	vec2 uv = (vec2(pos) + 0.5) / vec2(params.size);
	vec2 from_center = uv - 0.5;

	vec4 color;
	if (params.chromatic_aberration > 0.0) {
		// Lateral chromatic aberration: red and blue are magnified differently,
		// so they split apart towards the edges of the screen.
		vec2 offset = from_center * params.chromatic_aberration;
		color = texelFetch(source_color, pos, 0);
		color.r = textureLod(source_color, uv - offset, 0.0).r;
		color.b = textureLod(source_color, uv + offset, 0.0).b;
	} else {
		color = texelFetch(source_color, pos, 0);
	}

	if (params.vignette_intensity > 0.0) {
		// Distance from the center, 1.0 in the corners.
		float aspect = float(params.size.x) / float(params.size.y);
		float distance_to_center = length(from_center * vec2(aspect, 1.0)) / length(vec2(aspect, 1.0) * 0.5);
		float falloff = smoothstep(1.0 - params.vignette_smoothness, 1.0, distance_to_center);
		color.rgb *= 1.0 - params.vignette_intensity * falloff;
	}

	if (params.grain_intensity > 0.0) {
		// Applied to the scene's linear colors before tone mapping, as a
		// relative change, so it doesn't depend on exposure and fades out in
		// highlights after tone mapping, like film.
		color.rgb *= max(1.0 + grain(vec2(pos)) * params.grain_intensity, 0.0);
	}

	imageStore(dest_color, pos, color);
}
