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

#include "mvrimporter.h"
#include "runtime_storage.h"

#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

class wxInputStream;

namespace mvr {

// Owns an extracted MVR package and keeps its temporary workspace alive.
struct ImportPackage {
  runtime_storage::TemporaryWorkspace workspace;
  std::filesystem::path rootPath;
  std::filesystem::path sceneXmlPath;
  std::unordered_map<std::string, std::string> pathRemap;
};

// Normalizes a packaged resource path for importer lookup and remapping.
std::string NormalizeImportArchivePath(const std::string &archivePath);

// Safely extracts an MVR stream and locates its root scene description.
std::optional<ImportPackage>
AcquireImportPackage(wxInputStream &input,
                     std::vector<MvrImportDiagnostic> &diagnostics);

} // namespace mvr
