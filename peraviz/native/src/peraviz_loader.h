#pragma once

#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>

#include "scene_model.h"

namespace godot {

class PeravizLoader : public Object {
    GDCLASS(PeravizLoader, Object)

protected:
    static void _bind_methods();

public:
    Array load_mvr(const String &path, bool peraviz_debug_baseline = false,
                   bool peraviz_debug_coords = false);
    Dictionary load_3ds_mesh_data(const String &path) const;
    Array get_fixtures_patch() const;
    Dictionary build_fixture_dmx_bindings(int universe_offset = -1) const;
    Dictionary build_fixture_dimmer_bindings(int universe_offset = -1) const;

private:
    peraviz::SceneModel last_scene_model_;
};

} // namespace godot
