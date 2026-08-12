#include "truss_dimension_resolution.h"

#include <cmath>

namespace {
constexpr float kLegacyLengthMm = 1000.0f;
constexpr float kLegacyWidthMm = 400.0f;
constexpr float kLegacyHeightMm = 400.0f;
constexpr float kDimensionToleranceMm = 0.01f;

// Reports whether two dimensions are equal within metadata precision.
bool NearlyEqual(float lhs, float rhs) {
  return std::fabs(lhs - rhs) <= kDimensionToleranceMm;
}

// Reports whether legacy Perastage metadata matches the historical fallback.
bool IsLegacyFallbackMetadata(const Truss &truss,
                              const GeometryBounds &bounds,
                              bool legacyMetadataContext) {
  if (!legacyMetadataContext ||
      truss.dimensionSource != Truss::DimensionSource::PerastageMetadata)
    return false;
  if (!NearlyEqual(truss.lengthMm, kLegacyLengthMm) ||
      !NearlyEqual(truss.widthMm, kLegacyWidthMm) ||
      !NearlyEqual(truss.heightMm, kLegacyHeightMm))
    return false;
  const auto size = bounds.SizeMm();
  return !NearlyEqual(size[0], truss.lengthMm) ||
         !NearlyEqual(size[1], truss.widthMm) ||
         !NearlyEqual(size[2], truss.heightMm);
}
} // namespace

// Reports whether all three stored truss dimensions are finite and positive.
bool HasValidTrussDimensions(const Truss &truss) {
  return std::isfinite(truss.lengthMm) && truss.lengthMm > 0.0f &&
         std::isfinite(truss.widthMm) && truss.widthMm > 0.0f &&
         std::isfinite(truss.heightMm) && truss.heightMm > 0.0f;
}

// Resolves recoverable truss dimensions from measured geometry bounds.
bool ResolveTrussDimensionsFromGeometry(Truss &truss,
                                        bool legacyMetadataContext) {
  if (!truss.localGeometryBounds || !truss.localGeometryBounds->IsValid() ||
      truss.dimensionSource == Truss::DimensionSource::GdtfModel ||
      truss.dimensionSource == Truss::DimensionSource::ManualOverride)
    return false;

  const auto size = truss.localGeometryBounds->SizeMm();
  const bool legacyFallback = IsLegacyFallbackMetadata(
      truss, *truss.localGeometryBounds, legacyMetadataContext);
  bool changed = false;
  if (legacyFallback) {
    truss.dimensionSource = Truss::DimensionSource::LegacySyntheticFallback;
    truss.lengthMm = size[0];
    truss.widthMm = size[1];
    truss.heightMm = size[2];
    changed = true;
  } else {
    auto recover = [&](float &dimension, float measured) {
      if (!std::isfinite(dimension) || dimension <= 0.0f) {
        dimension = measured;
        changed = true;
      }
    };
    recover(truss.lengthMm, size[0]);
    recover(truss.widthMm, size[1]);
    recover(truss.heightMm, size[2]);
  }
  if (changed && !legacyFallback)
    truss.dimensionSource = Truss::DimensionSource::GeometryDerived;
  return changed;
}
