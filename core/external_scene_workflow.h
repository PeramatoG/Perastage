#pragma once

#include "command/command_contract.h"

#include <filesystem>
#include <string>
#include <vector>

namespace perastage::external_scene {

struct Request {
  std::filesystem::path inputPath;
  std::filesystem::path outputPath;
  std::vector<std::string> commands;
  bool overwrite = false;
};

struct Result {
  bool success = false;
  bool sceneChanged = false;
  bool outputPublished = false;
  std::vector<command::Result> commandResults;
  std::vector<std::string> diagnostics;
};

// Runs an isolated MVR read, semantic Command sequence, and canonical publish.
Result Execute(const Request &request);

} // namespace perastage::external_scene
