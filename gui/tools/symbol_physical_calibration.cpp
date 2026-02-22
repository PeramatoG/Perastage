#include "tools/symbol_physical_calibration.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <filesystem>
#include <unordered_map>

#include "configmanager.h"
#include "gdtfdictionary.h"
#include "projectutils.h"
#include "types.h"
#include "viewer3d/gdtfloader.h"

namespace fs = std::filesystem;

namespace tools {
namespace {

struct Bounds3D {
  std::array<float, 3> min = {FLT_MAX, FLT_MAX, FLT_MAX};
  std::array<float, 3> max = {-FLT_MAX, -FLT_MAX, -FLT_MAX};
  bool valid = false;
};

struct Bounds2D {
  float minX = 0.0f;
  float minY = 0.0f;
  float maxX = 0.0f;
  float maxY = 0.0f;
  bool valid = false;
};

std::array<float, 3> TransformPoint(const Matrix &m, const std::array<float, 3> &p) {
  return {m.u[0] * p[0] + m.v[0] * p[1] + m.w[0] * p[2] + m.o[0],
          m.u[1] * p[0] + m.v[1] * p[1] + m.w[1] * p[2] + m.o[1],
          m.u[2] * p[0] + m.v[2] * p[1] + m.w[2] * p[2] + m.o[2]};
}

void ExtendBounds(Bounds3D &bounds, const std::array<float, 3> &p) {
  bounds.min[0] = std::min(bounds.min[0], p[0]);
  bounds.min[1] = std::min(bounds.min[1], p[1]);
  bounds.min[2] = std::min(bounds.min[2], p[2]);
  bounds.max[0] = std::max(bounds.max[0], p[0]);
  bounds.max[1] = std::max(bounds.max[1], p[1]);
  bounds.max[2] = std::max(bounds.max[2], p[2]);
  bounds.valid = true;
}

std::string ResolveFixtureGdtfPath(const Fixture &fixture, const MvrScene &scene) {
  const fs::path specPath = fs::path(fixture.gdtfSpec);
  if (specPath.empty())
    return {};

  std::error_code ec;
  if (specPath.is_absolute() && fs::exists(specPath, ec) && !ec)
    return specPath.string();

  if (!scene.basePath.empty()) {
    ec.clear();
    const fs::path fromScene = fs::path(scene.basePath) / specPath;
    if (fs::exists(fromScene, ec) && !ec)
      return fromScene.string();
  }

  ec.clear();
  const fs::path fromCwd = fs::current_path(ec) / specPath;
  if (!ec && fs::exists(fromCwd, ec) && !ec)
    return fromCwd.string();

  const std::string specFileName = specPath.filename().string();
  const fs::path fixturesLibrary =
      fs::path(ProjectUtils::GetDefaultLibraryPath("fixtures"));

  auto dictOpt = GdtfDictionary::Load();
  if (dictOpt) {
    const auto &dict = *dictOpt;
    auto findByKey = [&](const std::string &key) -> std::string {
      if (key.empty())
        return {};
      auto it = dict.find(key);
      if (it == dict.end() || it->second.path.empty())
        return {};
      std::error_code localEc;
      if (!fs::exists(it->second.path, localEc) || localEc)
        return {};
      return it->second.path;
    };

    if (std::string byType = findByKey(fixture.typeName); !byType.empty())
      return byType;

    if (!specFileName.empty()) {
      for (const auto &[type, entry] : dict) {
        (void)type;
        if (entry.path.empty())
          continue;
        const fs::path entryPath = fs::path(entry.path);
        if (entryPath.filename() != specFileName)
          continue;
        std::error_code localEc;
        if (fs::exists(entryPath, localEc) && !localEc)
          return entryPath.string();
      }
    }
  }

  if (!specFileName.empty()) {
    ec.clear();
    const fs::path exactPath = fixturesLibrary / specFileName;
    if (fs::exists(exactPath, ec) && !ec)
      return exactPath.string();
  }

  return {};
}

bool ComputeFixtureBoundsMm(const std::string &gdtfPath, Bounds3D &bounds,
                            std::string &errorMessage) {
  std::vector<GdtfObject> objects;
  if (!LoadGdtf(gdtfPath, objects, &errorMessage))
    return false;

  for (const auto &object : objects) {
    for (size_t vi = 0; vi + 2 < object.mesh.vertices.size(); vi += 3) {
      const std::array<float, 3> local = {
          object.mesh.vertices[vi], object.mesh.vertices[vi + 1], object.mesh.vertices[vi + 2]};
      ExtendBounds(bounds, TransformPoint(object.transform, local));
    }
  }

  if (!bounds.valid) {
    errorMessage = "Could not determine fixture geometry bounds from GDTF meshes.";
    return false;
  }

  return true;
}

Bounds2D BuildTargetBounds(const Bounds3D &bounds, symbols::SymbolView view) {
  Bounds2D projected;
  switch (view) {
  case symbols::SymbolView::Top:
  case symbols::SymbolView::Bottom:
    projected = {bounds.min[0], bounds.min[1], bounds.max[0], bounds.max[1], true};
    break;
  case symbols::SymbolView::Front:
    projected = {bounds.min[0], bounds.min[2], bounds.max[0], bounds.max[2], true};
    break;
  case symbols::SymbolView::Left:
    projected = {bounds.min[1], bounds.min[2], bounds.max[1], bounds.max[2], true};
    break;
  }
  return projected;
}

bool RemapSymbolToBounds(symbols::Symbol2D &symbol, const Bounds2D &target) {
  if (!symbol.bounds.valid || !target.valid)
    return false;

  const float sourceMinX = symbol.bounds.min.x;
  const float sourceMinY = symbol.bounds.min.y;
  const float sourceWidth = symbol.bounds.max.x - symbol.bounds.min.x;
  const float sourceHeight = symbol.bounds.max.y - symbol.bounds.min.y;
  const float targetWidth = target.maxX - target.minX;
  const float targetHeight = target.maxY - target.minY;

  if (sourceWidth <= 0.0f || sourceHeight <= 0.0f || targetWidth <= 0.0f ||
      targetHeight <= 0.0f)
    return false;

  const float scaleX = targetWidth / sourceWidth;
  const float scaleY = targetHeight / sourceHeight;

  auto remapPoint = [&](symbols::Point2D &point) {
    point.x = target.minX + (point.x - sourceMinX) * scaleX;
    point.y = target.minY + (point.y - sourceMinY) * scaleY;
  };

  for (auto &polygon : symbol.fill) {
    for (auto &point : polygon.outer)
      remapPoint(point);
    for (auto &hole : polygon.holes) {
      for (auto &point : hole)
        remapPoint(point);
    }
  }

  for (auto &stroke : symbol.strokes) {
    for (auto &point : stroke)
      remapPoint(point);
  }

  symbol.bounds.min.x = target.minX;
  symbol.bounds.min.y = target.minY;
  symbol.bounds.max.x = target.maxX;
  symbol.bounds.max.y = target.maxY;
  symbol.bounds.valid = true;
  return true;
}

} // namespace

bool CalibrateFixtureSymbolsToPhysicalUnits(ConfigManager &cfg,
                                            const std::string &fixtureUuid,
                                            std::vector<symbols::Symbol2D> &symbols,
                                            std::string &errorMessage) {
  const auto &fixtures = cfg.GetScene().fixtures;
  const auto fixtureIt = fixtures.find(fixtureUuid);
  if (fixtureIt == fixtures.end()) {
    errorMessage = "Could not resolve selected fixture for symbol calibration.";
    return false;
  }

  const std::string gdtfPath = ResolveFixtureGdtfPath(fixtureIt->second, cfg.GetScene());
  if (gdtfPath.empty()) {
    errorMessage = "Could not resolve fixture GDTF path for symbol calibration.";
    return false;
  }

  Bounds3D fixtureBounds;
  if (!ComputeFixtureBoundsMm(gdtfPath, fixtureBounds, errorMessage))
    return false;

  bool remappedAny = false;
  for (auto &symbol : symbols) {
    if (!symbol.bounds.valid)
      continue;
    const Bounds2D target = BuildTargetBounds(fixtureBounds, symbol.view);
    if (!RemapSymbolToBounds(symbol, target))
      continue;
    remappedAny = true;
  }

  if (!remappedAny) {
    errorMessage = "No valid symbol views were available for physical calibration.";
    return false;
  }

  return true;
}

} // namespace tools
