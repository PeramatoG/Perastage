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
#include "symbolcache.h"
#include "layouts/LayoutCollection.h"
#include "viewer2dpanel.h"
#include <filesystem>
#include <memory>
#include <string>

constexpr double kMmToPt = 72.0 / 25.4;

// Options describing the paper size and orientation for the PDF export. A3
// portrait is used by default but callers can override the values to support
// additional formats and orientations later on.
struct Viewer2DPrintOptions {
  double pageWidthPt = 297.0 * kMmToPt;  // A3 portrait width
  double pageHeightPt = 420.0 * kMmToPt; // A3 portrait height
  double marginPt = 36.0;                // Half an inch margin for readability
  bool landscape = false;
  bool compressStreams = true;
  int floatPrecision = 3;
  bool useSimplifiedFootprints = true;
  bool printIncludeGrid = true;
};

struct Viewer2DExportResult {
  bool success = false;
  std::string message;
};

struct LayoutViewExportData {
  CommandBuffer buffer;
  Viewer2DViewState viewState;
  layouts::Layout2DViewFrame frame;
  std::shared_ptr<const SymbolDefinitionSnapshot> symbolSnapshot;
};

// Writes the captured 2D drawing commands to a vector PDF that mirrors the
// current viewport state. Returns structured information so callers can surface
// meaningful errors to the user.
Viewer2DExportResult ExportViewer2DToPdf(
    const CommandBuffer &buffer, const Viewer2DViewState &viewState,
    const Viewer2DPrintOptions &options,
    const std::filesystem::path &outputPath,
    std::shared_ptr<const SymbolDefinitionSnapshot> symbolSnapshot = nullptr);

// Writes multiple 2D views into a single vector PDF layout.
Viewer2DExportResult ExportLayoutToPdf(
    const std::vector<LayoutViewExportData> &views,
    const Viewer2DPrintOptions &options,
    const std::filesystem::path &outputPath);
