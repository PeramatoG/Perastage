#include "hello_world.h"

#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

void HelloWorld::_bind_methods() {
    ClassDB::bind_method(D_METHOD("ping"), &HelloWorld::ping);
}

String HelloWorld::ping() const {
    const String message = "pong from Peraviz native C++";
    UtilityFunctions::print("[PeravizNative] ", message);
    return message;
}

} // namespace godot
