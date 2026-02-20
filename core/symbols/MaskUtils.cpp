#include "symbols/MaskUtils.h"

#include <algorithm>

namespace symbols {
namespace {
float Luminance(uint8_t r, uint8_t g, uint8_t b) {
  return (0.2126f * static_cast<float>(r) + 0.7152f * static_cast<float>(g) +
          0.0722f * static_cast<float>(b)) /
         255.0f;
}

void Dilate(BinaryMask &mask, int width, int height) {
  BinaryMask out = mask;
  for (int y = 1; y < height - 1; ++y) {
    for (int x = 1; x < width - 1; ++x) {
      const int idx = y * width + x;
      if (mask[idx])
        continue;
      for (int oy = -1; oy <= 1; ++oy) {
        for (int ox = -1; ox <= 1; ++ox) {
          if (mask[(y + oy) * width + (x + ox)]) {
            out[idx] = 1;
            oy = 2;
            break;
          }
        }
      }
    }
  }
  mask.swap(out);
}

void Erode(BinaryMask &mask, int width, int height) {
  BinaryMask out = mask;
  for (int y = 1; y < height - 1; ++y) {
    for (int x = 1; x < width - 1; ++x) {
      const int idx = y * width + x;
      if (!mask[idx])
        continue;
      for (int oy = -1; oy <= 1; ++oy) {
        for (int ox = -1; ox <= 1; ++ox) {
          if (!mask[(y + oy) * width + (x + ox)]) {
            out[idx] = 0;
            oy = 2;
            break;
          }
        }
      }
    }
  }
  mask.swap(out);
}
} // namespace

BinaryMask ExtractShapeMask(const ImageRGBA &image, float alphaThreshold,
                            float luminanceThreshold) {
  BinaryMask mask(static_cast<size_t>(image.width * image.height), 0);
  for (int y = 0; y < image.height; ++y) {
    for (int x = 0; x < image.width; ++x) {
      const auto *px = image.Pixel(x, y);
      const float alpha = static_cast<float>(px[3]) / 255.0f;
      const float lum = Luminance(px[0], px[1], px[2]);
      if (alpha > alphaThreshold || lum < luminanceThreshold)
        mask[y * image.width + x] = 1;
    }
  }
  return mask;
}

BinaryMask ExtractLineMask(const ImageRGBA &image, float alphaThreshold,
                           float luminanceThreshold) {
  BinaryMask mask(static_cast<size_t>(image.width * image.height), 0);
  for (int y = 0; y < image.height; ++y) {
    for (int x = 0; x < image.width; ++x) {
      const auto *px = image.Pixel(x, y);
      const float alpha = static_cast<float>(px[3]) / 255.0f;
      const float lum = Luminance(px[0], px[1], px[2]);
      if (alpha > alphaThreshold && lum < luminanceThreshold)
        mask[y * image.width + x] = 1;
    }
  }
  return mask;
}

void MorphClose(BinaryMask &mask, int width, int height) {
  Dilate(mask, width, height);
  Erode(mask, width, height);
}

} // namespace symbols
