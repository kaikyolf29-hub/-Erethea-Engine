/**************************************************************************/
/*  erethea_scene_effects.cpp                                             */
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

#include "erethea_scene_effects.h"

#include "core/config/engine.h"
#include "core/config/project_settings.h"
#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "scene/3d/camera_3d.h"
#include "scene/3d/world_environment.h"
#include "scene/main/scene_tree.h"
#include "scene/main/window.h"
#include "scene/resources/compositor.h"

EretheaSceneEffects::QualityPreset EretheaSceneEffects::get_automatic_quality_preset() {
	Engine *engine = Engine::get_singleton();
	if (int(GLOBAL_GET("rendering/erethea/quality_preset")) != 4 || !engine->has_singleton("EretheaQuality")) {
		return QUALITY_UNKNOWN;
	}
	return QualityPreset(int(engine->get_singleton_object("EretheaQuality")->call("get_recommended_preset")));
}

void EretheaSceneEffects::_add_to_node(Node *p_node, const StringName &p_effect_class) {
	WorldEnvironment *environment = Object::cast_to<WorldEnvironment>(p_node);
	Camera3D *camera = Object::cast_to<Camera3D>(p_node);
	if (environment == nullptr && camera == nullptr) {
		return;
	}
	Ref<Compositor> compositor = environment != nullptr ? environment->get_compositor() : camera->get_compositor();
	if (compositor.is_null()) {
		// A camera without its own compositor uses the environment's.
		if (camera != nullptr) {
			return;
		}
		compositor.instantiate();
		environment->set_compositor(compositor);
	}
	TypedArray<CompositorEffect> effects = compositor->get_compositor_effects();
	for (int i = 0; i < effects.size(); i++) {
		const Object *effect = effects[i];
		if (effect != nullptr && effect->is_class(p_effect_class)) {
			return;
		}
	}
	Ref<CompositorEffect> effect = Object::cast_to<CompositorEffect>(ClassDB::instantiate(p_effect_class));
	ERR_FAIL_COND(effect.is_null());
	effects.push_back(effect);
	compositor->set_compositor_effects(effects);
}

void EretheaSceneEffects::_add_to_tree(Node *p_node, const StringName &p_effect_class) {
	_add_to_node(p_node, p_effect_class);
	for (int i = 0; i < p_node->get_child_count(); i++) {
		_add_to_tree(p_node->get_child(i), p_effect_class);
	}
}

void EretheaSceneEffects::_node_added(Node *p_node) {
	for (const StringName &effect_class : effect_classes) {
		_add_to_node(p_node, effect_class);
	}
}

void EretheaSceneEffects::add_to_running_scenes(const StringName &p_effect_class) {
	SceneTree *tree = SceneTree::get_singleton();
	if (tree == nullptr || tree->get_root() == nullptr || effect_classes.has(p_effect_class)) {
		return;
	}
	// A signal takes a callable once, even with other arguments bound: one
	// connection adds all the effects.
	if (effect_classes.is_empty()) {
		tree->connect("node_added", callable_mp_static(&EretheaSceneEffects::_node_added));
	}
	effect_classes.push_back(p_effect_class);
	_add_to_tree(tree->get_root(), p_effect_class);
}
