/*
 * This file is part of Perastage.
 * Copyright (C) 2026 Luisma Peramato
 *
 * Perastage is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "mvr_import_project_application.h"

#include "configmanager.h"
#include "fixture_label_overrides.h"
#include "logger.h"
#include "mvr_import_types.h"

#include <sstream>

namespace mvr {

// Creates an application boundary for the supplied active project.
MvrImportProjectApplication::MvrImportProjectApplication(ConfigManager &config)
    : config_(config) {}

// Replaces the active project and migrates fixture-label keys from the result.
ProjectApplicationResult
MvrImportProjectApplication::Apply(const MvrImportResult &importResult) const {
  config_.Reset();
  config_.GetScene() = importResult.scene;

  ProjectApplicationResult result;
  result.migratedFixtureLabelOverrides =
      viewer2d::RemapFixtureLabelOverrideKeys(
          config_, importResult.fixtureUuidRemap,
          &result.fixtureLabelOverrideCollisions);
  if (!importResult.fixtureUuidRemap.empty()) {
    std::ostringstream message;
    message << "MVR import fixture label override migration: remapped "
            << result.migratedFixtureLabelOverrides
            << " fixture override entries from "
            << importResult.fixtureUuidRemap.size() << " fixture UUID changes";
    if (result.fixtureLabelOverrideCollisions > 0)
      message << " (" << result.fixtureLabelOverrideCollisions
              << " collisions skipped)";
    Logger::Instance().Log(Logger::Level::Info, message.str());
  }
  return result;
}

} // namespace mvr
