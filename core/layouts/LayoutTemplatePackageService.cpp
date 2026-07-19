/*
 * This file is part of Perastage.
 * Copyright (C) 2025 Luisma Peramato
 *
 * Perastage is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#include "LayoutTemplatePackageService.h"

#include "LayoutImageResourceRegistry.h"
#include "LayoutTemplateSerializer.h"
#include "filesystem_path_utils.h"
#include "json.hpp"
#include "projectutils.h"
#include "runtime_storage.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <map>
#include <memory>
#include <set>
#include <sstream>

#include <wx/wfstream.h>
class wxZipStreamLink;
#include <wx/zipstrm.h>

namespace layouts {
namespace {
namespace fs = std::filesystem;

constexpr const char *kPackageFormat = "perastage-layout-package";
constexpr int kPackageVersion = 1;
constexpr const char *kManifestPath = "manifest.json";
constexpr const char *kLayoutPath = "layout.json";
constexpr const char *kImagePrefix = "resources/layout_images/";
constexpr std::uintmax_t kMaxManifestBytes = 64 * 1024;
constexpr std::uintmax_t kMaxLayoutBytes = 16 * 1024 * 1024;
constexpr std::uintmax_t kMaxImageBytes = 64 * 1024 * 1024;
constexpr std::uintmax_t kMaxTotalImageBytes = 256 * 1024 * 1024;
constexpr int kMaxEntries = 4096;
std::vector<runtime_storage::SceneResourceLeasePtr> g_layoutResourceLeases;

// Converts a filesystem path to a UTF-8 string for portable metadata.
std::string ToUtf8String(const fs::path &path) {
  const auto utf8 = path.u8string();
  return std::string(utf8.begin(), utf8.end());
}

// Returns true when an archive entry is a safe relative path.
bool IsSafeArchivePath(const std::string &name) {
  if (name.empty() || name.find('\\') != std::string::npos)
    return false;
  const fs::path path(name);
  if (path.is_absolute())
    return false;
  for (const auto &part : path) {
    const std::string text = part.generic_string();
    if (text.empty() || text == "." || text == "..")
      return false;
  }
  return true;
}

// Reads a complete filesystem file into memory.
bool ReadFileBytes(const std::string &path, std::vector<std::uint8_t> &out) {
  std::ifstream in(PathUtils::PathFromUtf8(path), std::ios::binary);
  if (!in.is_open())
    return false;
  out.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
  return in.good() || in.eof();
}

// Returns a lowercase extension suitable for content-addressed image names.
std::string NormalizedExtension(const std::string &path) {
  std::string ext = PathUtils::PathFromUtf8(path).extension().string();
  std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });
  return ext.empty() ? ".bin" : ext;
}

// Builds a deterministic FNV-1a based content-addressed package path.
std::string PackageImagePath(const std::vector<std::uint8_t> &bytes,
                             const std::string &sourceHint) {
  std::uint64_t hash = 1469598103934665603ULL;
  for (const auto byte : bytes) {
    hash ^= byte;
    hash *= 1099511628211ULL;
  }
  std::ostringstream name;
  name << kImagePrefix << "image_" << std::hex << std::setw(16)
       << std::setfill('0') << hash << NormalizedExtension(sourceHint);
  return name.str();
}

// Reads the current ZIP entry payload with a conservative size limit.
bool ReadCurrentEntry(wxZipInputStream &zip, std::vector<std::uint8_t> &out,
                      std::uintmax_t limit) {
  char buffer[8192];
  while (true) {
    zip.Read(buffer, sizeof(buffer));
    const size_t bytes = zip.LastRead();
    if (bytes == 0)
      break;
    if (out.size() + bytes > limit)
      return false;
    out.insert(out.end(), buffer, buffer + bytes);
  }
  return true;
}

// Writes one byte payload to a ZIP package entry.
bool WriteEntry(wxZipOutputStream &zip, const std::string &name,
                const std::vector<std::uint8_t> &bytes) {
  if (!IsSafeArchivePath(name))
    return false;
  if (!zip.PutNextEntry(wxString::FromUTF8(name.c_str())))
    return false;
  if (!bytes.empty()) {
    zip.Write(bytes.data(), bytes.size());
    if (!zip.IsOk())
      return false;
  }
  return zip.CloseEntry();
}

// Resolves bytes for one image using registry data before source files.
bool ResolveImageBytes(const LayoutImageDefinition &image,
                       std::vector<std::uint8_t> &bytes, std::string *error) {
  if (!image.projectResourcePath.empty() &&
      LayoutImageResourceRegistry::Get().GetResourceBytes(image.projectResourcePath, bytes))
    return true;
  if (!image.imagePath.empty() && ReadFileBytes(image.imagePath, bytes))
    return true;
  if (error)
    *error = "Could not resolve image bytes for layout image " +
             std::to_string(image.id) + ".";
  return false;
}


// Removes the resource prefix accepted by legacy JSON image references.
std::string StripResourcesPrefix(const std::string &relativePath) {
  constexpr const char *kForward = "resources/";
  constexpr const char *kBackward = "resources\\";
  if (relativePath.rfind(kForward, 0) == 0)
    return relativePath.substr(std::char_traits<char>::length(kForward));
  if (relativePath.rfind(kBackward, 0) == 0)
    return relativePath.substr(std::char_traits<char>::length(kBackward));
  return relativePath;
}

// Resolves one legacy JSON image path using the historical search locations.
std::string ResolveLegacyImagePath(const std::string &rawPath,
                                   const fs::path &templateDir) {
  if (rawPath.empty())
    return rawPath;
  const fs::path parsed = PathUtils::PathFromUtf8(rawPath);
  std::error_code ec;
  if (parsed.is_absolute())
    return fs::exists(parsed, ec) && !ec ? ToUtf8String(fs::absolute(parsed, ec)) : rawPath;
  auto existingAbsolute = [](const fs::path &candidate) -> std::string {
    std::error_code candidateEc;
    if (!fs::exists(candidate, candidateEc) || candidateEc)
      return {};
    fs::path absolute = fs::absolute(candidate, candidateEc);
    return candidateEc ? std::string() : ToUtf8String(absolute);
  };
  if (std::string resolved = existingAbsolute(templateDir / parsed); !resolved.empty())
    return resolved;
  const fs::path resourceRoot = ProjectUtils::GetResourceRoot();
  if (!resourceRoot.empty()) {
    if (std::string resolved = existingAbsolute(resourceRoot / parsed); !resolved.empty())
      return resolved;
    const std::string stripped = StripResourcesPrefix(rawPath);
    if (stripped != rawPath)
      if (std::string resolved = existingAbsolute(resourceRoot / PathUtils::PathFromUtf8(stripped));
          !resolved.empty())
        return resolved;
  }
  return rawPath;
}

// Materializes imported package images into session-owned runtime storage.
bool MaterializeImages(LayoutDefinition &layout,
                       const std::map<std::string, std::vector<std::uint8_t>> &entries,
                       std::string *error) {
  runtime_storage::TemporaryWorkspace workspace("layout-package");
  if (!workspace.IsValid()) {
    if (error)
      *error = "Could not create runtime storage for layout package images.";
    return false;
  }
  for (auto &image : layout.imageViews) {
    const std::string resource = image.projectResourcePath.empty()
                                     ? image.imagePath
                                     : image.projectResourcePath;
    if (resource.rfind(kImagePrefix, 0) != 0 || !IsSafeArchivePath(resource)) {
      if (error)
        *error = "Layout image references an invalid package resource.";
      return false;
    }
    auto it = entries.find(resource);
    if (it == entries.end()) {
      if (error)
        *error = "Layout package is missing referenced image resource: " + resource;
      return false;
    }
    const fs::path outPath = workspace.Path() / PathUtils::PathFromUtf8(resource).filename();
    std::ofstream out(outPath, std::ios::binary);
    if (!out.is_open()) {
      if (error)
        *error = "Could not materialize layout image resource.";
      return false;
    }
    out.write(reinterpret_cast<const char *>(it->second.data()),
              static_cast<std::streamsize>(it->second.size()));
    if (!out.good()) {
      if (error)
        *error = "Could not write materialized layout image resource.";
      return false;
    }
    image.imagePath = ToUtf8String(outPath);
    image.projectResourcePath = resource;
    image.originalImagePath.clear();
    LayoutImageResourceRegistry::Get().RegisterResourceBytes(resource, {}, it->second);
  }
  g_layoutResourceLeases.push_back(workspace.TransferToSceneLease());
  return true;
}

} // namespace

// Exports one layout as a portable, self-contained .pslayout package.
bool LayoutTemplatePackageService::ExportPackage(
    const LayoutDefinition &layout, const std::string &destinationPath,
    std::string *error) {
  LayoutDefinition packageLayout = layout;
  std::map<std::string, std::vector<std::uint8_t>> imageEntries;
  for (auto &image : packageLayout.imageViews) {
    std::vector<std::uint8_t> bytes;
    if (!ResolveImageBytes(image, bytes, error))
      return false;
    const std::string resourcePath = PackageImagePath(bytes, image.imagePath);
    imageEntries.emplace(resourcePath, bytes);
    image.imagePath = resourcePath;
    image.projectResourcePath = resourcePath;
    image.originalImagePath.clear();
  }

  const nlohmann::json manifest{{"format", kPackageFormat},
                                {"packageVersion", kPackageVersion},
                                {"layoutSchemaVersion", kLayoutTemplateSchemaVersion},
                                {"entryPoint", kLayoutPath},
                                {"createdWith", "Perastage"}};
  const std::string manifestText = manifest.dump(2);
  const std::string layoutText = ToTemplateDocument({packageLayout}).dump(2);
  const fs::path destination = PathUtils::PathFromUtf8(destinationPath);
  const fs::path temp = destination.parent_path() / (destination.filename().string() + ".tmp");
  std::error_code ec;
  fs::remove(temp, ec);
  {
    wxFFileOutputStream out(WxString::FromUTF8(ToUtf8String(temp).c_str()));
    if (!out.IsOk()) {
      if (error)
        *error = "Could not open temporary package file.";
      return false;
    }
    wxZipOutputStream zip(out);
    if (!WriteEntry(zip, kManifestPath, {manifestText.begin(), manifestText.end()}) ||
        !WriteEntry(zip, kLayoutPath, {layoutText.begin(), layoutText.end()})) {
      if (error)
        *error = "Could not write layout package.";
      fs::remove(temp, ec);
      return false;
    }
    for (const auto &[name, bytes] : imageEntries) {
      if (!WriteEntry(zip, name, bytes)) {
        if (error)
          *error = "Could not write image resource to layout package.";
        fs::remove(temp, ec);
        return false;
      }
    }
    if (!zip.Close()) {
      if (error)
        *error = "Could not finalize layout package.";
      fs::remove(temp, ec);
      return false;
    }
  }
  LayoutTemplateImportResult validation;
  if (!ImportPortablePackage(ToUtf8String(temp), validation, error)) {
    fs::remove(temp, ec);
    return false;
  }
  fs::rename(temp, destination, ec);
  if (ec) {
    fs::copy_file(temp, destination, fs::copy_options::overwrite_existing, ec);
    fs::remove(temp, ec);
  }
  if (ec) {
    if (error)
      *error = "Could not publish layout package.";
    return false;
  }
  return true;
}

// Imports either a portable .pslayout package or a legacy JSON template.
bool LayoutTemplatePackageService::ImportFile(
    const std::string &sourcePath, LayoutTemplateImportResult &result,
    std::string *error) {
  std::string ext = PathUtils::PathFromUtf8(sourcePath).extension().string();
  std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });
  if (ext == ".pslayout")
    return ImportPortablePackage(sourcePath, result, error);
  return ImportLegacyJson(sourcePath, result, error);
}

// Imports and validates a portable .pslayout ZIP package.
bool LayoutTemplatePackageService::ImportPortablePackage(
    const std::string &sourcePath, LayoutTemplateImportResult &result,
    std::string *error) {
  wxFFileInputStream input(WxString::FromUTF8(sourcePath.c_str()));
  if (!input.IsOk()) {
    if (error)
      *error = "Could not open layout package.";
    return false;
  }
  wxZipInputStream zip(input);
  std::map<std::string, std::vector<std::uint8_t>> entries;
  std::set<std::string> seen;
  std::unique_ptr<wxZipEntry> entry;
  int count = 0;
  std::uintmax_t totalImages = 0;
  while ((entry.reset(zip.GetNextEntry())), entry) {
    const std::string name = entry->GetName().ToStdString();
    if (++count > kMaxEntries || !IsSafeArchivePath(name) || !seen.insert(name).second) {
      if (error)
        *error = "Layout package contains unsafe or duplicate archive entries.";
      return false;
    }
    const bool isManifest = name == kManifestPath;
    const bool isLayout = name == kLayoutPath;
    const bool isImage = name.rfind(kImagePrefix, 0) == 0;
    if (!isManifest && !isLayout && !isImage)
      continue;
    std::vector<std::uint8_t> bytes;
    const auto limit = isManifest ? kMaxManifestBytes : isLayout ? kMaxLayoutBytes : kMaxImageBytes;
    if (!ReadCurrentEntry(zip, bytes, limit)) {
      if (error)
        *error = "Layout package entry exceeds the supported size limit.";
      return false;
    }
    if (isImage) {
      totalImages += bytes.size();
      if (totalImages > kMaxTotalImageBytes) {
        if (error)
          *error = "Layout package image resources exceed the supported size limit.";
        return false;
      }
    }
    entries[name] = std::move(bytes);
  }
  auto manifestIt = entries.find(kManifestPath);
  if (manifestIt == entries.end()) {
    if (error)
      *error = "Layout package is missing manifest.json.";
    return false;
  }
  nlohmann::json manifest;
  try {
    manifest = nlohmann::json::parse(manifestIt->second.begin(), manifestIt->second.end());
  } catch (const std::exception &ex) {
    if (error)
      *error = ex.what();
    return false;
  }
  if (manifest.value("format", "") != kPackageFormat) {
    if (error)
      *error = "Unsupported layout package format.";
    return false;
  }
  if (manifest.value("packageVersion", 0) != kPackageVersion) {
    if (error)
      *error = "Unsupported layout package version.";
    return false;
  }
  const std::string entryPoint = manifest.value("entryPoint", "");
  if (!IsSafeArchivePath(entryPoint)) {
    if (error)
      *error = "Layout package declares an invalid entry point.";
    return false;
  }
  auto layoutIt = entries.find(entryPoint);
  if (layoutIt == entries.end()) {
    if (error)
      *error = "Layout package is missing its layout entry point.";
    return false;
  }
  nlohmann::json parsed;
  try {
    parsed = nlohmann::json::parse(layoutIt->second.begin(), layoutIt->second.end());
  } catch (const std::exception &ex) {
    if (error)
      *error = ex.what();
    return false;
  }
  std::vector<LayoutDefinition> layouts;
  LayoutTemplateImportReport report;
  std::string parseError;
  if (!FromTemplateDocument(parsed, layouts, &report, &parseError) || layouts.size() != 1) {
    if (error)
      *error = parseError.empty() ? "Layout package must contain exactly one layout." : parseError;
    return false;
  }
  result.layout = layouts.front();
  result.sourceFormat = LayoutTemplateSourceFormat::PortablePackage;
  result.warnings = report.warnings;
  return MaterializeImages(result.layout, entries, error);
}

// Imports a legacy standalone JSON layout template for compatibility.
bool LayoutTemplatePackageService::ImportLegacyJson(
    const std::string &sourcePath, LayoutTemplateImportResult &result,
    std::string *error) {
  std::ifstream in(PathUtils::PathFromUtf8(sourcePath), std::ios::binary);
  if (!in.is_open()) {
    if (error)
      *error = "Could not open template file.";
    return false;
  }
  const std::string content((std::istreambuf_iterator<char>(in)),
                            std::istreambuf_iterator<char>());
  nlohmann::json parsed;
  try {
    parsed = nlohmann::json::parse(content);
  } catch (const std::exception &ex) {
    if (error)
      *error = ex.what();
    return false;
  }
  std::vector<LayoutDefinition> layouts;
  LayoutTemplateImportReport report;
  std::string parseError;
  if (!FromTemplateDocument(parsed, layouts, &report, &parseError) || layouts.empty()) {
    if (error)
      *error = parseError.empty() ? "Template does not contain any layouts." : parseError;
    return false;
  }
  result.layout = layouts.front();
  const fs::path templateDir = PathUtils::PathFromUtf8(sourcePath).parent_path();
  for (auto &image : result.layout.imageViews) {
    image.imagePath = ResolveLegacyImagePath(image.imagePath, templateDir);
    std::vector<std::uint8_t> ignoredBytes;
    if (!image.imagePath.empty() && !ReadFileBytes(image.imagePath, ignoredBytes))
      result.warnings.push_back("Legacy JSON image could not be resolved: " + image.imagePath);
  }
  result.sourceFormat = LayoutTemplateSourceFormat::LegacyJson;
  result.warnings = report.warnings;
  return true;
}

} // namespace layouts
