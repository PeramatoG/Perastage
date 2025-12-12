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

#include "canvas2d.h"
#include "viewer2dpanel.h"
#include <filesystem>
#include <string>

// Options describing the paper size and orientation for the PDF export. A3
// portrait is used by default but callers can override the values to support
// additional formats and orientations later on.
struct PlanPrintOptions {
  double pageWidthPt = 842.0;  // 297 mm in PostScript points
  double pageHeightPt = 1191.0; // 420 mm in PostScript points
  double marginPt = 36.0;       // Half an inch margin for readability
  bool landscape = false;
  bool compressStreams = true;
};

struct PlanExportResult {
  bool success = false;
  std::string message;
};

// Writes the captured 2D drawing commands to a vector PDF that mirrors the
// current viewport state. Returns structured information so callers can surface
// meaningful errors to the user.
PlanExportResult ExportPlanToPdf(const CommandBuffer &buffer,
                                 const Viewer2DViewState &viewState,
                                 const PlanPrintOptions &options,
                                 const std::filesystem::path &outputPath);

