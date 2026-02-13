#include "register_types.h"

#include "hello_world.h"
#include "peraviz_loader.h"

void initialize_peraviz_module(godot::ModuleInitializationLevel p_level) {
    if (p_level != godot::MODULE_INITIALIZATION_LEVEL_SCENE) {
        return;
    }

    godot::ClassDB::register_class<godot::HelloWorld>();
    godot::ClassDB::register_class<godot::PeravizLoader>();
}

void uninitialize_peraviz_module(godot::ModuleInitializationLevel p_level) {
    if (p_level != godot::MODULE_INITIALIZATION_LEVEL_SCENE) {
        return;
    }
}
