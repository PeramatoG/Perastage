#include "coordinate_mapper.h"

#include "peraviz_debug_runtime.h"

#include <cmath>

namespace {

std::array<float, 3> scale_axis(const std::array<float, 3> &v, float factor) {
    return {v[0] * factor, v[1] * factor, v[2] * factor};
}

std::array<float, 3> extract_scale(const Matrix &m) {
    auto len = [](const std::array<float, 3> &v) {
        return std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    };
    return {len(m.u), len(m.v), len(m.w)};
}

Matrix normalize_basis(const Matrix &m, const std::array<float, 3> &scale) {
    Matrix out = m;
    auto safe_div = [](float value, float s) {
        return (std::abs(s) > 1e-6F) ? value / s : value;
    };
    for (int i = 0; i < 3; ++i) {
        out.u[i] = safe_div(out.u[i], scale[0]);
        out.v[i] = safe_div(out.v[i], scale[1]);
        out.w[i] = safe_div(out.w[i], scale[2]);
    }
    return out;
}

} // namespace

namespace peraviz::coordinate_mapper {

Vec3 map_position_mm_to_m(const std::array<float, 3> &source_mm) {
    return Vec3{source_mm[0] / 1000.0F, source_mm[2] / 1000.0F,
                -source_mm[1] / 1000.0F};
}

std::array<float, 3> map_source_vector_to_godot(const std::array<float, 3> &source) {
    return {source[0], source[2], -source[1]};
}

Matrix to_godot_local_basis(const Matrix &source_local) {
    Matrix out;

    const auto mapped_u = map_source_vector_to_godot(source_local.u);
    const auto mapped_v = map_source_vector_to_godot(source_local.v);
    const auto mapped_w = map_source_vector_to_godot(source_local.w);

    // Convert local basis with C * R * C^-1 so parent/child composition remains correct.
    out.u = mapped_u;
    out.v = mapped_w;
    out.w = scale_axis(mapped_v, -1.0F);
    out.o = {0.0F, 0.0F, 0.0F};
    return out;
}

SceneTransform to_godot_transform(const Matrix &source_local) {
    SceneTransform transform;
    transform.position = map_position_mm_to_m(source_local.o);

    Matrix basis = to_godot_local_basis(source_local);
    transform.basis_x = {basis.u[0], basis.u[1], basis.u[2]};
    transform.basis_y = {basis.v[0], basis.v[1], basis.v[2]};
    transform.basis_z = {basis.w[0], basis.w[1], basis.w[2]};
    transform.has_basis = true;

    const auto scale = extract_scale(basis);
    transform.scale = {scale[0], scale[1], scale[2]};

    Matrix rotation_only = normalize_basis(basis, scale);
    const auto euler = MatrixUtils::MatrixToEuler(rotation_only);
    transform.rotation_degrees = {euler[0], euler[1], euler[2]};

    peraviz::debug_runtime::log_baseline_transform_comparison("coordinate_mapper::to_godot_transform",
                                                              source_local, transform);
    return transform;
}

} // namespace peraviz::coordinate_mapper
