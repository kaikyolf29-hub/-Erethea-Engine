/**************************************************************************/
/*  erethea_terrain_editor_plugin.cpp                                     */
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

#include "erethea_terrain_editor_plugin.h"

#include "core/input/input_event.h"
#include "core/object/callable_mp.h"
#include "core/string/translation_server.h"
#include "editor/editor_undo_redo_manager.h"
#include "scene/3d/camera_3d.h"
#include "scene/gui/box_container.h"
#include "scene/gui/button.h"
#include "scene/gui/label.h"
#include "scene/gui/option_button.h"
#include "scene/gui/spin_box.h"

using namespace EretheaTerrainBrush;

EretheaTerrainBrush::Settings EretheaTerrainEditorPlugin::_get_settings() const {
	Settings settings;
	settings.tool = Tool(CLAMP(tool_button->get_selected(), 0, int(TOOL_MAX) - 1));
	settings.radius = radius_spin->get_value();
	settings.strength = strength_spin->get_value();
	settings.material = int(material_spin->get_value());
	return settings;
}

bool EretheaTerrainEditorPlugin::_raycast_mouse(Camera3D *p_camera, const Vector2 &p_position, Vector3 &r_hit) const {
	ERR_FAIL_NULL_V(terrain, false);
	Ref<EretheaTerrainData> data = terrain->get_data();
	if (data.is_null() || data->get_height_map().is_null()) {
		return false;
	}
	const Transform3D to_local = terrain->get_global_transform().affine_inverse();
	const Vector3 origin = to_local.xform(p_camera->project_ray_origin(p_position));
	const Vector3 direction = to_local.basis.xform(p_camera->project_ray_normal(p_position)).normalized();
	// Far enough to cross the whole terrain from where the camera is.
	const real_t max_distance = origin.length() + data->get_size().length();
	return raycast(data, origin, direction, max_distance, r_hit);
}

void EretheaTerrainEditorPlugin::_start_stroke(const Vector3 &p_hit) {
	stroke_settings = _get_settings();
	// The flatten tool levels to the height where the stroke starts.
	stroke_settings.target_height = p_hit.y;
	stroke.begin(terrain->get_data());
	last_dab = Vector2(p_hit.x, p_hit.z);
	stroke.dab(stroke_settings, last_dab);
}

void EretheaTerrainEditorPlugin::_end_stroke() {
	if (!stroke.is_active()) {
		return;
	}
	Ref<EretheaTerrainData> data = terrain ? terrain->get_data() : Ref<EretheaTerrainData>();
	EretheaTerrainBrush::Stroke::Result result;
	if (!stroke.end(result) || data.is_null()) {
		return;
	}
	// The dabs are already in the data: the do step only has to be there for redo.
	const bool painting = stroke_settings.tool >= TOOL_PAINT_BASE;
	EditorUndoRedoManager *undo_redo = get_undo_redo();
	undo_redo->create_action(painting ? TTR("Paint Terrain") : TTR("Sculpt Terrain"), UndoRedo::MERGE_DISABLE, terrain);
	undo_redo->add_do_method(data.ptr(), "set_region", result.rect, result.after);
	undo_redo->add_undo_method(data.ptr(), "set_region", result.rect, result.before);
	undo_redo->commit_action(false);
	// The terrain and its data have to be saved with the new heights and materials.
	data->set_edited(true);
}

void EretheaTerrainEditorPlugin::_terrain_toggled(bool p_pressed) {
	if (!p_pressed) {
		_end_stroke();
	}
}

bool EretheaTerrainEditorPlugin::handles(Object *p_object) const {
	return Object::cast_to<EretheaTerrain3D>(p_object) != nullptr;
}

void EretheaTerrainEditorPlugin::edit(Object *p_object) {
	_end_stroke();
	terrain = Object::cast_to<EretheaTerrain3D>(p_object);
}

void EretheaTerrainEditorPlugin::make_visible(bool p_visible) {
	toolbar->set_visible(p_visible);
	if (!p_visible) {
		_end_stroke();
		terrain_button->set_pressed(false);
		terrain = nullptr;
	}
}

EditorPlugin::AfterGUIInput EretheaTerrainEditorPlugin::forward_3d_gui_input(Camera3D *p_camera, const Ref<InputEvent> &p_event) {
	if (!terrain || !terrain_button->is_pressed()) {
		return EditorPlugin::AFTER_GUI_INPUT_PASS;
	}

	Ref<InputEventMouseButton> button = p_event;
	if (button.is_valid() && button->get_button_index() == MouseButton::LEFT) {
		if (button->is_pressed()) {
			Vector3 hit;
			if (_raycast_mouse(p_camera, button->get_position(), hit)) {
				_start_stroke(hit);
				return EditorPlugin::AFTER_GUI_INPUT_STOP;
			}
			return EditorPlugin::AFTER_GUI_INPUT_PASS;
		}
		if (stroke.is_active()) {
			_end_stroke();
			return EditorPlugin::AFTER_GUI_INPUT_STOP;
		}
		return EditorPlugin::AFTER_GUI_INPUT_PASS;
	}

	Ref<InputEventMouseMotion> motion = p_event;
	if (motion.is_valid() && stroke.is_active()) {
		Vector3 hit;
		if (_raycast_mouse(p_camera, motion->get_position(), hit)) {
			const Vector2 at(hit.x, hit.z);
			// A dab every quarter of a radius, so that the strength doesn't
			// depend on how fast the mouse moves.
			if (at.distance_to(last_dab) >= stroke_settings.radius * 0.25) {
				stroke.dab(stroke_settings, at);
				last_dab = at;
			}
		}
		return EditorPlugin::AFTER_GUI_INPUT_STOP;
	}
	return EditorPlugin::AFTER_GUI_INPUT_PASS;
}

void EretheaTerrainEditorPlugin::_notification(int p_what) {
	if (p_what == NOTIFICATION_PREDELETE) {
		_end_stroke();
	}
}

EretheaTerrainEditorPlugin::EretheaTerrainEditorPlugin() {
	toolbar = memnew(HBoxContainer);
	toolbar->hide();
	add_control_to_container(CONTAINER_SPATIAL_EDITOR_MENU, toolbar);

	terrain_button = memnew(Button);
	terrain_button->set_text(TTR("Terrain"));
	terrain_button->set_toggle_mode(true);
	terrain_button->set_tooltip_text(TTR("Sculpt and paint the selected EretheaTerrain3D: drag with the left mouse button over the terrain."));
	terrain_button->connect("toggled", callable_mp(this, &EretheaTerrainEditorPlugin::_terrain_toggled));
	toolbar->add_child(terrain_button);

	tool_button = memnew(OptionButton);
	tool_button->add_item(TTR("Raise"), TOOL_RAISE);
	tool_button->add_item(TTR("Lower"), TOOL_LOWER);
	tool_button->add_item(TTR("Smooth"), TOOL_SMOOTH);
	tool_button->add_item(TTR("Flatten"), TOOL_FLATTEN);
	tool_button->add_item(TTR("Paint Base Material"), TOOL_PAINT_BASE);
	tool_button->add_item(TTR("Paint Overlay Material"), TOOL_PAINT_OVERLAY);
	tool_button->add_item(TTR("Erase Overlay"), TOOL_ERASE_OVERLAY);
	tool_button->set_tooltip_text(TTR("Brush. Flatten goes to the height where the drag starts."));
	toolbar->add_child(tool_button);

	Label *radius_label = memnew(Label);
	radius_label->set_text(TTR("Radius"));
	toolbar->add_child(radius_label);
	radius_spin = memnew(SpinBox);
	radius_spin->set_min(0.5);
	radius_spin->set_max(512);
	radius_spin->set_step(0.5);
	radius_spin->set_value(8);
	radius_spin->set_suffix("m");
	radius_spin->set_tooltip_text(TTR("Radius of the brush, in meters."));
	toolbar->add_child(radius_spin);

	Label *strength_label = memnew(Label);
	strength_label->set_text(TTR("Strength"));
	toolbar->add_child(strength_label);
	strength_spin = memnew(SpinBox);
	strength_spin->set_min(0.01);
	strength_spin->set_max(1);
	strength_spin->set_step(0.01);
	strength_spin->set_value(0.5);
	strength_spin->set_tooltip_text(TTR("What one dab does at its center: meters of height for Raise and Lower, the share of the way for Smooth and Flatten, the weight added for the overlay."));
	toolbar->add_child(strength_spin);

	Label *material_label = memnew(Label);
	material_label->set_text(TTR("Material"));
	toolbar->add_child(material_label);
	material_spin = memnew(SpinBox);
	material_spin->set_min(0);
	material_spin->set_max(EretheaTerrainData::MAX_MATERIALS - 1);
	material_spin->set_step(1);
	material_spin->set_value(0);
	material_spin->set_tooltip_text(TTR("Index of the material (0 to 31) that the paint brushes use."));
	toolbar->add_child(material_spin);
}
