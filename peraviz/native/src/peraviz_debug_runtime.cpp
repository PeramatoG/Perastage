#include "peraviz_debug_runtime.h"

#include <godot_cpp/variant/utility_functions.hpp>

#include <array>

namespace peraviz::debug_runtime {
namespace {

bool g_baseline_debug_enabled = false;

godot::String vec_to_string(const std::array<float, 3> &v) {
    return godot::String("(") + godot::String::num(v[0]) + ", " +
           godot::String::num(v[1]) + ", " + godot::String::num(v[2]) + ")";
}

godot::String matrix_to_string(const Matrix &m) {
    return godot::String("u=") + vec_to_string(m.u) + " v=" + vec_to_string(m.v) +
           " w=" + vec_to_string(m.w) + " o=" + vec_to_string(m.o);
}

godot::String scene_transform_to_string(const SceneTransform &transform) {
    return godot::String("pos=") + vec_to_string(transform.position) +
           " basis_x=" + vec_to_string(transform.basis_x) +
           " basis_y=" + vec_to_string(transform.basis_y) +
           " basis_z=" + vec_to_string(transform.basis_z) +
           " scale=" + vec_to_string(transform.scale) +
           " rot_deg=" + vec_to_string(transform.rotation_degrees);
}

} // namespace

void set_baseline_debug_enabled(bool enabled) {
    g_baseline_debug_enabled = enabled;
}

bool is_baseline_debug_enabled() {
    return g_baseline_debug_enabled;
}

void log_baseline_transform_comparison(const std::string &context,
                                       const Matrix &source_local,
                                       const SceneTransform &mapped_transform) {
    if (!g_baseline_debug_enabled) {
        return;
    }

    godot::UtilityFunctions::print("[PeravizBaseline] context=", godot::String(context.c_str()),
                                   " source_local=", matrix_to_string(source_local),
                                   " mapped=", scene_transform_to_string(mapped_transform));
}

void log_transform_adjustment(const std::string &context,
                              const std::string &reason,
                              const Matrix &before,
                              const Matrix &after) {
    godot::UtilityFunctions::print("[PeravizTransformAdjustment] context=",
                                   godot::String(context.c_str()),
                                   " reason=", godot::String(reason.c_str()),
                                   " before=", matrix_to_string(before),
                                   " after=", matrix_to_string(after));
}

} // namespace peraviz::debug_runtime

