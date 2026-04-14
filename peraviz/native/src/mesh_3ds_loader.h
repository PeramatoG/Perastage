#pragma once

#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>
#include <godot_cpp/variant/string.hpp>

namespace peraviz {

bool load_3ds_mesh_data(const godot::String &path,
                        godot::PackedVector3Array &out_vertices,
                        godot::PackedVector3Array &out_normals,
                        godot::PackedVector2Array &out_texcoords,
                        godot::PackedInt32Array &out_indices,
                        godot::String &out_texture_path,
                        godot::String &out_error);

} // namespace peraviz
