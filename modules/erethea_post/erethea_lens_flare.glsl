#[compute]

#version 450

#VERSION_DEFINES

// Screen-space lens flare in four passes, at a quarter of the resolution:
// keep the bright parts, mirror them through the center into ghosts and a
// halo (after John Chapman, "Pseudo Lens Flare", 2013), blur, then add the
// result to the scene.

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

layout(set = 0, binding = 0) uniform sampler2D source;
layout(rgba16f, set = 1, binding = 0) uniform restrict image2D dest;

layout(push_constant, std430) uniform Params {
	ivec2 size; // Of the destination.
	vec2 source_pixel_size;

	float threshold;
	float intensity;
	float ghost_spacing;
	float halo_width;

	vec2 blur_direction;
	int ghosts;
	float chromatic_distortion;
}
params;

#if defined(MODE_FEATURES)
vec3 sample_distorted(vec2 uv, vec2 direction) {
	vec2 offset = direction * params.chromatic_distortion;
	return vec3(
			textureLod(source, uv + offset, 0.0).r,
			textureLod(source, uv, 0.0).g,
			textureLod(source, uv - offset, 0.0).b);
}
#endif

void main() {
	ivec2 pos = ivec2(gl_GlobalInvocationID.xy);
	if (any(greaterThanEqual(pos, params.size))) {
		return;
	}
	vec2 uv = (vec2(pos) + 0.5) / vec2(params.size);

#if defined(MODE_DOWNSAMPLE)
	// Average the 4×4 source pixels under this one with 4 bilinear samples,
	// then keep only what is brighter than the threshold, with its color.
	vec2 o = params.source_pixel_size;
	vec3 color = textureLod(source, uv + vec2(-o.x, -o.y), 0.0).rgb;
	color += textureLod(source, uv + vec2(o.x, -o.y), 0.0).rgb;
	color += textureLod(source, uv + vec2(-o.x, o.y), 0.0).rgb;
	color += textureLod(source, uv + vec2(o.x, o.y), 0.0).rgb;
	color *= 0.25;
	float luminance = dot(color, vec3(0.2126, 0.7152, 0.0722));
	color *= max(luminance - params.threshold, 0.0) / max(luminance, 0.0001);
	imageStore(dest, pos, vec4(min(color, vec3(65000.0)), 1.0));

#elif defined(MODE_FEATURES)
	vec2 flipped = vec2(1.0) - uv;
	vec2 to_center = vec2(0.5) - flipped;
	vec2 ghost_vector = to_center * params.ghost_spacing;
	vec2 direction = length(to_center) > 0.0001 ? normalize(to_center) : vec2(0.0);
	const float max_distance = 0.70710678;

	vec3 result = vec3(0.0);
	for (int i = 0; i < params.ghosts; i++) {
		vec2 offset = fract(flipped + ghost_vector * float(i));
		// Only bright parts near the center make strong ghosts.
		float weight = pow(1.0 - length(vec2(0.5) - offset) / max_distance, 10.0);
		result += sample_distorted(offset, direction) * weight;
	}

	// A ring around the center, round whatever the screen's shape.
	float aspect = float(params.size.x) / float(params.size.y);
	vec2 halo_vector = to_center * vec2(aspect, 1.0);
	halo_vector = (length(halo_vector) > 0.0001 ? normalize(halo_vector) : vec2(0.0)) * params.halo_width;
	halo_vector.x /= aspect;
	vec2 halo_uv = flipped + halo_vector;
	float halo_weight = pow(1.0 - min(length(vec2(0.5) - halo_uv) / max_distance, 1.0), 5.0);
	result += sample_distorted(halo_uv, direction) * halo_weight;

	imageStore(dest, pos, vec4(result, 1.0));

#elif defined(MODE_BLUR)
	// Separable Gaussian blur, 9 taps.
	const float weights[5] = float[](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);
	vec2 step_uv = params.blur_direction / vec2(params.size);
	vec3 color = textureLod(source, uv, 0.0).rgb * weights[0];
	for (int i = 1; i < 5; i++) {
		color += textureLod(source, uv + step_uv * float(i), 0.0).rgb * weights[i];
		color += textureLod(source, uv - step_uv * float(i), 0.0).rgb * weights[i];
	}
	imageStore(dest, pos, vec4(color, 1.0));

#elif defined(MODE_COMPOSITE)
	vec4 color = imageLoad(dest, pos);
	color.rgb += textureLod(source, uv, 0.0).rgb * params.intensity;
	imageStore(dest, pos, color);
#endif
}
