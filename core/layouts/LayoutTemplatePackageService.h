/*
 * This file is part of Perastage.
 * Copyright (C) 2025 Luisma Peramato
 *
 * Perastage is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#pragma once

#include "LayoutCollection.h"

#include <cstdint>
#include <string>
#include <vector>

namespace layouts {

enum class LayoutTemplateSourceFormat { PortablePackage, LegacyJson };

struct LayoutTemplateImportResult {
  LayoutDefinition layout;
  LayoutTemplateSourceFormat sourceFormat = LayoutTemplateSourceFormat::LegacyJson;
  std::vector<std::string> warnings;
};

class LayoutTemplatePackageService {
public:
  static bool ExportPackage(const LayoutDefinition &layout,
                            const std::string &destinationPath,
                            std::string *error);
  static bool ImportFile(const std::string &sourcePath,
                         LayoutTemplateImportResult &result,
                         std::string *error);
  static bool ImportPortablePackage(const std::string &sourcePath,
                                    LayoutTemplateImportResult &result,
                                    std::string *error);
  static bool ImportLegacyJson(const std::string &sourcePath,
                               LayoutTemplateImportResult &result,
                               std::string *error);
};

} // namespace layouts
