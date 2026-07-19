#include "LayoutDefaultsLoader.h"
#include "filesystem_path_utils.h"

#include "LayoutTemplatePackageService.h"
#include "LayoutTemplateSerializer.h"
#include "json.hpp"
#include "projectutils.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iterator>

namespace layouts {
namespace {
namespace fs = std::filesystem;

// Converts a filesystem path to UTF-8 text for layout services.
std::string ToUtf8String(const fs::path &path) {
  const std::u8string utf8 = path.u8string();
  return std::string(utf8.begin(), utf8.end());
}

// Reports whether a file extension is a supported default layout template.
bool IsSupportedTemplateFile(const fs::path &filePath) {
  if (!filePath.has_extension())
    return false;
  std::string ext = filePath.extension().string();
  std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });
  return ext == ".pslayout" || ext == ".json";
}

// Reports whether a default template is a legacy JSON file.
bool IsLegacyJsonTemplateFile(const fs::path &filePath) {
  std::string ext = filePath.extension().string();
  std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });
  return ext == ".json";
}

// Removes the app-resource prefix accepted by legacy JSON templates.
std::string StripResourcesPrefix(const std::string &relativePath) {
  constexpr const char *kPrefixForward = "resources/";
  constexpr const char *kPrefixBackward = "resources\\";
  if (relativePath.rfind(kPrefixForward, 0) == 0)
    return relativePath.substr(std::char_traits<char>::length(kPrefixForward));
  if (relativePath.rfind(kPrefixBackward, 0) == 0)
    return relativePath.substr(std::char_traits<char>::length(kPrefixBackward));
  return relativePath;
}

// Resolves a legacy JSON image path against supported template roots.
std::string ResolveImagePath(const std::string &rawPath,
                             const fs::path &templateDir) {
  if (rawPath.empty())
    return rawPath;

  fs::path parsedPath = PathUtils::PathFromUtf8(rawPath);
  std::error_code ec;
  if (parsedPath.is_absolute()) {
    fs::path absolutePath = fs::absolute(parsedPath, ec);
    if (!ec)
      return ToUtf8String(absolutePath);
    return rawPath;
  }

  auto asAbsoluteIfExists = [](const fs::path &candidate) -> std::string {
    std::error_code candidateEc;
    if (!fs::exists(candidate, candidateEc) || candidateEc)
      return {};
    fs::path absolutePath = fs::absolute(candidate, candidateEc);
    if (candidateEc)
      return {};
    return ToUtf8String(absolutePath);
  };

  const fs::path fromTemplate = templateDir / parsedPath;
  if (std::string resolved = asAbsoluteIfExists(fromTemplate); !resolved.empty())
    return resolved;

  const fs::path resourceRoot = ProjectUtils::GetResourceRoot();
  if (!resourceRoot.empty()) {
    const fs::path fromResourceRoot = resourceRoot / parsedPath;
    if (std::string resolved = asAbsoluteIfExists(fromResourceRoot);
        !resolved.empty()) {
      return resolved;
    }

    const std::string strippedPath = StripResourcesPrefix(rawPath);
    if (strippedPath != rawPath) {
      const fs::path strippedCandidate = resourceRoot / PathUtils::PathFromUtf8(strippedPath);
      if (std::string resolved = asAbsoluteIfExists(strippedCandidate);
          !resolved.empty()) {
        return resolved;
      }
    }
  }

  return rawPath;
}

// Resolves all legacy JSON image paths in loaded default layouts.
void ResolveLayoutImagePaths(std::vector<LayoutDefinition> &loadedLayouts,
                             const fs::path &templateDir) {
  for (auto &layout : loadedLayouts) {
    for (auto &image : layout.imageViews)
      image.imagePath = ResolveImagePath(image.imagePath, templateDir);
  }
}

} // namespace

// Loads default layout templates from the configured library directory.
LayoutDefaultsLoadResult LoadLayoutDefaultsFromLibrary(
    const std::string &librarySubdir) {
  LayoutDefaultsLoadResult result;

  const std::string defaultsDirUtf8 =
      ProjectUtils::GetDefaultLibraryPath(librarySubdir);
  if (defaultsDirUtf8.empty())
    return result;

  std::error_code ec;
  const fs::path defaultsDir = PathUtils::PathFromUtf8(defaultsDirUtf8);
  if (!fs::exists(defaultsDir, ec) || ec || !fs::is_directory(defaultsDir, ec) ||
      ec) {
    return result;
  }

  std::vector<fs::path> templateFiles;
  for (const auto &entry : fs::directory_iterator(defaultsDir, ec)) {
    if (ec)
      break;
    if (!entry.is_regular_file())
      continue;
    if (!IsSupportedTemplateFile(entry.path()))
      continue;
    templateFiles.push_back(entry.path());
  }
  std::sort(templateFiles.begin(), templateFiles.end());

  for (const fs::path &templateFile : templateFiles) {
    ++result.filesScanned;

    if (!IsLegacyJsonTemplateFile(templateFile)) {
      LayoutTemplateImportResult importResult;
      std::string importError;
      if (!LayoutTemplatePackageService::ImportPortablePackage(
              ToUtf8String(templateFile), importResult, &importError)) {
        continue;
      }
      result.filesImported++;
      result.layouts.push_back(std::move(importResult.layout));
      continue;
    }

    std::ifstream in(templateFile, std::ios::binary);
    if (!in.is_open())
      continue;

    const std::string fileContent((std::istreambuf_iterator<char>(in)),
                                  std::istreambuf_iterator<char>());
    nlohmann::json parsed;
    try {
      parsed = nlohmann::json::parse(fileContent);
    } catch (...) {
      continue;
    }

    std::vector<LayoutDefinition> loadedLayouts;
    std::string parseError;
    if (!FromTemplateDocument(parsed, loadedLayouts, &parseError) ||
        loadedLayouts.empty()) {
      continue;
    }

    ResolveLayoutImagePaths(loadedLayouts, templateFile.parent_path());
    result.filesImported++;
    result.layouts.insert(result.layouts.end(),
                          std::make_move_iterator(loadedLayouts.begin()),
                          std::make_move_iterator(loadedLayouts.end()));
  }

  return result;
}

} // namespace layouts
