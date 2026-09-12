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

#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace mvr {

struct MvrImportedIdentity {
  std::string kind;
  std::string layerName;
  std::string objectName;
  std::string transform;
  std::string rawUuid;
  std::string legacyStableId;
};

struct MvrImportPostParseReferences {
  const std::unordered_set<std::string> &trussInfoUuids;
  const std::unordered_set<std::string> &consumedTrussInfoUuids;
  const std::unordered_set<std::string> &hoistInfoUuids;
  const std::unordered_set<std::string> &consumedHoistInfoUuids;
  const std::unordered_set<std::string> &projectFixtureMetadataUuids;
  const std::unordered_set<std::string> &consumedProjectFixtureMetadataUuids;
};

// Owns tolerant imported identity aliases and complete-scene reconciliation.
class MvrImportReferenceResolver {
public:
  using WarningSink = std::function<void(const std::string &)>;

  explicit MvrImportReferenceResolver(WarningSink warningSink = {});

  std::string ResolveStableUuid(const MvrImportedIdentity &identity);
  std::string ReferenceUuid(const MvrImportedIdentity &identity) const;
  void ImportPosition(const std::string &rawUuid, const std::string &name,
                      MvrScene &scene);
  std::string EnsurePosition(const std::string &positionId, MvrScene &scene);
  void RecordFixtureUuid(const std::string &rawUuid,
                         const std::string &resolvedUuid);
  std::unordered_map<std::string, std::string> &FixtureUuidRemap();
  const std::unordered_map<std::string, std::string> &FixtureUuidRemap() const;
  const std::unordered_map<std::string, std::string> &
  LegacyPositionRemap() const;
  void Reconcile(MvrImportResult &result,
                 const MvrImportPostParseReferences &references) const;

private:
  std::string StableSeed(const MvrImportedIdentity &identity) const;
  void Warn(const std::string &message) const;

  WarningSink warningSink_;
  std::unordered_set<std::string> usedStableUuids_;
  std::unordered_map<std::string, std::string> fixtureUuidRemap_;
  std::unordered_map<std::string, std::string> legacyPositionRemap_;
};

} // namespace mvr
