#include "dictionary_bundle.h"

#include "dictionary_json_contract.h"
#include "json.hpp"
#include "projectutils.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <unordered_map>
#include <utility>
#include <vector>

#include <wx/wfstream.h>
#include <wx/zipstrm.h>

namespace fs = std::filesystem;

namespace DictionaryBundle {
namespace {

constexpr const char *kBundleKind = "perastage.dictionary_bundle";
constexpr int kFormatVersion = 1;

struct AssetPayload {
  fs::path sourcePath;
  std::string archivePath;
  std::string checksum;
  uintmax_t size = 0;
};

uint64_t HashBytesFnv1a64(const std::vector<unsigned char> &bytes) {
  uint64_t hash = 1469598103934665603ull;
  for (unsigned char value : bytes) {
    hash ^= static_cast<uint64_t>(value);
    hash *= 1099511628211ull;
  }
  return hash;
}

std::string ToLowerCopy(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return value;
}

std::string BuildChecksumLabel(const std::vector<unsigned char> &bytes) {
  std::ostringstream oss;
  oss << "fnv1a64:" << std::hex << std::setfill('0') << std::setw(16)
      << HashBytesFnv1a64(bytes);
  return oss.str();
}

bool ReadFileBytes(const fs::path &path, std::vector<unsigned char> &outBytes) {
  outBytes.clear();
  std::ifstream in(path, std::ios::binary);
  if (!in.is_open())
    return false;

  in.seekg(0, std::ios::end);
  const std::streamoff length = in.tellg();
  if (length < 0)
    return false;
  in.seekg(0, std::ios::beg);

  outBytes.resize(static_cast<size_t>(length));
  if (length == 0)
    return true;

  in.read(reinterpret_cast<char *>(outBytes.data()), length);
  return in.good() || in.gcount() == length;
}

std::string TypeToString(const Type type) {
  switch (type) {
  case Type::Fixtures:
    return "fixtures";
  case Type::Trusses:
    return "trusses";
  }
  return "unknown";
}

bool IsZipByHeader(const fs::path &path) {
  std::ifstream input(path, std::ios::binary);
  if (!input.is_open())
    return false;
  unsigned char sig[2] = {};
  input.read(reinterpret_cast<char *>(sig), sizeof(sig));
  return input.gcount() == static_cast<std::streamsize>(sizeof(sig)) &&
         sig[0] == 'P' && sig[1] == 'K';
}

bool ReadZipEntryBytes(wxZipInputStream &zip, std::vector<unsigned char> &outBytes) {
  outBytes.clear();
  unsigned char chunk[4096];
  for (;;) {
    zip.Read(chunk, sizeof(chunk));
    const size_t readBytes = zip.LastRead();
    if (readBytes == 0)
      break;
    outBytes.insert(outBytes.end(), chunk, chunk + readBytes);
  }
  return true;
}

bool WriteZipEntryFromBytes(wxZipOutputStream &zip, const std::string &entryName,
                            const std::vector<unsigned char> &bytes) {
  auto *entry = new wxZipEntry(entryName);
  entry->SetMethod(wxZIP_METHOD_DEFLATE);
  zip.PutNextEntry(entry);
  if (!bytes.empty())
    zip.Write(bytes.data(), bytes.size());
  zip.CloseEntry();
  return true;
}

bool WriteZipEntryFromFile(wxZipOutputStream &zip, const std::string &entryName,
                           const fs::path &sourcePath) {
  std::vector<unsigned char> bytes;
  if (!ReadFileBytes(sourcePath, bytes))
    return false;
  return WriteZipEntryFromBytes(zip, entryName, bytes);
}

std::string BaseNameForArchive(const fs::path &path, size_t fallbackIndex) {
  const std::string filename = path.filename().string();
  if (!filename.empty())
    return filename;
  return "asset_" + std::to_string(fallbackIndex) + ".bin";
}

fs::path MakeTempStagingDir() {
  const auto timestamp =
      std::chrono::system_clock::now().time_since_epoch().count();
  fs::path dir = fs::temp_directory_path() /
                 ("perastage-dictionary-bundle-" + std::to_string(timestamp));
  std::error_code ec;
  fs::create_directories(dir, ec);
  if (ec)
    return {};
  return dir;
}

bool SerializeJsonToBytes(const nlohmann::json &json, std::vector<unsigned char> &outBytes) {
  const std::string text = json.dump(2);
  outBytes.assign(text.begin(), text.end());
  return true;
}

nlohmann::json BuildFixturesEntriesJson(
    const std::unordered_map<std::string, GdtfDictionary::Entry> &dict,
    std::vector<AssetPayload> &assets, std::string &error) {
  nlohmann::json entries = nlohmann::json::object();
  std::unordered_map<std::string, std::string> sourceToArchivePath;
  size_t assetIndex = 0;

  std::vector<std::string> keys;
  keys.reserve(dict.size());
  for (const auto &[name, _] : dict)
    keys.push_back(name);
  std::sort(keys.begin(), keys.end());

  for (const auto &name : keys) {
    const auto &entry = dict.at(name);
    if (entry.path.empty() && entry.mode.empty() && entry.category.empty())
      continue;

    nlohmann::json outputEntry = nlohmann::json::object();
    if (!entry.path.empty()) {
      const fs::path sourcePath = fs::u8path(entry.path);
      if (!fs::exists(sourcePath)) {
        error = "Missing fixture asset for entry '" + name + "': " + entry.path;
        return {};
      }

      const std::string sourceKey = sourcePath.lexically_normal().string();
      auto archiveIt = sourceToArchivePath.find(sourceKey);
      if (archiveIt == sourceToArchivePath.end()) {
        const std::string archivePath = "assets/" + BaseNameForArchive(sourcePath, assetIndex++);
        std::vector<unsigned char> assetBytes;
        if (!ReadFileBytes(sourcePath, assetBytes)) {
          error = "Could not read fixture asset: " + entry.path;
          return {};
        }

        AssetPayload payload;
        payload.sourcePath = sourcePath;
        payload.archivePath = archivePath;
        payload.checksum = BuildChecksumLabel(assetBytes);
        payload.size = assetBytes.size();
        assets.push_back(payload);
        sourceToArchivePath[sourceKey] = archivePath;
        outputEntry["file"] = archivePath;
      } else {
        outputEntry["file"] = archiveIt->second;
      }
    }

    if (!entry.mode.empty())
      outputEntry["mode"] = entry.mode;
    if (!entry.category.empty())
      outputEntry["category"] = entry.category;

    if (!outputEntry.empty())
      entries[name] = std::move(outputEntry);
  }

  return entries;
}

nlohmann::json BuildTrussEntriesJson(
    const std::unordered_map<std::string, std::string> &dict,
    std::vector<AssetPayload> &assets, std::string &error) {
  nlohmann::json entries = nlohmann::json::object();
  std::unordered_map<std::string, std::string> sourceToArchivePath;
  size_t assetIndex = 0;

  std::vector<std::string> keys;
  keys.reserve(dict.size());
  for (const auto &[name, _] : dict)
    keys.push_back(name);
  std::sort(keys.begin(), keys.end());

  for (const auto &name : keys) {
    const std::string &source = dict.at(name);
    if (source.empty())
      continue;

    const fs::path sourcePath = fs::u8path(source);
    if (!fs::exists(sourcePath)) {
      error = "Missing truss asset for entry '" + name + "': " + source;
      return {};
    }

    const std::string sourceKey = sourcePath.lexically_normal().string();
    auto archiveIt = sourceToArchivePath.find(sourceKey);
    if (archiveIt == sourceToArchivePath.end()) {
      const std::string archivePath = "assets/" + BaseNameForArchive(sourcePath, assetIndex++);
      std::vector<unsigned char> assetBytes;
      if (!ReadFileBytes(sourcePath, assetBytes)) {
        error = "Could not read truss asset: " + source;
        return {};
      }

      AssetPayload payload;
      payload.sourcePath = sourcePath;
      payload.archivePath = archivePath;
      payload.checksum = BuildChecksumLabel(assetBytes);
      payload.size = assetBytes.size();
      assets.push_back(payload);
      sourceToArchivePath[sourceKey] = archivePath;
      entries[name] = nlohmann::json{{"file", archivePath}};
    } else {
      entries[name] = nlohmann::json{{"file", archiveIt->second}};
    }
  }

  return entries;
}

bool WriteBundle(const fs::path &outputZipPath, const std::string &dictionaryType,
                 const nlohmann::json &entries, const std::vector<AssetPayload> &assets,
                 std::string &error) {
  wxFileOutputStream output(outputZipPath.string());
  if (!output.IsOk()) {
    error = "Could not create ZIP file";
    return false;
  }

  wxZipOutputStream zip(output);
  nlohmann::json manifest;
  manifest["kind"] = kBundleKind;
  manifest["format_version"] = kFormatVersion;
  manifest["dictionary_type"] = dictionaryType;
  manifest["dictionary_file"] = "dictionary.json";
  manifest["entries"] = entries;

  nlohmann::json manifestAssets = nlohmann::json::array();
  for (const auto &asset : assets) {
    manifestAssets.push_back(nlohmann::json{{"path", asset.archivePath},
                                            {"checksum", asset.checksum},
                                            {"size", asset.size}});
  }
  manifest["assets"] = std::move(manifestAssets);

  std::vector<unsigned char> manifestBytes;
  SerializeJsonToBytes(manifest, manifestBytes);
  WriteZipEntryFromBytes(zip, "manifest.json", manifestBytes);

  const nlohmann::json dictionaryRoot =
      DictionaryJsonContract::MakeRoot(dictionaryType, entries);
  std::vector<unsigned char> dictionaryBytes;
  SerializeJsonToBytes(dictionaryRoot, dictionaryBytes);
  WriteZipEntryFromBytes(zip, "dictionary.json", dictionaryBytes);

  for (const auto &asset : assets) {
    if (!WriteZipEntryFromFile(zip, asset.archivePath, asset.sourcePath)) {
      error = "Failed to pack asset into ZIP: " + asset.sourcePath.string();
      return false;
    }
  }

  zip.Close();
  return true;
}

bool ExtractArchive(const fs::path &zipPath, const fs::path &destination,
                    std::string &error) {
  wxFileInputStream input(zipPath.string());
  if (!input.IsOk()) {
    error = "Could not open ZIP file";
    return false;
  }

  wxZipInputStream zip(input);
  std::unique_ptr<wxZipEntry> entry;
  while ((entry.reset(zip.GetNextEntry())), entry) {
    if (entry->IsDir())
      continue;

    const fs::path outPath = destination / fs::u8path(entry->GetName().ToStdString());
    std::error_code ec;
    fs::create_directories(outPath.parent_path(), ec);

    std::ofstream out(outPath, std::ios::binary);
    if (!out.is_open()) {
      error = "Could not write extracted file: " + outPath.string();
      return false;
    }

    char buffer[4096];
    for (;;) {
      zip.Read(buffer, sizeof(buffer));
      const size_t bytes = zip.LastRead();
      if (bytes == 0)
        break;
      out.write(buffer, static_cast<std::streamsize>(bytes));
    }
  }

  return true;
}

bool ReadJsonFile(const fs::path &path, nlohmann::json &outJson, std::string &error) {
  std::ifstream in(path);
  if (!in.is_open()) {
    error = "Could not open JSON file: " + path.string();
    return false;
  }

  try {
    in >> outJson;
  } catch (const std::exception &ex) {
    error = "Could not parse JSON file '" + path.string() + "': " + ex.what();
    return false;
  }
  return true;
}

bool ResolveEntryFileField(nlohmann::json &entryJson, std::string &pathOut) {
  pathOut.clear();
  if (entryJson.is_string()) {
    pathOut = entryJson.get<std::string>();
    return !pathOut.empty();
  }
  if (!entryJson.is_object())
    return false;
  if (entryJson.contains("file") && entryJson["file"].is_string()) {
    pathOut = entryJson["file"].get<std::string>();
    return !pathOut.empty();
  }
  if (entryJson.contains("path") && entryJson["path"].is_string()) {
    pathOut = entryJson["path"].get<std::string>();
    return !pathOut.empty();
  }
  return false;
}

void RewriteEntryFileField(nlohmann::json &entryJson, const std::string &rewrittenPath) {
  if (entryJson.is_string()) {
    entryJson = rewrittenPath;
    return;
  }
  if (!entryJson.is_object()) {
    entryJson = nlohmann::json{{"file", rewrittenPath}};
    return;
  }
  entryJson["file"] = rewrittenPath;
  if (entryJson.contains("path"))
    entryJson.erase("path");
}

} // namespace

bool ExportFixturesBundle(
    const std::unordered_map<std::string, GdtfDictionary::Entry> &dict,
    const std::string &outputZipPath, std::string &error) {
  std::vector<AssetPayload> assets;
  const nlohmann::json entries = BuildFixturesEntriesJson(dict, assets, error);
  if (!error.empty())
    return false;

  return WriteBundle(fs::u8path(outputZipPath), "fixtures", entries, assets, error);
}

bool ExportTrussesBundle(
    const std::unordered_map<std::string, std::string> &dict,
    const std::string &outputZipPath, std::string &error) {
  std::vector<AssetPayload> assets;
  const nlohmann::json entries = BuildTrussEntriesJson(dict, assets, error);
  if (!error.empty())
    return false;

  return WriteBundle(fs::u8path(outputZipPath), "trusses", entries, assets, error);
}

PreparedImport PrepareBundleImport(const std::string &importPath, Type expectedType) {
  PreparedImport result;

  const fs::path path = fs::u8path(importPath);
  if (!fs::exists(path))
    return result;

  const std::string lowerExt = ToLowerCopy(path.extension().string());
  if (lowerExt != ".zip" && !IsZipByHeader(path))
    return result;

  const fs::path stagingDir = MakeTempStagingDir();
  if (stagingDir.empty()) {
    result.errors.push_back("Could not create temporary staging directory for bundle import");
    return result;
  }

  std::string extractError;
  if (!ExtractArchive(path, stagingDir, extractError)) {
    result.errors.push_back(extractError);
    result.staging_directory = stagingDir;
    return result;
  }

  const fs::path manifestPath = stagingDir / "manifest.json";
  if (!fs::exists(manifestPath)) {
    std::error_code cleanupEc;
    fs::remove_all(stagingDir, cleanupEc);
    return result;
  }

  nlohmann::json manifest;
  std::string parseError;
  if (!ReadJsonFile(manifestPath, manifest, parseError)) {
    result.errors.push_back(parseError);
    result.staging_directory = stagingDir;
    result.is_bundle = true;
    return result;
  }

  if (!manifest.contains("kind") || !manifest["kind"].is_string() ||
      manifest["kind"].get<std::string>() != kBundleKind) {
    std::error_code cleanupEc;
    fs::remove_all(stagingDir, cleanupEc);
    return result;
  }

  result.is_bundle = true;
  result.staging_directory = stagingDir;

  if (!manifest.contains("dictionary_type") || !manifest["dictionary_type"].is_string()) {
    result.errors.push_back("Bundle manifest missing 'dictionary_type'");
    return result;
  }

  const std::string manifestType = manifest["dictionary_type"].get<std::string>();
  const std::string expectedTypeText = TypeToString(expectedType);
  if (manifestType != expectedTypeText) {
    result.errors.push_back("Bundle dictionary_type mismatch (expected '" +
                            expectedTypeText + "')");
    return result;
  }

  if (!manifest.contains("entries")) {
    result.errors.push_back("Bundle manifest missing 'entries'");
    return result;
  }

  std::unordered_map<std::string, std::string> importedAssetToLibraryPath;
  if (!manifest.contains("assets") || !manifest["assets"].is_array()) {
    result.errors.push_back("Bundle manifest missing 'assets' array");
    return result;
  }

  const fs::path libraryDir = fs::u8path(ProjectUtils::GetDefaultLibraryPath(expectedTypeText));
  std::error_code ec;
  fs::create_directories(libraryDir, ec);

  for (const auto &assetJson : manifest["assets"]) {
    if (!assetJson.is_object()) {
      result.errors.push_back("Bundle manifest has invalid asset entry");
      continue;
    }
    if (!assetJson.contains("path") || !assetJson["path"].is_string() ||
        !assetJson.contains("checksum") || !assetJson["checksum"].is_string()) {
      result.errors.push_back("Bundle asset is missing required fields");
      continue;
    }

    const std::string archivePath = assetJson["path"].get<std::string>();
    const std::string expectedChecksum = assetJson["checksum"].get<std::string>();
    const fs::path extractedPath = stagingDir / fs::u8path(archivePath);
    if (!fs::exists(extractedPath)) {
      result.errors.push_back("Bundle asset not found: " + archivePath);
      continue;
    }

    std::vector<unsigned char> bytes;
    if (!ReadFileBytes(extractedPath, bytes)) {
      result.errors.push_back("Could not read extracted bundle asset: " + archivePath);
      continue;
    }

    const std::string actualChecksum = BuildChecksumLabel(bytes);
    if (actualChecksum != expectedChecksum) {
      result.errors.push_back("Checksum mismatch for asset: " + archivePath);
      continue;
    }

    const fs::path destPath = libraryDir / extractedPath.filename();
    std::error_code copyEc;
    fs::copy_file(extractedPath, destPath, fs::copy_options::overwrite_existing, copyEc);
    if (copyEc) {
      result.errors.push_back("Could not copy bundle asset into library: " +
                              destPath.string());
      continue;
    }

    importedAssetToLibraryPath[archivePath] = destPath.string();
  }

  if (!result.errors.empty())
    return result;

  nlohmann::json rewrittenEntries = manifest["entries"];
  auto rewriteSingle = [&](nlohmann::json &entryJson, const std::string &entryName) {
    std::string fileField;
    if (!ResolveEntryFileField(entryJson, fileField))
      return;

    const auto importedIt = importedAssetToLibraryPath.find(fileField);
    if (importedIt == importedAssetToLibraryPath.end()) {
      result.errors.push_back("Entry '" + entryName + "' references unknown bundle asset: " +
                              fileField);
      return;
    }
    RewriteEntryFileField(entryJson, importedIt->second);
  };

  if (rewrittenEntries.is_object()) {
    for (auto it = rewrittenEntries.begin(); it != rewrittenEntries.end(); ++it)
      rewriteSingle(it.value(), it.key());
  } else if (rewrittenEntries.is_array()) {
    for (size_t i = 0; i < rewrittenEntries.size(); ++i) {
      nlohmann::json &entryJson = rewrittenEntries[i];
      if (!entryJson.is_object() || !entryJson.contains("name") ||
          !entryJson["name"].is_string()) {
        continue;
      }
      rewriteSingle(entryJson, entryJson["name"].get<std::string>());
    }
  } else {
    result.errors.push_back("Bundle 'entries' must be an object or array");
    return result;
  }

  if (!result.errors.empty())
    return result;

  const nlohmann::json rewrittenRoot =
      DictionaryJsonContract::MakeRoot(expectedTypeText, rewrittenEntries);
  const fs::path rewrittenPath = stagingDir / "dictionary_rewritten.json";
  std::ofstream out(rewrittenPath);
  if (!out.is_open()) {
    result.errors.push_back("Could not write rewritten dictionary snapshot");
    return result;
  }
  out << rewrittenRoot.dump(2);
  result.rewritten_snapshot_path = rewrittenPath;

  return result;
}

void CleanupPreparedImport(const PreparedImport &preparedImport) {
  if (preparedImport.staging_directory.empty())
    return;
  std::error_code ec;
  fs::remove_all(preparedImport.staging_directory, ec);
}

} // namespace DictionaryBundle
