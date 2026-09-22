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

#include <functional>
#include <string>

namespace mvr {

// Reports read progress without exposing importer or GUI callback types.
using MvrReadProgressCallback =
    std::function<void(std::string stage, int completed, int total)>;

// Parses one already-acquired package through the production read model.
bool ReadAcquiredMvrPackage(const ImportPackage &package,
                            MvrImportResult &result,
                            const MvrImportOptions &options = {},
                            MvrReadProgressCallback progress = {});

} // namespace mvr
