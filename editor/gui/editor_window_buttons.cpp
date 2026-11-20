/**************************************************************************/
/*  editor_window_buttons.cpp                                             */
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

#include "editor_window_buttons.h"

#include "core/object/callable_mp.h"
#include "editor/editor_string_names.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/box_container.h"
#include "scene/main/window.h"
#include "servers/display/display_server.h"

// Sizes of the Windows 11 caption buttons, at 100% scale.
static constexpr int BUTTON_WIDTH = 46;
static constexpr int GLYPH_SIZE = 10;

// Colors of the Windows 11 close button.
static const Color CLOSE_HOVER_COLOR = Color(0.769, 0.169, 0.110); // #C42B1C
static const Color CLOSE_PRESSED_COLOR = Color(0.784, 0.235, 0.192); // #C83C31

Rect2 EditorWindowButton::get_extended_rect() const {
	Rect2 rect = Rect2(Point2(), get_size());
	if (!is_inside_tree()) {
		return rect;
	}

	// Reach the top edge of the window.
	const Point2 global_position = get_global_position();
	rect.position.y -= global_position.y;
	rect.size.y += global_position.y;

	if (edge) {
		// Reach the side edge of the window.
		if (is_layout_rtl()) {
			rect.position.x -= global_position.x;
			rect.size.x += global_position.x;
		} else {
			rect.size.x = MAX(rect.size.x, get_viewport_rect().size.x - global_position.x);
		}
	}
	return rect;
}

void EditorWindowButton::_draw_glyph(const Rect2 &p_rect, const Color &p_color) {
	const int size = MAX(8, int(Math::round(GLYPH_SIZE * EDSCALE)));
	const int stroke = MAX(1, int(Math::round(EDSCALE)));
	const Point2 origin = (p_rect.position + (p_rect.size - Size2(size, size)) / 2).floor();

	// Square outlines are drawn without overlapping corners, so that the glyph
	// stays even when it is drawn with transparency.
	const auto draw_square = [&](const Rect2 &p_square) {
		const Point2 pos = p_square.position;
		const Size2 sz = p_square.size;
		draw_rect(Rect2(pos.x, pos.y, sz.x, stroke), p_color);
		draw_rect(Rect2(pos.x, pos.y + sz.y - stroke, sz.x, stroke), p_color);
		draw_rect(Rect2(pos.x, pos.y + stroke, stroke, sz.y - stroke * 2), p_color);
		draw_rect(Rect2(pos.x + sz.x - stroke, pos.y + stroke, stroke, sz.y - stroke * 2), p_color);
	};

	switch (kind) {
		case KIND_MINIMIZE: {
			draw_rect(Rect2(origin.x, origin.y + size / 2, size, stroke), p_color);
		} break;
		case KIND_MAXIMIZE: {
			const Window *win = get_window();
			if (win && win->get_mode() == Window::MODE_MAXIMIZED) {
				// "Restore": a square in front of another one.
				const int offset = MAX(2, int(Math::round(2 * EDSCALE)));
				const int inner = size - offset;
				draw_square(Rect2(origin.x, origin.y + offset, inner, inner));
				draw_rect(Rect2(origin.x + offset, origin.y, inner, stroke), p_color);
				draw_rect(Rect2(origin.x + size - stroke, origin.y + stroke, stroke, inner - stroke), p_color);
			} else {
				draw_square(Rect2(origin, Size2(size, size)));
			}
		} break;
		case KIND_CLOSE: {
			const Vector2 half_pixel = Vector2(0.5, 0.5);
			draw_line(origin + half_pixel, origin + Vector2(size, size) - half_pixel, p_color, stroke, true);
			draw_line(origin + Vector2(size - 0.5, 0.5), origin + Vector2(0.5, size - 0.5), p_color, stroke, true);
		} break;
	}
}

void EditorWindowButton::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_THEME_CHANGED: {
			set_custom_minimum_size(Size2(Math::round(BUTTON_WIDTH * EDSCALE), 0));
		} break;
		case NOTIFICATION_WM_WINDOW_FOCUS_IN:
		case NOTIFICATION_WM_WINDOW_FOCUS_OUT: {
			queue_redraw();
		} break;
		case NOTIFICATION_DRAW: {
			const DrawMode mode = get_draw_mode();
			const bool pressed = mode == DRAW_PRESSED || mode == DRAW_HOVER_PRESSED;
			const bool hovered = pressed || mode == DRAW_HOVER;

			Color glyph_color = get_theme_color(SceneStringName(font_color));
			Color hover_color = Color(0, 0, 0, 0);
			if (kind == KIND_CLOSE && hovered) {
				hover_color = pressed ? CLOSE_PRESSED_COLOR : CLOSE_HOVER_COLOR;
				glyph_color = Color(1, 1, 1);
			} else if (hovered) {
				hover_color = glyph_color;
				hover_color.a = pressed ? 0.06 : 0.1;
			}

			const Window *win = get_window();
			if (!hovered && win && !win->has_focus()) {
				glyph_color.a *= 0.45;
			}

			const Rect2 extended_rect = get_extended_rect();
			if (background_color.a > 0) {
				draw_rect(extended_rect, background_color);
			}
			if (hover_color.a > 0) {
				draw_rect(extended_rect, hover_color);
			}
			_draw_glyph(Rect2(Point2(), get_size()), glyph_color);
		} break;
	}
}

bool EditorWindowButton::has_point(const Point2 &p_point) const {
	return get_extended_rect().has_point(p_point);
}

void EditorWindowButton::set_edge(bool p_edge) {
	edge = p_edge;
	queue_redraw();
}

void EditorWindowButton::set_background_color(const Color &p_color) {
	if (background_color == p_color) {
		return;
	}
	background_color = p_color;
	queue_redraw();
}

EditorWindowButton::EditorWindowButton(Kind p_kind) {
	kind = p_kind;
	set_flat(true);
	set_focus_mode(FOCUS_ACCESSIBILITY);
}

static void _propagate_close_request(Node *p_node) {
	p_node->notification(Node::NOTIFICATION_WM_CLOSE_REQUEST);
	for (int i = 0; i < p_node->get_child_count(); i++) {
		Node *child = p_node->get_child(i);
		if (Object::cast_to<Window>(child)) {
			continue; // Subwindows get their own close requests.
		}
		_propagate_close_request(child);
	}
}

void EditorWindowButtons::_update_buttons() {
	const Window *win = get_window();
	const bool maximized = win && win->get_mode() == Window::MODE_MAXIMIZED;
	const String maximize_text = maximized ? TTRC("Restore") : TTRC("Maximize");
	maximize_button->set_tooltip_text(maximize_text);
	maximize_button->set_accessibility_name(maximize_text);

	minimize_button->queue_redraw();
	maximize_button->queue_redraw();
	close_button->queue_redraw();
	_update_box();
}

void EditorWindowButtons::_update_box() {
	if (!is_inside_tree()) {
		return;
	}

	// Stay where the title bar puts us, unless it is cut off by the side of
	// the window.
	const Point2 global_position = get_global_position();
	const real_t width = box->get_combined_minimum_size().x;
	const real_t viewport_width = get_viewport_rect().size.x;
	real_t x = global_position.x;
	if (is_layout_rtl()) {
		x = MAX(x, 0);
	} else {
		x = MIN(x, viewport_width - width);
	}
	box->set_position(Point2(x, global_position.y));
	box->set_size(Size2(width, get_size().y));

	// Cover what is under the buttons.
	const Color background_color = Math::is_equal_approx(x, global_position.x) ? Color(0, 0, 0, 0) : get_theme_color(SNAME("background"), EditorStringName(Editor));
	minimize_button->set_background_color(background_color);
	maximize_button->set_background_color(background_color);
	close_button->set_background_color(background_color);

	_update_maximize_button_rect();
}

void EditorWindowButtons::_update_maximize_button_rect() {
	if (!is_inside_tree()) {
		return;
	}
	const Rect2 rect = maximize_button->get_global_transform().xform(maximize_button->get_extended_rect());
	DisplayServer::get_singleton()->window_set_maximize_button_rect(Rect2i(rect.position.round(), rect.size.round()), get_window()->get_window_id());
}

void EditorWindowButtons::_minimize() {
	get_window()->set_mode(Window::MODE_MINIMIZED);
}

void EditorWindowButtons::_toggle_maximize() {
	Window *win = get_window();
	win->set_mode(win->get_mode() == Window::MODE_MAXIMIZED ? Window::MODE_WINDOWED : Window::MODE_MAXIMIZED);
}

void EditorWindowButtons::_close() {
	// Same as a click on the system close button, see Window::_event_callback().
	Window *win = get_window();
	_propagate_close_request(win);
	win->emit_signal(SNAME("close_requested"));
}

void EditorWindowButtons::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			get_window()->connect(SNAME("size_changed"), callable_mp(this, &EditorWindowButtons::_update_buttons));
			_update_buttons();
		} break;
		case NOTIFICATION_EXIT_TREE: {
			get_window()->disconnect(SNAME("size_changed"), callable_mp(this, &EditorWindowButtons::_update_buttons));
			DisplayServer::get_singleton()->window_set_maximize_button_rect(Rect2i(), get_window()->get_window_id());
		} break;
		case NOTIFICATION_RESIZED:
		case NOTIFICATION_TRANSFORM_CHANGED:
		case NOTIFICATION_THEME_CHANGED:
		case NOTIFICATION_LAYOUT_DIRECTION_CHANGED: {
			_update_box();
		} break;
		case NOTIFICATION_VISIBILITY_CHANGED: {
			box->set_visible(is_visible_in_tree());
		} break;
	}
}

Size2 EditorWindowButtons::get_minimum_size() const {
	return Size2(box->get_combined_minimum_size().x, 0);
}

EditorWindowButtons::EditorWindowButtons() {
	set_mouse_filter(MOUSE_FILTER_IGNORE);
	set_notify_transform(true);

	box = memnew(HBoxContainer);
	box->set_as_top_level(true);
	box->add_theme_constant_override("separation", 0);
	box->connect(SceneStringName(minimum_size_changed), callable_mp((Control *)this, &Control::update_minimum_size));
	add_child(box, false, INTERNAL_MODE_FRONT);

	minimize_button = memnew(EditorWindowButton(EditorWindowButton::KIND_MINIMIZE));
	minimize_button->set_tooltip_text(TTRC("Minimize"));
	minimize_button->set_accessibility_name(TTRC("Minimize"));
	minimize_button->connect(SceneStringName(pressed), callable_mp(this, &EditorWindowButtons::_minimize));
	box->add_child(minimize_button);

	maximize_button = memnew(EditorWindowButton(EditorWindowButton::KIND_MAXIMIZE));
	maximize_button->connect(SceneStringName(pressed), callable_mp(this, &EditorWindowButtons::_toggle_maximize));
	maximize_button->connect(SceneStringName(item_rect_changed), callable_mp(this, &EditorWindowButtons::_update_maximize_button_rect));
	box->add_child(maximize_button);

	close_button = memnew(EditorWindowButton(EditorWindowButton::KIND_CLOSE));
	close_button->set_tooltip_text(TTRC("Close"));
	close_button->set_accessibility_name(TTRC("Close"));
	close_button->set_edge(true);
	close_button->connect(SceneStringName(pressed), callable_mp(this, &EditorWindowButtons::_close));
	box->add_child(close_button);
}
