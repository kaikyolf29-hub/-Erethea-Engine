/**************************************************************************/
/*  erethea_mcp_values.cpp                                                */
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

#include "erethea_mcp_values.h"

#include "core/io/resource.h"
#include "core/io/resource_loader.h"
#include "core/object/class_db.h"
#include "core/variant/variant_parser.h"
#include "core/variant/variant_utility.h"

namespace EretheaMcpValues {

// Longer packed arrays are summarized instead of listed.
static constexpr int MAX_LISTED_ITEMS = 256;

Variant to_json(const Variant &p_value) {
	switch (p_value.get_type()) {
		case Variant::NIL:
		case Variant::BOOL:
		case Variant::INT:
		case Variant::STRING:
			return p_value;
		case Variant::FLOAT: {
			const double number = p_value;
			// JSON has no infinity or NaN.
			return Math::is_finite(number) ? p_value : Variant(VariantUtilityFunctions::var_to_str(p_value));
		}
		case Variant::STRING_NAME:
		case Variant::NODE_PATH:
			return String(p_value);
		case Variant::OBJECT: {
			Object *object = p_value.get_validated_object();
			if (!object) {
				return Variant();
			}
			const Resource *resource = Object::cast_to<Resource>(object);
			if (resource && !resource->get_path().is_empty() && !resource->is_built_in()) {
				return resource->get_path();
			}
			Dictionary description;
			description["type"] = object->get_class();
			return description;
		}
		case Variant::ARRAY: {
			const Array array = p_value;
			Array result;
			for (const Variant &item : array) {
				result.push_back(to_json(item));
			}
			return result;
		}
		case Variant::DICTIONARY: {
			const Dictionary dictionary = p_value;
			Dictionary result;
			for (const KeyValue<Variant, Variant> &entry : dictionary) {
				result[String(entry.key)] = to_json(entry.value);
			}
			return result;
		}
		case Variant::PACKED_BYTE_ARRAY:
		case Variant::PACKED_INT32_ARRAY:
		case Variant::PACKED_INT64_ARRAY:
		case Variant::PACKED_FLOAT32_ARRAY:
		case Variant::PACKED_FLOAT64_ARRAY:
		case Variant::PACKED_STRING_ARRAY:
		case Variant::PACKED_VECTOR2_ARRAY:
		case Variant::PACKED_VECTOR3_ARRAY:
		case Variant::PACKED_COLOR_ARRAY:
		case Variant::PACKED_VECTOR4_ARRAY: {
			const Array array = p_value;
			if (array.size() > MAX_LISTED_ITEMS) {
				return vformat("<%s with %d items>", Variant::get_type_name(p_value.get_type()), array.size());
			}
			return to_json(array);
		}
		case Variant::RID:
		case Variant::CALLABLE:
		case Variant::SIGNAL:
			return String(p_value);
		default:
			// Vectors, colors, transforms...: a Godot literal, which
			// `from_json()` reads back.
			return VariantUtilityFunctions::var_to_str(p_value);
	}
}

static String _example(Variant::Type p_type) {
	switch (p_type) {
		case Variant::VECTOR2:
			return "[1, 2] or \"Vector2(1, 2)\"";
		case Variant::VECTOR2I:
			return "[1, 2] or \"Vector2i(1, 2)\"";
		case Variant::VECTOR3:
			return "[1, 2, 3] or \"Vector3(1, 2, 3)\"";
		case Variant::VECTOR3I:
			return "[1, 2, 3] or \"Vector3i(1, 2, 3)\"";
		case Variant::VECTOR4:
			return "[1, 2, 3, 4] or \"Vector4(1, 2, 3, 4)\"";
		case Variant::VECTOR4I:
			return "[1, 2, 3, 4] or \"Vector4i(1, 2, 3, 4)\"";
		case Variant::COLOR:
			return "\"#ff8800\", [1, 0.5, 0] or \"Color(1, 0.5, 0, 1)\"";
		case Variant::OBJECT:
			return "a resource path such as \"res://material.tres\", or {\"type\": \"BoxMesh\", \"properties\": {}} for a new resource";
		default: {
			Variant value;
			Callable::CallError error;
			Variant::construct(p_type, value, nullptr, 0, error);
			return "\"" + VariantUtilityFunctions::var_to_str(value) + "\"";
		}
	}
}

static bool _fail(const PropertyInfo &p_property, String &r_error) {
	r_error = vformat("expects a %s, such as %s", Variant::get_type_name(p_property.type), _example(p_property.type));
	return false;
}

static bool _is_number(const Variant &p_value) {
	return p_value.get_type() == Variant::INT || p_value.get_type() == Variant::FLOAT;
}

// Reads `p_count` numbers from an array, or from a dictionary with the keys
// in `p_keys` (the missing ones are `p_default`).
static bool _read_numbers(const Variant &p_json, int p_min_count, int p_count, const char *p_keys, double p_default, double *r_numbers, int &r_count) {
	if (p_json.get_type() == Variant::ARRAY) {
		const Array array = p_json;
		if (array.size() < p_min_count || array.size() > p_count) {
			return false;
		}
		for (int i = 0; i < array.size(); i++) {
			if (!_is_number(array[i])) {
				return false;
			}
			r_numbers[i] = array[i];
		}
		for (int i = array.size(); i < p_count; i++) {
			r_numbers[i] = p_default;
		}
		r_count = array.size();
		return true;
	}
	if (p_json.get_type() == Variant::DICTIONARY && p_keys) {
		const Dictionary dictionary = p_json;
		int found = 0;
		for (int i = 0; i < p_count; i++) {
			const String key = String::chr(p_keys[i]);
			r_numbers[i] = p_default;
			if (dictionary.has(key)) {
				if (!_is_number(dictionary[key])) {
					return false;
				}
				r_numbers[i] = dictionary[key];
				found++;
			}
		}
		// Every key must be known, and the required ones present.
		if (found != dictionary.size() || found < p_min_count) {
			return false;
		}
		r_count = p_count;
		return true;
	}
	return false;
}

static bool _whole(const double *p_numbers, int p_count) {
	for (int i = 0; i < p_count; i++) {
		if (p_numbers[i] != Math::floor(p_numbers[i])) {
			return false;
		}
	}
	return true;
}

static bool _vector_from_numbers(Variant::Type p_type, const Variant &p_json, Variant &r_value) {
	double n[4] = {};
	int count = 0;
	switch (p_type) {
		case Variant::VECTOR2:
			if (_read_numbers(p_json, 2, 2, "xy", 0.0, n, count)) {
				r_value = Vector2(n[0], n[1]);
				return true;
			}
			return false;
		case Variant::VECTOR2I:
			if (_read_numbers(p_json, 2, 2, "xy", 0.0, n, count) && _whole(n, 2)) {
				r_value = Vector2i(n[0], n[1]);
				return true;
			}
			return false;
		case Variant::VECTOR3:
			if (_read_numbers(p_json, 3, 3, "xyz", 0.0, n, count)) {
				r_value = Vector3(n[0], n[1], n[2]);
				return true;
			}
			return false;
		case Variant::VECTOR3I:
			if (_read_numbers(p_json, 3, 3, "xyz", 0.0, n, count) && _whole(n, 3)) {
				r_value = Vector3i(n[0], n[1], n[2]);
				return true;
			}
			return false;
		case Variant::VECTOR4:
			if (_read_numbers(p_json, 4, 4, "xyzw", 0.0, n, count)) {
				r_value = Vector4(n[0], n[1], n[2], n[3]);
				return true;
			}
			return false;
		case Variant::VECTOR4I:
			if (_read_numbers(p_json, 4, 4, "xyzw", 0.0, n, count) && _whole(n, 4)) {
				r_value = Vector4i(n[0], n[1], n[2], n[3]);
				return true;
			}
			return false;
		case Variant::QUATERNION:
			if (_read_numbers(p_json, 4, 4, "xyzw", 0.0, n, count)) {
				r_value = Quaternion(n[0], n[1], n[2], n[3]);
				return true;
			}
			return false;
		case Variant::RECT2:
			if (_read_numbers(p_json, 4, 4, nullptr, 0.0, n, count)) {
				r_value = Rect2(n[0], n[1], n[2], n[3]);
				return true;
			}
			return false;
		case Variant::RECT2I:
			if (_read_numbers(p_json, 4, 4, nullptr, 0.0, n, count) && _whole(n, 4)) {
				r_value = Rect2i(n[0], n[1], n[2], n[3]);
				return true;
			}
			return false;
		case Variant::PLANE:
			if (_read_numbers(p_json, 4, 4, nullptr, 0.0, n, count)) {
				r_value = Plane(n[0], n[1], n[2], n[3]);
				return true;
			}
			return false;
		case Variant::COLOR:
			// Alpha is optional.
			if (_read_numbers(p_json, 3, 4, "rgba", 1.0, n, count)) {
				r_value = Color(n[0], n[1], n[2], n[3]);
				return true;
			}
			return false;
		default:
			return false;
	}
}

// A Godot literal of exactly the expected type, such as "Vector3(1, 2, 3)".
static bool _from_literal(Variant::Type p_type, const String &p_text, Variant &r_value) {
	// Only literals of the expected type, so that the text can't construct
	// other objects or load resources.
	if (!p_text.strip_edges().begins_with(Variant::get_type_name(p_type) + "(")) {
		return false;
	}
	VariantParser::StreamString stream;
	stream.s = p_text;
	String error;
	int line = 0;
	Variant value;
	if (VariantParser::parse(&stream, value, error, line) != OK || value.get_type() != p_type) {
		return false;
	}
	r_value = value;
	return true;
}

static bool _enum_from_name(const PropertyInfo &p_property, const String &p_name, Variant &r_value) {
	const String wanted = p_name.to_lower().remove_chars(" _");
	int64_t value = 0;
	for (const String &item : p_property.hint_string.split(",")) {
		String name = item;
		const int colon = item.rfind_char(':');
		if (colon != -1) {
			name = item.substr(0, colon);
			value = item.substr(colon + 1).to_int();
		}
		if (name.to_lower().remove_chars(" _") == wanted) {
			r_value = value;
			return true;
		}
		value++;
	}
	return false;
}

static bool _object_from_json(const Variant &p_json, const PropertyInfo &p_property, Variant &r_value, String &r_error) {
	if (p_json.get_type() == Variant::NIL) {
		r_value = Variant((Object *)nullptr);
		return true;
	}

	Vector<String> accepted;
	if (p_property.hint == PROPERTY_HINT_RESOURCE_TYPE) {
		for (const String &type : p_property.hint_string.split(",", false)) {
			accepted.push_back(type.strip_edges());
		}
	} else if (p_property.hint == PROPERTY_HINT_NODE_TYPE) {
		r_error = "is a reference to a node, which this tool can't set yet";
		return false;
	} else if (p_property.class_name != StringName()) {
		accepted.push_back(p_property.class_name);
	}

	Ref<Resource> resource;
	if (p_json.get_type() == Variant::STRING) {
		const String path = p_json;
		if (!path.begins_with("res://") && !path.begins_with("uid://")) {
			return _fail(p_property, r_error);
		}
		if (!ResourceLoader::exists(path)) {
			r_error = vformat("\"%s\" doesn't exist", path);
			return false;
		}
		resource = ResourceLoader::load(path);
		if (resource.is_null()) {
			r_error = vformat("\"%s\" couldn't be loaded", path);
			return false;
		}
	} else if (p_json.get_type() == Variant::DICTIONARY) {
		const Dictionary description = p_json;
		const String type = description.get("type", String());
		if (!ClassDB::class_exists(type) || !ClassDB::is_parent_class(type, "Resource") || !ClassDB::can_instantiate(type)) {
			r_error = vformat("\"%s\" isn't a type of resource that can be created", type);
			return false;
		}
		resource = Ref<Resource>(Object::cast_to<Resource>(ClassDB::instantiate(type)));
		ERR_FAIL_COND_V(resource.is_null(), false);
		const Variant properties = description.get("properties", Dictionary());
		if (properties.get_type() != Variant::DICTIONARY) {
			r_error = "\"properties\" must be an object";
			return false;
		}
		String error;
		if (!set_properties(resource.ptr(), properties, error)) {
			r_error = vformat("new %s: %s", type, error);
			return false;
		}
	} else {
		return _fail(p_property, r_error);
	}

	if (!accepted.is_empty()) {
		bool matches = false;
		for (const String &type : accepted) {
			matches = matches || resource->is_class(type);
		}
		if (!matches) {
			r_error = vformat("takes a %s, but got a %s", String(" or ").join(accepted), resource->get_class());
			return false;
		}
	}
	r_value = resource;
	return true;
}

// The element of a typed array, from hints such as "int" or "24/17:Texture2D".
static bool _array_element(const PropertyInfo &p_property, PropertyInfo &r_element) {
	if (p_property.hint != PROPERTY_HINT_ARRAY_TYPE || p_property.hint_string.is_empty()) {
		return false;
	}
	const String hint = p_property.hint_string;
	const int colon = hint.find_char(':');
	if (colon != -1) {
		const String type_and_hint = hint.substr(0, colon);
		r_element.type = Variant::Type(type_and_hint.get_slicec('/', 0).to_int());
		r_element.hint = type_and_hint.contains_char('/') ? PropertyHint(type_and_hint.get_slicec('/', 1).to_int()) : PROPERTY_HINT_NONE;
		r_element.hint_string = hint.substr(colon + 1);
		return r_element.type > Variant::NIL && r_element.type < Variant::VARIANT_MAX;
	}
	const Variant::Type type = Variant::get_type_by_name(hint);
	if (type != Variant::VARIANT_MAX) {
		r_element.type = type;
		return true;
	}
	if (ClassDB::class_exists(hint)) {
		r_element.type = Variant::OBJECT;
		r_element.class_name = hint;
		if (ClassDB::is_parent_class(hint, "Resource")) {
			r_element.hint = PROPERTY_HINT_RESOURCE_TYPE;
			r_element.hint_string = hint;
		}
		return true;
	}
	return false;
}

static Variant::Type _packed_element_type(Variant::Type p_type) {
	switch (p_type) {
		case Variant::PACKED_BYTE_ARRAY:
		case Variant::PACKED_INT32_ARRAY:
		case Variant::PACKED_INT64_ARRAY:
			return Variant::INT;
		case Variant::PACKED_FLOAT32_ARRAY:
		case Variant::PACKED_FLOAT64_ARRAY:
			return Variant::FLOAT;
		case Variant::PACKED_STRING_ARRAY:
			return Variant::STRING;
		case Variant::PACKED_VECTOR2_ARRAY:
			return Variant::VECTOR2;
		case Variant::PACKED_VECTOR3_ARRAY:
			return Variant::VECTOR3;
		case Variant::PACKED_COLOR_ARRAY:
			return Variant::COLOR;
		case Variant::PACKED_VECTOR4_ARRAY:
			return Variant::VECTOR4;
		default:
			return Variant::NIL;
	}
}

static bool _array_from_json(const Variant &p_json, const PropertyInfo &p_element, Array &r_array, String &r_error) {
	const Array items = p_json;
	for (int i = 0; i < items.size(); i++) {
		Variant item;
		String error;
		if (!from_json(items[i], p_element, item, error)) {
			r_error = vformat("item %d %s", i, error);
			return false;
		}
		r_array.push_back(item);
	}
	return true;
}

bool from_json(const Variant &p_json, const PropertyInfo &p_property, Variant &r_value, String &r_error) {
	const Variant::Type type = p_property.type;
	const Variant::Type json_type = p_json.get_type();

	switch (type) {
		case Variant::NIL:
			// Untyped, such as some metadata: the value as it comes.
			r_value = p_json;
			return true;
		case Variant::BOOL:
			if (json_type == Variant::BOOL) {
				r_value = p_json;
				return true;
			}
			return _fail(p_property, r_error);
		case Variant::INT:
			if (json_type == Variant::INT) {
				r_value = p_json;
				return true;
			}
			if (json_type == Variant::FLOAT) {
				const double number = p_json;
				if (number != Math::floor(number)) {
					r_error = "expects a whole number";
					return false;
				}
				r_value = int64_t(number);
				return true;
			}
			if (json_type == Variant::STRING && p_property.hint == PROPERTY_HINT_ENUM) {
				if (_enum_from_name(p_property, p_json, r_value)) {
					return true;
				}
				r_error = vformat("expects one of: %s", p_property.hint_string);
				return false;
			}
			return _fail(p_property, r_error);
		case Variant::FLOAT:
			if (_is_number(p_json)) {
				r_value = double(p_json);
				return true;
			}
			return _fail(p_property, r_error);
		case Variant::STRING:
			if (json_type == Variant::STRING) {
				r_value = p_json;
				return true;
			}
			return _fail(p_property, r_error);
		case Variant::STRING_NAME:
			if (json_type == Variant::STRING) {
				r_value = StringName(String(p_json));
				return true;
			}
			return _fail(p_property, r_error);
		case Variant::NODE_PATH:
			if (json_type == Variant::STRING) {
				r_value = NodePath(String(p_json));
				return true;
			}
			return _fail(p_property, r_error);
		case Variant::OBJECT:
			return _object_from_json(p_json, p_property, r_value, r_error);
		case Variant::DICTIONARY:
			if (json_type == Variant::DICTIONARY) {
				r_value = p_json;
				return true;
			}
			return _fail(p_property, r_error);
		case Variant::ARRAY: {
			if (json_type != Variant::ARRAY) {
				return _fail(p_property, r_error);
			}
			PropertyInfo element;
			if (!_array_element(p_property, element)) {
				r_value = p_json;
				return true;
			}
			Array typed;
			typed.set_typed(element.type, element.type == Variant::OBJECT ? element.class_name : StringName(), Variant());
			if (!_array_from_json(p_json, element, typed, r_error)) {
				return false;
			}
			r_value = typed;
			return true;
		}
		default:
			break;
	}

	const Variant::Type packed_element = _packed_element_type(type);
	if (packed_element != Variant::NIL && json_type == Variant::ARRAY) {
		Array items;
		if (!_array_from_json(p_json, PropertyInfo(packed_element, String()), items, r_error)) {
			return false;
		}
		const Variant items_variant = items;
		const Variant *arguments[1] = { &items_variant };
		Callable::CallError error;
		Variant::construct(type, r_value, arguments, 1, error);
		return error.error == Callable::CallError::CALL_OK || _fail(p_property, r_error);
	}

	if (_vector_from_numbers(type, p_json, r_value)) {
		return true;
	}
	if (json_type == Variant::STRING) {
		const String text = p_json;
		if (type == Variant::COLOR && !text.contains_char('(')) {
			// "#ff8800", "ff8800" or a named color such as "red".
			const Color invalid(-1, -1, -1, -1);
			const Color color = Color::from_string(text, invalid);
			if (color != invalid) {
				r_value = color;
				return true;
			}
		}
		if (_from_literal(type, text, r_value)) {
			return true;
		}
	}
	return _fail(p_property, r_error);
}

bool find_property(const Object *p_object, const StringName &p_name, PropertyInfo &r_property) {
	List<PropertyInfo> properties;
	p_object->get_property_list(&properties);
	for (const PropertyInfo &property : properties) {
		if (property.usage & (PROPERTY_USAGE_CATEGORY | PROPERTY_USAGE_GROUP | PROPERTY_USAGE_SUBGROUP)) {
			continue;
		}
		if (property.name == p_name) {
			r_property = property;
			return true;
		}
	}
	return false;
}

bool set_properties(Object *p_object, const Dictionary &p_properties, String &r_error) {
	// Everything is converted first, so that a mistake changes nothing.
	LocalVector<Pair<StringName, Variant>> values;
	for (const KeyValue<Variant, Variant> &entry : p_properties) {
		const String name = entry.key;
		PropertyInfo property;
		if (!find_property(p_object, name, property)) {
			r_error = vformat("%s has no property \"%s\"", p_object->get_class(), name);
			return false;
		}
		Variant value;
		String error;
		if (!from_json(entry.value, property, value, error)) {
			r_error = vformat("\"%s\" %s", name, error);
			return false;
		}
		values.push_back(Pair<StringName, Variant>(name, value));
	}
	for (const Pair<StringName, Variant> &value : values) {
		p_object->set(value.first, value.second);
	}
	return true;
}

} // namespace EretheaMcpValues
