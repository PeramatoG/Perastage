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

#include "mvrscene.h"

#include <string>
#include <unordered_map>
#include <vector>

enum class MvrImportMode {
  ReplaceProject,
  ParseOnly,
};

enum class MvrImportSourceKind {
  ExternalImport,
  ProjectRestore,
  MergeImport,
};

struct MvrImportOptions {
  bool promptConflicts = true;
  bool applyDictionary = true;
  bool preserveMvrGdtfReferences = true;
  bool allowDummyFallback = true;
  MvrImportSourceKind sourceKind = MvrImportSourceKind::ExternalImport;
};

struct MvrImportDiagnostic {
  std::string code;
  std::string message;
};

struct MvrImportResult {
  MvrScene scene;
  std::unordered_map<std::string, std::string> fixtureUuidRemap;
  std::vector<MvrImportDiagnostic> diagnostics;
};
