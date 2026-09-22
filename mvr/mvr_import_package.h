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

#include "mvr_import_types.h"
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

class wxInputStream;

namespace mvr {

// Owns one automatically removed temporary MVR extraction directory.
class ImportWorkspace {
public:
  ImportWorkspace();
  ImportWorkspace(ImportWorkspace &&) noexcept = default;
  ImportWorkspace &operator=(ImportWorkspace &&) noexcept = default;
  ImportWorkspace(const ImportWorkspace &) = delete;
  ImportWorkspace &operator=(const ImportWorkspace &) = delete;

  bool IsValid() const;
  const std::filesystem::path &Path() const;
  std::shared_ptr<void> TransferToSceneLease();

private:
  struct State;
  std::shared_ptr<State> state_;
};

// Owns an extracted MVR package and keeps its temporary workspace alive.
struct ImportPackage {
  ImportWorkspace workspace;
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

// Safely extracts owned MVR bytes without exposing wxWidgets stream types.
std::optional<ImportPackage>
AcquireImportPackage(const std::vector<std::uint8_t> &bytes,
                     std::vector<MvrImportDiagnostic> &diagnostics);

} // namespace mvr
