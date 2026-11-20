/**************************************************************************/
/*  editor_window_buttons.h                                               */
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

#include "scene/gui/box_container.h"
#include "scene/gui/button.h"

// Erethea: Minimize, maximize and close buttons for a window whose title bar
// is drawn by the editor, on platforms where the system doesn't draw them over
// an extended title bar (WINDOW_FLAG_EXTEND_TO_TITLE on Windows).
//
// Put it at the end of the title bar. The buttons reach the top and side edges
// of the window, like the system ones, so that they can be hit by throwing the
// mouse into the corner of a maximized window.

class EditorWindowButton : public Button {
	GDCLASS(EditorWindowButton, Button);

public:
	enum Kind {
		KIND_MINIMIZE,
		KIND_MAXIMIZE,
		KIND_CLOSE,
	};

private:
	Kind kind = KIND_MINIMIZE;
	bool edge = false;

	Rect2 _get_extended_rect() const;
	void _draw_glyph(const Rect2 &p_rect, const Color &p_color);

protected:
	void _notification(int p_what);
	static void _bind_methods() {}

public:
	virtual bool has_point(const Point2 &p_point) const override;

	void set_edge(bool p_edge);

	EditorWindowButton(Kind p_kind = KIND_MINIMIZE);
};

class EditorWindowButtons : public HBoxContainer {
	GDCLASS(EditorWindowButtons, HBoxContainer);

	EditorWindowButton *minimize_button = nullptr;
	EditorWindowButton *maximize_button = nullptr;
	EditorWindowButton *close_button = nullptr;

	void _update_buttons();
	void _minimize();
	void _toggle_maximize();
	void _close();

protected:
	void _notification(int p_what);
	static void _bind_methods() {}

public:
	EditorWindowButtons();
};
