#[compute]

#version 450

#VERSION_DEFINES

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

layout(set = 0, binding = 0) uniform sampler2D source_color;
layout(set = 0, binding = 1) uniform sampler2D source_velocity;
layout(set = 0, binding = 2) uniform sampler2D source_depth;
layout(rgba16f, set = 1, binding = 0) uniform restrict writeonly image2D dest_color;

layout(push_constant, std430) uniform Params {
	// Reprojects the current frame's normalized device coordinates to the
	// previous frame's, for the sky, which has no motion vectors.
	mat4 reprojection;

	ivec2 size;
	float intensity;
	float max_length;

	int samples;
	uint frame;
	uint use_reprojection;
	uint pad;
}
params;

// Interleaved gradient noise (Jimenez, "Next Generation Post Processing in Call of Duty: Advanced Warfare", 2014).
float gradient_noise(vec2 position) {
	return fract(52.9829189 * fract(dot(position, vec2(0.06711056, 0.00583715))));
}

// Motion of a pixel since the previous frame, in UV units.
vec2 get_velocity(vec2 uv) {
	vec2 velocity = textureLod(source_velocity, uv, 0.0).xy;
	if (params.use_reprojection != 0u && textureLod(source_depth, uv, 0.0).r == 0.0) {
		// Nothing was drawn here (sky or background): use the camera's motion.
		vec4 previous = params.reprojection * vec4(uv * 2.0 - 1.0, 0.0, 1.0);
		if (previous.w > 0.0) {
			velocity = (previous.xy / previous.w) * 0.5 + 0.5 - uv;
		}
	}
	return velocity;
}

void main() {
	ivec2 pos = ivec2(gl_GlobalInvocationID.xy);
	if (any(greaterThanEqual(pos, params.size))) {
		return;
	}

	vec2 pixel_size = 1.0 / vec2(params.size);
	vec2 uv = (vec2(pos) + 0.5) * pixel_size;

	// The shutter is open for this fraction of the frame.
	vec2 blur = get_velocity(uv) * params.intensity;
	vec2 blur_pixels = blur * vec2(params.size);
	float blur_length = length(blur_pixels);
	if (blur_length < 0.5) {
		// Less than half a pixel: keep the pixel as it is.
		return;
	}
	float max_pixels = params.max_length * float(params.size.x);
	if (blur_length > max_pixels) {
		blur *= max_pixels / blur_length;
		blur_length = max_pixels;
	}

	float center_depth = textureLod(source_depth, uv, 0.0).r;
	float offset = gradient_noise(vec2(pos) + float(params.frame % 64u) * 5.588238);

	vec4 total = texelFetch(source_color, pos, 0);
	float total_weight = 1.0;
	for (int i = 0; i < params.samples; i++) {
		float t = (float(i) + offset) / float(params.samples) - 0.5;
		vec2 sample_uv = uv + blur * t;
		if (any(lessThan(sample_uv, vec2(0.0))) || any(greaterThan(sample_uv, vec2(1.0)))) {
			continue;
		}
		// Don't smear something that stays still in front of this pixel over
		// it, such as a character the camera follows over a moving background.
		float sample_depth = textureLod(source_depth, sample_uv, 0.0).r;
		vec2 sample_motion = textureLod(source_velocity, sample_uv, 0.0).xy * params.intensity * vec2(params.size);
		float weight = 1.0;
		if (sample_depth > center_depth && length(sample_motion) < 0.5 * blur_length) {
			weight = 0.0;
		}
		total += textureLod(source_color, sample_uv, 0.0) * weight;
		total_weight += weight;
	}

	imageStore(dest_color, pos, total / total_weight);
}
