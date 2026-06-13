#pragma once

#include <array>

namespace viewer3d::render {

// Internal rendering fallback for fixture meshes with no material or GDTF model color; never serialize this as MVR Fixture Color.
inline constexpr std::array<float, 3> DEFAULT_MESH_COLOR{1.0f, 1.0f, 1.0f};

} // namespace viewer3d::render
