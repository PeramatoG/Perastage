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
#include "LayoutImageResourceRegistry.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <cctype>
#include <iomanip>
#include <iterator>
#include <sstream>

namespace layouts {
namespace {
namespace fs = std::filesystem;

// Converts a filesystem path to a UTF-8 string for stable project metadata.
std::string Utf8StringFromPath(const fs::path &path) {
  const auto utf8 = path.u8string();
  return std::string(utf8.begin(), utf8.end());
}

// Reads the complete image file into memory so projects do not depend on the source file later.
bool ReadFileBytes(const std::string &path, std::vector<std::uint8_t> &out) {
  std::ifstream in(fs::u8path(path), std::ios::binary);
  if (!in.is_open())
    return false;
  out.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
  return in.good() || in.eof();
}

// Returns a lowercase extension while preserving common image format hints.
std::string NormalizedExtension(const std::string &path) {
  std::string ext = fs::u8path(path).extension().string();
  std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });
  if (ext.empty())
    return ".bin";
  return ext;
}

// Produces a deterministic resource key from the image payload and extension.
std::string BuildArchivePath(const std::vector<std::uint8_t> &bytes,
                             const std::string &sourcePath) {
  std::uint64_t hash = 1469598103934665603ULL;
  for (const auto byte : bytes) {
    hash ^= byte;
    hash *= 1099511628211ULL;
  }
  std::ostringstream name;
  name << "resources/layout_images/image_" << std::hex << std::setw(16)
       << std::setfill('0') << hash << NormalizedExtension(sourcePath);
  return name.str();
}

// Registers one image reference in a usage map when it has packaged metadata.
void CountImageUse(const LayoutImageDefinition &image,
                   std::unordered_map<std::string, int> &usage) {
  if (!image.projectResourcePath.empty())
    ++usage[image.projectResourcePath];
}

} // namespace

// Returns the process-wide registry used by layout services and project packaging.
LayoutImageResourceRegistry &LayoutImageResourceRegistry::Get() {
  static LayoutImageResourceRegistry instance;
  return instance;
}

// Captures the image bytes and assigns project resource metadata to a layout image.
bool LayoutImageResourceRegistry::AttachResource(LayoutImageDefinition &image) {
  if (image.imagePath.empty())
    return false;

  std::vector<std::uint8_t> bytes;
  if (!ReadFileBytes(image.imagePath, bytes)) {
    return !image.projectResourcePath.empty() &&
           HasResourceBytes(image.projectResourcePath);
  }

  if (image.originalImagePath.empty())
    image.originalImagePath = image.imagePath;
  const std::string archivePath = image.projectResourcePath.empty()
                                      ? BuildArchivePath(bytes, image.imagePath)
                                      : image.projectResourcePath;
  image.projectResourcePath = archivePath;

  auto &entry = resources[archivePath];
  entry.archivePath = archivePath;
  entry.originalPath = image.originalImagePath.empty() ? image.imagePath
                                                       : image.originalImagePath;
  entry.bytes = std::move(bytes);
  return true;
}

// Rebuilds usage counters from the current layout collection and forgets unused byte payloads.
void LayoutImageResourceRegistry::SynchronizeWithLayouts(
    const LayoutCollection &layouts) {
  std::unordered_map<std::string, int> usage;
  for (const auto &layout : layouts.Items()) {
    for (const auto &image : layout.imageViews)
      CountImageUse(image, usage);
  }

  for (auto it = resources.begin(); it != resources.end();) {
    const auto useIt = usage.find(it->first);
    if (useIt == usage.end()) {
      it = resources.erase(it);
      continue;
    }
    it->second.useCount = useIt->second;
    ++it;
  }

  for (const auto &[archivePath, count] : usage) {
    auto &entry = resources[archivePath];
    entry.archivePath = archivePath;
    entry.useCount = count;
  }
}

// Clears all tracked image resources and usage counters.
void LayoutImageResourceRegistry::Clear() { resources.clear(); }

// Returns only resources that are still used and have bytes available for packaging.
std::vector<LayoutImageResourceRegistry::ResourceEntry>
LayoutImageResourceRegistry::UsedResources() const {
  std::vector<ResourceEntry> used;
  for (const auto &[archivePath, entry] : resources) {
    if (entry.useCount > 0 && !entry.bytes.empty())
      used.push_back(entry);
  }
  std::sort(used.begin(), used.end(), [](const auto &lhs, const auto &rhs) {
    return lhs.archivePath < rhs.archivePath;
  });
  return used;
}

// Returns how many layout image elements currently reference a packaged image.
int LayoutImageResourceRegistry::UsageCount(const std::string &archivePath) const {
  auto it = resources.find(archivePath);
  return it == resources.end() ? 0 : it->second.useCount;
}

// Reports whether the registry has an in-memory payload for a packaged image.
bool LayoutImageResourceRegistry::HasResourceBytes(
    const std::string &archivePath) const {
  auto it = resources.find(archivePath);
  return it != resources.end() && !it->second.bytes.empty();
}

} // namespace layouts
