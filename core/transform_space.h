#pragma once

#include "matrixutils.h"

#include <array>
#include <cmath>

namespace transform_space {

enum class TransformSpace { World, Local };

namespace detail {
inline constexpr float kEpsilon = 1e-6f;

// Returns whether a scalar can be used safely in transform math.
inline bool IsFinite(float value) { return std::isfinite(value); }

// Computes a three-dimensional dot product.
inline float Dot(const std::array<float, 3> &a, const std::array<float, 3> &b) {
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

// Computes a three-dimensional cross product.
inline std::array<float, 3> Cross(const std::array<float, 3> &a,
                                  const std::array<float, 3> &b) {
  return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2],
          a[0] * b[1] - a[1] * b[0]};
}

// Computes the Euclidean length of a vector.
inline float Length(const std::array<float, 3> &v) {
  return std::sqrt(Dot(v, v));
}

// Normalizes a vector or returns the supplied fallback for degenerate input.
inline std::array<float, 3> NormalizeOr(const std::array<float, 3> &v,
                                        const std::array<float, 3> &fallback) {
  const float length = Length(v);
  if (length <= kEpsilon || !IsFinite(length))
    return fallback;
  return {v[0] / length, v[1] / length, v[2] / length};
}
} // namespace detail

// Returns the normalized orientation basis from a transform without scale or shear.
inline Matrix ExtractOrientation(const Matrix &transform) {
  Matrix orientation = MatrixUtils::Identity();
  orientation.u = detail::NormalizeOr(transform.u, {1.0f, 0.0f, 0.0f});
  orientation.v = detail::NormalizeOr(transform.v, {0.0f, 1.0f, 0.0f});
  orientation.v = detail::NormalizeOr(
      {orientation.v[0] - orientation.u[0] * detail::Dot(orientation.u, orientation.v),
       orientation.v[1] - orientation.u[1] * detail::Dot(orientation.u, orientation.v),
       orientation.v[2] - orientation.u[2] * detail::Dot(orientation.u, orientation.v)},
      {0.0f, 1.0f, 0.0f});
  orientation.w = detail::NormalizeOr(detail::Cross(orientation.u, orientation.v),
                                      {0.0f, 0.0f, 1.0f});
  orientation.o = {0.0f, 0.0f, 0.0f};
  return orientation;
}

// Transforms a direction by a normalized orientation basis.
inline std::array<float, 3> TransformDirection(
    const Matrix &orientation, const std::array<float, 3> &direction) {
  return {orientation.u[0] * direction[0] + orientation.v[0] * direction[1] +
              orientation.w[0] * direction[2],
          orientation.u[1] * direction[0] + orientation.v[1] * direction[1] +
              orientation.w[1] * direction[2],
          orientation.u[2] * direction[0] + orientation.v[2] * direction[1] +
              orientation.w[2] * direction[2]};
}

// Applies an incremental translation in world or target-local coordinates.
inline Matrix ApplyIncrementalTranslation(
    const Matrix &source, const std::array<float, 3> &delta,
    TransformSpace space) {
  Matrix out = source;
  const auto worldDelta = space == TransformSpace::Local
                              ? TransformDirection(ExtractOrientation(source), delta)
                              : delta;
  for (int i = 0; i < 3; ++i)
    out.o[i] += worldDelta[i];
  return out;
}

// Applies an incremental rotation while preserving translation and per-axis scale.
inline Matrix ApplyIncrementalRotation(const Matrix &source,
                                       const Matrix &deltaRotation,
                                       TransformSpace space) {
  const Matrix orientation = ExtractOrientation(source);
  const Matrix normalizedDelta = ExtractOrientation(deltaRotation);
  const Matrix composed = space == TransformSpace::Local
                              ? MatrixUtils::Multiply(orientation, normalizedDelta)
                              : MatrixUtils::Multiply(normalizedDelta, orientation);
  return MatrixUtils::ApplyRotationPreservingScale(source, composed, source.o);
}

} // namespace transform_space
