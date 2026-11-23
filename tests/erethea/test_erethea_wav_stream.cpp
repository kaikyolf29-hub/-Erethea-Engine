/**************************************************************************/
/*  test_erethea_wav_stream.cpp                                           */
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

#include "tests/test_macros.h"

TEST_FORCE_LINK(test_erethea_wav_stream)

#include "scene/resources/audio/audio_stream_wav.h"
#include "tests/test_tools.h"

namespace TestEretheaWavStream {

static void _put_32(Vector<uint8_t> &r_bytes, uint32_t p_value) {
	for (int i = 0; i < 4; i++) {
		r_bytes.push_back((p_value >> (i * 8)) & 0xFF);
	}
}

static void _put_16(Vector<uint8_t> &r_bytes, uint16_t p_value) {
	r_bytes.push_back(p_value & 0xFF);
	r_bytes.push_back(p_value >> 8);
}

static void _put_text(Vector<uint8_t> &r_bytes, const char *p_text) {
	for (int i = 0; i < 4; i++) {
		r_bytes.push_back(p_text[i]);
	}
}

// A mono 16-bit WAV file of `p_frames` frames at 24 kHz, with the given RIFF
// and data sizes, as a stream writer would leave them.
static Vector<uint8_t> _make_wav(int p_frames, uint32_t p_riff_size, uint32_t p_data_size) {
	Vector<uint8_t> bytes;
	_put_text(bytes, "RIFF");
	_put_32(bytes, p_riff_size);
	_put_text(bytes, "WAVE");
	_put_text(bytes, "fmt ");
	_put_32(bytes, 16);
	_put_16(bytes, 1); // PCM.
	_put_16(bytes, 1); // Mono.
	_put_32(bytes, 24000);
	_put_32(bytes, 48000);
	_put_16(bytes, 2);
	_put_16(bytes, 16);
	_put_text(bytes, "data");
	_put_32(bytes, p_data_size);
	for (int i = 0; i < p_frames; i++) {
		_put_16(bytes, uint16_t(int16_t((i % 100) * 300 - 15000)));
	}
	return bytes;
}

TEST_CASE("[Erethea][Audio][AudioStreamWAV] Streamed WAV headers load whole") {
	const int frames = 12000; // Half a second.
	// Reading the samples as chunk headers printed thousands of errors.
	ErrorDetector errors;

	SUBCASE("Sizes left at 0xFFFFFFFF") {
		const Ref<AudioStreamWAV> stream = AudioStreamWAV::load_from_buffer(_make_wav(frames, 0xFFFFFFFF, 0xFFFFFFFF), Dictionary());
		REQUIRE(stream.is_valid());
		CHECK(stream->get_mix_rate() == 24000);
		CHECK(stream->get_length() == doctest::Approx(0.5));
		CHECK_FALSE(errors.has_error);
	}

	SUBCASE("Sizes left at 0") {
		const Ref<AudioStreamWAV> stream = AudioStreamWAV::load_from_buffer(_make_wav(frames, 0, 0), Dictionary());
		REQUIRE(stream.is_valid());
		CHECK(stream->get_length() == doctest::Approx(0.5));
		CHECK_FALSE(errors.has_error);
	}

	SUBCASE("A normal file is read as before") {
		const Ref<AudioStreamWAV> stream = AudioStreamWAV::load_from_buffer(_make_wav(frames, 36 + frames * 2, frames * 2), Dictionary());
		REQUIRE(stream.is_valid());
		CHECK(stream->get_length() == doctest::Approx(0.5));
		CHECK_FALSE(errors.has_error);
	}

	SUBCASE("A chunk size that would wrap around ends the reading") {
		// A correct data chunk, then a chunk whose size reaches past the end of
		// the file: reading used to go back to the same chunk forever.
		Vector<uint8_t> bytes = _make_wav(frames, 36 + frames * 2 + 8, frames * 2);
		_put_text(bytes, "LIST");
		_put_32(bytes, 0xFFFFFFF7);
		const Ref<AudioStreamWAV> stream = AudioStreamWAV::load_from_buffer(bytes, Dictionary());
		REQUIRE(stream.is_valid());
		CHECK(stream->get_length() == doctest::Approx(0.5));
		CHECK_FALSE(errors.has_error);
	}
}

} // namespace TestEretheaWavStream
