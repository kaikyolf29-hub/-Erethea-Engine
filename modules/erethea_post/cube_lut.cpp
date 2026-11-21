/**************************************************************************/
/*  cube_lut.cpp                                                          */
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

#include "cube_lut.h"

Vector3 CubeLUT::_remap(const Vector3 &p_value, const Vector3 &p_min, const Vector3 &p_max) {
	Vector3 result;
	for (int i = 0; i < 3; i++) {
		const real_t range = p_max[i] - p_min[i];
		result[i] = CLAMP((p_value[i] - p_min[i]) / range, (real_t)0.0, (real_t)1.0);
	}
	return result;
}

Error CubeLUT::parse(const String &p_text, String *r_error) {
	title = String();
	size_1d = 0;
	size_3d = 0;
	table_1d.clear();
	table_3d.clear();
	domain_1d_min = Vector3(0, 0, 0);
	domain_1d_max = Vector3(1, 1, 1);
	domain_3d_min = Vector3(0, 0, 0);
	domain_3d_max = Vector3(1, 1, 1);

	Vector<Vector3> values;
	const Vector<String> lines = p_text.split("\n");

#define CUBE_LUT_FAIL(m_message) \
	{ \
		if (r_error) { \
			*r_error = vformat("Line %d: %s", line_number, String(m_message)); \
		} \
		return ERR_PARSE_ERROR; \
	}

	for (int line_index = 0; line_index < lines.size(); line_index++) {
		const int line_number = line_index + 1;
		String line = lines[line_index];
		const int comment = line.find_char('#');
		if (comment != -1) {
			line = line.substr(0, comment);
		}
		line = line.strip_edges();
		if (line.is_empty()) {
			continue;
		}

		const char32_t first = line[0];
		const bool is_number = is_digit(first) || first == '-' || first == '+' || first == '.';

		if (!is_number) {
			const Vector<String> parts = line.split_spaces();
			const String keyword = parts[0];

			if (keyword == "TITLE") {
				title = line.substr(5).strip_edges().trim_prefix("\"").trim_suffix("\"");
			} else if (keyword == "LUT_1D_SIZE" || keyword == "LUT_3D_SIZE") {
				if (!values.is_empty()) {
					CUBE_LUT_FAIL(keyword + " must come before the table.");
				}
				if (parts.size() != 2 || !parts[1].is_valid_int()) {
					CUBE_LUT_FAIL(keyword + " needs one whole number.");
				}
				const int size = parts[1].to_int();
				const bool is_1d = keyword == "LUT_1D_SIZE";
				if (size < 2 || size > (is_1d ? MAX_SIZE_1D : MAX_SIZE_3D)) {
					CUBE_LUT_FAIL(vformat("%s must be between 2 and %d.", keyword, is_1d ? MAX_SIZE_1D : MAX_SIZE_3D));
				}
				(is_1d ? size_1d : size_3d) = size;
			} else if (keyword == "DOMAIN_MIN" || keyword == "DOMAIN_MAX") {
				if (parts.size() != 4 || !parts[1].is_valid_float() || !parts[2].is_valid_float() || !parts[3].is_valid_float()) {
					CUBE_LUT_FAIL(keyword + " needs three numbers.");
				}
				const Vector3 domain = Vector3(parts[1].to_float(), parts[2].to_float(), parts[3].to_float());
				if (keyword == "DOMAIN_MIN") {
					domain_1d_min = domain;
					domain_3d_min = domain;
				} else {
					domain_1d_max = domain;
					domain_3d_max = domain;
				}
			} else if (keyword == "LUT_1D_INPUT_RANGE" || keyword == "LUT_3D_INPUT_RANGE") {
				// DaVinci Resolve's form of the domain: one range for the three channels.
				if (parts.size() != 3 || !parts[1].is_valid_float() || !parts[2].is_valid_float()) {
					CUBE_LUT_FAIL(keyword + " needs two numbers.");
				}
				const real_t range_min = parts[1].to_float();
				const real_t range_max = parts[2].to_float();
				if (keyword == "LUT_1D_INPUT_RANGE") {
					domain_1d_min = Vector3(range_min, range_min, range_min);
					domain_1d_max = Vector3(range_max, range_max, range_max);
				} else {
					domain_3d_min = Vector3(range_min, range_min, range_min);
					domain_3d_max = Vector3(range_max, range_max, range_max);
				}
			}
			// Other keywords (such as LUT_IN_VIDEO_RANGE) don't change the table and are ignored.
			continue;
		}

		const Vector<String> parts = line.split_spaces();
		if (parts.size() != 3 || !parts[0].is_valid_float() || !parts[1].is_valid_float() || !parts[2].is_valid_float()) {
			CUBE_LUT_FAIL("Expected three numbers (red, green and blue).");
		}
		values.push_back(Vector3(parts[0].to_float(), parts[1].to_float(), parts[2].to_float()));
	}

#undef CUBE_LUT_FAIL

	if (size_1d == 0 && size_3d == 0) {
		if (r_error) {
			*r_error = "The file has neither LUT_1D_SIZE nor LUT_3D_SIZE.";
		}
		return ERR_PARSE_ERROR;
	}

	for (int i = 0; i < 3; i++) {
		if (domain_1d_max[i] <= domain_1d_min[i] || domain_3d_max[i] <= domain_3d_min[i]) {
			if (r_error) {
				*r_error = "Each domain maximum must be greater than its minimum.";
			}
			return ERR_PARSE_ERROR;
		}
	}

	// In a file with both tables, the 1D shaper comes first.
	const int64_t expected = int64_t(size_1d) + int64_t(size_3d) * size_3d * size_3d;
	if (values.size() != expected) {
		if (r_error) {
			*r_error = vformat("Expected %d table rows, found %d.", expected, values.size());
		}
		return ERR_PARSE_ERROR;
	}

	table_1d = values.slice(0, size_1d);
	table_3d = values.slice(size_1d);
	return OK;
}

Color CubeLUT::sample(const Color &p_color) const {
	Vector3 color = Vector3(p_color.r, p_color.g, p_color.b);

	if (size_1d > 0) {
		const Vector3 position = _remap(color, domain_1d_min, domain_1d_max) * real_t(size_1d - 1);
		for (int i = 0; i < 3; i++) {
			const int index = MIN(int(position[i]), size_1d - 2);
			const real_t weight = position[i] - index;
			color[i] = Math::lerp(table_1d[index][i], table_1d[index + 1][i], weight);
		}
	}

	if (size_3d > 0) {
		const Vector3 position = _remap(color, domain_3d_min, domain_3d_max) * real_t(size_3d - 1);
		const int r = MIN(int(position.x), size_3d - 2);
		const int g = MIN(int(position.y), size_3d - 2);
		const int b = MIN(int(position.z), size_3d - 2);
		const Vector3 weight = position - Vector3(r, g, b);

		// Red changes fastest in the table, then green, then blue.
		const int stride_g = size_3d;
		const int stride_b = size_3d * size_3d;
		const Vector3 *table = table_3d.ptr() + r + g * stride_g + b * stride_b;

		const Vector3 c00 = table[0].lerp(table[1], weight.x);
		const Vector3 c10 = table[stride_g].lerp(table[stride_g + 1], weight.x);
		const Vector3 c01 = table[stride_b].lerp(table[stride_b + 1], weight.x);
		const Vector3 c11 = table[stride_b + stride_g].lerp(table[stride_b + stride_g + 1], weight.x);
		color = c00.lerp(c10, weight.y).lerp(c01.lerp(c11, weight.y), weight.z);
	}

	return Color(color.x, color.y, color.z, p_color.a);
}

int CubeLUT::get_texture_size() const {
	// The renderer clamps colors darker than 0.5 / size to the first texel,
	// so a large texture keeps dark tones accurate, whatever the table's size.
	return 65;
}

Vector<Ref<Image>> CubeLUT::make_texture_slices(bool *r_clipped) const {
	Vector<Ref<Image>> slices;
	ERR_FAIL_COND_V(size_1d == 0 && size_3d == 0, slices);

	const int size = get_texture_size();
	bool clipped = false;

	Vector<real_t> inputs;
	inputs.resize(size);
	for (int i = 0; i < size; i++) {
		inputs.write[i] = (i + (real_t)0.5) / size;
	}
	inputs.write[0] = 0.0;
	inputs.write[size - 1] = 1.0;

	for (int b = 0; b < size; b++) {
		Vector<uint8_t> data;
		data.resize(size * size * 4);
		uint8_t *w = data.ptrw();
		for (int g = 0; g < size; g++) {
			for (int r = 0; r < size; r++) {
				const Color output = sample(Color(inputs[r], inputs[g], inputs[b]));
				for (int i = 0; i < 3; i++) {
					if (output[i] < 0.0 || output[i] > 1.0) {
						clipped = true;
					}
					w[i] = uint8_t(Math::round(CLAMP(output[i], 0.0f, 1.0f) * 255.0f));
				}
				w[3] = 255;
				w += 4;
			}
		}
		slices.push_back(Image::create_from_data(size, size, false, Image::FORMAT_RGBA8, data));
	}

	if (r_clipped) {
		*r_clipped = clipped;
	}
	return slices;
}
