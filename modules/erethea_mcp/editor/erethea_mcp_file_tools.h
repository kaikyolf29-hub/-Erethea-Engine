/**************************************************************************/
/*  erethea_mcp_file_tools.h                                              */
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

#include "../erethea_mcp_protocol.h"

// The MCP tools that read the project's files and change text files, for
// agents that can't open the project folder themselves, such as the
// assistant panel. Scenes are changed through the scene tools instead.
namespace EretheaMcpFileTools {

EretheaMcpToolResult read_file(const Dictionary &p_arguments);
EretheaMcpToolResult list_files(const Dictionary &p_arguments);
EretheaMcpToolResult edit_file(const Dictionary &p_arguments);

// Files that read_file shows as text.
bool is_text_file(const String &p_path);
// Text files that edit_file may change: not scenes, resources or the project
// settings, which the editor keeps in memory and would write over.
bool is_editable_text_file(const String &p_path);

// Replaces `p_old` with `p_new` in `p_text`: exactly one occurrence, or every
// one with `p_all`. Returns how many were replaced (0 when not found, or the
// count when there are several and `p_all` is false, without replacing).
int replace_text(const String &p_text, const String &p_old, const String &p_new, bool p_all, String &r_text);

} // namespace EretheaMcpFileTools
