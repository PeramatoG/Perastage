#pragma once

#include <filesystem>
#include <string>

namespace TrussAssetIngestion {

struct Request {
  std::filesystem::path activeDictionaryPath;
  std::filesystem::path defaultDictionaryPath;
  std::filesystem::path sourcePath;
};

struct Result {
  bool success = false;
  std::filesystem::path finalPath;
  std::string serializedPath;
  std::string sha256;
  bool reusedExisting = false;
  std::string error;
};

Result Ingest(const Request &request);

} // namespace TrussAssetIngestion
