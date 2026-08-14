#include "truss_attachment_paths.h"

#include "filesystem_path_utils.h"
#include "gdtf_archive_reader.h"
#include "loader3ds.h"
#include "loaderglb.h"
#include "matrixutils.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <numeric>
#include <system_error>
#include <tuple>
#include <tinyxml2.h>

namespace truss_attachment_paths {
namespace {

constexpr int kCrossSectionSamples = 17;
constexpr float kEndInsetFraction = 0.04f;
constexpr float kMinimumAxisDominance = 2.0f;
constexpr float kClusterScale = 0.09f;
constexpr float kMinimumClusterRadiusMm = 8.0f;
constexpr float kTrackMatchScale = 0.14f;
constexpr float kMinimumPersistence = 0.70f;
constexpr float kMaximumStabilityScale = 0.12f;

using Point = std::array<float, 3>;
using Point2 = std::array<float, 2>;

struct Track {
  std::vector<std::pair<int, Point2>> observations;
};

// Converts a GDTF position from metres to the millimetre scene convention.
Matrix GdtfPositionMm(Matrix position) {
  for (float &value : position.o)
    value *= 1000.0f;
  return position;
}

// Applies a composed GDTF geometry transform to loader-normalized mesh data.
void TransformVertices(std::vector<float> &vertices, const Matrix &transform) {
  for (std::size_t offset = 0; offset + 2 < vertices.size(); offset += 3) {
    Matrix point = MatrixUtils::Identity();
    point.o = {vertices[offset], vertices[offset + 1], vertices[offset + 2]};
    const auto transformed = MatrixUtils::Multiply(transform, point).o;
    std::copy(transformed.begin(), transformed.end(), vertices.begin() + offset);
  }
}

// Traverses GDTF geometries and records model references with full hierarchy positions.
void CollectGdtfGeometrySources(const tinyxml2::XMLElement *node,
                                const Matrix &parent,
                                std::vector<Resolver::GdtfSource> &structures,
                                std::vector<Resolver::GdtfSource> &models) {
  for (const auto *current = node; current; current = current->NextSiblingElement()) {
    Matrix local = MatrixUtils::Identity();
    if (const char *text = current->Attribute("Position");
        text && !MatrixUtils::ParseMatrix(text, local))
      continue;
    const Matrix composed = MatrixUtils::Multiply(parent, local);
    const char *model = current->Attribute("Model");
    if (model && *model) {
      const bool structure = std::string(current->Name()) == "Structure";
      (structure ? structures : models)
          .push_back({model, GdtfPositionMm(composed),
                      structure ? Provenance::GdtfStructureGeometry
                                : Provenance::GdtfModelGeometry,
                      structure ? "structure" : "model"});
    }
    CollectGdtfGeometrySources(current->FirstChildElement(), composed,
                               structures, models);
  }
}

// Resolves GDTF geometry references to archive model resource paths.
std::vector<Resolver::GdtfSource>
ReadGdtfSources(const gdtf::ArchiveReadResult &archive) {
  tinyxml2::XMLDocument document;
  if (document.Parse(archive.descriptionXml.c_str(), archive.descriptionXml.size()) !=
      tinyxml2::XML_SUCCESS)
    return {};
  const auto *root = document.FirstChildElement("GDTF");
  const auto *fixture = root ? root->FirstChildElement("FixtureType") : nullptr;
  const auto *modelNodes = fixture ? fixture->FirstChildElement("Models") : nullptr;
  std::map<std::string, std::string> files;
  for (const auto *model = modelNodes ? modelNodes->FirstChildElement("Model") : nullptr;
       model; model = model->NextSiblingElement("Model")) {
    const char *name = model->Attribute("Name");
    const char *file = model->Attribute("File");
    if (name && file)
      files[name] = file;
  }
  std::vector<Resolver::GdtfSource> structures, generic;
  const auto *geometries = fixture ? fixture->FirstChildElement("Geometries") : nullptr;
  CollectGdtfGeometrySources(geometries ? geometries->FirstChildElement() : nullptr,
                             MatrixUtils::Identity(), structures, generic);
  auto resolve = [&](std::vector<Resolver::GdtfSource> &sources) {
    for (auto &source : sources) {
      const auto found = files.find(source.modelFile);
      source.modelFile = found == files.end() ? std::string{} : found->second;
    }
    sources.erase(std::remove_if(sources.begin(), sources.end(),
                                 [](const auto &source) { return source.modelFile.empty(); }),
                  sources.end());
  };
  resolve(structures);
  resolve(generic);
  structures.insert(structures.end(), generic.begin(), generic.end());
  return structures;
}

// Returns the squared Euclidean distance between two transverse points.
float DistanceSquared(const Point2 &a, const Point2 &b) {
  const float x = a[0] - b[0];
  const float y = a[1] - b[1];
  return x * x + y * y;
}

// Returns the version identity for a geometry resource.
std::string VersionKey(const std::filesystem::path &path) {
  std::error_code error;
  const auto canonical = std::filesystem::weakly_canonical(path, error);
  if (error)
    return {};
  const auto size = std::filesystem::file_size(canonical, error);
  if (error)
    return {};
  const auto modified = std::filesystem::last_write_time(canonical, error);
  if (error)
    return {};
  using PortableFileTicks = std::chrono::duration<std::int64_t, std::nano>;
  const std::int64_t modifiedTicks =
      std::chrono::duration_cast<PortableFileTicks>(modified.time_since_epoch())
          .count();
  return PathUtils::BuildFilesystemIdentityKey(canonical) + "|" +
         std::to_string(size) + "|" + std::to_string(modifiedTicks);
}

// Resolves a scene-relative resource without using the process working
// directory.
std::filesystem::path ResolveResource(const MvrScene &scene,
                                      const std::string &reference) {
  auto path = PathUtils::PathFromUtf8(reference);
  if (path.empty() || path.is_absolute())
    return path;
  if (scene.basePath.empty())
    return {};
  return PathUtils::PathFromUtf8(scene.basePath) / path;
}

// Computes an axis-aligned mesh extent and rejects malformed vertices.
bool Measure(const std::vector<float> &vertices, Point &minimum,
             Point &maximum) {
  if (vertices.size() < 9 || vertices.size() % 3 != 0)
    return false;
  minimum = maximum = {vertices[0], vertices[1], vertices[2]};
  for (std::size_t offset = 0; offset < vertices.size(); offset += 3) {
    for (int axis = 0; axis < 3; ++axis) {
      const float value = vertices[offset + axis];
      if (!std::isfinite(value))
        return false;
      minimum[axis] = std::min(minimum[axis], value);
      maximum[axis] = std::max(maximum[axis], value);
    }
  }
  return true;
}

// Collects triangle intersections with a longitudinal sampling plane.
std::vector<Point2> Intersections(const std::vector<float> &vertices,
                                  const std::vector<std::uint32_t> &indices,
                                  int longitudinal, int transverseA,
                                  int transverseB, float plane) {
  std::vector<Point2> points;
  auto vertex = [&](std::uint32_t index) {
    return Point{vertices[index * 3], vertices[index * 3 + 1],
                 vertices[index * 3 + 2]};
  };
  for (std::size_t offset = 0; offset + 2 < indices.size(); offset += 3) {
    if (indices[offset] * 3 + 2 >= vertices.size() ||
        indices[offset + 1] * 3 + 2 >= vertices.size() ||
        indices[offset + 2] * 3 + 2 >= vertices.size())
      continue;
    const Point triangle[3] = {vertex(indices[offset]),
                               vertex(indices[offset + 1]),
                               vertex(indices[offset + 2])};
    for (int edge = 0; edge < 3; ++edge) {
      const Point &a = triangle[edge];
      const Point &b = triangle[(edge + 1) % 3];
      const float denominator = b[longitudinal] - a[longitudinal];
      if (std::fabs(denominator) < 1e-6f)
        continue;
      const float parameter = (plane - a[longitudinal]) / denominator;
      if (parameter < 0.0f || parameter > 1.0f)
        continue;
      points.push_back(
          {a[transverseA] + parameter * (b[transverseA] - a[transverseA]),
           a[transverseB] + parameter * (b[transverseB] - a[transverseB])});
    }
  }
  return points;
}

// Clusters nearby cross-section intersections into occupied transverse regions.
std::vector<Point2> Cluster(const std::vector<Point2> &points, float radius) {
  std::vector<Point2> centers;
  std::vector<bool> assigned(points.size(), false);
  const float radiusSquared = radius * radius;
  for (std::size_t seed = 0; seed < points.size(); ++seed) {
    if (assigned[seed])
      continue;
    std::vector<std::size_t> members{seed};
    assigned[seed] = true;
    for (std::size_t cursor = 0; cursor < members.size(); ++cursor) {
      for (std::size_t candidate = 0; candidate < points.size(); ++candidate) {
        if (!assigned[candidate] &&
            DistanceSquared(points[members[cursor]], points[candidate]) <=
                radiusSquared) {
          assigned[candidate] = true;
          members.push_back(candidate);
        }
      }
    }
    if (members.size() < 2)
      continue;
    Point2 center{};
    for (std::size_t member : members) {
      center[0] += points[member][0];
      center[1] += points[member][1];
    }
    center[0] /= static_cast<float>(members.size());
    center[1] /= static_cast<float>(members.size());
    centers.push_back(center);
  }
  return centers;
}

// Returns a normalized transformed direction.
std::optional<Point> TransformDirection(const Matrix &transform,
                                        const std::optional<Point> &direction) {
  if (!direction)
    return std::nullopt;
  Point result{};
  for (int component = 0; component < 3; ++component)
    result[component] = transform.u[component] * (*direction)[0] +
                        transform.v[component] * (*direction)[1] +
                        transform.w[component] * (*direction)[2];
  const float length = std::sqrt(result[0] * result[0] + result[1] * result[1] +
                                 result[2] * result[2]);
  if (length <= 1e-6f)
    return std::nullopt;
  for (float &component : result)
    component /= length;
  return result;
}

} // namespace

// Detects persistent longitudinal chord paths in one indexed local mesh.
std::vector<Path> AnalyzeMesh(const std::vector<float> &verticesMm,
                              const std::vector<std::uint32_t> &triangleIndices,
                              Provenance provenance) {
  Point minimum{}, maximum{};
  if (!Measure(verticesMm, minimum, maximum) || triangleIndices.size() < 3)
    return {};
  const Point extent{maximum[0] - minimum[0], maximum[1] - minimum[1],
                     maximum[2] - minimum[2]};
  int longitudinal = static_cast<int>(
      std::max_element(extent.begin(), extent.end()) - extent.begin());
  const float second =
      std::max(extent[(longitudinal + 1) % 3], extent[(longitudinal + 2) % 3]);
  if (second <= 1e-5f || extent[longitudinal] / second < kMinimumAxisDominance)
    return {};
  int transverseA = (longitudinal + 1) % 3;
  int transverseB = (longitudinal + 2) % 3;
  const float transverseSpan =
      std::max(extent[transverseA], extent[transverseB]);
  const float clusterRadius =
      std::max(kMinimumClusterRadiusMm, transverseSpan * kClusterScale);
  const float matchRadius =
      std::max(clusterRadius * 1.5f, transverseSpan * kTrackMatchScale);
  std::vector<Track> tracks;
  for (int sample = 0; sample < kCrossSectionSamples; ++sample) {
    const float fraction =
        kEndInsetFraction +
        (1.0f - 2.0f * kEndInsetFraction) * sample / (kCrossSectionSamples - 1);
    const float plane = minimum[longitudinal] + extent[longitudinal] * fraction;
    auto centers =
        Cluster(Intersections(verticesMm, triangleIndices, longitudinal,
                              transverseA, transverseB, plane),
                clusterRadius);
    std::vector<bool> used(tracks.size(), false);
    for (const Point2 &center : centers) {
      std::size_t best = tracks.size();
      float bestDistance = matchRadius * matchRadius;
      for (std::size_t index = 0; index < tracks.size(); ++index) {
        if (used[index] ||
            tracks[index].observations.back().first != sample - 1)
          continue;
        const float distance =
            DistanceSquared(center, tracks[index].observations.back().second);
        if (distance < bestDistance) {
          bestDistance = distance;
          best = index;
        }
      }
      if (best == tracks.size()) {
        tracks.push_back({{{sample, center}}});
        // Keep per-sample assignments aligned when this sample creates tracks.
        used.push_back(true);
      } else {
        tracks[best].observations.push_back({sample, center});
        used[best] = true;
      }
    }
  }

  std::vector<Path> paths;
  for (const Track &track : tracks) {
    const float coverage =
        static_cast<float>(track.observations.size()) / kCrossSectionSamples;
    if (coverage < kMinimumPersistence)
      continue;
    Point2 center{};
    for (const auto &observation : track.observations) {
      center[0] += observation.second[0];
      center[1] += observation.second[1];
    }
    center[0] /= track.observations.size();
    center[1] /= track.observations.size();
    float variance = 0.0f;
    for (const auto &observation : track.observations)
      variance += DistanceSquared(center, observation.second);
    const float stability = std::sqrt(variance / track.observations.size());
    if (stability > transverseSpan * kMaximumStabilityScale)
      continue;
    Point start{}, end{};
    const auto samplePlane = [&](int sample) {
      const float fraction =
          kEndInsetFraction + (1.0f - 2.0f * kEndInsetFraction) * sample /
                                  (kCrossSectionSamples - 1);
      return minimum[longitudinal] + extent[longitudinal] * fraction;
    };
    const float sampleStep = extent[longitudinal] *
                             (1.0f - 2.0f * kEndInsetFraction) /
                             (kCrossSectionSamples - 1);
    start[longitudinal] = std::max(
        minimum[longitudinal], samplePlane(track.observations.front().first) - sampleStep);
    end[longitudinal] = std::min(
        maximum[longitudinal], samplePlane(track.observations.back().first) + sampleStep);
    start[transverseA] = end[transverseA] = center[0];
    start[transverseB] = end[transverseB] = center[1];
    Point outward{};
    outward[transverseA] =
        center[0] - (minimum[transverseA] + maximum[transverseA]) * 0.5f;
    outward[transverseB] =
        center[1] - (minimum[transverseB] + maximum[transverseB]) * 0.5f;
    const float outwardLength =
        std::hypot(outward[transverseA], outward[transverseB]);
    if (outwardLength > 1e-5f) {
      outward[transverseA] /= outwardLength;
      outward[transverseB] /= outwardLength;
    }
    Path path;
    path.stableId = "geometry-chord-" + std::to_string(paths.size());
    path.localPointsMm = {start, end};
    path.localOutwardDirection = outward;
    path.provenance = provenance;
    path.diagnostics = {
        std::clamp(coverage * (1.0f - stability / transverseSpan), 0.0f, 1.0f),
        coverage, stability, static_cast<int>(track.observations.size()),
        kCrossSectionSamples};
    paths.push_back(std::move(path));
  }
  std::sort(paths.begin(), paths.end(),
            [transverseA, transverseB](const Path &a, const Path &b) {
              return std::tie(a.localPointsMm[0][transverseA],
                              a.localPointsMm[0][transverseB]) <
                     std::tie(b.localPointsMm[0][transverseA],
                              b.localPointsMm[0][transverseB]);
            });
  for (std::size_t index = 0; index < paths.size(); ++index)
    paths[index].stableId = "geometry-chord-" + std::to_string(index);
  return paths;
}

// Applies an instance transform to a local path without changing cached data.
Path TransformPath(const Path &localPath, const Matrix &transform,
                   const std::string &ownerTrussUuid) {
  Path result = localPath;
  result.ownerTrussUuid = ownerTrussUuid;
  result.stableId = ownerTrussUuid + ":" + localPath.stableId;
  result.worldPointsMm.clear();
  for (const Point &point : localPath.localPointsMm) {
    Matrix local = MatrixUtils::Identity();
    local.o = point;
    result.worldPointsMm.push_back(MatrixUtils::Multiply(transform, local).o);
  }
  result.worldOutwardDirection =
      TransformDirection(transform, localPath.localOutwardDirection);
  return result;
}

// Returns the closest point on any segment of a path polyline.
std::optional<ClosestPoint>
ClosestPointOnPath(const Path &path, const Point &pointMm, bool worldSpace) {
  const auto &points = worldSpace ? path.worldPointsMm : path.localPointsMm;
  if (points.size() < 2)
    return std::nullopt;
  float totalLength = 0.0f;
  std::vector<float> lengths;
  for (std::size_t i = 1; i < points.size(); ++i) {
    const Point delta{points[i][0] - points[i - 1][0],
                      points[i][1] - points[i - 1][1],
                      points[i][2] - points[i - 1][2]};
    const float length = std::sqrt(delta[0] * delta[0] + delta[1] * delta[1] +
                                   delta[2] * delta[2]);
    lengths.push_back(length);
    totalLength += length;
  }
  ClosestPoint best;
  best.distanceMm = std::numeric_limits<float>::max();
  float preceding = 0.0f;
  for (std::size_t i = 1; i < points.size(); ++i) {
    const Point segment{points[i][0] - points[i - 1][0],
                        points[i][1] - points[i - 1][1],
                        points[i][2] - points[i - 1][2]};
    const Point relative{pointMm[0] - points[i - 1][0],
                         pointMm[1] - points[i - 1][1],
                         pointMm[2] - points[i - 1][2]};
    const float lengthSquared = lengths[i - 1] * lengths[i - 1];
    const float parameter =
        lengthSquared > 1e-8f
            ? std::clamp((relative[0] * segment[0] + relative[1] * segment[1] +
                          relative[2] * segment[2]) /
                             lengthSquared,
                         0.0f, 1.0f)
            : 0.0f;
    Point candidate{points[i - 1][0] + segment[0] * parameter,
                    points[i - 1][1] + segment[1] * parameter,
                    points[i - 1][2] + segment[2] * parameter};
    const float distance =
        std::sqrt((candidate[0] - pointMm[0]) * (candidate[0] - pointMm[0]) +
                  (candidate[1] - pointMm[1]) * (candidate[1] - pointMm[1]) +
                  (candidate[2] - pointMm[2]) * (candidate[2] - pointMm[2]));
    if (distance < best.distanceMm) {
      best = {candidate, distance,
              totalLength > 1e-8f
                  ? (preceding + parameter * lengths[i - 1]) / totalLength
                  : 0.0f};
    }
    preceding += lengths[i - 1];
  }
  return best;
}

// Resolves and instantiates cached local fixture attachment paths.
Resolution Resolver::Resolve(const MvrScene &scene, const Truss &truss) {
  Resolution fallback;
  const auto gdtfPath = ResolveResource(scene, truss.gdtfSpec);
  const std::string archiveVersion = VersionKey(gdtfPath);
  if (!archiveVersion.empty()) {
    auto archiveFound = m_archiveCache.find(archiveVersion);
    if (archiveFound == m_archiveCache.end()) {
      ArchiveCacheEntry entry;
      entry.sourceIdentity = PathUtils::BuildFilesystemIdentityKey(gdtfPath);
      ++m_archiveParseCount;
      const auto archive = gdtf::ReadGdtfArchive(gdtfPath);
      if (archive.Success())
        entry.sources = ReadGdtfSources(archive);
      for (auto iterator = m_archiveCache.begin();
           iterator != m_archiveCache.end();) {
        if (iterator->second.sourceIdentity == entry.sourceIdentity)
          iterator = m_archiveCache.erase(iterator);
        else
          ++iterator;
      }
      archiveFound =
          m_archiveCache.emplace(archiveVersion, std::move(entry)).first;
    }
    for (const auto &source : archiveFound->second.sources) {
        const std::string transformIdentity =
            std::to_string(source.position.u[0]) + "," +
            std::to_string(source.position.u[1]) + "," +
            std::to_string(source.position.u[2]) + "," +
            std::to_string(source.position.v[0]) + "," +
            std::to_string(source.position.v[1]) + "," +
            std::to_string(source.position.v[2]) + "," +
            std::to_string(source.position.w[0]) + "," +
            std::to_string(source.position.w[1]) + "," +
            std::to_string(source.position.w[2]) + "," +
            std::to_string(source.position.o[0]) + "," +
            std::to_string(source.position.o[1]) + "," +
            std::to_string(source.position.o[2]);
        const std::string key = archiveVersion + "|" + source.role + "|" +
                                source.modelFile + "|" + transformIdentity;
        auto found = m_cache.find(key);
        if (found == m_cache.end()) {
          CacheEntry entry;
          entry.sourceIdentity = PathUtils::BuildFilesystemIdentityKey(gdtfPath);
          gdtf::GdtfResourceReadResult resource;
          std::string extension;
          for (const auto &candidate :
               {std::string("models/gltf/") + source.modelFile + ".glb",
                std::string("models/3ds/") + source.modelFile + ".3ds"}) {
            resource = gdtf::ReadGdtfArchiveResource(gdtfPath, candidate,
                                                     128ull * 1024ull * 1024ull);
            if (resource.Success()) {
              extension = std::filesystem::path(candidate).extension().string();
              break;
            }
          }
          if (resource.Success()) {
            const auto temporary = std::filesystem::temp_directory_path() /
                ("perastage-attachment-" + std::to_string(std::hash<std::string>{}(key)) +
                 extension);
            {
              std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
              output.write(reinterpret_cast<const char *>(resource.bytes.data()),
                           static_cast<std::streamsize>(resource.bytes.size()));
            }
            Mesh mesh;
            std::string error;
            ++m_geometryParseCount;
            const bool loaded = extension == ".glb"
                                    ? LoadGLB(temporary.string(), mesh, &error, false)
                                    : Load3DS(temporary.string(), mesh, true, &error, false);
            std::error_code removeError;
            std::filesystem::remove(temporary, removeError);
            if (loaded) {
              TransformVertices(mesh.vertices, source.position);
              entry.localPaths = AnalyzeMesh(mesh.vertices, mesh.indices,
                                             source.provenance);
            }
            if (entry.localPaths.empty())
              entry.diagnostics.push_back(error.empty()
                  ? "GDTF geometry did not contain stable straight longitudinal chords."
                  : error);
          } else {
            entry.diagnostics.push_back("Referenced GDTF model resource was unavailable.");
          }
          found = m_cache.emplace(key, std::move(entry)).first;
        }
        Resolution result;
        result.sourceIdentity = found->second.sourceIdentity;
        result.diagnostics = found->second.diagnostics;
        for (const Path &local : found->second.localPaths)
          result.paths.push_back(TransformPath(local, truss.transform, truss.uuid));
        if (!result.paths.empty())
          return result;
        fallback.diagnostics.insert(fallback.diagnostics.end(),
                                    result.diagnostics.begin(), result.diagnostics.end());
    }
  }
  for (const std::string *reference : {&truss.symbolFile, &truss.modelFile}) {
    const auto path = ResolveResource(scene, *reference);
    std::string extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   ::tolower);
    if (extension != ".glb" && extension != ".3ds")
      continue;
    const std::string key = VersionKey(path);
    if (key.empty())
      continue;
    auto found = m_cache.find(key);
    if (found == m_cache.end()) {
      Mesh mesh;
      std::string error;
      ++m_geometryParseCount;
      const bool loaded = extension == ".glb"
                              ? LoadGLB(path.string(), mesh, &error, false)
                              : Load3DS(path.string(), mesh, true, &error, false);
      CacheEntry entry;
      entry.sourceIdentity = PathUtils::BuildFilesystemIdentityKey(path);
      if (loaded) {
        entry.localPaths = AnalyzeMesh(mesh.vertices, mesh.indices,
                                       Provenance::MvrGeometry);
        if (entry.localPaths.empty())
          entry.diagnostics.push_back(
              "Geometry did not contain stable straight longitudinal chords.");
      } else {
        entry.diagnostics.push_back(
            error.empty() ? "Geometry could not be loaded." : error);
      }
      for (auto iterator = m_cache.begin(); iterator != m_cache.end();) {
        if (iterator->second.sourceIdentity == entry.sourceIdentity)
          iterator = m_cache.erase(iterator);
        else
          ++iterator;
      }
      found = m_cache.emplace(key, std::move(entry)).first;
    }
    Resolution result;
    result.diagnostics = found->second.diagnostics;
    result.sourceIdentity = found->second.sourceIdentity;
    for (const Path &local : found->second.localPaths)
      result.paths.push_back(TransformPath(local, truss.transform, truss.uuid));
    if (!result.paths.empty())
      return result;
    fallback.diagnostics.insert(fallback.diagnostics.end(),
                                result.diagnostics.begin(),
                                result.diagnostics.end());
  }
  fallback.usedBoundsFallback = true;
  fallback.diagnostics.push_back("No analyzable truss mesh was available; "
                                 "oriented bounds fallback is required.");
  return fallback;
}

// Clears all cached local analyses.
void Resolver::Clear() {
  m_cache.clear();
  m_archiveCache.clear();
}

// Returns the number of physical geometry parses performed by this resolver.
std::size_t Resolver::GeometryParseCount() const {
  return m_geometryParseCount;
}

// Returns the number of GDTF archives parsed by this resolver.
std::size_t Resolver::ArchiveParseCount() const { return m_archiveParseCount; }

} // namespace truss_attachment_paths
