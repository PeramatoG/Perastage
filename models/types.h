#pragma once

#include <string>
#include <array>

// Transformation matrix used in MVR for spatial positioning
struct Matrix {
    std::array<float, 3> u{ 1.0f, 0.0f, 0.0f };
    std::array<float, 3> v{ 0.0f, 1.0f, 0.0f };
    std::array<float, 3> w{ 0.0f, 0.0f, 1.0f };
    std::array<float, 3> o{ 0.0f, 0.0f, 0.0f };
};
