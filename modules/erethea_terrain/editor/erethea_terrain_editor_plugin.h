/**************************************************************************/
/*  erethea_terrain_editor_plugin.h                                       */
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

#include "../erethea_terrain_3d.h"
#include "../erethea_terrain_brush.h"

#include "editor/plugins/editor_plugin.h"

class Button;
class HBoxContainer;
class OptionButton;
class SpinBox;

// Sculpts and paints an EretheaTerrain3D in the 3D editor. When a terrain is
// selected, a toolbar appears in the 3D editor's menu bar. Once its Terrain
// button is on, dragging the left mouse button over the terrain applies the
// chosen brush there, and each drag is one step of the undo history.
class EretheaTerrainEditorPlugin : public EditorPlugin {
	GDCLASS(EretheaTerrainEditorPlugin, EditorPlugin);

	EretheaTerrain3D *terrain = nullptr;
	HBoxContainer *toolbar = nullptr;
	Button *terrain_button = nullptr;
	OptionButton *tool_button = nullptr;
	SpinBox *radius_spin = nullptr;
	SpinBox *strength_spin = nullptr;
	SpinBox *material_spin = nullptr;

	EretheaTerrainBrush::Stroke stroke;
	EretheaTerrainBrush::Settings stroke_settings;
	Vector2 last_dab;

	EretheaTerrainBrush::Settings _get_settings() const;
	// Where the mouse is over the terrain, as x and z in its space. False if it isn't.
	bool _raycast_mouse(Camera3D *p_camera, const Vector2 &p_position, Vector3 &r_hit) const;
	void _start_stroke(const Vector3 &p_hit);
	void _end_stroke();
	void _terrain_toggled(bool p_pressed);

protected:
	void _notification(int p_what);

public:
	virtual String get_plugin_name() const override { return "EretheaTerrain"; }
	virtual bool handles(Object *p_object) const override;
	virtual void edit(Object *p_object) override;
	virtual void make_visible(bool p_visible) override;
	virtual EditorPlugin::AfterGUIInput forward_3d_gui_input(Camera3D *p_camera, const Ref<InputEvent> &p_event) override;

	EretheaTerrainEditorPlugin();
};
