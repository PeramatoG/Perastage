#pragma once

#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>

namespace godot {

class PeravizLoader : public Object {
    GDCLASS(PeravizLoader, Object)

protected:
    static void _bind_methods();

public:
    Array load_mvr(const String &path) const;
    Dictionary load_3ds_mesh_data(const String &path) const;
};

} // namespace godot
