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

#include "viewer2dpdfexporter.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <cstdlib>
#include <system_error>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <vector>


#include "logger.h"
#include "layout_pdf_symbol_usage.h"
#include "layout_pdf_legend_renderer.h"
#include "layout_pdf_event_table_renderer.h"
#include "layout_pdf_page_mapping.h"
#include "layout_pdf_text_renderer.h"
#include "pdf_draw_commands.h"
#include "pdf_font_metrics.h"
#include "pdf_fixture_symbol_source.h"
#include "pdf_objects.h"
#include "pdf_label_fonts.h"
#include "symbols/fixture_symbol_availability.h"
#include "viewer2dcommandrenderer.h"

namespace {
using namespace layout_pdf_internal;

constexpr double kPdfPointsPerPixel = 72.0 / 96.0;
constexpr double kPdfStrokeWidthScreenMatchFactor = 0.75;
constexpr double kSymbolBBoxPaddingPt = 0.25;

// Returns whether label-order tracing diagnostics are enabled through the environment.
static bool ShouldTraceLabelOrder() {
  static const bool enabled = std::getenv("PERASTAGE_TRACE_LABELS") != nullptr;
  return enabled;
}

using viewer2d::pdf::ShouldLoadLegendSvgFromKey;
void InflateBBox(double &minX, double &minY, double &maxX, double &maxY,
                 double padding) {
  if (padding <= 0.0)
    return;
  minX -= padding;
  minY -= padding;
  maxX += padding;
  maxY += padding;
}

int SymbolViewRank(SymbolViewKind kind) {
  switch (kind) {
  case SymbolViewKind::Top:
    return 0;
  case SymbolViewKind::Bottom:
    return 1;
  case SymbolViewKind::Front:
    return 2;
  case SymbolViewKind::Left:
    return 3;
  case SymbolViewKind::Right:
    return 4;
  case SymbolViewKind::Back:
  default:
    return 5;
  }
}

const SymbolDefinition *FindSymbolDefinition(
    const SymbolDefinitionSnapshot *symbols, const std::string &modelKey) {
  if (!symbols || modelKey.empty())
    return nullptr;
  const SymbolDefinition *best = nullptr;
  int bestRank = std::numeric_limits<int>::max();
  for (const auto &entry : *symbols) {
    if (entry.second.key.modelKey != modelKey)
      continue;
    int rank = SymbolViewRank(entry.second.key.viewKind);
    if (!best || rank < bestRank) {
      best = &entry.second;
      bestRank = rank;
    }
  }
  return best;
}

const SymbolDefinition *FindSymbolDefinitionPreferred(
    const SymbolDefinitionSnapshot *symbols, const std::string &modelKey,
    SymbolViewKind preferred) {
  if (!symbols || modelKey.empty())
    return nullptr;
  for (const auto &entry : *symbols) {
    if (entry.second.key.modelKey == modelKey &&
        entry.second.key.viewKind == preferred) {
      return &entry.second;
    }
  }
  return FindSymbolDefinition(symbols, modelKey);
}

const SymbolDefinition *FindSymbolDefinitionExact(
    const SymbolDefinitionSnapshot *symbols, const std::string &modelKey,
    SymbolViewKind view) {
  if (!symbols || modelKey.empty())
    return nullptr;
  for (const auto &entry : *symbols) {
    if (entry.second.key.modelKey == modelKey &&
        entry.second.key.viewKind == view) {
      return &entry.second;
    }
  }
  return nullptr;
}

// Groups vector inks while preserving source and transform barriers.
std::string RenderCommandsToStream(
    const std::vector<CanvasCommand> &commands,
    const std::vector<CommandMetadata> &metadata,
    const std::vector<std::string> &sources, const Mapping &mapping,
    const FloatFormatter &formatter, const RenderOptions &options) {
  Transform current{};
  std::vector<Transform> stack;
  std::ostringstream content;
  GraphicsStateCache stateCache;

  std::vector<size_t> group;
  std::string currentSource;

  auto flushGroup = [&]() {
    if (group.empty())
      return;

    // Use dedicated buffers for strokes and fills so layering is explicit and
    // future exporters can reorder or post-process the layers independently.
    std::ostringstream strokeLayer;
    std::ostringstream fillLayer;

    // Render all strokes first. They will be visually pushed underneath by the
    // subsequent fill layer, mirroring how the real-time viewer relies on
    // depth testing to hide internal wireframe segments.
    for (size_t idx : group) {
      if (!metadata[idx].hasStroke)
        continue;
      EmitCommandStroke(strokeLayer, stateCache, formatter, mapping, current,
                        commands[idx], options);
    }

    // Render fills afterwards so they sit on top of any wireframe lines from
    // the same piece, matching the 2D viewer's occlusion behavior.
    for (size_t idx : group) {
      if (!metadata[idx].hasFill)
        continue;
      EmitCommandFill(fillLayer, stateCache, formatter, mapping, current,
                      commands[idx]);
    }

    content << strokeLayer.str() << fillLayer.str();

    group.clear();
  };

  auto handleBarrier = [&](const auto &cmd, size_t idx) {
    using T = std::decay_t<decltype(cmd)>;
    if constexpr (std::is_same_v<T, SaveCommand>) {
      stack.push_back(current);
    } else if constexpr (std::is_same_v<T, RestoreCommand>) {
      if (!stack.empty()) {
        current = stack.back();
        stack.pop_back();
      }
    } else if constexpr (std::is_same_v<T, TransformCommand>) {
      current.scale = cmd.transform.scale;
      current.offsetX = cmd.transform.offsetX;
      current.offsetY = cmd.transform.offsetY;
    } else if constexpr (std::is_same_v<T, TextCommand>) {
      if (!options.includeText)
        return;
      auto pos = MapPointWithTransform(cmd.x, cmd.y, current, mapping);
      if (ShouldTraceLabelOrder()) {
        std::ostringstream trace;
        trace << "[label-replay] index=" << idx;
        if (idx < sources.size())
          trace << " source=" << sources[idx];
        trace << " text=\"" << cmd.text << "\" x=" << pos.x << " y="
              << pos.y << " size=" << cmd.style.fontSize << " vAlign=";
        switch (cmd.style.vAlign) {
        case CanvasTextStyle::VerticalAlign::Baseline:
          trace << "Baseline";
          break;
        case CanvasTextStyle::VerticalAlign::Middle:
          trace << "Middle";
          break;
        case CanvasTextStyle::VerticalAlign::Top:
          trace << "Top";
          break;
        case CanvasTextStyle::VerticalAlign::Bottom:
          trace << "Bottom";
          break;
        }
        Logger::Instance().Log(trace.str());
      }
      AppendText(content, formatter, pos, cmd, cmd.style, mapping.scale,
                 options.fonts);
    } else if constexpr (std::is_same_v<T, PlaceSymbolCommand>) {
      if (!options.symbolKeyNames)
        return;
      auto nameIt = options.symbolKeyNames->find(cmd.key);
      if (nameIt == options.symbolKeyNames->end())
        return;
      Transform2D local = TransformFromCanvas(cmd.transform);
      AppendSymbolInstance(content, formatter, mapping, local, nameIt->second);
    } else if constexpr (std::is_same_v<T, SymbolInstanceCommand>) {
      if (!options.symbolIdNames)
        return;
      auto nameIt = options.symbolIdNames->find(cmd.symbolId);
      if (nameIt == options.symbolIdNames->end())
        return;
      AppendSymbolInstance(content, formatter, mapping, cmd.transform,
                           nameIt->second);
    } else {
      // Symbol control commands are handled at a higher level but must preserve
      // ordering relative to drawing commands.
    }
  };

  for (size_t i = 0; i < commands.size(); ++i) {
    const auto &cmd = commands[i];
    if (const auto *text = std::get_if<TextCommand>(&cmd);
        text && options.includeText && text->style.fontFamily.starts_with("perastage-sans")) {
      flushGroup();
      i = AppendComposedTextRun(content, formatter, mapping, current, commands, sources, i, options.fonts) - 1;
      continue;
    }

    bool isBarrier = std::visit(
        [&](auto &&c) {
          using T = std::decay_t<decltype(c)>;
          return std::is_same_v<T, SaveCommand> || std::is_same_v<T, RestoreCommand> ||
                 std::is_same_v<T, TransformCommand> ||
                 std::is_same_v<T, BeginSymbolCommand> ||
                 std::is_same_v<T, EndSymbolCommand> ||
                 std::is_same_v<T, PlaceSymbolCommand> ||
                 std::is_same_v<T, SymbolInstanceCommand> ||
                 std::is_same_v<T, TextCommand>;
        },
        cmd);

    if (isBarrier) {
      flushGroup();
      std::visit([&](const auto &barrierCmd) { handleBarrier(barrierCmd, i); },
                 cmd);
      continue;
    }

    if (group.empty())
      currentSource = sources[i];

    if (sources[i] != currentSource) {
      flushGroup();
      currentSource = sources[i];
    }

    group.push_back(i);
  }

  flushGroup();

  return content.str();
}

std::string MakePdfName(const std::string &key) {
  std::string name = "X";
  for (char ch : key) {
    if (std::isalnum(static_cast<unsigned char>(ch)))
      name.push_back(ch);
    else
      name.push_back('_');
  }
  if (name.size() == 1)
    name += "Obj";
  return name;
}

std::string MakeSymbolKeyName(const std::string &key) {
  return "K" + MakePdfName(key);
}

std::string MakeSymbolIdName(uint32_t symbolId) {
  return "S" + std::to_string(symbolId);
}

} // namespace

Viewer2DExportResult ExportViewer2DToPdf(
    const CommandBuffer &buffer, const Viewer2DViewState &viewState,
    const Viewer2DPrintOptions &options,
    const std::filesystem::path &outputPath,
    std::shared_ptr<const SymbolDefinitionSnapshot> symbolSnapshot) {
  Viewer2DExportResult result{};

  // Nothing to write if the render pass did not produce commands.
  if (buffer.commands.empty()) {
    result.message = "Nothing to export";
    return result;
  }

  // Fail fast when the output location is not usable to avoid performing any
  // rendering work that cannot be saved.
  if (outputPath.empty() || outputPath.filename().empty()) {
    result.message = "No output file was provided for the PDF plan.";
    return result;
  }

  const auto parent = outputPath.parent_path();
  std::error_code pathEc;
  if (!parent.empty() && !std::filesystem::exists(parent, pathEc)) {
    result.message = pathEc ?
                      "Unable to verify the selected folder for the PDF plan." :
                      "The selected folder does not exist.";
    return result;
  }

  // Validate viewport dimensions before calculating scales to avoid divide by
  // zero and produce a clear explanation for the caller.
  if (viewState.viewportWidth <= 0 || viewState.viewportHeight <= 0) {
    result.message = "The 2D viewport is not ready for export.";
    return result;
  }

  if (!std::isfinite(viewState.zoom) || viewState.zoom <= 0.0f) {
    result.message = "Invalid zoom value provided for export.";
    return result;
  }

  (void)viewState.view; // Orientation reserved for future layout tweaks.

  double pageW = options.pageWidthPt;
  double pageH = options.pageHeightPt;
  double margin = options.marginPt;
  double drawW = pageW - margin * 2.0;
  double drawH = pageH - margin * 2.0;
  // Ensure the paper configuration leaves a drawable area.
  if (drawW <= 0.0 || drawH <= 0.0) {
    result.message = "The selected paper size and margins leave no space for drawing.";
    return result;
  }

  viewer2d::Viewer2DRenderMapping viewMapping;
  if (!viewer2d::BuildViewMapping(viewState, pageW, pageH, margin,
                                  viewMapping)) {
    result.message = "Viewport dimensions are invalid for export.";
    return result;
  }

  double scale = viewMapping.scale;
  double offsetX = viewMapping.offsetX;
  double offsetY = viewMapping.offsetY;
  double minX = viewMapping.minX;
  double minY = viewMapping.minY;

  FloatFormatter formatter(options.floatPrecision);
  // Maintain a consistent physical stroke size in PDF (points) and apply a
  // slight reduction so printed lines better match the perceived on-screen
  // weight.
  const double strokeScale =
      (kPdfPointsPerPixel * kPdfStrokeWidthScreenMatchFactor) / scale;

  struct CommandGroup {
    std::vector<CanvasCommand> commands;
    std::vector<CommandMetadata> metadata;
    std::vector<std::string> sources;
  };

  CommandGroup mainCommands;
  std::unordered_map<std::string, CommandGroup> symbolDefinitions;
  std::unordered_set<uint32_t> usedSymbolIds;
  std::unordered_set<std::string> usedSymbolKeys;
  std::string capturingKey;
  std::vector<CanvasCommand> captureBuffer;
  std::vector<CommandMetadata> captureMetadata;
  std::vector<std::string> captureSources;

  for (size_t i = 0; i < buffer.commands.size(); ++i) {
    const auto &cmd = buffer.commands[i];
    const auto &meta = buffer.metadata[i];
    const auto &source = buffer.sources[i];

    if (const auto *begin = std::get_if<BeginSymbolCommand>(&cmd)) {
      capturingKey = begin->key;
      captureBuffer.clear();
      captureMetadata.clear();
      captureSources.clear();
      continue;
    }
    if (const auto *end = std::get_if<EndSymbolCommand>(&cmd)) {
      if (!capturingKey.empty() && capturingKey == end->key &&
          !symbolDefinitions.count(capturingKey)) {
        symbolDefinitions.emplace(capturingKey,
                                  CommandGroup{captureBuffer, captureMetadata,
                                               captureSources});
      }
      capturingKey.clear();
      captureBuffer.clear();
      captureMetadata.clear();
      captureSources.clear();
      continue;
    }
    if (const auto *place = std::get_if<PlaceSymbolCommand>(&cmd)) {
      usedSymbolKeys.insert(place->key);
    }
    if (const auto *instance = std::get_if<SymbolInstanceCommand>(&cmd)) {
      usedSymbolIds.insert(instance->symbolId);
    }

    if (!capturingKey.empty()) {
      captureBuffer.push_back(cmd);
      captureMetadata.push_back(meta);
      captureSources.push_back(source);
      continue;
    }

    mainCommands.commands.push_back(cmd);
    mainCommands.metadata.push_back(meta);
    mainCommands.sources.push_back(source);
  }

  Mapping pageMapping{minX, minY, scale, offsetX, offsetY,
                      viewMapping.drawHeight, false};
  std::unordered_map<std::string, std::string> xObjectKeyNames;
  std::unordered_map<uint32_t, std::string> xObjectIdNames;
  std::unordered_map<std::string, size_t> xObjectKeyIds;
  std::unordered_map<uint32_t, size_t> xObjectIdIds;

  for (const auto &entry : symbolDefinitions) {
    if (usedSymbolKeys.count(entry.first) == 0)
      continue;
    xObjectKeyNames.emplace(entry.first, MakeSymbolKeyName(entry.first));
  }

  if (symbolSnapshot) {
    for (uint32_t symbolId : usedSymbolIds) {
      if (symbolSnapshot->count(symbolId) == 0)
        continue;
      xObjectIdNames.emplace(symbolId, MakeSymbolIdName(symbolId));
    }
  }

  LegacyFontPair legacyFonts;
  auto &regularFont = legacyFonts.regular;
  auto &boldFont = legacyFonts.bold;

  bool regularMetricsLoaded = LoadPdfFontMetrics(regularFont, false);
  bool boldMetricsLoaded = LoadPdfFontMetrics(boldFont, true);
  if (!boldMetricsLoaded && regularMetricsLoaded)
    boldFont.metrics = regularFont.metrics;

  PdfFontCatalog fontCatalog{&regularFont, &boldFont};
  LabelFonts labelFonts;
  std::vector<PdfObject> objects;
  if (!labelFonts.Prepare(objects, fontCatalog, HasComposedLabels(buffer))) {
    result.message = "Bundled label fonts are unavailable; PDF export stopped.";
    return result;
  }


  RenderOptions mainOptions{};
  mainOptions.includeText = true;
  mainOptions.symbolKeyNames = &xObjectKeyNames;
  mainOptions.symbolIdNames = &xObjectIdNames;
  mainOptions.fonts = &fontCatalog;
  mainOptions.strokeScale = strokeScale;
  std::string contentStr =
      RenderCommandsToStream(mainCommands.commands, mainCommands.metadata,
                             mainCommands.sources, pageMapping, formatter,
                             mainOptions);
  std::string compressedContent;
  bool useCompression = false;
  if (options.compressStreams) {
    std::string error;
    if (PdfDeflater::Compress(contentStr, compressedContent, error)) {
      useCompression = true;
    }
  }
  if (regularMetricsLoaded && AppendEmbeddedFontObjects(objects, regularFont)) {
    // Embedded font loaded successfully.
  } else {
    Logger::Instance().Log(
        "PDF export: falling back to Type1 Helvetica (embedded font not found)");
    AppendFallbackType1Font(objects, regularFont, "Helvetica");
  }

  if (boldMetricsLoaded && AppendEmbeddedFontObjects(objects, boldFont)) {
    // Embedded bold font loaded successfully.
  } else if (regularFont.objectId != 0) {
    boldFont.objectId = regularFont.objectId;
    boldFont.embedded = regularFont.embedded;
    boldFont.metrics = regularFont.metrics;
  } else {
    Logger::Instance().Log(
        "PDF export: falling back to Type1 Helvetica-Bold (embedded font not found)");
    AppendFallbackType1Font(objects, boldFont, "Helvetica-Bold");
  }

  Mapping symbolMapping{};
  symbolMapping.scale = 1.0;
  symbolMapping.flipY = false;

  auto appendSymbolObject = [&](const std::string &name,
                                const std::vector<CanvasCommand> &commands,
                                const std::vector<CommandMetadata> &metadata,
                                const std::vector<std::string> &sources,
                                const SymbolBounds &bounds) {
    RenderOptions symbolOptions{};
    symbolOptions.includeText = false;
    symbolOptions.strokeScale = strokeScale;
    std::string symbolContent =
        RenderCommandsToStream(commands, metadata, sources, symbolMapping,
                               formatter, symbolOptions);
    std::string compressedSymbol;
    bool compressed = false;
    if (options.compressStreams) {
      std::string error;
      compressed = PdfDeflater::Compress(symbolContent, compressedSymbol, error);
    }
    const std::string &symbolStream =
        compressed ? compressedSymbol : symbolContent;
    double minX = bounds.min.x * symbolMapping.scale;
    double minY = bounds.min.y * symbolMapping.scale;
    double maxX = bounds.max.x * symbolMapping.scale;
    double maxY = bounds.max.y * symbolMapping.scale;
    if (minX > maxX)
      std::swap(minX, maxX);
    if (minY > maxY)
      std::swap(minY, maxY);
    InflateBBox(minX, minY, maxX, maxY, kSymbolBBoxPaddingPt);
    std::ostringstream xobj;
    xobj << "<< /Type /XObject /Subtype /Form /BBox ["
         << formatter.Format(minX) << ' ' << formatter.Format(minY) << ' '
         << formatter.Format(maxX) << ' ' << formatter.Format(maxY)
         << "] /Resources << >> /Length " << symbolStream.size();
    if (compressed)
      xobj << " /Filter /FlateDecode";
    xobj << " >>\nstream\n" << symbolStream << "endstream";
    objects.push_back({xobj.str()});
    return objects.size();
  };

  for (const auto &entry : symbolDefinitions) {
    auto nameIt = xObjectKeyNames.find(entry.first);
    if (nameIt == xObjectKeyNames.end())
      continue;
    SymbolBounds bounds = ComputeSymbolBounds(entry.second.commands);
    xObjectKeyIds[entry.first] =
        appendSymbolObject(nameIt->second, entry.second.commands,
                           entry.second.metadata, entry.second.sources, bounds);
  }

  if (symbolSnapshot) {
    for (uint32_t symbolId : usedSymbolIds) {
      auto nameIt = xObjectIdNames.find(symbolId);
      if (nameIt == xObjectIdNames.end())
        continue;
      const auto &definition = symbolSnapshot->at(symbolId);
      xObjectIdIds[symbolId] =
          appendSymbolObject(nameIt->second, definition.localCommands.commands,
                             definition.localCommands.metadata,
                             definition.localCommands.sources,
                             definition.bounds);
    }
  }

  std::ostringstream contentObj;
  const std::string &streamData = useCompression ? compressedContent : contentStr;
  contentObj << "<< /Length " << streamData.size();
  if (useCompression)
    contentObj << " /Filter /FlateDecode";
  contentObj << " >>\nstream\n" << streamData << "endstream";
  objects.push_back({contentObj.str()});

  std::ostringstream resources;
  resources << "<< /Font << /F1 " << regularFont.objectId << " 0 R";
  labelFonts.Resources(resources);
  if (boldFont.objectId != 0 && boldFont.objectId != regularFont.objectId)
    resources << " /F2 " << boldFont.objectId << " 0 R";
  else if (boldFont.objectId == regularFont.objectId)
    resources << " /F2 " << regularFont.objectId << " 0 R";
  resources << " >>";
  if (!xObjectKeyIds.empty() || !xObjectIdIds.empty()) {
    resources << " /XObject << ";
    for (const auto &entry : xObjectKeyIds) {
      auto nameIt = xObjectKeyNames.find(entry.first);
      if (nameIt == xObjectKeyNames.end())
        continue;
      resources << '/' << nameIt->second << ' ' << entry.second << " 0 R ";
    }
    for (const auto &entry : xObjectIdIds) {
      auto nameIt = xObjectIdNames.find(entry.first);
      if (nameIt == xObjectIdNames.end())
        continue;
      resources << '/' << nameIt->second << ' ' << entry.second << " 0 R ";
    }
    resources << ">>";
  }
  resources << " >>";

  std::ostringstream pageObj;
  size_t contentIndex = objects.size();
  size_t pageIndex = contentIndex + 1;
  size_t pagesIndex = pageIndex + 1;
  size_t catalogIndex = pagesIndex + 1;

  pageObj << "<< /Type /Page /Parent " << pagesIndex << " 0 R /MediaBox [0 0 "
          << formatter.Format(pageW) << ' ' << formatter.Format(pageH)
          << "] /Contents " << contentIndex << " 0 R /Resources "
          << resources.str() << " >>";
  objects.push_back({pageObj.str()});
  objects.push_back({"<< /Type /Pages /Kids [" + std::to_string(pageIndex) +
                    " 0 R] /Count 1 >>"});
  objects.push_back({"<< /Type /Catalog /Pages " + std::to_string(pagesIndex) +
                    " 0 R >>"});

  try {
    std::ofstream file(outputPath, std::ios::binary);
    if (!file.is_open()) {
      result.message = "Unable to open the destination file for writing.";
      return result;
    }

    file << "%PDF-1.4\n";
    std::vector<long> offsets;
    offsets.reserve(objects.size());
    for (size_t i = 0; i < objects.size(); ++i) {
      offsets.push_back(static_cast<long>(file.tellp()));
      file << (i + 1) << " 0 obj\n" << objects[i].body << "\nendobj\n";
    }

    long xrefPos = static_cast<long>(file.tellp());
    file << "xref\n0 " << (objects.size() + 1)
         << "\n0000000000 65535 f \n";
    for (long off : offsets) {
      file << std::setw(10) << std::setfill('0') << off << " 00000 n \n";
    }
    file << "trailer\n<< /Size " << (objects.size() + 1)
         << " /Root " << catalogIndex
         << " 0 R >>\nstartxref\n" << xrefPos << "\n%%EOF";
    result.success = true;
  } catch (const std::exception &ex) {
    result.message = std::string("Failed to generate PDF content: ") + ex.what();
    return result;
  } catch (...) {
    result.message = "An unknown error occurred while generating the PDF plan.";
    return result;
  }

  return result;
}

Viewer2DExportResult ExportLayoutToPdf(
    const std::vector<LayoutViewExportData> &views,
    const std::vector<LayoutLegendExportData> &legends,
    const std::vector<LayoutEventTableExportData> &tables,
    const std::vector<LayoutTextExportData> &texts,
    const std::vector<LayoutImageExportData> &images,
    const Viewer2DPrintOptions &options,
    const std::filesystem::path &outputPath) {
  Viewer2DExportResult result{};

  if (views.empty() && legends.empty() && tables.empty() && texts.empty() &&
      images.empty()) {
    result.message = "The selected layout is empty.";
    return result;
  }

  if (outputPath.empty() || outputPath.filename().empty()) {
    result.message = "No output file was provided for the PDF layout.";
    return result;
  }

  const auto parent = outputPath.parent_path();
  std::error_code pathEc;
  if (!parent.empty() && !std::filesystem::exists(parent, pathEc)) {
    result.message =
        pathEc ? "Unable to verify the selected folder for the PDF layout." :
                 "The selected folder does not exist.";
    return result;
  }

  const double pageW = options.pageWidthPt;
  const double pageH = options.pageHeightPt;
  try {
    layouts::composition::UniformPageFit(options.layoutScaleX, options.layoutScaleY);
    layouts::composition::PageFitForSizes(pageW/options.layoutScaleX,
        pageH/options.layoutScaleY, pageW, pageH);
  } catch (const std::invalid_argument &error) {
    result.message = std::string("Invalid Layout page mapping: ") + error.what();
    return result;
  }


  struct CommandGroup {
    std::vector<CanvasCommand> commands;
    std::vector<CommandMetadata> metadata;
    std::vector<std::string> sources;
  };

  struct LayoutCommandGroup {
    CommandGroup commands;
    Mapping mapping;
    double frameX = 0.0;
    double frameY = 0.0;
    double frameW = 0.0;
    double frameH = 0.0;
    std::unordered_set<std::string> usedSymbolKeys;
    std::unordered_set<uint32_t> usedSymbolIds;
    std::shared_ptr<const SymbolDefinitionSnapshot> symbolSnapshot = nullptr;
    double strokeScale = 1.0;
    size_t viewIndex = 0;
  };

  std::unordered_map<std::string, CommandGroup> symbolDefinitions;
  std::vector<LayoutCommandGroup> layoutGroups;
  std::shared_ptr<const SymbolDefinitionSnapshot> symbolSnapshot = nullptr;

  auto captureCommands = [&](const CommandBuffer &buffer, CommandGroup &out,
                             std::unordered_set<std::string> &viewSymbolKeys,
                             std::unordered_set<uint32_t> &viewSymbolIds) {
    std::string capturingKey;
    std::vector<CanvasCommand> captureBuffer;
    std::vector<CommandMetadata> captureMetadata;
    std::vector<std::string> captureSources;

    for (size_t i = 0; i < buffer.commands.size(); ++i) {
      const auto &cmd = buffer.commands[i];
      const auto &meta = buffer.metadata[i];
      const auto &source = buffer.sources[i];

      if (const auto *begin = std::get_if<BeginSymbolCommand>(&cmd)) {
        capturingKey = begin->key;
        captureBuffer.clear();
        captureMetadata.clear();
        captureSources.clear();
        continue;
      }
      if (const auto *end = std::get_if<EndSymbolCommand>(&cmd)) {
        if (!capturingKey.empty() && capturingKey == end->key &&
            !symbolDefinitions.count(capturingKey)) {
          symbolDefinitions.emplace(capturingKey,
                                    CommandGroup{captureBuffer, captureMetadata,
                                                 captureSources});
        }
        capturingKey.clear();
        captureBuffer.clear();
        captureMetadata.clear();
        captureSources.clear();
        continue;
      }
      if (const auto *place = std::get_if<PlaceSymbolCommand>(&cmd)) {
        viewSymbolKeys.insert(place->key);
      }
      if (const auto *instance = std::get_if<SymbolInstanceCommand>(&cmd)) {
        viewSymbolIds.insert(instance->symbolId);
      }

      if (!capturingKey.empty()) {
        captureBuffer.push_back(cmd);
        captureMetadata.push_back(meta);
        captureSources.push_back(source);
        continue;
      }

      out.commands.push_back(cmd);
      out.metadata.push_back(meta);
      out.sources.push_back(source);
    }
  };

  for (size_t idx = 0; idx < views.size(); ++idx) {
    const auto &view = views[idx];
    if (view.buffer.commands.empty()) {
      result.message = "Unable to capture one or more layout views.";
      return result;
    }

    if (view.viewState.viewportWidth <= 0 ||
        view.viewState.viewportHeight <= 0) {
      result.message = "The 2D viewport is not ready for layout export.";
      return result;
    }

    if (!std::isfinite(view.viewState.zoom) || view.viewState.zoom <= 0.0f) {
      result.message = "Invalid zoom value provided for layout export.";
      return result;
    }

    if (view.frame.width <= 0 || view.frame.height <= 0) {
      result.message = "Layout frame dimensions are invalid for export.";
      return result;
    }

    viewer2d::Viewer2DRenderMapping viewMapping;
    if (!viewer2d::BuildViewMapping(view.viewState, view.frame.width,
                                    view.frame.height, 0.0, viewMapping)) {
      result.message = "Layout view dimensions are invalid for export.";
      return result;
    }

    const double frameOriginY =
        pageH - view.frame.y - static_cast<double>(view.frame.height);
    Mapping mapping{viewMapping.minX,
                    viewMapping.minY,
                    viewMapping.scale,
                    viewMapping.offsetX + view.frame.x,
                    viewMapping.offsetY + frameOriginY,
                    viewMapping.drawHeight,
                    false};

    CommandGroup mainCommands;
    std::unordered_set<std::string> viewSymbolKeys;
    std::unordered_set<uint32_t> viewSymbolIds;
    captureCommands(view.buffer, mainCommands, viewSymbolKeys, viewSymbolIds);
    // Maintain a consistent physical stroke size in PDF (points) and apply a
    // slight reduction so printed lines better match the perceived on-screen
    // weight.
    const double strokeScale =
        (kPdfPointsPerPixel * kPdfStrokeWidthScreenMatchFactor) /
        viewMapping.scale;
    layoutGroups.push_back({std::move(mainCommands),
                            mapping,
                            static_cast<double>(view.frame.x),
                            frameOriginY,
                            static_cast<double>(view.frame.width),
                            static_cast<double>(view.frame.height),
                            std::move(viewSymbolKeys),
                            std::move(viewSymbolIds),
                            view.symbolSnapshot,
                            strokeScale,
                            idx});

    if (!symbolSnapshot && view.symbolSnapshot)
      symbolSnapshot = view.symbolSnapshot;
  }

  if (!symbolSnapshot) {
    for (const auto &legend : legends) {
      if (legend.symbolSnapshot) {
        symbolSnapshot = legend.symbolSnapshot;
        break;
      }
    }
  }

  for (const auto &group : layoutGroups) {
    const SymbolDefinitionSnapshot *groupSymbols =
        group.symbolSnapshot ? group.symbolSnapshot.get() : symbolSnapshot.get();
    const auto usage = CountLayoutPdfSymbolUsage(
        views[group.viewIndex].buffer, groupSymbols);
    result.perastageSymbolInstances += usage.perastageSymbolInstances;
    result.fallbackSymbolInstances += usage.fallbackSymbolInstances;
  }

  if (layoutGroups.empty() && legends.empty() && tables.empty() &&
      texts.empty() && images.empty()) {
    result.message = "Nothing to export";
    return result;
  }

  FloatFormatter formatter(options.floatPrecision);
  std::unordered_map<std::string, size_t> xObjectNameIds;
  struct LegendSvgCacheKey {
    std::string gdtfPath;
    SymbolViewKind viewKind = SymbolViewKind::Top;

    bool operator==(const LegendSvgCacheKey &other) const {
      return gdtfPath == other.gdtfPath && viewKind == other.viewKind;
    }
  };
  struct LegendSvgCacheHasher {
    size_t operator()(const LegendSvgCacheKey &key) const {
      return std::hash<std::string>{}(key.gdtfPath) ^
             (static_cast<size_t>(key.viewKind) << 1);
    }
  };
  std::unordered_map<LegendSvgCacheKey, std::optional<PerastageSvgSymbolData>,
                     LegendSvgCacheHasher>
      legendSvgCache;
  auto findLegendSvg = [&](const std::string &symbolKey,
                           const std::string &resolvedGdtfPath,
                           SymbolViewKind viewKind)
      -> const PerastageSvgSymbolData * {
    const std::string &loadPath =
        viewer2d::pdf::ResolveLegendFixtureSymbolSource(symbolKey, resolvedGdtfPath);
    if (!ShouldLoadLegendSvgFromKey(loadPath))
      return nullptr;
    LegendSvgCacheKey cacheKey{loadPath, viewKind};
    auto it = legendSvgCache.find(cacheKey);
    if (it == legendSvgCache.end()) {
      std::optional<PerastageSvgSymbolData> loaded;
      std::string loadError;
      if (const auto data = symbol_cache::LoadUsableFixtureSymbol(
              loadPath, viewKind, &loadError))
        loaded = *data;
      it = legendSvgCache.emplace(std::move(cacheKey), std::move(loaded)).first;
    }
    return it->second ? &it->second.value() : nullptr;
  };
  std::ostringstream contentStream;
  auto makeLayoutKeyName = [&](size_t viewIndex, const std::string &key) {
    return "K" + MakePdfName("V" + std::to_string(viewIndex) + "_" + key);
  };
  auto makeLayoutIdName = [&](size_t viewIndex, uint32_t id) {
    return "S" + MakePdfName("V" + std::to_string(viewIndex) + "_" +
                             std::to_string(id));
  };

  std::vector<PdfObject> objects;
  LegacyFontPair legacyFonts;
  auto &regularFont = legacyFonts.regular;
  auto &boldFont = legacyFonts.bold;

  auto loadFont = [&](PdfFontDefinition &font, bool bold) {
    if (!LoadPdfFontMetrics(font, bold))
      return false;
    return AppendEmbeddedFontObjects(objects, font);
  };

  if (!loadFont(regularFont, false)) {
    Logger::Instance().Log(
        "PDF export: falling back to Type1 Helvetica (embedded font not found)");
    AppendFallbackType1Font(objects, regularFont, "Helvetica");
  }

  bool boldLoaded = loadFont(boldFont, true);
  if (!boldLoaded && regularFont.objectId != 0) {
    boldFont = regularFont;
    boldFont.key = "F2";
    boldFont.family = "sans-bold";
  } else if (!boldLoaded) {
    Logger::Instance().Log(
        "PDF export: falling back to Type1 Helvetica-Bold (embedded font not found)");
    AppendFallbackType1Font(objects, boldFont, "Helvetica-Bold");
  }

  PdfFontCatalog fontCatalog{&regularFont, &boldFont};
  LabelFonts labelFonts;
  if (!labelFonts.Prepare(objects, fontCatalog, !legends.empty() || !tables.empty() || std::any_of(views.begin(), views.end(), [](const auto &v) { return HasComposedLabels(v.buffer); }))) {
    result.message = "Bundled label fonts are unavailable; PDF export stopped.";
    return result;
  }


  auto appendSymbolObject = [&](const std::string &name,
                                const std::vector<CanvasCommand> &commands,
                                const std::vector<CommandMetadata> &metadata,
                                const std::vector<std::string> &sources,
                                double symbolScale,
                                double strokeScale,
                                const SymbolBounds &bounds) {
    Mapping symbolMapping{};
    symbolMapping.scale = symbolScale;
    symbolMapping.flipY = false;
    RenderOptions symbolOptions{};
    symbolOptions.includeText = false;
    symbolOptions.strokeScale = strokeScale;
    std::string symbolContent =
        RenderCommandsToStream(commands, metadata, sources, symbolMapping,
                               formatter, symbolOptions);
    std::string compressedSymbol;
    bool compressed = false;
    if (options.compressStreams) {
      std::string error;
      compressed = PdfDeflater::Compress(symbolContent, compressedSymbol, error);
    }
    const std::string &symbolStream =
        compressed ? compressedSymbol : symbolContent;
    double minX = bounds.min.x * symbolMapping.scale;
    double minY = bounds.min.y * symbolMapping.scale;
    double maxX = bounds.max.x * symbolMapping.scale;
    double maxY = bounds.max.y * symbolMapping.scale;
    if (minX > maxX)
      std::swap(minX, maxX);
    if (minY > maxY)
      std::swap(minY, maxY);
    InflateBBox(minX, minY, maxX, maxY, kSymbolBBoxPaddingPt);
    std::ostringstream xobj;
    xobj << "<< /Type /XObject /Subtype /Form /BBox ["
         << formatter.Format(minX) << ' ' << formatter.Format(minY) << ' '
         << formatter.Format(maxX) << ' ' << formatter.Format(maxY)
         << "] /Resources << >> /Length " << symbolStream.size();
    if (compressed)
      xobj << " /Filter /FlateDecode";
    xobj << " >>\nstream\n" << symbolStream << "endstream";
    objects.push_back({xobj.str()});
    return objects.size();
  };

  auto appendPerastageSvgSymbolObject = [&](const PerastageSvgSymbolData &svg,
                                            double symbolScale,
                                            double strokeScale,
                                            const std::array<double, 3> &fillRgb) {
    std::ostringstream symbolContent;
    symbolContent << formatter.Format(fillRgb[0]) << " "
                  << formatter.Format(fillRgb[1]) << " "
                  << formatter.Format(fillRgb[2]) << " rg\n";
    symbolContent << "0 0 0 RG " << formatter.Format(strokeScale) << " w\n";
    symbolContent << "q\n1 0 0 1 "
                  << formatter.Format(svg.offsetXmm * symbolScale) << " "
                  << formatter.Format(svg.offsetYmm * symbolScale) << " cm\n";

    auto appendPolygonPath = [&](const std::vector<PerastageSvgPoint> &points) {
      if (points.size() < 3)
        return;
      symbolContent << formatter.Format(points[0].x * symbolScale) << ' '
                    << formatter.Format((svg.viewBoxHeight - points[0].y) * symbolScale)
                    << " m\n";
      for (size_t i = 1; i < points.size(); ++i) {
        symbolContent << formatter.Format(points[i].x * symbolScale) << ' '
                      << formatter.Format((svg.viewBoxHeight - points[i].y) * symbolScale)
                      << " l\n";
      }
      symbolContent << "h\n";
    };

    for (const auto &polygon : svg.fills) {
      if (polygon.points.size() < 3)
        continue;
      appendPolygonPath(polygon.points);
      for (const auto &hole : polygon.holes)
        appendPolygonPath(hole);
      symbolContent << (polygon.holes.empty() ? "f\n" : "f*\n");
    }
    for (const auto &line : svg.strokes) {
      if (line.points.size() < 2)
        continue;
      symbolContent << formatter.Format(line.points[0].x * symbolScale) << ' '
                    << formatter.Format((svg.viewBoxHeight - line.points[0].y) * symbolScale)
                    << " m\n";
      for (size_t i = 1; i < line.points.size(); ++i) {
        symbolContent << formatter.Format(line.points[i].x * symbolScale) << ' '
                      << formatter.Format((svg.viewBoxHeight - line.points[i].y) * symbolScale)
                      << " l\n";
      }
      symbolContent << "S\n";
    }
    symbolContent << "Q\n";

    std::string symbolStream = symbolContent.str();
    std::string compressedSymbol;
    bool compressed = false;
    if (options.compressStreams) {
      std::string error;
      compressed = PdfDeflater::Compress(symbolStream, compressedSymbol, error);
    }
    const std::string &stream = compressed ? compressedSymbol : symbolStream;
    double minX = std::min(svg.offsetXmm * symbolScale,
                           (svg.offsetXmm + svg.viewBoxWidth) * symbolScale);
    double minY = std::min(svg.offsetYmm * symbolScale,
                           (svg.offsetYmm + svg.viewBoxHeight) * symbolScale);
    double maxX = std::max(svg.offsetXmm * symbolScale,
                           (svg.offsetXmm + svg.viewBoxWidth) * symbolScale);
    double maxY = std::max(svg.offsetYmm * symbolScale,
                           (svg.offsetYmm + svg.viewBoxHeight) * symbolScale);
    InflateBBox(minX, minY, maxX, maxY, kSymbolBBoxPaddingPt);
    std::ostringstream xobj;
    xobj << "<< /Type /XObject /Subtype /Form /BBox ["
         << formatter.Format(minX) << ' ' << formatter.Format(minY) << ' '
         << formatter.Format(maxX) << ' ' << formatter.Format(maxY)
         << "] /Resources << >> /Length " << stream.size();
    if (compressed)
      xobj << " /Filter /FlateDecode";
    xobj << " >>\nstream\n" << stream << "endstream";
    objects.push_back({xobj.str()});
    return objects.size();
  };

  auto appendImageObject = [&](const LayoutImageExportData &image,
                               const std::string &name) -> size_t {
    if (image.pixelWidth <= 0 || image.pixelHeight <= 0)
      return 0;
    if (image.pixels.size() !=
        static_cast<size_t>(image.pixelWidth) * static_cast<size_t>(image.pixelHeight) * 3u) {
      Logger::Instance().Log("PDF export: skipping invalid layout image '" +
                             name + "' due to pixel buffer mismatch");
      return 0;
    }
    std::string rawData(reinterpret_cast<const char *>(image.pixels.data()),
                        image.pixels.size());
    std::string encodedData;
    bool compressed = false;
    if (options.compressStreams) {
      std::string error;
      if (PdfDeflater::Compress(rawData, encodedData, error)) {
        compressed = true;
      }
    }
    const std::string &streamData = compressed ? encodedData : rawData;

    std::ostringstream imageObj;
    imageObj << "<< /Type /XObject /Subtype /Image /Width " << image.pixelWidth
             << " /Height " << image.pixelHeight
             << " /ColorSpace /DeviceRGB /BitsPerComponent 8";
    if (compressed)
      imageObj << " /Filter /FlateDecode";
    imageObj << " /Length " << streamData.size() << " >>\nstream\n"
             << streamData << "endstream";
    objects.push_back({imageObj.str()});
    return objects.size();
  };


  auto populateViewSymbolNames =
      [&](const LayoutCommandGroup &group,
          std::unordered_map<std::string, std::string> &viewKeyNames,
          std::unordered_map<uint32_t, std::string> &viewIdNames) {
        viewKeyNames.reserve(group.usedSymbolKeys.size());
        viewIdNames.reserve(group.usedSymbolIds.size());

        for (const auto &key : group.usedSymbolKeys) {
          viewKeyNames.emplace(key, makeLayoutKeyName(group.viewIndex, key));
        }
        for (const auto &id : group.usedSymbolIds) {
          viewIdNames.emplace(id, makeLayoutIdName(group.viewIndex, id));
        }
      };

  for (const auto &group : layoutGroups) {
    std::unordered_map<std::string, std::string> viewKeyNames;
    std::unordered_map<uint32_t, std::string> viewIdNames;
    populateViewSymbolNames(group, viewKeyNames, viewIdNames);

    for (const auto &entry : viewKeyNames) {
      auto defIt = symbolDefinitions.find(entry.first);
      if (defIt == symbolDefinitions.end())
        continue;
      SymbolBounds bounds = ComputeSymbolBounds(defIt->second.commands);
      xObjectNameIds[entry.second] =
          appendSymbolObject(entry.second, defIt->second.commands,
                             defIt->second.metadata, defIt->second.sources,
                             1.0, group.strokeScale, bounds);
    }

    const SymbolDefinitionSnapshot *groupSymbols =
        group.symbolSnapshot ? group.symbolSnapshot.get() : symbolSnapshot.get();
    if (groupSymbols) {
      for (const auto &entry : viewIdNames) {
        auto defIt = groupSymbols->find(entry.first);
        if (defIt == groupSymbols->end())
          continue;
        xObjectNameIds[entry.second] =
            appendSymbolObject(entry.second,
                               defIt->second.localCommands.commands,
                               defIt->second.localCommands.metadata,
                               defIt->second.localCommands.sources,
                               1.0, group.strokeScale,
                               defIt->second.bounds);
      }
    }
  }

  std::vector<std::string> imageXObjectNames(images.size());
  for (size_t idx = 0; idx < images.size(); ++idx) {
    const auto &image = images[idx];
    if (image.pixelWidth <= 0 || image.pixelHeight <= 0)
      continue;
    if (image.frame.width <= 0 || image.frame.height <= 0)
      continue;
    const std::string xObjectName = "I" + std::to_string(idx);
    size_t objectId = appendImageObject(image, xObjectName);
    if (objectId == 0)
      continue;
    xObjectNameIds[xObjectName] = objectId;
    imageXObjectNames[idx] = xObjectName;
  }

  struct LayoutRenderElement {
    enum class Type { View, Legend, EventTable, Text, Image };
    Type type = Type::View;
    size_t index = 0;
    int zIndex = 0;
    size_t order = 0;
  };

  std::vector<LayoutRenderElement> renderOrder;
  renderOrder.reserve(layoutGroups.size() + legends.size() + tables.size() +
                      texts.size() + images.size());
  size_t renderOrderIndex = 0;
  for (size_t idx = 0; idx < layoutGroups.size(); ++idx) {
    renderOrder.push_back(
        {LayoutRenderElement::Type::View, idx, views[idx].zIndex,
         renderOrderIndex++});
  }
  for (size_t idx = 0; idx < legends.size(); ++idx) {
    renderOrder.push_back(
        {LayoutRenderElement::Type::Legend, idx, legends[idx].zIndex,
         renderOrderIndex++});
  }
  for (size_t idx = 0; idx < tables.size(); ++idx) {
    renderOrder.push_back(
        {LayoutRenderElement::Type::EventTable, idx, tables[idx].zIndex,
         renderOrderIndex++});
  }
  for (size_t idx = 0; idx < texts.size(); ++idx) {
    renderOrder.push_back(
        {LayoutRenderElement::Type::Text, idx, texts[idx].zIndex,
         renderOrderIndex++});
  }
  for (size_t idx = 0; idx < images.size(); ++idx) {
    renderOrder.push_back(
        {LayoutRenderElement::Type::Image, idx, images[idx].zIndex,
         renderOrderIndex++});
  }

  std::stable_sort(renderOrder.begin(), renderOrder.end(),
                   [](const auto &lhs, const auto &rhs) {
                     if (lhs.zIndex != rhs.zIndex)
                       return lhs.zIndex < rhs.zIndex;
                     return lhs.order < rhs.order;
                   });

  auto renderViewGroup = [&](size_t idx) {
    const auto &group = layoutGroups[idx];
    ScopedDocumentPageMapping pageMapping(contentStream, pageH, options.layoutScaleX, options.layoutScaleY, formatter);
    std::unordered_map<std::string, std::string> viewKeyNames;
    std::unordered_map<uint32_t, std::string> viewIdNames;
    populateViewSymbolNames(group, viewKeyNames, viewIdNames);

    RenderOptions mainOptions{};
    mainOptions.includeText = true;
    mainOptions.symbolKeyNames = &viewKeyNames;
    mainOptions.symbolIdNames = &viewIdNames;
    mainOptions.fonts = &fontCatalog;
    mainOptions.strokeScale = group.strokeScale;
    contentStream << "q\n" << formatter.Format(group.frameX) << ' '
                  << formatter.Format(group.frameY) << ' '
                  << formatter.Format(group.frameW) << ' '
                  << formatter.Format(group.frameH) << " re W n\n";
    contentStream << "1 1 1 rg " << formatter.Format(group.frameX) << ' '
                  << formatter.Format(group.frameY) << ' '
                  << formatter.Format(group.frameW) << ' '
                  << formatter.Format(group.frameH) << " re f\n";
    contentStream << RenderCommandsToStream(group.commands.commands,
                                            group.commands.metadata,
                                            group.commands.sources,
                                            group.mapping, formatter,
                                            mainOptions);
    contentStream << "Q\n";
    if (views[idx].drawFrame) {
      contentStream << "q\n0 0 0 RG 0.5 w "
                    << formatter.Format(group.frameX) << ' '
                    << formatter.Format(group.frameY) << ' '
                    << formatter.Format(group.frameW) << ' '
                    << formatter.Format(group.frameH) << " re S\nQ\n";
    }
  };

  auto renderLegend = [&](size_t idx) {
    const auto &legend = legends[idx];
    const auto rendered = RenderLegendPdf(contentStream, legend,
        legend.symbolSnapshot ? legend.symbolSnapshot.get() : symbolSnapshot.get(),
        pageH, options.layoutScaleX, options.layoutScaleY, fontCatalog,
        formatter, findLegendSvg, [&](const CommandBuffer &commands, const Mapping &mapping, double strokeScale) {
          RenderOptions options;
          options.includeText = false;
          options.strokeScale = strokeScale;
          return RenderCommandsToStream(commands.commands, commands.metadata,
              commands.sources, mapping, formatter, options);
        });
    result.perastageSymbolInstances += rendered.symbols.perastageSymbolInstances;
    result.fallbackSymbolInstances += rendered.symbols.fallbackSymbolInstances;
    result.legendTextSubstitutions += rendered.textSubstitutions;
    for (const auto &diagnostic : rendered.diagnostics)
      if (result.legendTextDiagnostics.size() < 16) result.legendTextDiagnostics.push_back(diagnostic);
  };

  auto renderEventTable = [&](size_t idx) {
    const auto plan = RenderEventTablePdf(contentStream, tables[idx], pageH,
        options.layoutScaleX, options.layoutScaleY, fontCatalog, formatter);
    result.eventTableTextSubstitutions += plan.unsupportedGlyphs;
    for (const auto &message : plan.diagnostics)
      if (result.eventTableTextDiagnostics.size() < 16)
        result.eventTableTextDiagnostics.push_back(message);
  };

  auto renderText = [&](size_t idx) {
    RenderLayoutTextPdf(contentStream, texts[idx], pageH, options.layoutScaleX,
                        options.layoutScaleY, fontCatalog, formatter);
  };

  auto renderImage = [&](size_t idx) {
    if (idx >= images.size() || idx >= imageXObjectNames.size())
      return;
    const auto &image = images[idx];
    const std::string &xObjectName = imageXObjectNames[idx];
    if (xObjectName.empty())
      return;
    const double frameW = static_cast<double>(image.frame.width);
    const double frameH = static_cast<double>(image.frame.height);
    if (frameW <= 0.0 || frameH <= 0.0)
      return;
    ScopedDocumentPageMapping pageMapping(contentStream, pageH, options.layoutScaleX, options.layoutScaleY, formatter);
    const double frameX = static_cast<double>(image.frame.x);
    const double frameY =
        pageH - image.frame.y - static_cast<double>(image.frame.height);
    contentStream << "q\n"
                  << formatter.Format(frameW) << " 0 0 "
                  << formatter.Format(frameH) << ' '
                  << formatter.Format(frameX) << ' '
                  << formatter.Format(frameY) << " cm\n/"
                  << xObjectName << " Do\nQ\n";
  };

  for (const auto &entry : renderOrder) {
    ScopedDocumentPageClip pageClip(contentStream, pageW, pageH,
        options.layoutScaleX, options.layoutScaleY, formatter);
    if (entry.type == LayoutRenderElement::Type::View) {
      renderViewGroup(entry.index);
    } else if (entry.type == LayoutRenderElement::Type::Legend) {
      renderLegend(entry.index);
    } else if (entry.type == LayoutRenderElement::Type::EventTable) {
      renderEventTable(entry.index);
    } else if (entry.type == LayoutRenderElement::Type::Image) {
      renderImage(entry.index);
    } else {
      renderText(entry.index);
    }
  }

  std::string contentStr = contentStream.str();

  std::string compressedContent;
  bool useCompression = false;
  if (options.compressStreams) {
    std::string error;
    if (PdfDeflater::Compress(contentStr, compressedContent, error)) {
      useCompression = true;
    }
  }

  std::ostringstream contentObj;
  const std::string &streamData =
      useCompression ? compressedContent : contentStr;
  contentObj << "<< /Length " << streamData.size();
  if (useCompression)
    contentObj << " /Filter /FlateDecode";
  contentObj << " >>\nstream\n" << streamData << "endstream";
  objects.push_back({contentObj.str()});

  std::ostringstream resources;
  resources << "<< /Font << /F1 " << regularFont.objectId << " 0 R";
  labelFonts.Resources(resources);
  if (boldFont.objectId != 0 && boldFont.objectId != regularFont.objectId)
    resources << " /F2 " << boldFont.objectId << " 0 R";
  else
    resources << " /F2 " << regularFont.objectId << " 0 R";
  resources << " >>";
  if (!xObjectNameIds.empty()) {
    resources << " /XObject << ";
    for (const auto &entry : xObjectNameIds) {
      resources << '/' << entry.first << ' ' << entry.second << " 0 R ";
    }
    resources << ">>";
  }
  resources << " >>";

  std::ostringstream pageObj;
  size_t contentIndex = objects.size();
  size_t pageIndex = contentIndex + 1;
  size_t pagesIndex = pageIndex + 1;
  size_t catalogIndex = pagesIndex + 1;

  pageObj << "<< /Type /Page /Parent " << pagesIndex
          << " 0 R /MediaBox [0 0 " << formatter.Format(pageW) << ' '
          << formatter.Format(pageH) << "] /Contents " << contentIndex
          << " 0 R /Resources " << resources.str() << " >>";
  objects.push_back({pageObj.str()});
  objects.push_back({"<< /Type /Pages /Kids [" + std::to_string(pageIndex) +
                    " 0 R] /Count 1 >>"});
  objects.push_back({"<< /Type /Catalog /Pages " +
                    std::to_string(pagesIndex) + " 0 R >>"});

  try {
    std::ofstream file(outputPath, std::ios::binary);
    if (!file.is_open()) {
      result.message = "Unable to open the destination file for writing.";
      return result;
    }

    file << "%PDF-1.4\n";
    std::vector<long> offsets;
    offsets.reserve(objects.size());
    for (size_t i = 0; i < objects.size(); ++i) {
      offsets.push_back(static_cast<long>(file.tellp()));
      file << (i + 1) << " 0 obj\n" << objects[i].body << "\nendobj\n";
    }

    long xrefPos = static_cast<long>(file.tellp());
    file << "xref\n0 " << (objects.size() + 1)
         << "\n0000000000 65535 f \n";
    for (long off : offsets) {
      file << std::setw(10) << std::setfill('0') << off << " 00000 n \n";
    }
    file << "trailer\n<< /Size " << (objects.size() + 1)
         << " /Root " << catalogIndex
         << " 0 R >>\nstartxref\n" << xrefPos << "\n%%EOF";
    result.success = true;
  } catch (const std::exception &ex) {
    result.message =
        std::string("Failed to generate PDF content: ") + ex.what();
    return result;
  } catch (...) {
    result.message = "An unknown error occurred while generating the PDF plan.";
    return result;
  }

  return result;
}
