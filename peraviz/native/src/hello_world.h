#pragma once

#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/string.hpp>

namespace godot {

class HelloWorld : public Object {
    GDCLASS(HelloWorld, Object)

protected:
    static void _bind_methods();

public:
    HelloWorld() = default;
    ~HelloWorld() override = default;

    String ping() const;
};

} // namespace godot
