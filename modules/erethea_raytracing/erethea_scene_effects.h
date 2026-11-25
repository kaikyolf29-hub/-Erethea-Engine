/**************************************************************************/
/*  erethea_scene_effects.h                                               */
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

#include "core/string/string_name.h"
#include "core/templates/local_vector.h"

class Node;

// Erethea: Puts compositor effects in the running project's scenes, for the
// project settings that turn an effect on for the whole project.
class EretheaSceneEffects {
	// The effects added to the running scenes, by class.
	static inline LocalVector<StringName> effect_classes;

	static void _add_to_node(Node *p_node, const StringName &p_effect_class);
	static void _add_to_tree(Node *p_node, const StringName &p_effect_class);
	static void _node_added(Node *p_node);

public:
	// The quality presets of modules/erethea_quality, read by name so that
	// this module doesn't need that one.
	enum QualityPreset {
		QUALITY_UNKNOWN = -1,
		QUALITY_LOW = 1,
		QUALITY_BALANCED = 2,
		QUALITY_CINEMATIC = 3,
	};

	// The preset that the "Automatic" quality preset picks for this graphics
	// card, or QUALITY_UNKNOWN when the project's preset isn't "Automatic".
	static QualityPreset get_automatic_quality_preset();
	// Adds an effect of class p_effect_class to the compositor of each
	// WorldEnvironment (creating one where there is none) and of each camera
	// that has its own compositor, in the running scenes and in the ones added
	// later. A compositor that already has one keeps it.
	static void add_to_running_scenes(const StringName &p_effect_class);
};
