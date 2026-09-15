/*
 * This file is part of Perastage.
 * Copyright (C) 2026 Luisma Peramato
 *
 * Perastage is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace mvr_export_archive {

struct Resource {
  std::filesystem::path sourcePath;
  std::string archivePath;
};

struct Request {
  std::filesystem::path destinationPath;
  std::string sceneXml;
  std::vector<Resource> resources;
};

struct Result {
  bool success{false};
  std::string operation;
  std::string archivePath;
  std::string sourcePath;
  std::string reason;
};

// Writes prepared XML and resources to an MVR ZIP package.
Result Write(const Request &request);

} // namespace mvr_export_archive
