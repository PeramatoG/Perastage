#include "viewer2d_support_selection.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#ifdef __APPLE__
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl.h>
#include <OpenGL/glu.h>
#else
#include <GL/gl.h>
#include <GL/glu.h>
#endif

#include "support.h"
#include "configservices.h"
#include "viewer3d_types.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace Viewer2DSupportSelection {
namespace {

constexpr int kHoistHitRadiusPixels = 14;

std::string NormalizeLayerName(const std::string &layer) {
  return layer.empty() ? std::string(DEFAULT_LAYER_NAME) : layer;
}

bool IsLayerVisible(const std::unordered_set<std::string> &hiddenLayers,
                    const std::string &layer) {
  return hiddenLayers.find(NormalizeLayerName(layer)) == hiddenLayers.end();
}

bool ProjectSupportCenter(const Support &support, int viewportHeight,
                          wxPoint &outScreenPos) {
  GLdouble model[16];
  GLdouble projection[16];
  GLint viewport[4];
  glGetDoublev(GL_MODELVIEW_MATRIX, model);
  glGetDoublev(GL_PROJECTION_MATRIX, projection);
  glGetIntegerv(GL_VIEWPORT, viewport);

  const double x = static_cast<double>(support.transform.o[0] * RENDER_SCALE);
  const double y = static_cast<double>(support.transform.o[1] * RENDER_SCALE);
  const double z = static_cast<double>(support.transform.o[2] * RENDER_SCALE);

  double sx = 0.0;
  double sy = 0.0;
  double sz = 0.0;
  if (gluProject(x, y, z, model, projection, viewport, &sx, &sy, &sz) != GL_TRUE)
    return false;

  outScreenPos.x = static_cast<int>(sx);
  outScreenPos.y = static_cast<int>(viewportHeight - sy);
  return true;
}

wxString BuildHoistLabel(const std::string &uuid, const Support &support) {
  wxString label = support.name.empty() ? wxString::FromUTF8(uuid)
                                        : wxString::FromUTF8(support.name);
  label += wxString::Format("\nh = %.2f m", support.transform.o[2] / 1000.0f);
  return label;
}

} // namespace

bool FindHoistAtScreenPoint(int mouseX, int mouseY, int viewportHeight,
                            const MvrScene &scene,
                            const std::unordered_set<std::string> &hiddenLayers,
                            std::string &outUuid, wxPoint &outScreenPos,
                            wxString &outLabel) {
  bool found = false;
  int bestDistanceSquared = std::numeric_limits<int>::max();
  for (const auto &[uuid, support] : scene.supports) {
    if (!IsLayerVisible(hiddenLayers, support.layer))
      continue;

    wxPoint projected;
    if (!ProjectSupportCenter(support, viewportHeight, projected))
      continue;

    const int dx = projected.x - mouseX;
    const int dy = projected.y - mouseY;
    const int distanceSquared = dx * dx + dy * dy;
    if (distanceSquared > kHoistHitRadiusPixels * kHoistHitRadiusPixels)
      continue;
    if (distanceSquared >= bestDistanceSquared)
      continue;

    found = true;
    bestDistanceSquared = distanceSquared;
    outUuid = uuid;
    outScreenPos = projected;
    outLabel = BuildHoistLabel(uuid, support);
  }
  return found;
}

std::vector<std::string>
GetHoistsInScreenRect(int x1, int y1, int x2, int y2, int viewportWidth,
                      int viewportHeight, const MvrScene &scene,
                      const std::unordered_set<std::string> &hiddenLayers) {
  const int minX = std::max(0, std::min(x1, x2));
  const int maxX = std::min(viewportWidth, std::max(x1, x2));
  const int minY = std::max(0, std::min(y1, y2));
  const int maxY = std::min(viewportHeight, std::max(y1, y2));

  std::vector<std::string> selection;
  selection.reserve(scene.supports.size());

  for (const auto &[uuid, support] : scene.supports) {
    if (!IsLayerVisible(hiddenLayers, support.layer))
      continue;

    wxPoint projected;
    if (!ProjectSupportCenter(support, viewportHeight, projected))
      continue;

    if (projected.x < minX || projected.x > maxX || projected.y < minY ||
        projected.y > maxY) {
      continue;
    }
    selection.push_back(uuid);
  }

  return selection;
}

} // namespace Viewer2DSupportSelection
