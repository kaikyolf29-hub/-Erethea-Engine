#[compute]

#version 450

#VERSION_DEFINES

// Screen-space light shafts: each pixel marches towards the sun's position on
// screen and adds up the bright sky it passes over, fading with the distance
// (after Kenny Mitchell, "Volumetric Light Scattering as a Post-Process",
// GPU Gems 3, 2007).

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

layout(set = 0, binding = 0) uniform sampler2D source_color;
layout(set = 0, binding = 1) uniform sampler2D source_depth;
layout(rgba16f, set = 1, binding = 0) uniform restrict writeonly image2D dest_color;

layout(push_constant, std430) uniform Params {
	ivec2 size;
	vec2 sun_uv;

	vec3 tint;
	float intensity; // Already scaled by how visible the sun is.

	float threshold;
	float density;
	float decay;
	int samples;

	uint frame;
	uint pad0;
	uint pad1;
	uint pad2;
}
params;

// Interleaved gradient noise (Jimenez, "Next Generation Post Processing in Call of Duty: Advanced Warfare", 2014).
float gradient_noise(vec2 position) {
	return fract(52.9829189 * fract(dot(position, vec2(0.06711056, 0.00583715))));
}

void main() {
	ivec2 pos = ivec2(gl_GlobalInvocationID.xy);
	if (any(greaterThanEqual(pos, params.size))) {
		return;
	}

	vec2 uv = (vec2(pos) + 0.5) / vec2(params.size);
	vec4 color = texelFetch(source_color, pos, 0);

	// The ray covers this fraction of the way to the sun, in equal steps. The
	// start is jittered per pixel and frame to hide the steps.
	float sample_count = float(params.samples);
	vec2 step_uv = (params.sun_uv - uv) * (params.density / sample_count);
	float jitter = gradient_noise(vec2(pos) + float(params.frame % 64u) * 5.588238);

	vec3 shafts = vec3(0.0);
	for (int i = 0; i < params.samples; i++) {
		float t = float(i) + jitter;
		vec2 sample_uv = uv + step_uv * t;
		if (any(lessThan(sample_uv, vec2(0.0))) || any(greaterThan(sample_uv, vec2(1.0)))) {
			continue;
		}
		// Only the sky, where nothing was drawn (the depth is 0 at the far plane), lights the shafts.
		if (textureLod(source_depth, sample_uv, 0.0).r != 0.0) {
			continue;
		}
		vec3 sky = textureLod(source_color, sample_uv, 0.0).rgb;
		float luminance = dot(sky, vec3(0.2126, 0.7152, 0.0722));
		sky *= max(luminance - params.threshold, 0.0) / max(luminance, 0.0001);
		shafts += min(sky, vec3(16.0)) * exp(-params.decay * 4.0 * t / sample_count);
	}

	color.rgb += shafts / sample_count * params.tint * params.intensity;
	imageStore(dest_color, pos, color);
}
