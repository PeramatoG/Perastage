#include "register_types.h"

#include "hello_world.h"
#include "peraviz_loader.h"
#include "gobo_vectorizer.h"

#ifdef PERAVIZ_ENABLE_DMX
#include "peraviz_dmx_receiver.h"
#endif

void initialize_peraviz_module(godot::ModuleInitializationLevel p_level) {
    if (p_level != godot::MODULE_INITIALIZATION_LEVEL_SCENE) {
        return;
    }

    godot::ClassDB::register_class<godot::HelloWorld>();
    godot::ClassDB::register_class<godot::PeravizLoader>();
    godot::ClassDB::register_class<godot::PeravizGoboVectorizer>();
#ifdef PERAVIZ_ENABLE_DMX
    godot::ClassDB::register_class<godot::PeravizDmxReceiver>();
#endif
}

void uninitialize_peraviz_module(godot::ModuleInitializationLevel p_level) {
    if (p_level != godot::MODULE_INITIALIZATION_LEVEL_SCENE) {
        return;
    }
}
