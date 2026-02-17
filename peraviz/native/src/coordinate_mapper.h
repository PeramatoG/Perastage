#pragma once

#include "matrixutils.h"
#include "scene_model.h"

#include <array>

namespace peraviz::coordinate_mapper {

Vec3 map_position_mm_to_m(const std::array<float, 3> &source_mm);

std::array<float, 3> map_source_vector_to_godot(const std::array<float, 3> &source);

Matrix to_godot_local_basis(const Matrix &source_local);

SceneTransform to_godot_transform(const Matrix &source_local);

} // namespace peraviz::coordinate_mapper
