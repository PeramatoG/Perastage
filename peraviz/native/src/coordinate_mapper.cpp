#include "coordinate_mapper.h"

#include "peraviz_debug_runtime.h"

#include <cmath>
#include <limits>

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

float dot_product(const std::array<float, 3> &a, const std::array<float, 3> &b) {
    return (a[0] * b[0]) + (a[1] * b[1]) + (a[2] * b[2]);
}

std::array<float, 3> cross_product(const std::array<float, 3> &a, const std::array<float, 3> &b) {
    return {a[1] * b[2] - a[2] * b[1],
            a[2] * b[0] - a[0] * b[2],
            a[0] * b[1] - a[1] * b[0]};
}

float wrap_degrees(float value) {
    while (value > 180.0F) {
        value -= 360.0F;
    }
    while (value < -180.0F) {
        value += 360.0F;
    }
    return value;
}

float euler_score(const std::array<float, 3> &euler_degrees) {
    return std::fabs(wrap_degrees(euler_degrees[0])) +
           std::fabs(wrap_degrees(euler_degrees[1])) +
           std::fabs(wrap_degrees(euler_degrees[2]));
}

Matrix apply_axis_signs(const Matrix &m, int sx, int sy, int sz) {
    Matrix out = m;
    out.u = scale_axis(out.u, (sx < 0) ? -1.0F : 1.0F);
    out.v = scale_axis(out.v, (sy < 0) ? -1.0F : 1.0F);
    out.w = scale_axis(out.w, (sz < 0) ? -1.0F : 1.0F);
    return out;
}

void decompose_rotation_and_signed_scale(const Matrix &normalized_basis,
                                         const std::array<float, 3> &positive_scale,
                                         Matrix &rotation_only,
                                         std::array<float, 3> &signed_scale) {
    const float handedness = dot_product(cross_product(normalized_basis.u, normalized_basis.v),
                                         normalized_basis.w);
    const int target_parity = (handedness < 0.0F) ? -1 : 1;

    float best_score = std::numeric_limits<float>::infinity();
    Matrix best_rotation = normalized_basis;
    std::array<float, 3> best_scale = positive_scale;

    for (int sx : {-1, 1}) {
        for (int sy : {-1, 1}) {
            for (int sz : {-1, 1}) {
                if ((sx * sy * sz) != target_parity) {
                    continue;
                }

                const Matrix candidate_rotation = apply_axis_signs(normalized_basis, sx, sy, sz);
                const auto candidate_euler = MatrixUtils::MatrixToEuler(candidate_rotation);
                const float score = euler_score(candidate_euler);
                if (score >= best_score) {
                    continue;
                }

                best_score = score;
                best_rotation = candidate_rotation;
                best_scale = {positive_scale[0] * static_cast<float>(sx),
                              positive_scale[1] * static_cast<float>(sy),
                              positive_scale[2] * static_cast<float>(sz)};
            }
        }
    }

    rotation_only = best_rotation;
    signed_scale = best_scale;
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

    const Matrix basis = to_godot_local_basis(source_local);
    const auto positive_scale = extract_scale(basis);
    const Matrix normalized_basis = normalize_basis(basis, positive_scale);

    Matrix rotation_only;
    std::array<float, 3> signed_scale;
    decompose_rotation_and_signed_scale(normalized_basis, positive_scale,
                                        rotation_only, signed_scale);

    transform.scale = {signed_scale[0], signed_scale[1], signed_scale[2]};
    transform.basis_x = {rotation_only.u[0], rotation_only.u[1], rotation_only.u[2]};
    transform.basis_y = {rotation_only.v[0], rotation_only.v[1], rotation_only.v[2]};
    transform.basis_z = {rotation_only.w[0], rotation_only.w[1], rotation_only.w[2]};
    transform.has_basis = true;

    const auto euler = MatrixUtils::MatrixToEuler(rotation_only);
    transform.rotation_degrees = {euler[0], euler[1], euler[2]};

    peraviz::debug_runtime::log_baseline_transform_comparison("coordinate_mapper::to_godot_transform",
                                                              source_local, transform);
    return transform;
}

} // namespace peraviz::coordinate_mapper
