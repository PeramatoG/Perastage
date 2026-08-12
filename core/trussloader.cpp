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
#include "runtime_storage.h"
#include "trussloader.h"
#include "filesystem_path_utils.h"
#include "wx_path_utils.h"

#include "truss_gdtf_builder.h"
#include "geometry_bounds_resolver.h"
#include "truss_dimension_resolution.h"
#include "logger.h"

#include <tinyxml2.h>
#include <wx/filename.h>
#include <wx/wfstream.h>
#include <wx/zipstrm.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cmath>
#include <filesystem>
#include <functional>
#include <fstream>
#include <memory>
#include <sstream>

namespace fs = std::filesystem;

namespace {

constexpr const char *kSupportedTrussFileDialogWildcard =
    "Truss files (*.gdtf;*.gtruss;*.glb;*.3ds)|*.gdtf;*.gtruss;*.glb;*.3ds";

// Converts a filesystem path to a UTF-8 string for storage and UI handoff.
static std::string ToUtf8String(const fs::path &path) {
  std::u8string utf8 = path.u8string();
  return std::string(utf8.begin(), utf8.end());
}

// Converts a UTF-8 string into a filesystem path without losing non-ASCII characters.
static fs::path FromUtf8String(const std::string &text) {
#if defined(__cpp_lib_char8_t)
  const char8_t *begin = reinterpret_cast<const char8_t *>(text.data());
  return fs::path(std::u8string(begin, begin + text.size()));
#else
  return PathUtils::PathFromUtf8(text);
#endif
}

// Returns a lowercase file extension for extension-based loader dispatch.
static std::string LowerExt(fs::path path) {
  std::string ext = path.extension().string();
  std::transform(ext.begin(), ext.end(), ext.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return ext;
}

// Parses a floating-point XML attribute into the provided output value.
static bool ParseFloatAttr(tinyxml2::XMLElement *node, const char *name, float &out) {
  if (!node)
    return false;
  float parsed = 0.0f;
  if (node->QueryFloatAttribute(name, &parsed) == tinyxml2::XML_SUCCESS) {
    out = parsed;
    return true;
  }
  return false;
}

// Checks whether a path is inside or equal to a normalized base path.
static bool IsPathWithinBase(const fs::path &path, const fs::path &base) {
  auto pathIt = path.begin();
  auto baseIt = base.begin();
  for (; baseIt != base.end(); ++baseIt, ++pathIt) {
    if (pathIt == path.end() || *pathIt != *baseIt)
      return false;
  }
  return true;
}

// Reports whether an archive entry name is absolute on supported path syntaxes.
static bool IsAbsoluteArchiveEntryName(const std::string &entryName, const fs::path &entryPath) {
  if (entryPath.is_absolute() || entryPath.has_root_name())
    return true;
  return entryName.size() >= 1 &&
         (entryName[0] == '/' || entryName[0] == '\\' ||
          (entryName.size() >= 2 && std::isalpha(static_cast<unsigned char>(entryName[0])) &&
           entryName[1] == ':'));
}

// Writes a warning for an unsafe archive entry rejected during extraction.
static void LogUnsafeArchiveEntry(const fs::path &archivePath, const std::string &entryName) {
  std::ostringstream msg;
  msg << "Rejected unsafe truss archive entry '" << entryName << "' in '"
      << ToUtf8String(archivePath) << "'";
  Logger::Instance().Log(Logger::Level::Warn, msg.str());
}

// Resolves a ZIP entry path safely below the requested extraction destination.
static bool ResolveArchiveEntryTarget(const fs::path &archivePath,
                                      const fs::path &destinationRoot,
                                      const std::string &entryName,
                                      fs::path &target) {
  fs::path entryPath = fs::path(entryName);
  if (IsAbsoluteArchiveEntryName(entryName, entryPath)) {
    LogUnsafeArchiveEntry(archivePath, entryName);
    return false;
  }

  entryPath = entryPath.lexically_normal();
  if (entryPath.empty()) {
    target.clear();
    return true;
  }

  target = (destinationRoot / entryPath).lexically_normal();
  if (!IsPathWithinBase(target, destinationRoot)) {
    LogUnsafeArchiveEntry(archivePath, entryName);
    return false;
  }
  return true;
}

// Extracts a ZIP-based archive into the requested destination directory.
static bool ExtractArchive(const fs::path &archivePath, const fs::path &destination) {
  wxFileInputStream input(WxPathUtils::WxStringFromFilesystemPath(archivePath));
  if (!input.IsOk())
    return false;

  std::error_code ec;
  fs::create_directories(destination, ec);
  fs::path destinationRoot = fs::weakly_canonical(destination, ec);
  if (ec) {
    ec.clear();
    destinationRoot = fs::absolute(destination, ec);
  }
  if (ec)
    return false;
  destinationRoot = destinationRoot.lexically_normal();

  wxZipInputStream zip(input);
  std::unique_ptr<wxZipEntry> entry;
  while ((entry.reset(zip.GetNextEntry())), entry) {
    const std::string entryName = entry->GetName().ToStdString();
    fs::path target;
    if (!ResolveArchiveEntryTarget(archivePath, destinationRoot, entryName, target))
      return false;
    if (target.empty())
      continue;
    if (entry->IsDir()) {
      wxFileName::Mkdir(WxPathUtils::WxStringFromFilesystemPath(target),
                       wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL);
      continue;
    }
    wxFileName::Mkdir(WxPathUtils::WxStringFromFilesystemPath(target.parent_path()),
                     wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL);
    std::ofstream out(target, std::ios::binary);
    if (!out.is_open())
      return false;
    char buf[4096];
    while (true) {
      zip.Read(buf, sizeof(buf));
      size_t bytes = zip.LastRead();
      if (bytes == 0)
        break;
      out.write(buf, bytes);
    }
  }
  return true;
}


// Computes a deterministic FNV-1a hash for extraction cache directory names.
static std::string StableHashString(const std::string &text) {
  uint64_t hash = 14695981039346656037ull;
  for (unsigned char c : text) {
    hash ^= c;
    hash *= 1099511628211ull;
  }
  std::ostringstream out;
  out << std::hex << hash;
  return out.str();
}

// Builds a stable extraction cache path for a GDTF archive.
static fs::path BuildGdtfExtractionCacheDir(const fs::path &gdtfPath) {
  std::string key = PathUtils::BuildFilesystemIdentityKey(gdtfPath);
  std::error_code timeEc;
  const auto writeTime = fs::last_write_time(gdtfPath, timeEc);
  if (!timeEc)
    key += "#" +
           std::to_string(static_cast<long long>(writeTime.time_since_epoch().count()));
  std::error_code sizeEc;
  const auto size = fs::file_size(gdtfPath, sizeEc);
  if (!sizeEc)
    key += "#" + std::to_string(static_cast<unsigned long long>(size));

  fs::path cacheRoot = runtime_storage::GetPerastageCacheRoot() / "truss-gdtf";
  return cacheRoot / StableHashString(key);
}

// Finds the first candidate path that exists below the provided base directory.
static fs::path FindFirstExisting(const fs::path &base,
                                  std::initializer_list<fs::path> candidates) {
  for (const auto &candidate : candidates) {
    fs::path fullPath = base / candidate;
    if (fs::exists(fullPath))
      return fullPath;
  }
  return {};
}

// Writes a concise truss definition load diagnostic for add-truss validation.
static void LogTrussDefinitionLoadResult(const fs::path &path, const std::string &ext,
                                         bool success, const Truss &truss) {
  std::ostringstream msg;
  msg << "Truss definition load: extension='" << ext << "'"
      << " parsingSucceeded=" << (success ? "true" : "false")
      << " symbolFile='" << truss.symbolFile << "'"
      << " dimensionsMm=" << truss.lengthMm << "x" << truss.widthMm
      << "x" << truss.heightMm << " path='" << ToUtf8String(path) << "'";
  Logger::Instance().Log(success ? Logger::Level::Info : Logger::Level::Warn,
                         msg.str());
}

} // namespace

// Returns the extraction cache directory used for a GDTF archive in tests.
fs::path BuildGdtfExtractionCacheDirForTesting(const fs::path &gdtfPath) {
  return BuildGdtfExtractionCacheDir(gdtfPath);
}

// Reports whether a raw truss archive entry resolves safely below a destination root.
bool IsTrussArchiveEntryTargetSafeForTesting(const std::string &entryName,
                                             const std::filesystem::path &destinationRoot) {
  fs::path target;
  return ResolveArchiveEntryTarget(fs::path("test.gdtf"), destinationRoot.lexically_normal(),
                                   entryName, target);
}

// Returns the file dialog wildcard matching the supported truss loader inputs.
std::string GetTrussDefinitionFileDialogWildcard() {
  return kSupportedTrussFileDialogWildcard;
}

// Reports whether the path extension is supported by the truss loader and renderer.
bool IsSupportedTrussDefinitionExtension(const std::string &path) {
  const std::string ext = LowerExt(FromUtf8String(path));
  return ext == ".gdtf" || ext == ".gtruss" || ext == ".glb" || ext == ".3ds";
}

// Loads GDTF truss metadata and resolves renderable 3D geometry when available.
bool LoadTrussGdtf(const std::string &gdtfPath, Truss &outTruss) {
  fs::path inputPath = FromUtf8String(gdtfPath);
  if (!fs::exists(inputPath))
    return false;

  outTruss = Truss{};
  outTruss.modelFile = ToUtf8String(inputPath);
  outTruss.gdtfSpec = ToUtf8String(inputPath);

  fs::path baseDir = BuildGdtfExtractionCacheDir(inputPath);
  std::error_code ec;
  fs::create_directories(baseDir, ec);
  fs::path descPath = baseDir / "description.xml";
  if (!fs::exists(descPath)) {
    if (!ExtractArchive(inputPath, baseDir))
      return false;
  }

  if (!fs::exists(descPath))
    return false;

  tinyxml2::XMLDocument doc;
  if (doc.LoadFile(descPath.string().c_str()) != tinyxml2::XML_SUCCESS)
    return false;

  tinyxml2::XMLElement *fixtureType = doc.FirstChildElement("GDTF");
  fixtureType = fixtureType ? fixtureType->FirstChildElement("FixtureType")
                            : doc.FirstChildElement("FixtureType");
  if (!fixtureType)
    return false;

  if (const char *manufacturer = fixtureType->Attribute("Manufacturer"))
    outTruss.manufacturer = manufacturer;
  if (const char *name = fixtureType->Attribute("Name")) {
    outTruss.model = name;
    outTruss.name = name;
  }

  tinyxml2::XMLElement *models = fixtureType->FirstChildElement("Models");
  if (tinyxml2::XMLElement *model = models ? models->FirstChildElement("Model") : nullptr) {
    float lengthM = 0.0f;
    float widthM = 0.0f;
    float heightM = 0.0f;
    if (ParseFloatAttr(model, "Length", lengthM))
      outTruss.lengthMm = lengthM * 1000.0f;
    if (ParseFloatAttr(model, "Width", widthM))
      outTruss.widthMm = widthM * 1000.0f;
    if (ParseFloatAttr(model, "Height", heightM))
      outTruss.heightMm = heightM * 1000.0f;
    if (HasValidTrussDimensions(outTruss))
      outTruss.dimensionSource = Truss::DimensionSource::GdtfModel;

    std::string fileBase = "main";
    if (const char *fileAttr = model->Attribute("File"); fileAttr && *fileAttr)
      fileBase = fileAttr;
    fs::path modelPath = FindFirstExisting(
        baseDir,
        {fs::path("models/gltf") / (fileBase + ".glb"),
         fs::path("models/3ds") / (fileBase + ".3ds")});
    if (!modelPath.empty())
      outTruss.symbolFile = ToUtf8String(modelPath);

    if (!modelPath.empty()) {
      outTruss.localGeometryBounds = GeometryBoundsResolver::Resolve(modelPath);
      if (outTruss.localGeometryBounds) {
        ResolveTrussDimensionsFromGeometry(outTruss, false);
      }
    }

  }

  tinyxml2::XMLElement *phys = fixtureType->FirstChildElement("PhysicalDescriptions");
  tinyxml2::XMLElement *props = phys ? phys->FirstChildElement("Properties") : nullptr;
  tinyxml2::XMLElement *weight = props ? props->FirstChildElement("Weight") : nullptr;
  if (weight)
    ParseFloatAttr(weight, "Value", outTruss.weightKg);

  tinyxml2::XMLElement *dmxModes = fixtureType->FirstChildElement("DMXModes");
  if (tinyxml2::XMLElement *mode = dmxModes ? dmxModes->FirstChildElement("DMXMode") : nullptr) {
    if (const char *modeName = mode->Attribute("Name"))
      outTruss.gdtfMode = modeName;
  }
  if (outTruss.gdtfMode.empty())
    outTruss.gdtfMode = "Default";

  return true;
}

// Loads a truss archive through the general truss definition loader.
bool LoadTrussArchive(const std::string &archivePath, Truss &outTruss) {
  return LoadTrussDefinition(archivePath, outTruss);
}

// Loads a supported truss definition or direct renderable model file.
bool LoadTrussDefinition(const std::string &path, Truss &outTruss) {
  fs::path inputPath = FromUtf8String(path);
  std::string ext = LowerExt(inputPath);
  bool success = false;

  if (ext == ".gdtf") {
    success = LoadTrussGdtf(ToUtf8String(inputPath), outTruss);
    LogTrussDefinitionLoadResult(inputPath, ext, success, outTruss);
    return success;
  }

  if (ext == ".gtruss") {
    fs::path migratedPath = inputPath;
    migratedPath.replace_extension(".gdtf");
    std::string error;
    if (fs::exists(migratedPath) ||
        ConvertLegacyGtrussToGdtf(inputPath, migratedPath, &error)) {
      success = LoadTrussGdtf(ToUtf8String(migratedPath), outTruss);
    }
    LogTrussDefinitionLoadResult(inputPath, ext, success, outTruss);
    return success;
  }

  if (ext == ".glb" || ext == ".3ds") {
    std::error_code ec;
    if (fs::exists(inputPath, ec) && !ec && fs::is_regular_file(inputPath, ec) &&
        !ec) {
      outTruss = Truss{};
      outTruss.symbolFile = ToUtf8String(inputPath);
      outTruss.modelFile = ToUtf8String(inputPath);
      outTruss.sourceRepresentation = Truss::GeometryRepresentation::NativePerastage;
      std::string diagnostic;
      outTruss.localGeometryBounds =
          GeometryBoundsResolver::Resolve(inputPath, &diagnostic);
      if (outTruss.localGeometryBounds) {
        const auto size = outTruss.localGeometryBounds->SizeMm();
        outTruss.lengthMm = size[0];
        outTruss.widthMm = size[1];
        outTruss.heightMm = size[2];
        outTruss.dimensionSource = Truss::DimensionSource::GeometryDerived;
        success = true;
      } else {
        Logger::Instance().Log(Logger::Level::Warn,
                               "Truss geometry bounds unavailable: " + diagnostic);
      }
    }
    LogTrussDefinitionLoadResult(inputPath, ext, success, outTruss);
    return success;
  }

  LogTrussDefinitionLoadResult(inputPath, ext, false, outTruss);
  return false;
}
