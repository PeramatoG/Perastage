/*
 * This file is part of Perastage.
 * Copyright (C) 2025 Luisma Peramato
 *
 * Perastage is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Perastage is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Perastage. If not, see <https://www.gnu.org/licenses/>.
 */
#pragma once

#include <string>
#include <array>
#include <sstream>
#include <cmath>
#include <vector>
#include <limits>
#include "types.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif



// Utility functions for handling 4x3 transformation matrices
namespace MatrixUtils {

    inline std::array<float, 3> ExtractScale(const Matrix& m)
    {
        auto norm = [](const std::array<float, 3>& v) {
            return std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
        };
        return {norm(m.u), norm(m.v), norm(m.w)};
    }

    inline Matrix ApplyRotationPreservingScale(const Matrix& source,
                                               const Matrix& rotation,
                                               const std::array<float, 3>& translation)
    {
        Matrix out = rotation;
        const auto scales = ExtractScale(source);

        for (int i = 0; i < 3; ++i) {
            out.u[i] *= scales[0];
            out.v[i] *= scales[1];
            out.w[i] *= scales[2];
        }
        out.o = translation;
        return out;
    }


    inline Matrix NormalizeRotation(const Matrix& source)
    {
        Matrix out = source;
        const auto scales = ExtractScale(source);
        auto safeDiv = [](float value, float scale) {
            return std::abs(scale) > 1e-6f ? value / scale : value;
        };

        for (int i = 0; i < 3; ++i) {
            out.u[i] = safeDiv(out.u[i], scales[0]);
            out.v[i] = safeDiv(out.v[i], scales[1]);
            out.w[i] = safeDiv(out.w[i], scales[2]);
        }
        return out;
    }

    inline float BasisDeterminant(const Matrix& m)
    {
        return m.u[0] * (m.v[1] * m.w[2] - m.v[2] * m.w[1]) -
               m.v[0] * (m.u[1] * m.w[2] - m.u[2] * m.w[1]) +
               m.w[0] * (m.u[1] * m.v[2] - m.u[2] * m.v[1]);
    }

    inline float WrapDegrees(float value)
    {
        while (value > 180.0f)
            value -= 360.0f;
        while (value < -180.0f)
            value += 360.0f;
        return value;
    }

    inline float EulerMagnitudeScore(const std::array<float, 3>& euler)
    {
        return std::abs(WrapDegrees(euler[0])) +
               std::abs(WrapDegrees(euler[1])) +
               std::abs(WrapDegrees(euler[2]));
    }

    inline Matrix ApplyAxisSigns(const Matrix& source, int sx, int sy, int sz)
    {
        Matrix out = source;
        const float signX = (sx < 0) ? -1.0f : 1.0f;
        const float signY = (sy < 0) ? -1.0f : 1.0f;
        const float signZ = (sz < 0) ? -1.0f : 1.0f;
        for (int i = 0; i < 3; ++i) {
            out.u[i] *= signX;
            out.v[i] *= signY;
            out.w[i] *= signZ;
        }
        return out;
    }

    // Parse a matrix string which can be either the MVR 4x3 format
    // "{a,b,c}{d,e,f}{g,h,i}{j,k,l}" or the GDTF 4x4 format
    // "{a,b,c,d}{e,f,g,h}{i,j,k,l}{m,n,o,p}". Both are stored row-major
    // in the files but mathematically defined as column-major. The last
    // row of the 4x4 representation is usually "0 0 0 1" and is ignored.
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
        std::vector<float> values;
        float v;
        while (ss >> v)
            values.push_back(v);

        if (values.size() == 16) {
            // GDTF 4x4 matrix. Translation is stored in the fourth column
            outMatrix.u = std::array<float, 3>{ values[0], values[4], values[8] };
            outMatrix.v = std::array<float, 3>{ values[1], values[5], values[9] };
            outMatrix.w = std::array<float, 3>{ values[2], values[6], values[10] };
            outMatrix.o = std::array<float, 3>{ values[3], values[7], values[11] };
            return true;
        } else if (values.size() == 12) {
            // MVR 4x3 matrix in column-major order
            outMatrix.u = std::array<float, 3>{ values[0], values[1], values[2] };
            outMatrix.v = std::array<float, 3>{ values[3], values[4], values[5] };
            outMatrix.w = std::array<float, 3>{ values[6], values[7], values[8] };
            outMatrix.o = std::array<float, 3>{ values[9], values[10], values[11] };
            return true;
        }

        return false;
    }

    // Convert rotation part of matrix to Euler angles (degrees, yaw/pitch/roll)
    inline std::array<float, 3> MatrixToEuler(const Matrix& m)
    {
        const Matrix normalized = NormalizeRotation(m);
        const float determinant = BasisDeterminant(normalized);
        const int requiredParity = (determinant < 0.0f) ? -1 : 1;

        const float toDeg = 180.0f / static_cast<float>(M_PI);
        auto rawEulerFromMatrix = [&](const Matrix& basis) {
            const float r00 = basis.u[0];
            const float r01 = basis.u[1];
            const float r10 = basis.v[0];
            const float r11 = basis.v[1];
            const float r20 = basis.w[0];
            const float r21 = basis.w[1];
            const float r22 = basis.w[2];

            const float pitch = std::atan2(-r20, std::sqrt(r00 * r00 + r10 * r10));
            float yaw = 0.0f;
            float roll = 0.0f;
            if (std::abs(std::cos(pitch)) > 1e-6f) {
                yaw = std::atan2(r10, r00);
                roll = std::atan2(r21, r22);
            } else {
                yaw = 0.0f;
                roll = std::atan2(-r01, r11);
            }
            return std::array<float, 3>{yaw * toDeg, pitch * toDeg, roll * toDeg};
        };

        std::array<float, 3> bestEuler{0.0f, 0.0f, 0.0f};
        float bestScore = std::numeric_limits<float>::infinity();

        for (int sx : {-1, 1}) {
            for (int sy : {-1, 1}) {
                for (int sz : {-1, 1}) {
                    const int negatives = (sx < 0 ? 1 : 0) + (sy < 0 ? 1 : 0) + (sz < 0 ? 1 : 0);
                    if ((sx * sy * sz) != requiredParity)
                        continue;
                    if (requiredParity < 0 && negatives != 1)
                        continue;

                    const Matrix candidate = ApplyAxisSigns(normalized, sx, sy, sz);
                    const auto euler = rawEulerFromMatrix(candidate);
                    const float score = EulerMagnitudeScore(euler);
                    if (score >= bestScore)
                        continue;

                    bestScore = score;
                    bestEuler = euler;
                }
            }
        }

        return bestEuler;
    }

    // Build a rotation matrix from Euler angles in degrees (yaw/pitch/roll)
    inline Matrix EulerToMatrix(float yawDeg, float pitchDeg, float rollDeg)
    {
        const float toRad = static_cast<float>(M_PI) / 180.0f;
        float yaw = yawDeg * toRad;
        float pitch = pitchDeg * toRad;
        float roll = rollDeg * toRad;

        float cy = std::cos(yaw);
        float sy = std::sin(yaw);
        float cp = std::cos(pitch);
        float sp = std::sin(pitch);
        float cr = std::cos(roll);
        float sr = std::sin(roll);

        Matrix m;
        m.u = std::array<float, 3>{ cy * cp, cy * sp * sr - sy * cr, cy * sp * cr + sy * sr };
        m.v = std::array<float, 3>{ sy * cp, sy * sp * sr + cy * cr, sy * sp * cr - cy * sr };
        m.w = std::array<float, 3>{ -sp,     cp * sr,                 cp * cr                 };
        return m;
    }

    // Convert matrix to the MVR 4x3 string representation
    inline std::string FormatMatrix(const Matrix& m)
    {
        std::ostringstream ss;
        ss << "{" << m.u[0] << "," << m.u[1] << "," << m.u[2] << "}"
           << "{" << m.v[0] << "," << m.v[1] << "," << m.v[2] << "}"
           << "{" << m.w[0] << "," << m.w[1] << "," << m.w[2] << "}"
           << "{" << m.o[0] << "," << m.o[1] << "," << m.o[2] << "}";
        return ss.str();
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
