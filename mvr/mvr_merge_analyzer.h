/*
 * This file is part of Perastage.
 * Copyright (C) 2025 Luisma Peramato
 *
 * Perastage is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Perastage is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Perastage. If not, see <https://www.gnu.org/licenses/>.
 */
#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "mvrscene.h"

namespace mvr {

struct MvrSceneMergeResult {
  std::size_t fixturesAdded = 0;
  std::size_t trussesAdded = 0;
  std::size_t supportsAdded = 0;
  std::size_t sceneObjectsAdded = 0;
  std::size_t groupObjectsAdded = 0;
  std::size_t layersAdded = 0;
  std::size_t uuidCollisionsResolved = 0;
  std::unordered_map<std::string, std::string> fixtureUuidRemap;
};

enum class MvrMergeUuidCollisionBehavior {
  GenerateStableUuid,
  ReplaceExisting,
  SkipIncoming,
};

struct MvrMergeOptions {
  MvrMergeUuidCollisionBehavior uuidCollisionBehavior =
      MvrMergeUuidCollisionBehavior::GenerateStableUuid;
};

struct MvrMergeAnalysis {
  std::unordered_map<std::string, std::string> uuidMap;
  std::unordered_map<std::string, std::string> fixtureUuidRemap;
  std::unordered_set<std::string> skippedIncomingUuids;
  std::size_t uuidCollisionsDetected = 0;
  std::size_t uuidCollisionsResolved = 0;
};

// Analyzes imported scene UUIDs and prepares collision-safe reference remaps.
MvrMergeAnalysis
AnalyzeImportedSceneMerge(const MvrScene &target, const MvrScene &imported,
                          const MvrMergeOptions &options = MvrMergeOptions{});

// Reports whether an imported object UUID should be skipped during merge apply.
bool ShouldSkipImportedUuid(const std::string &uuid,
                            const MvrMergeAnalysis &analysis);

// Resolves an imported UUID reference using the prepared merge analysis.
std::string RemapImportedUuidReference(const std::string &uuid,
                                       const MvrMergeAnalysis &analysis);

} // namespace mvr
