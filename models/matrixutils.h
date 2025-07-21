#pragma once

#include <string>
#include <array>
#include <sstream>
#include <cmath>
#include "types.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif



// Utility functions for handling 4x3 transformation matrices
namespace MatrixUtils {

    // Parse a matrix string in the format "{a,b,c}{d,e,f}{g,h,i}{j,k,l}".
    inline bool ParseMatrix(const std::string& text, Matrix& outMatrix)
    {
        std::string cleaned;
        cleaned.reserve(text.size());
        for (char c : text) {
            if (c == '{' || c == '}' || c == ',')
                cleaned.push_back(' ');
            else
                cleaned.push_back(c);
        }

        std::stringstream ss(cleaned);
        float values[12];
        for (int i = 0; i < 12; ++i) {
            if (!(ss >> values[i]))
                return false;
        }

        outMatrix.u = { values[0], values[1], values[2] };
        outMatrix.v = { values[3], values[4], values[5] };
        outMatrix.w = { values[6], values[7], values[8] };
        outMatrix.o = { values[9], values[10], values[11] };

        return true;
    }

    // Convert rotation part of matrix to Euler angles (degrees, yaw/pitch/roll)
    inline std::array<float, 3> MatrixToEuler(const Matrix& m)
    {
        float r00 = m.u[0];
        float r01 = m.u[1];
        float r02 = m.u[2];
        float r10 = m.v[0];
        float r11 = m.v[1];
        float r12 = m.v[2];
        float r20 = m.w[0];
        float r21 = m.w[1];
        float r22 = m.w[2];

        float pitch = std::atan2(-r20, std::sqrt(r00 * r00 + r10 * r10));
        float yaw, roll;
        if (std::abs(std::cos(pitch)) > 1e-6f) {
            yaw = std::atan2(r10, r00);
            roll = std::atan2(r21, r22);
        }
        else {
            yaw = 0.0f;
            roll = std::atan2(-r01, r11);
        }

        const float toDeg = 180.0f / static_cast<float>(M_PI);
        return { yaw * toDeg, pitch * toDeg, roll * toDeg };
    }

    inline Matrix Identity()
    {
        return Matrix{};
    }

    inline Matrix Multiply(const Matrix& a, const Matrix& b)
    {
        Matrix r;
        for (int i = 0; i < 3; ++i) {
            r.u[i] = a.u[i] * b.u[0] + a.v[i] * b.u[1] + a.w[i] * b.u[2];
            r.v[i] = a.u[i] * b.v[0] + a.v[i] * b.v[1] + a.w[i] * b.v[2];
            r.w[i] = a.u[i] * b.w[0] + a.v[i] * b.w[1] + a.w[i] * b.w[2];
            r.o[i] = a.u[i] * b.o[0] + a.v[i] * b.o[1] + a.w[i] * b.o[2] + a.o[i];
        }
        return r;
    }

} // namespace MatrixUtils
