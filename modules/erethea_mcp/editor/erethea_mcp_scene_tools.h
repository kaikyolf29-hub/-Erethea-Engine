/**************************************************************************/
/*  erethea_mcp_scene_tools.h                                             */
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

// The MCP tools that read and change scenes, nodes and scripts in the editor.
// Changes to the scene being edited go through the editor's undo history.
namespace EretheaMcpSceneTools {

EretheaMcpToolResult get_scene_tree(const Dictionary &p_arguments);
EretheaMcpToolResult get_node_properties(const Dictionary &p_arguments);
EretheaMcpToolResult open_scene(const Dictionary &p_arguments);
EretheaMcpToolResult create_scene(const Dictionary &p_arguments);
EretheaMcpToolResult create_node(const Dictionary &p_arguments);
EretheaMcpToolResult set_node_properties(const Dictionary &p_arguments);
EretheaMcpToolResult delete_node(const Dictionary &p_arguments);
EretheaMcpToolResult write_script(const Dictionary &p_arguments);
EretheaMcpToolResult attach_script(const Dictionary &p_arguments);
EretheaMcpToolResult save_scene(const Dictionary &p_arguments);
EretheaMcpToolResult rescan_files(const Dictionary &p_arguments);

// "levels/forest.tscn" or an absolute path inside the project becomes
// "res://levels/forest.tscn". Paths outside the project are refused.
bool project_path(const Variant &p_path, String &r_path, String &r_error);

} // namespace EretheaMcpSceneTools
