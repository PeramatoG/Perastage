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

#include "LayoutCollection.h"
#include "json.hpp"

#include <string>
#include <vector>

namespace layouts {

constexpr int kLayoutTemplateSchemaVersion = 1;

struct LayoutTemplateImportReport {
  struct ElementStats {
    int imported = 0;
    int skipped = 0;
  };

  struct LayoutStats {
    int imported = 0;
    int skipped = 0;
  } layouts;

  ElementStats view2dViews;
  ElementStats legendViews;
  ElementStats eventTables;
  ElementStats textViews;
  ElementStats imageViews;

  std::vector<std::string> warnings;
  std::vector<std::string> errors;
};

nlohmann::json ToJson(const LayoutDefinition &layout);
bool FromJson(const nlohmann::json &value, LayoutDefinition &layout,
              std::string *error);

nlohmann::json ToTemplateDocument(const std::vector<LayoutDefinition> &layouts);
bool FromTemplateDocument(const nlohmann::json &value,
                          std::vector<LayoutDefinition> &layouts,
                          std::string *error);
bool FromTemplateDocument(const nlohmann::json &value,
                          std::vector<LayoutDefinition> &layouts,
                          LayoutTemplateImportReport *report,
                          std::string *error);

} // namespace layouts
