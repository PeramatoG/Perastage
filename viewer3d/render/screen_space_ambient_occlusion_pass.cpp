#include "screen_space_ambient_occlusion_pass.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#include <GL/glew.h>
#ifdef __APPLE__
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

#include <algorithm>
#include <cmath>
#include <vector>

namespace ScreenSpaceAmbientOcclusionPass {
namespace {

float Clamp01(float value) {
  if (value < 0.0f)
    return 0.0f;
  if (value > 1.0f)
    return 1.0f;
  return value;
}

bool ShouldRender(const RenderFrameContext &context) {
  return context.useLighting && context.useAmbientOcclusion &&
         !context.wireframe && !context.is2DViewer;
}

} // namespace

void Render(const RenderFrameContext &context) {
  if (!ShouldRender(context))
    return;

  GLint viewport[4] = {0, 0, 0, 0};
  glGetIntegerv(GL_VIEWPORT, viewport);
  const int width = viewport[2];
  const int height = viewport[3];
  if (width <= 8 || height <= 8)
    return;

  std::vector<float> depth(static_cast<size_t>(width) *
                           static_cast<size_t>(height));
  glReadPixels(0, 0, width, height, GL_DEPTH_COMPONENT, GL_FLOAT, depth.data());

  std::vector<unsigned char> mask(static_cast<size_t>(width) *
                                  static_cast<size_t>(height));

  const float strength = Clamp01(context.ambientOcclusionStrength);
  const int sampleRadius = 4;
  const float bias = 0.0009f;

  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      const size_t idx = static_cast<size_t>(y) * static_cast<size_t>(width) +
                         static_cast<size_t>(x);
      const float center = depth[idx];

      if (center >= 0.9999f) {
        mask[idx] = 255;
        continue;
      }

      float occlusion = 0.0f;
      int sampleCount = 0;

      for (int oy = -sampleRadius; oy <= sampleRadius; ++oy) {
        const int ny = y + oy;
        if (ny < 0 || ny >= height)
          continue;
        for (int ox = -sampleRadius; ox <= sampleRadius; ++ox) {
          if (ox == 0 && oy == 0)
            continue;
          const int nx = x + ox;
          if (nx < 0 || nx >= width)
            continue;

          const float distance = std::sqrt(static_cast<float>(ox * ox + oy * oy));
          if (distance > static_cast<float>(sampleRadius))
            continue;

          const size_t nidx = static_cast<size_t>(ny) * static_cast<size_t>(width) +
                              static_cast<size_t>(nx);
          const float neighbor = depth[nidx];
          const float delta = center - neighbor;
          if (delta > bias) {
            const float rangeWeight = 1.0f - (distance / static_cast<float>(sampleRadius));
            occlusion += std::min(delta * 24.0f, 1.0f) * rangeWeight;
          }
          ++sampleCount;
        }
      }

      const float normalized =
          (sampleCount > 0) ? Clamp01(occlusion / static_cast<float>(sampleCount)) : 0.0f;
      const float darkness = Clamp01(normalized * (0.85f * strength));
      const float multiplier = 1.0f - darkness;
      mask[idx] = static_cast<unsigned char>(std::round(multiplier * 255.0f));
    }
  }

  GLuint aoTexture = 0;
  glGenTextures(1, &aoTexture);
  if (aoTexture == 0)
    return;

  glBindTexture(GL_TEXTURE_2D, aoTexture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, width, height, 0, GL_LUMINANCE,
               GL_UNSIGNED_BYTE, mask.data());

  glPushAttrib(GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT |
               GL_TRANSFORM_BIT | GL_TEXTURE_BIT);

  glDisable(GL_LIGHTING);
  glDisable(GL_DEPTH_TEST);
  glEnable(GL_TEXTURE_2D);
  glEnable(GL_BLEND);
  glBlendFunc(GL_DST_COLOR, GL_ZERO);

  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  glLoadIdentity();
  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glLoadIdentity();

  glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
  glBindTexture(GL_TEXTURE_2D, aoTexture);
  glBegin(GL_QUADS);
  glTexCoord2f(0.0f, 0.0f);
  glVertex2f(-1.0f, -1.0f);
  glTexCoord2f(1.0f, 0.0f);
  glVertex2f(1.0f, -1.0f);
  glTexCoord2f(1.0f, 1.0f);
  glVertex2f(1.0f, 1.0f);
  glTexCoord2f(0.0f, 1.0f);
  glVertex2f(-1.0f, 1.0f);
  glEnd();

  glPopMatrix();
  glMatrixMode(GL_PROJECTION);
  glPopMatrix();
  glMatrixMode(GL_MODELVIEW);

  glPopAttrib();

  glBindTexture(GL_TEXTURE_2D, 0);
  glDeleteTextures(1, &aoTexture);
}

} // namespace ScreenSpaceAmbientOcclusionPass
