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

#include "mvr_import_package.h"
#include "mvr_scene_node_reader.h"

#include <functional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace mvr {

// Reports read progress without exposing importer or GUI callback types.
using MvrReadProgressCallback =
    std::function<void(std::string stage, int completed, int total)>;
enum class MvrReadLogLevel { Debug, Info, Warning, Error };
using MvrReadLogCallback =
    std::function<void(MvrReadLogLevel, const std::string &)>;

// Supplies resource and model enrichment without changing XML traversal.
struct MvrReadEnvironment {
  MvrSceneResourceServices resources;
  MvrSceneReadServices::ModelServices model;
};

// Carries neutral parser state needed by optional application post-read work.
struct MvrReadContext {
  std::vector<SceneReadGdtfConflict> gdtfConflicts;
  std::vector<std::pair<std::string, std::string>> manualCategoryUpdates;
  std::unordered_map<std::string, std::string> authoredLayerNameByUuid;
  std::unordered_map<std::string, std::string> layerUuidByNodeUuid;
  std::unordered_map<std::string, std::vector<std::string>>
      directChildUuidsByLayerUuid;
};

// Parses one already-acquired package through the production read model.
bool ReadAcquiredMvrPackage(const ImportPackage &package,
                            MvrImportResult &result,
                            const MvrImportOptions &options = {},
                            const MvrReadEnvironment *environment = nullptr,
                            MvrReadContext *context = nullptr,
                            MvrReadProgressCallback progress = {},
                            MvrReadLogCallback log = {});

} // namespace mvr
