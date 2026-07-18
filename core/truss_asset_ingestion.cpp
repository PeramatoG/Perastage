#include "runtime_storage.h"
#include "truss_asset_ingestion.h"

#include "active_dictionary_storage.h"
#include "file_import_utils.h"
#include "filesystem_path_utils.h"
#include "gdtf_metadata_summary.h"
#include "truss.h"
#include "truss_gdtf_builder.h"

#include <algorithm>
#include <cctype>
#include <filesystem>

namespace fs = std::filesystem;

namespace TrussAssetIngestion {
namespace {

// Returns a lowercase extension for supported source comparisons.
std::string LowerExt(const fs::path &path) {
  std::string ext = path.extension().string();
  std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });
  return ext;
}

// Returns true when the path is already stored in the active owned asset directory.
bool IsAlreadyOwned(const fs::path &path,
                    const ActiveDictionaryStorage::AssetStorageLayout &layout) {
  std::error_code ec;
  return fs::exists(path, ec) && !ec && fs::exists(layout.ownedAssetDirectory, ec) &&
         !ec && fs::equivalent(path.parent_path(), layout.ownedAssetDirectory, ec) &&
         !ec;
}

// Creates an intermediate GDTF in staging for non-GDTF truss source formats.
bool ConvertToStagedGdtf(const fs::path &sourcePath, const fs::path &stagingPath,
                         std::string &error) {
  const std::string ext = LowerExt(sourcePath);
  if (ext == ".gtruss")
    return ConvertLegacyGtrussToGdtf(sourcePath, stagingPath, &error);
  if (ext == ".glb" || ext == ".3ds") {
    Truss truss;
    truss.modelFile = PathUtils::PathToUtf8(sourcePath);
    truss.model = sourcePath.stem().string();
    return BuildTrussGdtfFromInstance(truss, stagingPath, &error);
  }
  error = "Unsupported truss format";
  return false;
}

// Verifies that the GDTF can be opened as a metadata-bearing GDTF archive.
bool ValidateGdtf(const fs::path &path, std::string &error) {
  GdtfMetadataSummary summary;
  if (LoadGdtfMetadataSummary(PathUtils::PathToUtf8(path), summary))
    return true;
  error = "Generated truss GDTF could not be validated";
  return false;
}

} // namespace

// Ingests a selected truss source into the active dictionary-owned asset storage.
Result Ingest(const Request &request) {
  Result result;
  if (request.sourcePath.empty() || !fs::exists(request.sourcePath)) {
    result.error = "Truss source file does not exist";
    return result;
  }

  const std::string ext = LowerExt(request.sourcePath);
  if (ext != ".gdtf" && ext != ".gtruss" && ext != ".3ds" && ext != ".glb") {
    result.error = "Unsupported truss format";
    return result;
  }

  const auto layout = ActiveDictionaryStorage::BuildLayout(
      ActiveDictionaryStorage::DictionaryKind::Trusses, request.activeDictionaryPath,
      request.defaultDictionaryPath);
  if (ext == ".gdtf" && IsAlreadyOwned(request.sourcePath, layout)) {
    std::string validationError;
    if (!ValidateGdtf(request.sourcePath, validationError)) {
      result.error = validationError;
      return result;
    }
    result.success = true;
    result.finalPath = request.sourcePath;
    result.serializedPath = ActiveDictionaryStorage::MakeSerializedReference(
        layout, request.sourcePath);
    if (const auto hash = FileImportUtils::ComputeFileSha256(request.sourcePath))
      result.sha256 = *hash;
    result.reusedExisting = true;
    return result;
  }

  fs::path sourceForCopy = request.sourcePath;
  fs::path stagingDir;
  std::error_code ec;
  if (ext != ".gdtf") {
    stagingDir = runtime_storage::GetPerastageOperationRoot() /
                 ("truss-ingest-" + request.sourcePath.stem().string());
    fs::remove_all(stagingDir, ec);
    fs::create_directories(stagingDir, ec);
    if (ec) {
      result.error = "Failed to create truss conversion staging directory";
      return result;
    }
    sourceForCopy = stagingDir / (request.sourcePath.stem().string() + ".gdtf");
    if (!ConvertToStagedGdtf(request.sourcePath, sourceForCopy, result.error)) {
      fs::remove_all(stagingDir, ec);
      return result;
    }
  }

  if (!ValidateGdtf(sourceForCopy, result.error)) {
    fs::remove_all(stagingDir, ec);
    return result;
  }

  const auto copyResult = ActiveDictionaryStorage::CopyAssetIntoDictionaryStorage(
      {ActiveDictionaryStorage::DictionaryKind::Trusses, request.activeDictionaryPath,
       request.defaultDictionaryPath, sourceForCopy, {},
       FileImportUtils::ConflictPolicy::Rename});
  fs::remove_all(stagingDir, ec);
  if (!copyResult.success) {
    result.error = "Failed to copy truss GDTF into dictionary-owned storage";
    return result;
  }

  result.success = true;
  result.finalPath = copyResult.finalPath;
  result.serializedPath = copyResult.serializedPath;
  result.sha256 = copyResult.finalSha256;
  result.reusedExisting = copyResult.reusedExisting;
  return result;
}

} // namespace TrussAssetIngestion
