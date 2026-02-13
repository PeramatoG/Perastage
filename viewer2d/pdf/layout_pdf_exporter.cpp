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
#include "pdf_draw_commands.h"
#include "pdf_font_metrics.h"
#include "pdf_objects.h"
#include "viewer2dcommandrenderer.h"

namespace {
using namespace layout_pdf_internal;

constexpr double kLegendContentScale = 0.7;
constexpr double kPdfPointsPerPixel = 72.0 / 96.0;
constexpr double kLegendSymbolSize =
    96.0 * 2.0 / 3.0 * kLegendContentScale;
constexpr double kLegendFontScale =
    (2.0 / 3.0) * kLegendContentScale;
constexpr std::array<const char *, 7> kEventTableLabels = {
    "Venue:", "Location:", "Date:", "Stage:",
    "Version:", "Design:", "Mail:"};

static bool ShouldTraceLabelOrder() {
  static const bool enabled = std::getenv("PERASTAGE_TRACE_LABELS") != nullptr;
  return enabled;
}

double ComputeTextLineAdvance(double ascent, double descent) {
  // Negative because PDF moves the text cursor downward with a negative y
  // translation. The advance mirrors the ascent + descent used by the
  // on-screen viewer when positioning multi-line labels.
  return -(ascent + descent);
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

SymbolBounds ComputeSymbolBounds(const std::vector<CanvasCommand> &commands) {
  SymbolBounds bounds{};
  bool hasPoint = false;

  auto addPoint = [&](float x, float y) {
    if (!hasPoint) {
      bounds.min = {x, y};
      bounds.max = {x, y};
      hasPoint = true;
      return;
    }
    bounds.min.x = std::min(bounds.min.x, x);
    bounds.min.y = std::min(bounds.min.y, y);
    bounds.max.x = std::max(bounds.max.x, x);
    bounds.max.y = std::max(bounds.max.y, y);
  };

  auto addPointWithPadding = [&](float x, float y, float padding) {
    if (padding <= 0.0f) {
      addPoint(x, y);
      return;
    }
    addPoint(x - padding, y - padding);
    addPoint(x + padding, y + padding);
  };

  auto addPoints = [&](const std::vector<float> &points, float padding) {
    for (size_t i = 0; i + 1 < points.size(); i += 2)
      addPointWithPadding(points[i], points[i + 1], padding);
  };

  for (const auto &cmd : commands) {
    if (const auto *line = std::get_if<LineCommand>(&cmd)) {
      float padding = line->stroke.width * 0.5f;
      addPointWithPadding(line->x0, line->y0, padding);
      addPointWithPadding(line->x1, line->y1, padding);
    } else if (const auto *polyline = std::get_if<PolylineCommand>(&cmd)) {
      float padding = polyline->stroke.width * 0.5f;
      addPoints(polyline->points, padding);
    } else if (const auto *poly = std::get_if<PolygonCommand>(&cmd)) {
      float padding = poly->stroke.width * 0.5f;
      addPoints(poly->points, padding);
    } else if (const auto *rect = std::get_if<RectangleCommand>(&cmd)) {
      float padding = rect->stroke.width * 0.5f;
      addPoint(rect->x - padding, rect->y - padding);
      addPoint(rect->x + rect->w + padding, rect->y - padding);
      addPoint(rect->x + rect->w + padding, rect->y + rect->h + padding);
      addPoint(rect->x - padding, rect->y + rect->h + padding);
    } else if (const auto *circle = std::get_if<CircleCommand>(&cmd)) {
      float padding = circle->stroke.width * 0.5f;
      float radius = circle->radius + padding;
      addPoint(circle->cx - radius, circle->cy - radius);
      addPoint(circle->cx + radius, circle->cy + radius);
    }
  }

  if (!hasPoint) {
    bounds.min = {};
    bounds.max = {};
  }
  return bounds;
}

// Emits only the stroke portion of a drawing command. Keeping strokes and
// fills in separate functions allows the caller to control layering
// explicitly, which is required to match the on-screen 2D viewer where fills
// occlude internal wireframe edges within the same group.
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
  // Maintain a consistent physical stroke size in PDF (points) to mirror
  // on-screen pixel widths regardless of view scale.
  const double strokeScale = kPdfPointsPerPixel / scale;

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

  PdfFontDefinition regularFont;
  regularFont.key = "F1";
  regularFont.family = "sans";
  regularFont.baseName = "PerastageSans";
  PdfFontDefinition boldFont;
  boldFont.key = "F2";
  boldFont.family = "sans-bold";
  boldFont.baseName = "PerastageSansBold";

  bool regularMetricsLoaded = LoadPdfFontMetrics(regularFont, false);
  bool boldMetricsLoaded = LoadPdfFontMetrics(boldFont, true);
  if (!boldMetricsLoaded && regularMetricsLoaded)
    boldFont.metrics = regularFont.metrics;

  PdfFontCatalog fontCatalog{&regularFont, &boldFont};

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
  std::vector<PdfObject> objects;
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
  symbolMapping.scale = scale;
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
    double minX = bounds.min.x * scale;
    double minY = bounds.min.y * scale;
    double maxX = bounds.max.x * scale;
    double maxY = bounds.max.y * scale;
    if (minX > maxX)
      std::swap(minX, maxX);
    if (minY > maxY)
      std::swap(minY, maxY);
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
    const Viewer2DPrintOptions &options,
    const std::filesystem::path &outputPath) {
  Viewer2DExportResult result{};

  if (views.empty()) {
    result.message = "No layout views were provided for export.";
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
  if (pageW <= 0.0 || pageH <= 0.0) {
    result.message = "The selected paper size leaves no space for drawing.";
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
    // Maintain a consistent physical stroke size in PDF (points) to mirror
    // on-screen pixel widths regardless of view scale.
    const double strokeScale =
        kPdfPointsPerPixel / viewMapping.scale;
    layoutGroups.push_back({std::move(mainCommands),
                            mapping,
                            static_cast<double>(view.frame.x),
                            frameOriginY,
                            static_cast<double>(view.frame.width),
                            static_cast<double>(view.frame.height),
                            std::move(viewSymbolKeys),
                            std::move(viewSymbolIds),
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

  if (layoutGroups.empty()) {
    result.message = "Nothing to export";
    return result;
  }

  FloatFormatter formatter(options.floatPrecision);
  std::unordered_map<std::string, size_t> xObjectNameIds;
  std::unordered_map<uint32_t, std::string> legendSymbolNames;
  const double legendStrokeScale = 1.0 / viewer2d::kViewer2DPixelsPerMeter;
  auto makeLegendIdName = [](uint32_t symbolId) {
    return "L" + std::to_string(symbolId);
  };
  auto addLegendSymbol = [&](const SymbolDefinition *symbol) {
    if (!symbol)
      return;
    legendSymbolNames.emplace(symbol->symbolId,
                              makeLegendIdName(symbol->symbolId));
  };
  for (const auto &legend : legends) {
    const SymbolDefinitionSnapshot *legendSymbols =
        legend.symbolSnapshot ? legend.symbolSnapshot.get() : symbolSnapshot.get();
    if (!legendSymbols)
      continue;
    for (const auto &item : legend.items) {
      if (item.symbolKey.empty())
        continue;
      const SymbolDefinition *topSymbol = FindSymbolDefinitionPreferred(
          legendSymbols, item.symbolKey, SymbolViewKind::Top);
      const SymbolDefinition *frontSymbol = FindSymbolDefinitionExact(
          legendSymbols, item.symbolKey, SymbolViewKind::Front);
      addLegendSymbol(topSymbol);
      addLegendSymbol(frontSymbol);
    }
  }

  std::ostringstream contentStream;
  auto encodeText = [&](const std::string &text) {
    return EncodeWinAnsi(text);
  };
  auto escapeText = [](const std::string &text) {
    std::string escaped;
    escaped.reserve(text.size());
    for (unsigned char ch : text) {
      switch (ch) {
      case '(':
      case ')':
      case '\\':
        escaped.push_back('\\');
        escaped.push_back(static_cast<char>(ch));
        break;
      case '\n':
        escaped.append("\\n");
        break;
      case '\r':
        escaped.append("\\r");
        break;
      case '\t':
        escaped.append("\\t");
        break;
      case '\b':
        escaped.append("\\b");
        break;
      case '\f':
        escaped.append("\\f");
        break;
      default:
        if (ch < 0x20 || ch > 0x7e) {
          char buffer[5] = {};
          std::snprintf(buffer, sizeof(buffer), "\\%03o", ch);
          escaped.append(buffer);
        } else {
          escaped.push_back(static_cast<char>(ch));
        }
        break;
      }
    }
    return escaped;
  };
  auto trimTextToWidth = [&](const std::string &text, double maxWidth,
                             double fontSize,
                             const PdfFontDefinition *font) {
    if (maxWidth <= 0.0)
      return std::string();
    if (MeasureTextWidth(text, fontSize, font) <= maxWidth)
      return text;
    const std::string ellipsis = "...";
    const double ellipsisWidth = MeasureTextWidth(ellipsis, fontSize, font);
    if (ellipsisWidth >= maxWidth)
      return ellipsis.substr(0, 1);
    std::string trimmed = text;
    while (!trimmed.empty() &&
           MeasureTextWidth(trimmed, fontSize, font) + ellipsisWidth >
               maxWidth) {
      trimmed.pop_back();
    }
    return trimmed + ellipsis;
  };
  auto makeLayoutKeyName = [&](size_t viewIndex, const std::string &key) {
    return "K" + MakePdfName("V" + std::to_string(viewIndex) + "_" + key);
  };
  auto makeLayoutIdName = [&](size_t viewIndex, uint32_t id) {
    return "S" + MakePdfName("V" + std::to_string(viewIndex) + "_" +
                             std::to_string(id));
  };

  std::vector<PdfObject> objects;
  PdfFontDefinition regularFont;
  regularFont.key = "F1";
  regularFont.family = "sans";
  regularFont.baseName = "PerastageSans";
  PdfFontDefinition boldFont;
  boldFont.key = "F2";
  boldFont.family = "sans-bold";
  boldFont.baseName = "PerastageSansBold";

  auto loadFont = [&](PdfFontDefinition &font, bool bold) {
    std::filesystem::path path = FindFontPath(bold);
    if (path.empty())
      return false;
    if (!LoadTtfFontMetrics(path, font.metrics))
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
                             group.mapping.scale, group.strokeScale, bounds);
    }

    if (symbolSnapshot) {
      for (const auto &entry : viewIdNames) {
        auto defIt = symbolSnapshot->find(entry.first);
        if (defIt == symbolSnapshot->end())
          continue;
        xObjectNameIds[entry.second] =
            appendSymbolObject(entry.second,
                               defIt->second.localCommands.commands,
                               defIt->second.localCommands.metadata,
                               defIt->second.localCommands.sources,
                               group.mapping.scale, group.strokeScale,
                               defIt->second.bounds);
      }
    }
  }

  if (symbolSnapshot) {
    for (const auto &entry : legendSymbolNames) {
      if (xObjectNameIds.count(entry.second) != 0)
        continue;
      auto defIt = symbolSnapshot->find(entry.first);
      if (defIt == symbolSnapshot->end())
        continue;
      const double symbolW =
          defIt->second.bounds.max.x - defIt->second.bounds.min.x;
      const double symbolH =
          defIt->second.bounds.max.y - defIt->second.bounds.min.y;
      double symbolScale = 1.0;
      if (symbolW > 0.0 && symbolH > 0.0) {
        symbolScale =
            std::min(kLegendSymbolSize / symbolW, kLegendSymbolSize / symbolH);
      }
      xObjectNameIds[entry.second] =
          appendSymbolObject(entry.second,
                             defIt->second.localCommands.commands,
                             defIt->second.localCommands.metadata,
                             defIt->second.localCommands.sources,
                             symbolScale, legendStrokeScale,
                             defIt->second.bounds);
    }
  }


  struct LayoutRenderElement {
    enum class Type { View, Legend, EventTable, Text };
    Type type = Type::View;
    size_t index = 0;
    int zIndex = 0;
    size_t order = 0;
  };

  std::vector<LayoutRenderElement> renderOrder;
  renderOrder.reserve(layoutGroups.size() + legends.size() + tables.size() +
                      texts.size());
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

  std::stable_sort(renderOrder.begin(), renderOrder.end(),
                   [](const auto &lhs, const auto &rhs) {
                     if (lhs.zIndex != rhs.zIndex)
                       return lhs.zIndex < rhs.zIndex;
                     return lhs.order < rhs.order;
                   });

  auto renderViewGroup = [&](size_t idx) {
    const auto &group = layoutGroups[idx];
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
    contentStream << "q\n0 0 0 RG 0.5 w "
                  << formatter.Format(group.frameX) << ' '
                  << formatter.Format(group.frameY) << ' '
                  << formatter.Format(group.frameW) << ' '
                  << formatter.Format(group.frameH) << " re S\nQ\n";
  };

  auto renderLegend = [&](size_t idx) {
    const auto &legend = legends[idx];
    const double frameX = static_cast<double>(legend.frame.x);
    const double frameY =
        pageH - legend.frame.y - static_cast<double>(legend.frame.height);
    const double frameW = static_cast<double>(legend.frame.width);
    const double frameH = static_cast<double>(legend.frame.height);
    if (frameW <= 0.0 || frameH <= 0.0)
      return;

    contentStream << "q\n" << formatter.Format(frameX) << ' '
                  << formatter.Format(frameY) << ' '
                  << formatter.Format(frameW) << ' '
                  << formatter.Format(frameH) << " re W n\n";
    contentStream << "1 1 1 rg " << formatter.Format(frameX) << ' '
                  << formatter.Format(frameY) << ' '
                  << formatter.Format(frameW) << ' '
                  << formatter.Format(frameH) << " re f\n";

    const double paddingLeft = 4.0;
    const double paddingRight = 4.0;
    const double paddingTop = 6.0;
    const double paddingBottom = 2.0;
    const double columnGap = 8.0;
    const double symbolColumnGap = 2.0;
    constexpr double kLegendLineSpacingScale = 1.0;
    constexpr double kLegendSymbolColumnScale = 1.0;
    constexpr double kLegendSymbolPairOverlapScale = 0.5;
    const double separatorGap = 2.0;
    const size_t totalRows = legend.items.size() + 1;
    const double availableHeight =
        frameH - paddingTop - paddingBottom - separatorGap;
    double fontSize =
        totalRows > 0 ? (availableHeight / totalRows) - 2.0 : 10.0;
    fontSize = std::clamp(fontSize, 6.0, 14.0);
    fontSize *= kLegendFontScale;
    const double fontScale =
        std::clamp(fontSize / (14.0 * kLegendFontScale), 0.0, 1.0);

    double maxCountWidth =
        MeasureTextWidth("Count", fontSize, fontCatalog.bold);
    double maxChWidth =
        MeasureTextWidth("Ch", fontSize, fontCatalog.bold);
    for (const auto &item : legend.items) {
      maxCountWidth = std::max(
          maxCountWidth,
          MeasureTextWidth(std::to_string(item.count), fontSize,
                           fontCatalog.regular));
      std::string chText =
          item.channelCount ? std::to_string(*item.channelCount) : "-";
      maxChWidth = std::max(
          maxChWidth,
          MeasureTextWidth(chText, fontSize, fontCatalog.regular));
    }
    const double leftTrim = MeasureTextWidth("000", fontSize, fontCatalog.regular);
    const double chExtraWidth =
        MeasureTextWidth("0", fontSize, fontCatalog.regular);
    maxChWidth += chExtraWidth;

    const double rowHeightCandidate =
        totalRows > 0 ? (availableHeight / totalRows) : 0.0;
    const double availableHeightPdf = std::max(0.0, availableHeight);
    const double textHeightEstimate = fontSize * 1.2;
    const double lineHeight = textHeightEstimate + separatorGap;
    const double symbolSize =
        std::max(4.0, kLegendSymbolSize * fontScale);
    const double symbolPairGapSize =
        -std::max(1.0, symbolSize * kLegendSymbolPairOverlapScale);
    auto symbolDrawWidth = [&](const SymbolDefinition *symbol) -> double {
      if (!symbol)
        return 0.0;
      const double symbolW = symbol->bounds.max.x - symbol->bounds.min.x;
      const double symbolH = symbol->bounds.max.y - symbol->bounds.min.y;
      if (symbolW <= 0.0 || symbolH <= 0.0)
        return 0.0;
      double scale = std::min(symbolSize / symbolW, symbolSize / symbolH);
      return symbolW * scale;
    };
    auto symbolDrawHeight = [&](const SymbolDefinition *symbol) -> double {
      if (!symbol)
        return 0.0;
      const double symbolW = symbol->bounds.max.x - symbol->bounds.min.x;
      const double symbolH = symbol->bounds.max.y - symbol->bounds.min.y;
      if (symbolW <= 0.0 || symbolH <= 0.0)
        return 0.0;
      double scale = std::min(symbolSize / symbolW, symbolSize / symbolH);
      return symbolH * scale;
    };
    double maxSymbolPairWidth = symbolSize;
    const SymbolDefinitionSnapshot *legendSymbolsForSizing =
        legend.symbolSnapshot ? legend.symbolSnapshot.get()
                              : symbolSnapshot.get();
    if (legendSymbolsForSizing) {
      for (const auto &item : legend.items) {
        if (item.symbolKey.empty())
          continue;
        const SymbolDefinition *topSymbol = FindSymbolDefinitionPreferred(
            legendSymbolsForSizing, item.symbolKey, SymbolViewKind::Top);
        const SymbolDefinition *frontSymbol = FindSymbolDefinitionExact(
            legendSymbolsForSizing, item.symbolKey, SymbolViewKind::Front);
        const double topDrawW = symbolDrawWidth(topSymbol);
        const double frontDrawW = symbolDrawWidth(frontSymbol);
        double rowPairWidth = std::max(topDrawW, frontDrawW);
        if (topDrawW > 0.0 && frontDrawW > 0.0)
          rowPairWidth = topDrawW + frontDrawW + symbolPairGapSize;
        maxSymbolPairWidth = std::max(maxSymbolPairWidth, rowPairWidth);
      }
    }
    const double symbolSlotSize = std::max(
        4.0, maxSymbolPairWidth * kLegendSymbolColumnScale);
    const double rowHeight =
        std::max(rowHeightCandidate * kLegendLineSpacingScale, lineHeight);
    const double contentGap =
        std::max(0.0, availableHeightPdf - rowHeight * totalRows);
    const double textOffset =
        std::max(0.0, (rowHeight - textHeightEstimate) * 0.5);
    double xSymbol = frameX + paddingLeft - leftTrim;
    double xCount = xSymbol + symbolSlotSize + symbolColumnGap;
    double xType = xCount + maxCountWidth + columnGap;
    double xCh = frameX + frameW - paddingRight - maxChWidth;
    if (xCh < xType + columnGap)
      xCh = xType + columnGap;
    double typeWidth = std::max(0.0, xCh - xType - columnGap);

    auto appendText = [&](double x, double y, const std::string &text,
                          const char *fontKey, double r, double g, double b) {
      contentStream << "BT\n/" << fontKey << ' ' << formatter.Format(fontSize)
                    << " Tf\n"
                    << formatter.Format(r) << ' ' << formatter.Format(g) << ' '
                    << formatter.Format(b) << " rg\n"
                    << formatter.Format(x) << ' '
                    << formatter.Format(y) << " Td\n("
                    << escapeText(text) << ") Tj\nET\n";
    };

    double rowTop = frameY + frameH - paddingTop - contentGap;
    // Use a bold PDF font for legend headers to keep emphasis consistent with
    // the UI and avoid diverging header styling between PDF and on-screen views.
    appendText(xCount, rowTop - textOffset - fontSize, encodeText("Count"), "F2", 0.08,
               0.08, 0.08);
    appendText(xType, rowTop - textOffset - fontSize, encodeText("Type"), "F2", 0.08, 0.08,
               0.08);
    appendText(xCh, rowTop - textOffset - fontSize, encodeText("Ch"), "F2", 0.08,
               0.08, 0.08);

    const double separatorY = rowTop - rowHeight;
    contentStream << formatter.Format(0.78) << ' ' << formatter.Format(0.78)
                  << ' ' << formatter.Format(0.78) << " RG 0.5 w "
                  << formatter.Format(xSymbol) << ' '
                  << formatter.Format(separatorY) << " m "
                  << formatter.Format(frameX + frameW - paddingRight) << ' '
                  << formatter.Format(separatorY) << " l S\n";

    rowTop = separatorY - separatorGap;
    for (const auto &item : legend.items) {
      if (rowTop - rowHeight < frameY + paddingBottom)
        break;
      const std::string countText = encodeText(std::to_string(item.count));
      std::string typeText =
          trimTextToWidth(encodeText(item.typeName), typeWidth, fontSize,
                          fontCatalog.regular);
      std::string chText =
          encodeText(item.channelCount ? std::to_string(*item.channelCount) : "-");
      if (!item.symbolKey.empty()) {
        const SymbolDefinitionSnapshot *legendSymbols =
            legend.symbolSnapshot ? legend.symbolSnapshot.get()
                                  : symbolSnapshot.get();
        const SymbolDefinition *topSymbol = FindSymbolDefinitionPreferred(
            legendSymbols, item.symbolKey, SymbolViewKind::Top);
        const SymbolDefinition *frontSymbol = FindSymbolDefinitionExact(
            legendSymbols, item.symbolKey, SymbolViewKind::Front);
        const double topDrawW = symbolDrawWidth(topSymbol);
        const double frontDrawW = symbolDrawWidth(frontSymbol);
        const double topDrawH = symbolDrawHeight(topSymbol);
        const double frontDrawH = symbolDrawHeight(frontSymbol);
        if (topDrawW > 0.0 || frontDrawW > 0.0) {
          double rowBottom = rowTop - rowHeight;
          double symbolBoxY = rowBottom + (rowHeight - symbolSize) * 0.5;
          double rowPairWidth = std::max(topDrawW, frontDrawW);
          if (topDrawW > 0.0 && frontDrawW > 0.0)
            rowPairWidth = topDrawW + frontDrawW + symbolPairGapSize;
          const double rowStart =
              xSymbol + std::max(0.0, (symbolSlotSize - rowPairWidth) * 0.5);
          double leftSlotWidth = rowPairWidth;
          double rightSlotWidth = rowPairWidth;
          double topSlotLeft = rowStart;
          double frontSlotLeft = rowStart;
          if (topDrawW > 0.0 && frontDrawW > 0.0) {
            leftSlotWidth = topDrawW;
            rightSlotWidth = frontDrawW;
            frontSlotLeft = rowStart + topDrawW + symbolPairGapSize;
          } else if (frontDrawW > 0.0) {
            frontSlotLeft = rowStart;
          }
          auto drawSymbol = [&](const SymbolDefinition *symbol,
                                double drawW, double drawH,
                                double drawLeft) {
            if (!symbol || drawW <= 0.0 || drawH <= 0.0)
              return;
            auto nameIt = legendSymbolNames.find(symbol->symbolId);
            if (nameIt == legendSymbolNames.end())
              return;
            const double symbolW =
                symbol->bounds.max.x - symbol->bounds.min.x;
            const double symbolH =
                symbol->bounds.max.y - symbol->bounds.min.y;
            if (symbolW <= 0.0 || symbolH <= 0.0)
              return;
            double scale =
                std::min(symbolSize / symbolW, symbolSize / symbolH);
            double symbolOffsetX =
                drawLeft - symbol->bounds.min.x * scale;
            double symbolOffsetY =
                symbolBoxY + (symbolSize - drawH) * 0.5 -
                symbol->bounds.min.y * scale;
            contentStream << "q\n1 0 0 1 "
                          << formatter.Format(symbolOffsetX) << ' '
                          << formatter.Format(symbolOffsetY) << " cm\n/"
                          << nameIt->second << " Do\nQ\n";
          };
          if (topDrawW > 0.0) {
            double symbolLeft =
                topSlotLeft + std::max(0.0, (leftSlotWidth - topDrawW) * 0.5);
            drawSymbol(topSymbol, topDrawW, topDrawH, symbolLeft);
          }
          if (frontDrawW > 0.0) {
            double symbolLeft =
                frontSlotLeft +
                std::max(0.0, (rightSlotWidth - frontDrawW) * 0.5);
            drawSymbol(frontSymbol, frontDrawW, frontDrawH, symbolLeft);
          }
        }
      }
      appendText(xCount, rowTop - textOffset - fontSize, countText, "F1", 0.08,
                 0.08, 0.08);
      appendText(xType, rowTop - textOffset - fontSize, typeText, "F1", 0.08,
                 0.08, 0.08);
      appendText(xCh, rowTop - textOffset - fontSize, chText, "F1", 0.08, 0.08,
                 0.08);
      rowTop -= rowHeight;
    }

    contentStream << "Q\n";
    contentStream << "q\n0 0 0 RG 0.5 w " << formatter.Format(frameX) << ' '
                  << formatter.Format(frameY) << ' '
                  << formatter.Format(frameW) << ' '
                  << formatter.Format(frameH) << " re S\nQ\n";
  };

  auto renderEventTable = [&](size_t idx) {
    const auto &table = tables[idx];
    const double frameX = static_cast<double>(table.frame.x);
    const double frameY =
        pageH - table.frame.y - static_cast<double>(table.frame.height);
    const double frameW = static_cast<double>(table.frame.width);
    const double frameH = static_cast<double>(table.frame.height);
    if (frameW <= 0.0 || frameH <= 0.0)
      return;

    contentStream << "q\n" << formatter.Format(frameX) << ' '
                  << formatter.Format(frameY) << ' '
                  << formatter.Format(frameW) << ' '
                  << formatter.Format(frameH) << " re W n\n";
    contentStream << "1 1 1 rg " << formatter.Format(frameX) << ' '
                  << formatter.Format(frameY) << ' '
                  << formatter.Format(frameW) << ' '
                  << formatter.Format(frameH) << " re f\n";

    const double paddingLeft = 6.0;
    const double paddingRight = 6.0;
    const double paddingTop = 6.0;
    const double paddingBottom = 6.0;
    const double columnGap = 10.0;
    const size_t totalRows = kEventTableLabels.size();
    const double availableHeight =
        frameH - paddingTop - paddingBottom;
    double fontSize =
        totalRows > 0 ? (availableHeight / totalRows) - 2.0 : 10.0;
    fontSize = std::clamp(fontSize, 6.0, 14.0);
    fontSize *= kLegendFontScale;
    const double emphasizedFontSize =
        std::max(fontSize + 1.0, fontSize * 1.1);

    double maxLabelWidth = 0.0;
    for (const auto &label : kEventTableLabels) {
      maxLabelWidth = std::max(
          maxLabelWidth,
          MeasureTextWidth(label, fontSize, fontCatalog.bold));
    }

    const double rowHeight =
        totalRows > 0 ? (availableHeight / totalRows) : 0.0;
    const double textHeightEstimate = fontSize * 1.2;
    const double textOffset =
        std::max(0.0, (rowHeight - textHeightEstimate) * 0.5);
    const double labelX = frameX + paddingLeft;
    const double valueX = labelX + maxLabelWidth + columnGap;
    const double maxValueWidth =
        std::max(0.0, frameX + frameW - paddingRight - valueX);

    auto appendText = [&](double x, double y, const std::string &text,
                          const char *fontKey, double size, double r, double g,
                          double b) {
      contentStream << "BT\n/" << fontKey << ' ' << formatter.Format(size)
                    << " Tf\n"
                    << formatter.Format(r) << ' ' << formatter.Format(g) << ' '
                    << formatter.Format(b) << " rg\n"
                    << formatter.Format(x) << ' '
                    << formatter.Format(y) << " Td\n("
                    << escapeText(text) << ") Tj\nET\n";
    };

    for (size_t row = 0; row < totalRows; ++row) {
      const double rowTop = frameY + frameH - paddingTop - row * rowHeight;
      appendText(labelX, rowTop - textOffset - fontSize,
                 encodeText(kEventTableLabels[row]), "F2", fontSize, 0.08,
                 0.08, 0.08);

      std::string valueText;
      if (row < table.fields.size())
        valueText = encodeText(table.fields[row]);
      const double valueFontSize =
          row == 0 ? emphasizedFontSize : fontSize;
      const char *valueFontKey = row == 0 ? "F2" : "F1";
      std::string trimmed =
          trimTextToWidth(valueText, maxValueWidth, valueFontSize,
                          row == 0 ? fontCatalog.bold : fontCatalog.regular);
      appendText(valueX, rowTop - textOffset - valueFontSize, trimmed,
                 valueFontKey, valueFontSize, 0.08, 0.08, 0.08);
    }

    contentStream << "Q\n";
    contentStream << "q\n0 0 0 RG 0.5 w " << formatter.Format(frameX) << ' '
                  << formatter.Format(frameY) << ' '
                  << formatter.Format(frameW) << ' '
                  << formatter.Format(frameH) << " re S\nQ\n";
  };

  auto renderText = [&](size_t idx) {
    const auto &text = texts[idx];
    const double frameW = static_cast<double>(text.frame.width);
    const double frameH = static_cast<double>(text.frame.height);
    if (frameW <= 0.0 || frameH <= 0.0)
      return;
    const double frameX = static_cast<double>(text.frame.x);
    const double frameY =
        pageH - text.frame.y - static_cast<double>(text.frame.height);

    contentStream << "q\n" << formatter.Format(frameX) << ' '
                  << formatter.Format(frameY) << ' '
                  << formatter.Format(frameW) << ' '
                  << formatter.Format(frameH) << " re W n\n";
    if (text.solidBackground) {
      contentStream << "1 1 1 rg " << formatter.Format(frameX) << ' '
                    << formatter.Format(frameY) << ' '
                    << formatter.Format(frameW) << ' '
                    << formatter.Format(frameH) << " re f\n";
    }

    const double padding = 4.0;
    const double availableH = std::max(0.0, frameH - padding * 2.0);
    constexpr double kItalicSkew = 0.2;
    auto appendTextRun = [&](double x, double y, const std::string &line,
                             const char *fontKey, double size, bool italic) {
      contentStream << "BT\n/" << fontKey << ' ' << formatter.Format(size)
                    << " Tf\n0 0 0 rg\n";
      if (italic) {
        contentStream << "1 0 " << formatter.Format(kItalicSkew) << " 1 "
                      << formatter.Format(x) << ' ' << formatter.Format(y)
                      << " Tm\n(" << escapeText(line) << ") Tj\nET\n";
        return;
      }
      contentStream << formatter.Format(x) << ' ' << formatter.Format(y)
                    << " Td\n(" << escapeText(line) << ") Tj\nET\n";
    };

    double usedHeight = 0.0;
    for (const auto &line : text.lines) {
      double lineFontSize =
          text.fontSize > 0 ? static_cast<double>(text.fontSize) : 12.0;
      for (const auto &run : line.runs) {
        const double runSize =
            run.fontSize > 0 ? static_cast<double>(run.fontSize) : lineFontSize;
        lineFontSize = std::max(lineFontSize, runSize);
      }
      const double lineHeight = lineFontSize * 1.2;
      if (usedHeight + lineHeight > availableH && usedHeight > 0.0)
        break;
      double lineWidth = 0.0;
      for (const auto &run : line.runs) {
        const double runSize =
            run.fontSize > 0 ? static_cast<double>(run.fontSize) : lineFontSize;
        const PdfFontDefinition *font =
            run.bold ? fontCatalog.bold : fontCatalog.regular;
        const std::string encoded = encodeText(run.text);
        lineWidth += MeasureTextWidth(encoded, runSize, font);
      }
      double x = frameX + padding;
      if (text.alignment == LayoutTextExportData::Alignment::Center) {
        x = frameX + std::max(0.0, (frameW - lineWidth) * 0.5);
      } else if (text.alignment == LayoutTextExportData::Alignment::Right) {
        x = frameX + frameW - padding - lineWidth;
      }
      const double y = frameY + frameH - padding - usedHeight - lineFontSize;
      double cursorX = x;
      if (line.runs.empty()) {
        usedHeight += lineHeight;
        continue;
      }
      for (const auto &run : line.runs) {
        const double runSize =
            run.fontSize > 0 ? static_cast<double>(run.fontSize) : lineFontSize;
        const char *fontKey = run.bold ? "F2" : "F1";
        const PdfFontDefinition *font =
            run.bold ? fontCatalog.bold : fontCatalog.regular;
        const std::string encoded = encodeText(run.text);
        const double runWidth = MeasureTextWidth(encoded, runSize, font);
        appendTextRun(cursorX, y, encoded, fontKey, runSize, run.italic);
        cursorX += runWidth;
      }
      usedHeight += lineHeight;
    }
    contentStream << "Q\n";

    if (text.drawFrame) {
      contentStream << "q\n0 0 0 RG 0.5 w " << formatter.Format(frameX) << ' '
                    << formatter.Format(frameY) << ' '
                    << formatter.Format(frameW) << ' '
                    << formatter.Format(frameH) << " re S\nQ\n";
    }
  };

  for (const auto &entry : renderOrder) {
    if (entry.type == LayoutRenderElement::Type::View) {
      renderViewGroup(entry.index);
    } else if (entry.type == LayoutRenderElement::Type::Legend) {
      renderLegend(entry.index);
    } else if (entry.type == LayoutRenderElement::Type::EventTable) {
      renderEventTable(entry.index);
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
