#include "symbols/OffscreenSymbolRenderer.h"

#include "gdtfloader.h"
#include "matrixutils.h"
#include "projectutils.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <unordered_map>

namespace symbols {
namespace {

struct Vec3 {
  float x;
  float y;
  float z;
};

Vec3 TransformPoint(const Matrix &m, const Vec3 &p) {
  return {m.u[0] * p.x + m.v[0] * p.y + m.w[0] * p.z + m.o[0],
          m.u[1] * p.x + m.v[1] * p.y + m.w[1] * p.z + m.o[1],
          m.u[2] * p.x + m.v[2] * p.y + m.w[2] * p.z + m.o[2]};
}

std::array<float, 3> Project(const Vec3 &p, SymbolView view) {
  switch (view) {
  case SymbolView::Front:
    return {p.x, p.z, -p.y};
  case SymbolView::Top:
    return {p.x, p.y, -p.z};
  case SymbolView::Bottom:
    return {p.x, -p.y, p.z};
  case SymbolView::Left:
    return {p.y, p.z, p.x};
  }
  return {p.x, p.y, p.z};
}

void DrawLine(int x0, int y0, int x1, int y1,
              const std::vector<float> &depth,
              int width,
              int height,
              float z0,
              float z1,
              std::vector<uint8_t> &lineMask) {
  int dx = std::abs(x1 - x0);
  int sx = x0 < x1 ? 1 : -1;
  int dy = -std::abs(y1 - y0);
  int sy = y0 < y1 ? 1 : -1;
  int err = dx + dy;
  int steps = std::max(std::abs(x1 - x0), std::abs(y1 - y0));
  int i = 0;

  while (true) {
    if (x0 >= 0 && y0 >= 0 && x0 < width && y0 < height) {
      const int idx = y0 * width + x0;
      const float t = steps == 0 ? 0.0f : static_cast<float>(i) / static_cast<float>(steps);
      const float z = z0 + (z1 - z0) * t;
      if (z <= depth[idx] + 0.25f)
        lineMask[idx] = 1;
    }
    if (x0 == x1 && y0 == y1)
      break;
    int e2 = 2 * err;
    if (e2 >= dy) {
      err += dy;
      x0 += sx;
    }
    if (e2 <= dx) {
      err += dx;
      y0 += sy;
    }
    ++i;
  }
}

} // namespace

RenderResult OffscreenSymbolRenderer::RenderFixtureTechnical(
    const std::string &gdtfSpec,
    SymbolView view,
    int width,
    int height,
    RenderMode mode) const {
  RenderResult result;
  result.rgba.width = width;
  result.rgba.height = height;
  result.rgba.pixels.assign(static_cast<size_t>(width * height * 4), 0);
  result.depth.assign(static_cast<size_t>(width * height), std::numeric_limits<float>::infinity());

  const auto gdtfPath = ProjectUtils::GetDefaultLibraryPath("fixtures") + "/" + gdtfSpec;
  GdtfGeometryTree tree;
  std::string error;
  if (!LoadGdtfGeometryTree(gdtfPath, tree, &error))
    return result;

  std::vector<std::array<float, 3>> projected;
  projected.reserve(1024);

  struct TriData {
    std::array<float, 3> p0;
    std::array<float, 3> p1;
    std::array<float, 3> p2;
  };
  std::vector<TriData> triangles;

  float minX = std::numeric_limits<float>::infinity();
  float minY = std::numeric_limits<float>::infinity();
  float maxX = -std::numeric_limits<float>::infinity();
  float maxY = -std::numeric_limits<float>::infinity();

  for (const auto &node : tree.nodes) {
    if (!node.hasMesh)
      continue;
    const auto &m = node.worldTransform;
    const auto &mesh = node.mesh;
    for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
      const auto idx0 = mesh.indices[i] * 3;
      const auto idx1 = mesh.indices[i + 1] * 3;
      const auto idx2 = mesh.indices[i + 2] * 3;
      Vec3 v0{mesh.vertices[idx0], mesh.vertices[idx0 + 1], mesh.vertices[idx0 + 2]};
      Vec3 v1{mesh.vertices[idx1], mesh.vertices[idx1 + 1], mesh.vertices[idx1 + 2]};
      Vec3 v2{mesh.vertices[idx2], mesh.vertices[idx2 + 1], mesh.vertices[idx2 + 2]};
      const auto p0 = Project(TransformPoint(m, v0), view);
      const auto p1 = Project(TransformPoint(m, v1), view);
      const auto p2 = Project(TransformPoint(m, v2), view);
      minX = std::min({minX, p0[0], p1[0], p2[0]});
      minY = std::min({minY, p0[1], p1[1], p2[1]});
      maxX = std::max({maxX, p0[0], p1[0], p2[0]});
      maxY = std::max({maxY, p0[1], p1[1], p2[1]});
      triangles.push_back({p0, p1, p2});
    }
  }

  if (triangles.empty() || !std::isfinite(minX) || !std::isfinite(minY))
    return result;

  const float spanX = std::max(1.0f, maxX - minX);
  const float spanY = std::max(1.0f, maxY - minY);
  const float margin = 0.08f;
  const float sx = (1.0f - 2.0f * margin) * static_cast<float>(width) / spanX;
  const float sy = (1.0f - 2.0f * margin) * static_cast<float>(height) / spanY;
  const float scale = std::min(sx, sy);
  const float cx = (minX + maxX) * 0.5f;
  const float cy = (minY + maxY) * 0.5f;

  auto toScreen = [&](const std::array<float, 3> &p) {
    const float x = (p[0] - cx) * scale + width * 0.5f;
    const float y = height * 0.5f - (p[1] - cy) * scale;
    return std::array<float, 3>{x, y, p[2]};
  };

  std::vector<std::array<float, 3>> triScreen;
  triScreen.reserve(triangles.size() * 3);
  for (const auto &tri : triangles) {
    triScreen.push_back(toScreen(tri.p0));
    triScreen.push_back(toScreen(tri.p1));
    triScreen.push_back(toScreen(tri.p2));
  }

  for (size_t t = 0; t < triangles.size(); ++t) {
    const auto &a = triScreen[t * 3];
    const auto &b = triScreen[t * 3 + 1];
    const auto &c = triScreen[t * 3 + 2];

    const int minXi = std::max(0, static_cast<int>(std::floor(std::min({a[0], b[0], c[0]}))));
    const int maxXi = std::min(width - 1, static_cast<int>(std::ceil(std::max({a[0], b[0], c[0]}))));
    const int minYi = std::max(0, static_cast<int>(std::floor(std::min({a[1], b[1], c[1]}))));
    const int maxYi = std::min(height - 1, static_cast<int>(std::ceil(std::max({a[1], b[1], c[1]}))));

    const float area = (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0]);
    if (std::abs(area) < 1e-6f)
      continue;

    for (int y = minYi; y <= maxYi; ++y) {
      for (int x = minXi; x <= maxXi; ++x) {
        const float px = static_cast<float>(x) + 0.5f;
        const float py = static_cast<float>(y) + 0.5f;
        const float w0 = ((b[0] - a[0]) * (py - a[1]) - (b[1] - a[1]) * (px - a[0])) / area;
        const float w1 = ((c[0] - b[0]) * (py - b[1]) - (c[1] - b[1]) * (px - b[0])) / area;
        const float w2 = ((a[0] - c[0]) * (py - c[1]) - (a[1] - c[1]) * (px - c[0])) / area;
        if (w0 < -0.0001f || w1 < -0.0001f || w2 < -0.0001f)
          continue;
        const float z = w0 * c[2] + w1 * a[2] + w2 * b[2];
        const int idx = y * width + x;
        if (z < result.depth[idx])
          result.depth[idx] = z;
      }
    }
  }

  std::vector<uint8_t> lineMask(static_cast<size_t>(width * height), 0);
  for (size_t t = 0; t < triangles.size(); ++t) {
    const auto &a = triScreen[t * 3];
    const auto &b = triScreen[t * 3 + 1];
    const auto &c = triScreen[t * 3 + 2];
    DrawLine(static_cast<int>(std::lround(a[0])), static_cast<int>(std::lround(a[1])),
             static_cast<int>(std::lround(b[0])), static_cast<int>(std::lround(b[1])),
             result.depth, width, height, a[2], b[2], lineMask);
    DrawLine(static_cast<int>(std::lround(b[0])), static_cast<int>(std::lround(b[1])),
             static_cast<int>(std::lround(c[0])), static_cast<int>(std::lround(c[1])),
             result.depth, width, height, b[2], c[2], lineMask);
    DrawLine(static_cast<int>(std::lround(c[0])), static_cast<int>(std::lround(c[1])),
             static_cast<int>(std::lround(a[0])), static_cast<int>(std::lround(a[1])),
             result.depth, width, height, c[2], a[2], lineMask);
  }

  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      const int idx = y * width + x;
      const bool filled = std::isfinite(result.depth[idx]);
      if (!filled)
        continue;
      auto *px = result.rgba.Pixel(x, y);
      px[3] = 255;
      if (mode == RenderMode::ShapeBlack) {
        px[0] = 0;
        px[1] = 0;
        px[2] = 0;
      } else {
        if (lineMask[idx]) {
          px[0] = 0;
          px[1] = 0;
          px[2] = 0;
        } else {
          px[0] = 255;
          px[1] = 255;
          px[2] = 255;
        }
      }
    }
  }

  return result;
}

} // namespace symbols
