#include "LayoutDefaultsLoader.h"

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

std::string ToUtf8String(const fs::path &path) {
  const std::u8string utf8 = path.u8string();
  return std::string(utf8.begin(), utf8.end());
}

bool IsJsonTemplateFile(const fs::path &filePath) {
  if (!filePath.has_extension())
    return false;
  std::string ext = filePath.extension().string();
  std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });
  return ext == ".json";
}

std::string StripResourcesPrefix(const std::string &relativePath) {
  constexpr const char *kPrefixForward = "resources/";
  constexpr const char *kPrefixBackward = "resources\\";
  if (relativePath.rfind(kPrefixForward, 0) == 0)
    return relativePath.substr(std::char_traits<char>::length(kPrefixForward));
  if (relativePath.rfind(kPrefixBackward, 0) == 0)
    return relativePath.substr(std::char_traits<char>::length(kPrefixBackward));
  return relativePath;
}

std::string ResolveImagePath(const std::string &rawPath,
                             const fs::path &templateDir) {
  if (rawPath.empty())
    return rawPath;

  fs::path parsedPath = fs::u8path(rawPath);
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
      const fs::path strippedCandidate = resourceRoot / fs::u8path(strippedPath);
      if (std::string resolved = asAbsoluteIfExists(strippedCandidate);
          !resolved.empty()) {
        return resolved;
      }
    }
  }

  return rawPath;
}

void ResolveLayoutImagePaths(std::vector<LayoutDefinition> &loadedLayouts,
                             const fs::path &templateDir) {
  for (auto &layout : loadedLayouts) {
    for (auto &image : layout.imageViews)
      image.imagePath = ResolveImagePath(image.imagePath, templateDir);
  }
}

} // namespace

LayoutDefaultsLoadResult LoadLayoutDefaultsFromLibrary(
    const std::string &librarySubdir) {
  LayoutDefaultsLoadResult result;

  const std::string defaultsDirUtf8 =
      ProjectUtils::GetDefaultLibraryPath(librarySubdir);
  if (defaultsDirUtf8.empty())
    return result;

  std::error_code ec;
  const fs::path defaultsDir = fs::u8path(defaultsDirUtf8);
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
    if (!IsJsonTemplateFile(entry.path()))
      continue;
    templateFiles.push_back(entry.path());
  }
  std::sort(templateFiles.begin(), templateFiles.end());

  for (const fs::path &templateFile : templateFiles) {
    ++result.filesScanned;

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
