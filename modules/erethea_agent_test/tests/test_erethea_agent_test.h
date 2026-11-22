/**************************************************************************/
/*  test_erethea_agent_test.h                                             */
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

#pragma once

#include "../erethea_agent_test.h"

#include "core/io/json.h"
#include "tests/test_macros.h"

namespace TestEretheaAgentTest {

static List<String> make_args(std::initializer_list<const char *> p_args) {
	List<String> result;
	for (const char *arg : p_args) {
		result.push_back(arg);
	}
	return result;
}

static EretheaAgentTestScript parse_script(const String &p_json) {
	Ref<JSON> json;
	json.instantiate();
	REQUIRE(json->parse(p_json) == OK);
	return EretheaAgentTestScript::parse(json->get_data());
}

TEST_CASE("[Erethea][AgentTest] Command line options") {
	EretheaAgentTestOptions options = EretheaAgentTestOptions::parse(make_args({ "--path", "game", "res://main.tscn" }));
	CHECK_FALSE(options.enabled);

	options = EretheaAgentTestOptions::parse(make_args({ "--agent-test" }));
	CHECK(options.enabled);
	CHECK(options.folder == EretheaAgentTestOptions::DEFAULT_FOLDER);
	CHECK(options.seconds == doctest::Approx(EretheaAgentTestOptions::DEFAULT_SECONDS));

	options = EretheaAgentTestOptions::parse(make_args({ "--agent-test=C:/tests/out", "--agent-test-seconds=4.5", "--agent-test-inputs=inputs.json" }));
	CHECK(options.enabled);
	CHECK(options.folder == "C:/tests/out");
	CHECK(options.seconds == doctest::Approx(4.5));
	CHECK(options.inputs_file == "inputs.json");
	CHECK(options.error.is_empty());

	options = EretheaAgentTestOptions::parse(make_args({ "--agent-test=out", "--agent-test-seconds=soon" }));
	CHECK(options.enabled);
	CHECK_FALSE(options.error.is_empty());

	options = EretheaAgentTestOptions::parse(make_args({ "--agent-test=out", "--agent-test-seconds=0" }));
	CHECK_FALSE(options.error.is_empty());
}

TEST_CASE("[Erethea][AgentTest] Inputs script") {
	EretheaAgentTestScript script = parse_script(R"({
		"screenshots": [3, 1.5],
		"steps": [
			{"at": 0.5, "for": 2, "action": "move_forward"},
			{"at": 1, "action": "turn", "strength": 0.5},
			{"at": 2, "key": "Space"},
			{"at": 3, "mouse_button": "right", "for": 0.2},
			{"at": 4, "mouse_motion": [100, -20]}
		]
	})");
	REQUIRE(script.error.is_empty());
	REQUIRE(script.steps.size() == 5);
	CHECK(script.screenshots.size() == 2);
	CHECK(script.screenshots[0] == doctest::Approx(1.5));

	CHECK(script.steps[0].kind == EretheaAgentTestStep::KIND_ACTION);
	CHECK(script.steps[0].action == "move_forward");
	CHECK(script.steps[0].duration == doctest::Approx(2.0));
	CHECK(script.steps[1].strength == doctest::Approx(0.5));
	CHECK(script.steps[1].duration == doctest::Approx(EretheaAgentTestStep::DEFAULT_DURATION));
	CHECK(script.steps[2].kind == EretheaAgentTestStep::KIND_KEY);
	CHECK(script.steps[2].key == Key::SPACE);
	CHECK(script.steps[3].button == MouseButton::RIGHT);
	CHECK(script.steps[4].kind == EretheaAgentTestStep::KIND_MOUSE_MOTION);
	CHECK(script.steps[4].motion.is_equal_approx(Vector2(100, -20)));
	CHECK(script.steps[4].duration == doctest::Approx(EretheaAgentTestStep::DEFAULT_MOTION_DURATION));

	CHECK(String(script.steps[2].to_dictionary()["key"]) == "Space");
	CHECK(String(script.steps[0].to_dictionary()["result"]) == "not reached");

	// Only the list of steps.
	script = parse_script(R"([{"at": 0, "action": "jump"}])");
	CHECK(script.error.is_empty());
	CHECK(script.steps.size() == 1);

	// Mistakes are explained, with the step.
	CHECK(parse_script(R"([{"action": "jump"}])").error.contains("Step 1"));
	CHECK(parse_script(R"([{"at": 0, "action": "jump"}, {"at": 1}])").error.contains("Step 2"));
	CHECK(parse_script(R"([{"at": 0, "key": "NotAKey"}])").error.contains("NotAKey"));
	CHECK_FALSE(parse_script(R"([{"at": 0, "action": "jump", "strength": 2}])").error.is_empty());
	CHECK_FALSE(parse_script(R"([{"at": 0, "mouse_button": "fourth"}])").error.is_empty());
	CHECK_FALSE(parse_script(R"([{"at": 0, "mouse_motion": [1]}])").error.is_empty());
	CHECK_FALSE(parse_script(R"([{"at": -1, "action": "jump"}])").error.is_empty());
	CHECK_FALSE(parse_script(R"({"steps": [], "screenshots": ["soon"]})").error.is_empty());
	CHECK_FALSE(parse_script(R"("jump")").error.is_empty());
}

TEST_CASE("[Erethea][AgentTest] Default inputs") {
	// Projects made with the Fornalha walk like the Fornalha's own test.
	EretheaAgentTestScript script = EretheaAgentTestScript::make_default(true, Vector<String>(), 10.0);
	bool walks_forward = false;
	bool jumps = false;
	bool attacks = false;
	for (const EretheaAgentTestStep &step : script.steps) {
		walks_forward = walks_forward || (step.action == "frente" && step.duration > 1.0);
		jumps = jumps || step.action == "pular";
		attacks = attacks || step.action == "atacar";
	}
	CHECK(walks_forward);
	CHECK(jumps);
	CHECK(attacks);
	CHECK(script.screenshots.size() == 5);
	// Screenshots after the end are left out.
	CHECK(EretheaAgentTestScript::make_default(true, Vector<String>(), 4.0).screenshots.size() == 2);

	// Other projects get their own actions pressed in turn, until the end.
	script = EretheaAgentTestScript::make_default(false, { "jump", "move_left" }, 5.0);
	REQUIRE(script.steps.size() > 2);
	CHECK(script.steps[0].action == "jump");
	CHECK(script.steps[1].action == "move_left");
	CHECK(script.steps[2].action == "jump");
	for (const EretheaAgentTestStep &step : script.steps) {
		CHECK(step.at + step.duration <= 5.0);
	}
	CHECK(script.screenshots.size() == 3);

	// Without actions, the ones that move through menus.
	script = EretheaAgentTestScript::make_default(false, Vector<String>(), 5.0);
	REQUIRE_FALSE(script.steps.is_empty());
	CHECK(script.steps[0].action == "ui_right");
}

TEST_CASE("[Erethea][AgentTest] Frame rate summary") {
	EretheaAgentTestFrames frames;
	CHECK(int(frames.get_summary()["frames"]) == 0);

	// Loading frames are left out.
	for (uint32_t i = 0; i < EretheaAgentTestFrames::SKIPPED_FRAMES; i++) {
		frames.add(1000000);
	}
	// 99 frames at 100 FPS and one at 20 FPS.
	for (int i = 0; i < 99; i++) {
		frames.add(10000);
	}
	frames.add(50000);
	const Dictionary summary = frames.get_summary();
	CHECK(int(summary["frames"]) == 100);
	CHECK(double(summary["average_fps"]) == doctest::Approx(96.2).epsilon(0.01));
	CHECK(double(summary["min_fps"]) == doctest::Approx(20.0));
	CHECK(double(summary["low_1_percent_fps"]) == doctest::Approx(20.0));
	CHECK(double(summary["median_frame_ms"]) == doctest::Approx(10.0));
	CHECK(double(summary["max_frame_ms"]) == doctest::Approx(50.0));
}

TEST_CASE("[Erethea][AgentTest] Errors and warnings") {
	EretheaAgentTestLog log;
	log.add(Logger::ERR_ERROR, "Something failed.", "res://player.gd", 10, "_ready");
	log.add(Logger::ERR_ERROR, "Something failed.", "res://player.gd", 10, "_ready");
	log.add(Logger::ERR_SCRIPT, "Invalid call.", "res://enemy.gd", 4, "attack");
	log.add(Logger::ERR_WARNING, "Careful.", "core/thing.cpp", 1, "thing", "res://enemy.gd", 7);

	CHECK(log.get_error_count() == 3);
	CHECK(log.get_warning_count() == 1);
	const Array errors = log.get_errors();
	REQUIRE(errors.size() == 2);
	const Dictionary first = errors[0];
	CHECK(String(first["type"]) == "error");
	CHECK(int(first["count"]) == 2);
	CHECK(String(Dictionary(errors[1])["type"]) == "script_error");
	const Dictionary warning = log.get_warnings()[0];
	CHECK(String(warning["script_file"]) == "res://enemy.gd");
	CHECK(int(warning["script_line"]) == 7);

	// Only so many different ones are kept, but all are counted.
	for (int i = 0; i < EretheaAgentTestLog::MAX_ENTRIES + 10; i++) {
		log.add(Logger::ERR_ERROR, vformat("Error %d.", i), "res://spam.gd", i, "spam");
	}
	CHECK(log.get_errors().size() == EretheaAgentTestLog::MAX_ENTRIES);
	CHECK(log.get_error_count() == 3 + EretheaAgentTestLog::MAX_ENTRIES + 10);
}

TEST_CASE("[Erethea][AgentTest] Passing") {
	Dictionary report;
	report["finished"] = true;
	report["error_count"] = 0;
	CHECK(EretheaAgentTest::is_passed(report));

	report["error_count"] = 1;
	CHECK_FALSE(EretheaAgentTest::is_passed(report));
	report["error_count"] = 0;

	Dictionary player;
	player["fell"] = true;
	report["player"] = player;
	CHECK_FALSE(EretheaAgentTest::is_passed(report));
	report["player"] = Variant();

	report["error"] = "The inputs file isn't valid JSON.";
	CHECK_FALSE(EretheaAgentTest::is_passed(report));
	report.erase("error");

	report["finished"] = false;
	CHECK_FALSE(EretheaAgentTest::is_passed(report));
}

} // namespace TestEretheaAgentTest
