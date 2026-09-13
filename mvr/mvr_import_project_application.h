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

#include <cstddef>

class ConfigManager;
struct MvrImportResult;

namespace mvr {

struct ProjectApplicationResult {
  size_t migratedFixtureLabelOverrides = 0;
  size_t fixtureLabelOverrideCollisions = 0;
};

// Owns active-project replacement and Perastage-specific post-import migration.
class MvrImportProjectApplication {
public:
  explicit MvrImportProjectApplication(ConfigManager &config);

  ProjectApplicationResult Apply(const MvrImportResult &importResult) const;

private:
  ConfigManager &config_;
};

} // namespace mvr
