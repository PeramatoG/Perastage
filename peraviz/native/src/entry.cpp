#include "register_types.h"

#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/godot.hpp>

extern "C" {

GDExtensionBool GDE_EXPORT peraviz_library_init(const GDExtensionInterfaceGetProcAddress p_get_proc_address,
        GDExtensionClassLibraryPtr p_library,
        GDExtensionInitialization *r_initialization) {
    godot::GDExtensionBinding::InitObject init_obj(p_get_proc_address, p_library, r_initialization);

    init_obj.register_initializer(initialize_peraviz_module);
    init_obj.register_terminator(uninitialize_peraviz_module);
    init_obj.set_minimum_library_initialization_level(godot::MODULE_INITIALIZATION_LEVEL_SCENE);

    return init_obj.init();
}
}
