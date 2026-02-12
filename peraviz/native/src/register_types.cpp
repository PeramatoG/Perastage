#include "register_types.h"

#include "hello_world.h"

void initialize_peraviz_module(godot::ModuleInitializationLevel p_level) {
    if (p_level != godot::MODULE_INITIALIZATION_LEVEL_SCENE) {
        return;
    }

    godot::ClassDB::register_class<godot::HelloWorld>();
}

void uninitialize_peraviz_module(godot::ModuleInitializationLevel p_level) {
    if (p_level != godot::MODULE_INITIALIZATION_LEVEL_SCENE) {
        return;
    }
}
