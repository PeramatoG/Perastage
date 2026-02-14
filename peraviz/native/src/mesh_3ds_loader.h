#pragma once

#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>

namespace peraviz {

// Loads a 3DS file and returns a Dictionary with keys:
// - ok (bool)
// - vertices (PackedVector3Array)
// - normals (PackedVector3Array)
// - indices (PackedInt32Array)
// - error (String, optional)
godot::Dictionary load_3ds_mesh_data(const godot::String &path);

} // namespace peraviz
