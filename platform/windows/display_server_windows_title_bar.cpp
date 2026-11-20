/**************************************************************************/
/*  display_server_windows_title_bar.cpp                                  */
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

// Erethea: Integrated title bar on Windows (WINDOW_FLAG_EXTEND_TO_TITLE).
//
// The window keeps its normal WS_OVERLAPPEDWINDOW style, so Aero Snap, the
// minimize and maximize animations, the drop shadow and the Windows 11
// rounded corners keep working. Only the caption goes away:
// - WM_NCCALCSIZE gives the caption and the top border to the client area,
//   and keeps the left, right and bottom resize borders computed by Windows.
// - WM_NCHITTEST turns the top edge of the client area back into a resize
//   border.
// The application draws its own title bar and caption buttons (the editor
// uses EditorWindowButtons), and moves the window with window_start_drag().
//
// The application also tells where its maximize button is, with
// window_set_maximize_button_rect(). WM_NCHITTEST reports that area as
// HTMAXBUTTON, so that Windows 11 shows its snap layouts when the mouse hovers
// it. Windows then sends non-client mouse messages for that area, which are
// passed on to the engine as client ones, so that the button still reacts.

#include "display_server_windows.h"

#include "core/config/engine.h"

#include <shellapi.h>

// Space left free on the side of an auto-hide taskbar, so that it can still
// be revealed when a window is maximized.
static constexpr int AUTOHIDE_TASKBAR_GAP = 2;

bool DisplayServerWindows::_is_extend_to_title_supported() const {
	if (os_ver.dwBuildNumber < 10240) {
		return false; // Windows 10+ only. Also excludes Wine, which leaves `os_ver` empty.
	}
	// Only the editor and the project manager draw their own caption buttons.
	// Exported projects keep the system title bar, so a project that enables
	// `display/window/size/extend_to_title` for macOS looks the same on Windows
	// as it does in Godot.
	return Engine::get_singleton()->is_editor_hint() || Engine::get_singleton()->is_project_manager_hint();
}

bool DisplayServerWindows::_is_window_extended_to_title(const WindowData &p_wd) const {
	return p_wd.extend_to_title && !p_wd.fullscreen && !p_wd.borderless && !p_wd.parent_hwnd;
}

void DisplayServerWindows::_window_adjust_rect(const WindowData *p_wd, RECT &r_rect, DWORD p_style, DWORD p_style_ex) const {
	const LONG top = r_rect.top;
	AdjustWindowRectEx(&r_rect, p_style, FALSE, p_style_ex);
	if (p_wd && _is_window_extended_to_title(*p_wd)) {
		// No caption and no top border, see _extend_to_title_nccalcsize().
		r_rect.top = top;
	}
}

LRESULT DisplayServerWindows::_extend_to_title_nccalcsize(WindowData &p_wd, HWND hWnd, WPARAM wParam, LPARAM lParam) {
	NCCALCSIZE_PARAMS *params = reinterpret_cast<NCCALCSIZE_PARAMS *>(lParam);
	const RECT window_rect = params->rgrc[0];

	// Let Windows compute the borders first.
	const LRESULT ret = DefWindowProcW(hWnd, WM_NCCALCSIZE, wParam, lParam);
	if (ret != 0) {
		return ret;
	}

	RECT &client_rect = params->rgrc[0];

	// The top border of a sizing frame is as thick as the side ones.
	p_wd.extend_to_title_border = MAX(0, int(client_rect.left - window_rect.left));

	// Give the caption and the top border to the client area.
	client_rect.top = window_rect.top;

	if (IsZoomed(hWnd)) {
		// A maximized window overflows the monitor by its border thickness on
		// every side. Keep the top border off-screen, like the other ones.
		client_rect.top += p_wd.extend_to_title_border;

		// If the client area covered the whole monitor, Windows would treat the
		// window as fullscreen and an auto-hide taskbar could not be revealed.
		APPBARDATA state = {};
		state.cbSize = sizeof(APPBARDATA);
		if (SHAppBarMessage(ABM_GETSTATE, &state) & ABS_AUTOHIDE) {
			HMONITOR monitor = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
			MONITORINFO monitor_info = {};
			monitor_info.cbSize = sizeof(MONITORINFO);
			if (monitor && GetMonitorInfoW(monitor, &monitor_info)) {
				const UINT edges[] = { ABE_TOP, ABE_BOTTOM, ABE_LEFT, ABE_RIGHT };
				for (UINT edge : edges) {
					APPBARDATA bar = {};
					bar.cbSize = sizeof(APPBARDATA);
					bar.uEdge = edge;
					bar.rc = monitor_info.rcMonitor;
					if (!SHAppBarMessage(ABM_GETAUTOHIDEBAREX, &bar)) {
						continue;
					}
					switch (edge) {
						case ABE_TOP: {
							client_rect.top += AUTOHIDE_TASKBAR_GAP;
						} break;
						case ABE_BOTTOM: {
							client_rect.bottom -= AUTOHIDE_TASKBAR_GAP;
						} break;
						case ABE_LEFT: {
							client_rect.left += AUTOHIDE_TASKBAR_GAP;
						} break;
						case ABE_RIGHT: {
							client_rect.right -= AUTOHIDE_TASKBAR_GAP;
						} break;
					}
				}
			}
		}
	}

	return 0;
}

LRESULT DisplayServerWindows::_extend_to_title_nchittest(WindowData &p_wd, HWND hWnd, WPARAM wParam, LPARAM lParam) {
	// Windows still handles the left, right and bottom borders.
	const LRESULT hit = DefWindowProcW(hWnd, WM_NCHITTEST, wParam, lParam);
	if (hit != HTCLIENT) {
		return hit;
	}

	POINT point = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
	ScreenToClient(hWnd, &point);

	// The top border now lies inside the client area.
	const int border = p_wd.extend_to_title_border;
	if (p_wd.resizable && !IsZoomed(hWnd) && point.y >= 0 && point.y < border) {
		RECT client_rect;
		GetClientRect(hWnd, &client_rect);
		if (point.x < border) {
			return HTTOPLEFT;
		}
		if (point.x >= client_rect.right - border) {
			return HTTOPRIGHT;
		}
		return HTTOP;
	}

	if (!p_wd.no_max_btn && p_wd.maximize_button_rect.has_point(Point2i(point.x, point.y))) {
		return HTMAXBUTTON;
	}

	return HTCLIENT;
}

bool DisplayServerWindows::_extend_to_title_is_cursor_on_maximize_button(const WindowData &p_wd, HWND hWnd) const {
	if (!p_wd.maximize_button_rect.has_area()) {
		return false;
	}
	POINT cursor;
	if (!GetCursorPos(&cursor) || WindowFromPoint(cursor) != hWnd) {
		return false;
	}
	ScreenToClient(hWnd, &cursor);
	return p_wd.maximize_button_rect.has_point(Point2i(cursor.x, cursor.y));
}

static void _track_mouse_leave(HWND hWnd, DWORD p_flags) {
	TRACKMOUSEEVENT tme = {};
	tme.cbSize = sizeof(TRACKMOUSEEVENT);
	tme.dwFlags = p_flags;
	tme.hwndTrack = hWnd;
	tme.dwHoverTime = HOVER_DEFAULT;
	TrackMouseEvent(&tme);
}

bool DisplayServerWindows::_extend_to_title_nc_mouse(WindowData &p_wd, HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT &r_result) {
	if (uMsg == WM_NCMOUSELEAVE) {
		POINT cursor;
		GetCursorPos(&cursor);
		if (WindowFromPoint(cursor) == hWnd) {
			// Back in the client area, Windows needs a new request to report when the cursor leaves it.
			_track_mouse_leave(hWnd, TME_LEAVE);
			return false;
		}
		// The cursor left the window from the maximize button.
		r_result = WndProc(hWnd, WM_MOUSELEAVE, 0, 0);
		return true;
	}

	if (wParam != HTMAXBUTTON) {
		return false;
	}

	POINT point = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
	ScreenToClient(hWnd, &point);
	const LPARAM client_lparam = MAKELPARAM(point.x, point.y);

	WPARAM keys = 0;
	if (GetKeyState(VK_LBUTTON) < 0) {
		keys |= MK_LBUTTON;
	}
	if (GetKeyState(VK_RBUTTON) < 0) {
		keys |= MK_RBUTTON;
	}
	if (GetKeyState(VK_MBUTTON) < 0) {
		keys |= MK_MBUTTON;
	}
	if (GetKeyState(VK_SHIFT) < 0) {
		keys |= MK_SHIFT;
	}
	if (GetKeyState(VK_CONTROL) < 0) {
		keys |= MK_CONTROL;
	}

	// The button is drawn by the engine: pass the messages on as client ones,
	// and keep DefWindowProc() from tracking a system caption button.
	switch (uMsg) {
		case WM_NCMOUSEMOVE: {
			WndProc(hWnd, WM_MOUSEMOVE, keys, client_lparam);
			// Report when the cursor leaves the button, which is non-client area.
			_track_mouse_leave(hWnd, TME_LEAVE | TME_NONCLIENT);
			r_result = 0;
			return true;
		}
		case WM_NCLBUTTONDOWN: {
			r_result = WndProc(hWnd, WM_LBUTTONDOWN, keys | MK_LBUTTON, client_lparam);
			return true;
		}
		case WM_NCLBUTTONDBLCLK: {
			r_result = WndProc(hWnd, WM_LBUTTONDBLCLK, keys | MK_LBUTTON, client_lparam);
			return true;
		}
		case WM_NCLBUTTONUP: {
			r_result = WndProc(hWnd, WM_LBUTTONUP, keys & ~WPARAM(MK_LBUTTON), client_lparam);
			return true;
		}
	}
	return false;
}

void DisplayServerWindows::window_set_maximize_button_rect(const Rect2i &p_rect, DisplayServerEnums::WindowID p_window) {
	_THREAD_SAFE_METHOD_

	ERR_FAIL_COND(!windows.has(p_window));
	windows[p_window].maximize_button_rect = p_rect;
}
