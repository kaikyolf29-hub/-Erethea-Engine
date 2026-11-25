/**************************************************************************/
/*  test_erethea_raytracing.h                                             */
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

#include "../erethea_contact_shadows.h"
#include "../erethea_ray_traced_shadows.h"

#include "tests/test_macros.h"

namespace TestEretheaRayTracing {

TEST_CASE("[Modules][EretheaRayTracing] Ray traced shadows defaults and ranges") {
	Ref<EretheaRayTracedShadows> shadows;
	shadows.instantiate();

	SUBCASE("Defaults: a sun-sized light, a few rays, 100 m around the camera") {
		CHECK(shadows->get_max_distance() == doctest::Approx(100.0));
		CHECK(shadows->get_light_size() == doctest::Approx(0.5));
		CHECK(shadows->get_samples() == 4);
		CHECK(shadows->get_normal_bias() == doctest::Approx(0.02));
		// It reads the resolved depth, before the opaque pass.
		CHECK(shadows->get_access_resolved_depth());
	}

	SUBCASE("Values are kept in their ranges") {
		shadows->set_max_distance(0.0);
		CHECK(shadows->get_max_distance() == doctest::Approx(1.0));
		shadows->set_light_size(-1.0);
		CHECK(shadows->get_light_size() == doctest::Approx(0.0));
		shadows->set_light_size(45.0);
		CHECK(shadows->get_light_size() == doctest::Approx(10.0));
		shadows->set_samples(0);
		CHECK(shadows->get_samples() == 1);
		shadows->set_samples(100);
		CHECK(shadows->get_samples() == EretheaRayTracedShadows::MAX_SAMPLES);
		shadows->set_normal_bias(2.0);
		CHECK(shadows->get_normal_bias() == doctest::Approx(0.5));
	}
}

TEST_CASE("[Modules][EretheaRayTracing] Contact shadows defaults and ranges") {
	Ref<EretheaContactShadows> contact;
	contact.instantiate();

	SUBCASE("Defaults: short rays, a few steps") {
		CHECK(contact->get_length() == doctest::Approx(0.5));
		CHECK(contact->get_thickness() == doctest::Approx(0.2));
		CHECK(contact->get_steps() == 16);
		CHECK(contact->get_normal_bias() == doctest::Approx(0.01));
		// It reads the resolved depth, before the opaque pass.
		CHECK(contact->get_access_resolved_depth());
	}

	SUBCASE("Values are kept in their ranges") {
		contact->set_length(0.0);
		CHECK(contact->get_length() == doctest::Approx(0.01));
		contact->set_thickness(-1.0);
		CHECK(contact->get_thickness() == doctest::Approx(0.01));
		contact->set_steps(1);
		CHECK(contact->get_steps() == 4);
		contact->set_steps(1000);
		CHECK(contact->get_steps() == EretheaContactShadows::MAX_STEPS);
		contact->set_normal_bias(2.0);
		CHECK(contact->get_normal_bias() == doctest::Approx(0.5));
	}
}

} // namespace TestEretheaRayTracing
