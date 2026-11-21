/**************************************************************************/
/*  erethea_mcp_values.h                                                  */
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

#include "core/object/object.h"
#include "core/variant/variant.h"

// Converts between Godot values and the JSON values that MCP tools exchange.
//
// Out: numbers, booleans and strings stay as they are, resources become their
// path, and other types become Godot literals such as "Vector3(0, 1, 0)".
// In: the same literals are read back, and vectors and colors also come as
// arrays ([0, 1, 0]), colors as "#ff8800", and new resources as
// {"type": "BoxMesh", "properties": {...}}.
namespace EretheaMcpValues {

Variant to_json(const Variant &p_value);

// Converts `p_json` for the property `p_property`. On failure, returns false
// and explains why in `r_error`.
bool from_json(const Variant &p_json, const PropertyInfo &p_property, Variant &r_value, String &r_error);

// Sets properties on `p_object` from a JSON object, all or nothing: on
// failure, nothing is set. Used for new resources and new nodes.
bool set_properties(Object *p_object, const Dictionary &p_properties, String &r_error);

// Finds a property that the editor shows, or returns false.
bool find_property(const Object *p_object, const StringName &p_name, PropertyInfo &r_property);

} // namespace EretheaMcpValues
