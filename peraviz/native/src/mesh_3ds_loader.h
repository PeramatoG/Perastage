#pragma once

#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/vector3.hpp>

namespace peraviz {

bool load_3ds_mesh_data(const godot::String &path,
                        godot::PackedVector3Array &out_vertices,
                        godot::PackedVector3Array &out_normals,
                        godot::PackedVector2Array &out_texcoords,
                        godot::PackedInt32Array &out_indices,
                        godot::String &out_texture_path,
                        bool &out_has_material_base_color,
                        godot::Vector3 &out_material_base_color,
                        godot::String &out_error);

} // namespace peraviz
