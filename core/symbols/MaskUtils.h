#pragma once

#include "symbols/OffscreenSymbolRenderer.h"

#include <cstdint>
#include <vector>

namespace symbols {

using BinaryMask = std::vector<uint8_t>;

BinaryMask ExtractShapeMask(const ImageRGBA &image, float alphaThreshold = 0.5f,
                            float luminanceThreshold = 0.2f);
BinaryMask ExtractLineMask(const ImageRGBA &image, float alphaThreshold = 0.5f,
                           float luminanceThreshold = 0.2f);

void MorphClose(BinaryMask &mask, int width, int height);

} // namespace symbols
