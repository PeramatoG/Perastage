#pragma once

#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>

namespace godot {

class PeravizGoboVectorizer : public Object {
    GDCLASS(PeravizGoboVectorizer, Object)

protected:
    static void _bind_methods();

public:
    Dictionary vectorize_image(const String &image_path,
                               int max_size = 192,
                               float luma_alpha_threshold = 0.5,
                               bool apply_edge_mask_correction = true) const;
};

} // namespace godot
