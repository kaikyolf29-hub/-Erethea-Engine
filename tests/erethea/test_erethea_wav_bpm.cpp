/**************************************************************************/
/*  test_erethea_wav_bpm.cpp                                              */
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

TEST_FORCE_LINK(test_erethea_wav_bpm)

#include "scene/resources/audio/audio_stream_wav.h"
#include "tests/test_tools.h"

#ifdef MODULE_INTERACTIVE_MUSIC_ENABLED
#include "modules/interactive_music/audio_stream_interactive.h"
#include "servers/audio/audio_server.h"
#endif

namespace TestEretheaWavBpm {

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

// A mono 16-bit WAV file of `p_frames` frames at 24 kHz.
static Vector<uint8_t> _make_wav(int p_frames) {
	Vector<uint8_t> bytes;
	_put_text(bytes, "RIFF");
	_put_32(bytes, 36 + p_frames * 2);
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
	_put_32(bytes, p_frames * 2);
	for (int i = 0; i < p_frames; i++) {
		_put_16(bytes, uint16_t(int16_t((i % 100) * 300 - 15000)));
	}
	return bytes;
}

static Dictionary _music_options(double p_bpm, int p_beat_count, int p_bar_beats) {
	Dictionary options;
	options["edit/loop_mode"] = 2; // Forward.
	options["edit/loop_begin"] = 0;
	options["edit/loop_end"] = -1;
	options["bpm"] = p_bpm;
	options["beat_count"] = p_beat_count;
	options["bar_beats"] = p_bar_beats;
	return options;
}

TEST_CASE("[Erethea][Audio][AudioStreamWAV] The tempo is a property of the stream") {
	Ref<AudioStreamWAV> stream;
	stream.instantiate();

	SUBCASE("Defaults match the Ogg Vorbis stream") {
		CHECK(stream->get_bpm() == 0);
		CHECK(stream->get_beat_count() == 0);
		CHECK(stream->get_bar_beats() == 4);
	}

	SUBCASE("Setters and getters") {
		stream->set_bpm(120.5);
		stream->set_beat_count(32);
		stream->set_bar_beats(3);
		CHECK(stream->get_bpm() == doctest::Approx(120.5));
		CHECK(stream->get_beat_count() == 32);
		CHECK(stream->get_bar_beats() == 3);
		CHECK(Object::cast_to<AudioStream>(stream.ptr())->get_bpm() == doctest::Approx(120.5));
	}

	SUBCASE("Values out of range are refused") {
		ErrorDetector errors;
		stream->set_bpm(-1);
		stream->set_beat_count(-1);
		stream->set_bar_beats(1);
		CHECK(errors.has_error);
		CHECK(stream->get_bpm() == 0);
		CHECK(stream->get_beat_count() == 0);
		CHECK(stream->get_bar_beats() == 4);
	}

	SUBCASE("load_from_buffer reads the options") {
		const Ref<AudioStreamWAV> loaded = AudioStreamWAV::load_from_buffer(_make_wav(24000), _music_options(90, 16, 4));
		REQUIRE(loaded.is_valid());
		CHECK(loaded->get_bpm() == doctest::Approx(90));
		CHECK(loaded->get_beat_count() == 16);
		CHECK(loaded->get_bar_beats() == 4);
		CHECK(loaded->get_loop_mode() == AudioStreamWAV::LOOP_FORWARD);
		CHECK(loaded->get_loop_begin() == 0);
		CHECK(loaded->get_loop_end() == 23999);
	}

	SUBCASE("load_from_buffer without the options leaves the defaults") {
		const Ref<AudioStreamWAV> loaded = AudioStreamWAV::load_from_buffer(_make_wav(24000), Dictionary());
		REQUIRE(loaded.is_valid());
		CHECK(loaded->get_bpm() == 0);
		CHECK(loaded->get_beat_count() == 0);
		CHECK(loaded->get_bar_beats() == 4);
		CHECK(loaded->get_loop_mode() == AudioStreamWAV::LOOP_DISABLED);
	}
}

#ifdef MODULE_INTERACTIVE_MUSIC_ENABLED

TEST_CASE("[Erethea][Audio][AudioStreamWAV] Interactive music switches at the end of the bar") {
	// 120 bpm, 8 beats of 4 per bar: a bar is 2 seconds, a clip is 4 seconds.
	Ref<AudioStreamInteractive> interactive;
	interactive.instantiate();
	interactive->set_clip_count(2);
	for (int i = 0; i < 2; i++) {
		const Ref<AudioStreamWAV> clip = AudioStreamWAV::load_from_buffer(_make_wav(24000 * 4), _music_options(120, 8, 4));
		REQUIRE(clip.is_valid());
		interactive->set_clip_name(i, i == 0 ? "a" : "b");
		interactive->set_clip_stream(i, clip);
	}
	interactive->set_initial_clip(0);
	interactive->add_transition(AudioStreamInteractive::CLIP_ANY, AudioStreamInteractive::CLIP_ANY, AudioStreamInteractive::TRANSITION_FROM_TIME_NEXT_BAR, AudioStreamInteractive::TRANSITION_TO_TIME_START, AudioStreamInteractive::FADE_DISABLED, 1);

	Ref<AudioStreamPlayback> base_playback = interactive->instantiate_playback();
	Ref<AudioStreamPlaybackInteractive> playback = base_playback;
	REQUIRE(playback.is_valid());
	playback->start(0);
	REQUIRE(playback->is_playing());

	const double mix_rate = AudioServer::get_singleton()->get_mix_rate();
	const int block = 256;
	AudioFrame buffer[block];
	int64_t frames_mixed = 0;
	bool requested = false;

	// Mixes up to the given time in seconds, asking for the switch at 0.5 s.
	const auto mix_until = [&](double p_time) {
		while (frames_mixed / mix_rate < p_time) {
			if (!requested && frames_mixed / mix_rate >= 0.5) {
				playback->switch_to_clip(1);
				requested = true;
			}
			playback->mix(buffer, 1.0, block);
			frames_mixed += block;
		}
	};

	mix_until(1.9);
	CHECK(requested);
	CHECK(playback->get_current_clip_index() == 0);
	mix_until(2.1);
	CHECK(playback->get_current_clip_index() == 1);
}

#endif // MODULE_INTERACTIVE_MUSIC_ENABLED

} // namespace TestEretheaWavBpm
