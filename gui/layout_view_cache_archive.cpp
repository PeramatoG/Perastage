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
#include "layout_view_cache_archive.h"
#include "layoutviewerpanel.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

#include <wx/wfstream.h>
#include <wx/zipstrm.h>

#include "configmanager.h"
#include "guiconfigservices.h"
#include "json.hpp"
#include "layoutviewerviewrenderer.h"
#include "logger.h"
#include "symbol_cache_manifest.h"

namespace {
namespace fs = std::filesystem;

using gui::layoutcache::kLayoutViewCacheArchiveEntry;
using gui::layoutcache::kLayoutViewCacheSchemaVersion;

constexpr size_t kMaxPersistentCommandCount = 75000;
constexpr size_t kMaxPersistentCacheBytes = 8 * 1024 * 1024;

// Combines a byte into a deterministic FNV-1a hash accumulator.
void HashByte(std::uint64_t &hash, unsigned char value) {
  hash ^= value;
  hash *= 1099511628211ull;
}

// Combines a string into a deterministic FNV-1a hash accumulator.
void HashString(std::uint64_t &hash, const std::string &value) {
  for (unsigned char ch : value)
    HashByte(hash, ch);
}

// Returns a portable UTF-8 representation of a filesystem path.
std::string PathToUtf8(const fs::path &path) {
  const auto utf8 = path.generic_u8string();
  return std::string(utf8.begin(), utf8.end());
}

// Computes a deterministic hash for a file using its relative path and bytes.
bool HashFileContent(const fs::path &root, const fs::path &filePath,
                     std::uint64_t &hash) {
  std::error_code ec;
  const fs::path relative = fs::relative(filePath, root, ec);
  if (ec)
    return false;
  HashString(hash, PathToUtf8(relative));
  HashByte(hash, 0);

  std::ifstream input(filePath, std::ios::binary);
  if (!input.is_open())
    return false;
  std::array<char, 4096> buffer{};
  while (input) {
    input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    const std::streamsize count = input.gcount();
    for (std::streamsize i = 0; i < count; ++i)
      HashByte(hash,
               static_cast<unsigned char>(buffer[static_cast<size_t>(i)]));
  }
  HashByte(hash, 0);
  return input.good() || input.eof();
}

// Resolves a scene asset reference against the extracted MVR source root.
std::optional<fs::path> ResolveSourceAssetPath(const fs::path &root,
                                               const std::string &reference) {
  if (reference.empty())
    return std::nullopt;
  std::error_code ec;
  fs::path path = fs::u8path(reference);
  if (!path.is_absolute())
    path = root / path;
  path = fs::weakly_canonical(path, ec);
  if (ec || !fs::is_regular_file(path, ec))
    return std::nullopt;
  return path;
}

// Adds an existing scene asset reference to the source hash candidate list.
void AddSourceAssetPath(const fs::path &root, const std::string &reference,
                        std::vector<fs::path> &files) {
  if (auto path = ResolveSourceAssetPath(root, reference))
    files.push_back(*path);
}

// Collects existing files that are referenced by the scene and affect layout rendering.
std::vector<fs::path> CollectReferencedSourceAssets(const fs::path &root,
                                                    const MvrScene &scene) {
  std::vector<fs::path> files;
  for (const auto &[uuid, fixture] : scene.fixtures) {
    (void)uuid;
    AddSourceAssetPath(root, fixture.gdtfSpec, files);
  }
  for (const auto &[uuid, truss] : scene.trusses) {
    (void)uuid;
    AddSourceAssetPath(root, truss.gdtfSpec, files);
    AddSourceAssetPath(root, truss.symbolFile, files);
    AddSourceAssetPath(root, truss.modelFile, files);
    AddSourceAssetPath(root, truss.perastageAuxGdtfArchivePath, files);
  }
  for (const auto &[uuid, support] : scene.supports) {
    (void)uuid;
    AddSourceAssetPath(root, support.gdtfSpec, files);
  }
  for (const auto &[uuid, object] : scene.sceneObjects) {
    (void)uuid;
    AddSourceAssetPath(root, object.modelFile, files);
    for (const auto &geometry : object.geometries)
      AddSourceAssetPath(root, geometry.modelFile, files);
  }
  for (const auto &[uuid, symdefFile] : scene.symdefFiles) {
    (void)uuid;
    AddSourceAssetPath(root, symdefFile, files);
  }

  std::sort(files.begin(), files.end(), [](const fs::path &lhs,
                                           const fs::path &rhs) {
    return PathToUtf8(lhs) < PathToUtf8(rhs);
  });
  files.erase(std::unique(files.begin(), files.end()), files.end());
  return files;
}

// Computes a stable hash for authoritative extracted project source assets.
std::optional<std::string> ComputeSourceAssetHash(const MvrScene &scene) {
  if (scene.basePath.empty())
    return std::nullopt;
  std::error_code ec;
  const fs::path root = fs::weakly_canonical(fs::u8path(scene.basePath), ec);
  if (ec || !fs::is_directory(root, ec))
    return std::nullopt;

  const std::vector<fs::path> files =
      CollectReferencedSourceAssets(root, scene);
  std::uint64_t hash = 14695981039346656037ull;
  for (const fs::path &file : files) {
    if (!HashFileContent(root, file, hash))
      return std::nullopt;
  }
  return std::to_string(hash);
}

// Computes a deterministic FNV-1a hash for serialized JSON content.
std::string StableJsonHash(const nlohmann::json &value) {
  const std::string text = value.dump();
  std::uint64_t hash = 14695981039346656037ull;
  for (unsigned char ch : text)
    HashByte(hash, ch);
  return std::to_string(hash);
}

// Serializes a matrix into ordered JSON values for stable hashing.
nlohmann::json MatrixToJson(const Matrix &matrix) {
  return {matrix.u[0], matrix.u[1], matrix.u[2], matrix.v[0],
          matrix.v[1], matrix.v[2], matrix.w[0], matrix.w[1],
          matrix.w[2], matrix.o[0], matrix.o[1], matrix.o[2]};
}

// Serializes a layout view definition into stable JSON for cache validation.
nlohmann::json
ViewDefinitionToHashJson(const layouts::Layout2DViewDefinition &view) {
  nlohmann::json json;
  json["id"] = view.id;
  json["camera"] = {view.camera.offsetPixelsX,  view.camera.offsetPixelsY,
                    view.camera.zoom,           view.camera.viewportWidth,
                    view.camera.viewportHeight, view.camera.view};
  nlohmann::json forceBottom = nullptr;
  if (view.renderOptions.forceBottomViewForTopFixtures.has_value())
    forceBottom = view.renderOptions.forceBottomViewForTopFixtures.value();
  json["renderOptions"] = {
      {"renderMode", view.renderOptions.renderMode},
      {"darkMode", view.renderOptions.darkMode},
      {"forceBottomViewForTopFixtures", forceBottom},
      {"showGrid", view.renderOptions.showGrid},
      {"gridStyle", view.renderOptions.gridStyle},
      {"gridColorR", view.renderOptions.gridColorR},
      {"gridColorG", view.renderOptions.gridColorG},
      {"gridColorB", view.renderOptions.gridColorB},
      {"gridDrawAbove", view.renderOptions.gridDrawAbove},
      {"gridSpacingMeters", view.renderOptions.gridSpacingMeters},
      {"showRuler", view.renderOptions.showRuler},
      {"rulerColorR", view.renderOptions.rulerColorR},
      {"rulerColorG", view.renderOptions.rulerColorG},
      {"rulerColorB", view.renderOptions.rulerColorB},
      {"showLabelName", view.renderOptions.showLabelName},
      {"showLabelId", view.renderOptions.showLabelId},
      {"showLabelDmx", view.renderOptions.showLabelDmx},
      {"labelFontSizeName", view.renderOptions.labelFontSizeName},
      {"labelFontSizeId", view.renderOptions.labelFontSizeId},
      {"labelFontSizeDmx", view.renderOptions.labelFontSizeDmx},
      {"labelOffsetDistance", view.renderOptions.labelOffsetDistance},
      {"labelOffsetAngle", view.renderOptions.labelOffsetAngle}};
  json["layers"] = {{"hiddenLayers", view.layers.hiddenLayers},
                    {"hiddenFixtureTypes", view.layers.hiddenFixtureTypes}};
  return json;
}

// Serializes scene fields that affect 2D layout captures into stable JSON.
nlohmann::json SceneToHashJson(const MvrScene &scene) {
  nlohmann::json json;
  auto fixtures = nlohmann::json::array();
  for (const auto &[uuid, fixture] : std::map<std::string, Fixture>(
           scene.fixtures.begin(), scene.fixtures.end())) {
    fixtures.push_back({uuid, fixture.layer, fixture.instanceName,
                        fixture.typeName, fixture.gdtfSpec, fixture.gdtfMode,
                        fixture.color, MatrixToJson(fixture.transform)});
  }
  auto trusses = nlohmann::json::array();
  for (const auto &[uuid, truss] : std::map<std::string, Truss>(
           scene.trusses.begin(), scene.trusses.end())) {
    trusses.push_back({uuid, truss.layer, truss.name, truss.symbolFile,
                       truss.modelFile, MatrixToJson(truss.transform)});
  }
  auto supports = nlohmann::json::array();
  for (const auto &[uuid, support] : std::map<std::string, Support>(
           scene.supports.begin(), scene.supports.end())) {
    supports.push_back({uuid, support.layer, support.name,
                        support.hoistFunction,
                        MatrixToJson(support.transform)});
  }
  auto objects = nlohmann::json::array();
  for (const auto &[uuid, object] : std::map<std::string, SceneObject>(
           scene.sceneObjects.begin(), scene.sceneObjects.end())) {
    auto geometries = nlohmann::json::array();
    for (const auto &geometry : object.geometries) {
      geometries.push_back(
          {geometry.modelFile, MatrixToJson(geometry.localTransform)});
    }
    objects.push_back({uuid, object.layer, object.name, object.modelFile,
                       MatrixToJson(object.transform), geometries});
  }
  json["fixtures"] = fixtures;
  json["trusses"] = trusses;
  json["supports"] = supports;
  json["sceneObjects"] = objects;
  return json;
}

// Serializes a canvas color into JSON.
nlohmann::json ColorToJson(const CanvasColor &color) {
  return {color.r, color.g, color.b, color.a};
}

// Parses a canvas color from JSON.
CanvasColor ColorFromJson(const nlohmann::json &json) {
  CanvasColor color;
  if (json.is_array() && json.size() >= 4) {
    color.r = json[0].get<float>();
    color.g = json[1].get<float>();
    color.b = json[2].get<float>();
    color.a = json[3].get<float>();
  }
  return color;
}

// Serializes a canvas stroke into JSON.
nlohmann::json StrokeToJson(const CanvasStroke &stroke) {
  return {{"color", ColorToJson(stroke.color)}, {"width", stroke.width}};
}

// Parses a canvas stroke from JSON.
CanvasStroke StrokeFromJson(const nlohmann::json &json) {
  CanvasStroke stroke;
  if (json.is_object()) {
    stroke.color = ColorFromJson(json.value("color", nlohmann::json::array()));
    stroke.width = json.value("width", 1.0f);
  }
  return stroke;
}

// Serializes a canvas fill into JSON.
nlohmann::json FillToJson(const CanvasFill &fill) {
  return {{"color", ColorToJson(fill.color)}};
}

// Parses a canvas fill from JSON.
CanvasFill FillFromJson(const nlohmann::json &json) {
  CanvasFill fill;
  if (json.is_object())
    fill.color = ColorFromJson(json.value("color", nlohmann::json::array()));
  return fill;
}

// Serializes a canvas transform into JSON.
nlohmann::json CanvasTransformToJson(const CanvasTransform &transform) {
  return {transform.scale, transform.offsetX, transform.offsetY};
}

// Parses a canvas transform from JSON.
CanvasTransform CanvasTransformFromJson(const nlohmann::json &json) {
  CanvasTransform transform;
  if (json.is_array() && json.size() >= 3) {
    transform.scale = json[0].get<float>();
    transform.offsetX = json[1].get<float>();
    transform.offsetY = json[2].get<float>();
  }
  return transform;
}

// Serializes an affine transform into JSON.
nlohmann::json Transform2DToJson(const Transform2D &transform) {
  return {transform.a, transform.b,  transform.c,
          transform.d, transform.tx, transform.ty};
}

// Parses an affine transform from JSON.
Transform2D Transform2DFromJson(const nlohmann::json &json) {
  Transform2D transform;
  if (json.is_array() && json.size() >= 6) {
    transform.a = json[0].get<float>();
    transform.b = json[1].get<float>();
    transform.c = json[2].get<float>();
    transform.d = json[3].get<float>();
    transform.tx = json[4].get<float>();
    transform.ty = json[5].get<float>();
  }
  return transform;
}

// Serializes a text style into JSON.
nlohmann::json TextStyleToJson(const CanvasTextStyle &style) {
  return {{"fontFamily", style.fontFamily},
          {"fontSize", style.fontSize},
          {"ascent", style.ascent},
          {"descent", style.descent},
          {"lineHeight", style.lineHeight},
          {"extraLineSpacing", style.extraLineSpacing},
          {"color", ColorToJson(style.color)},
          {"outlineColor", ColorToJson(style.outlineColor)},
          {"outlineWidth", style.outlineWidth},
          {"hAlign", static_cast<int>(style.hAlign)},
          {"vAlign", static_cast<int>(style.vAlign)}};
}

// Parses a text style from JSON.
CanvasTextStyle TextStyleFromJson(const nlohmann::json &json) {
  CanvasTextStyle style;
  if (!json.is_object())
    return style;
  style.fontFamily = json.value("fontFamily", std::string{});
  style.fontSize = json.value("fontSize", 12.0f);
  style.ascent = json.value("ascent", 0.0f);
  style.descent = json.value("descent", 0.0f);
  style.lineHeight = json.value("lineHeight", 0.0f);
  style.extraLineSpacing = json.value("extraLineSpacing", 0.0f);
  style.color = ColorFromJson(json.value("color", nlohmann::json::array()));
  style.outlineColor =
      ColorFromJson(json.value("outlineColor", nlohmann::json::array()));
  style.outlineWidth = json.value("outlineWidth", 0.0f);
  style.hAlign =
      static_cast<CanvasTextStyle::HorizontalAlign>(json.value("hAlign", 0));
  style.vAlign =
      static_cast<CanvasTextStyle::VerticalAlign>(json.value("vAlign", 0));
  return style;
}

// Serializes one canvas command into tagged JSON.
nlohmann::json CommandToJson(const CanvasCommand &command) {
  return std::visit(
      [](const auto &cmd) -> nlohmann::json {
        using T = std::decay_t<decltype(cmd)>;
        nlohmann::json json;
        if constexpr (std::is_same_v<T, LineCommand>) {
          json = {{"type", "line"}, {"x0", cmd.x0},
                  {"y0", cmd.y0},   {"x1", cmd.x1},
                  {"y1", cmd.y1},   {"stroke", StrokeToJson(cmd.stroke)}};
        } else if constexpr (std::is_same_v<T, PolylineCommand>) {
          json = {{"type", "polyline"},
                  {"points", cmd.points},
                  {"stroke", StrokeToJson(cmd.stroke)}};
        } else if constexpr (std::is_same_v<T, PolygonCommand>) {
          json = {{"type", "polygon"},
                  {"points", cmd.points},
                  {"stroke", StrokeToJson(cmd.stroke)},
                  {"fill", FillToJson(cmd.fill)},
                  {"hasFill", cmd.hasFill}};
        } else if constexpr (std::is_same_v<T, RectangleCommand>) {
          json = {{"type", "rectangle"},
                  {"x", cmd.x},
                  {"y", cmd.y},
                  {"w", cmd.w},
                  {"h", cmd.h},
                  {"stroke", StrokeToJson(cmd.stroke)},
                  {"fill", FillToJson(cmd.fill)},
                  {"hasFill", cmd.hasFill}};
        } else if constexpr (std::is_same_v<T, CircleCommand>) {
          json = {{"type", "circle"},
                  {"cx", cmd.cx},
                  {"cy", cmd.cy},
                  {"radius", cmd.radius},
                  {"stroke", StrokeToJson(cmd.stroke)},
                  {"fill", FillToJson(cmd.fill)},
                  {"hasFill", cmd.hasFill}};
        } else if constexpr (std::is_same_v<T, TextCommand>) {
          json = {{"type", "text"},
                  {"x", cmd.x},
                  {"y", cmd.y},
                  {"text", cmd.text},
                  {"style", TextStyleToJson(cmd.style)}};
        } else if constexpr (std::is_same_v<T, SaveCommand>) {
          json = {{"type", "save"}};
        } else if constexpr (std::is_same_v<T, RestoreCommand>) {
          json = {{"type", "restore"}};
        } else if constexpr (std::is_same_v<T, TransformCommand>) {
          json = {{"type", "transform"},
                  {"transform", CanvasTransformToJson(cmd.transform)}};
        } else if constexpr (std::is_same_v<T, BeginSymbolCommand>) {
          json = {{"type", "beginSymbol"}, {"key", cmd.key}};
        } else if constexpr (std::is_same_v<T, EndSymbolCommand>) {
          json = {{"type", "endSymbol"}, {"key", cmd.key}};
        } else if constexpr (std::is_same_v<T, PlaceSymbolCommand>) {
          json = {{"type", "placeSymbol"},
                  {"key", cmd.key},
                  {"transform", CanvasTransformToJson(cmd.transform)}};
        } else if constexpr (std::is_same_v<T, SymbolInstanceCommand>) {
          json = {{"type", "symbolInstance"},
                  {"symbolId", cmd.symbolId},
                  {"transform", Transform2DToJson(cmd.transform)}};
        }
        return json;
      },
      command);
}

// Parses one tagged JSON object into a canvas command.
std::optional<CanvasCommand> CommandFromJson(const nlohmann::json &json) {
  if (!json.is_object())
    return std::nullopt;
  const std::string type = json.value("type", std::string{});
  if (type == "line")
    return LineCommand{json.value("x0", 0.0f), json.value("y0", 0.0f),
                       json.value("x1", 0.0f), json.value("y1", 0.0f),
                       StrokeFromJson(json.value("stroke", nlohmann::json{}))};
  if (type == "polyline")
    return PolylineCommand{
        json.value("points", std::vector<float>{}),
        StrokeFromJson(json.value("stroke", nlohmann::json{}))};
  if (type == "polygon")
    return PolygonCommand{
        json.value("points", std::vector<float>{}),
        StrokeFromJson(json.value("stroke", nlohmann::json{})),
        FillFromJson(json.value("fill", nlohmann::json{})),
        json.value("hasFill", false)};
  if (type == "rectangle")
    return RectangleCommand{
        json.value("x", 0.0f),
        json.value("y", 0.0f),
        json.value("w", 0.0f),
        json.value("h", 0.0f),
        StrokeFromJson(json.value("stroke", nlohmann::json{})),
        FillFromJson(json.value("fill", nlohmann::json{})),
        json.value("hasFill", false)};
  if (type == "circle")
    return CircleCommand{json.value("cx", 0.0f),
                         json.value("cy", 0.0f),
                         json.value("radius", 0.0f),
                         StrokeFromJson(json.value("stroke", nlohmann::json{})),
                         FillFromJson(json.value("fill", nlohmann::json{})),
                         json.value("hasFill", false)};
  if (type == "text")
    return TextCommand{
        json.value("x", 0.0f), json.value("y", 0.0f),
        json.value("text", std::string{}),
        TextStyleFromJson(json.value("style", nlohmann::json{}))};
  if (type == "save")
    return SaveCommand{};
  if (type == "restore")
    return RestoreCommand{};
  if (type == "transform")
    return TransformCommand{
        CanvasTransformFromJson(json.value("transform", nlohmann::json{}))};
  if (type == "beginSymbol")
    return BeginSymbolCommand{json.value("key", std::string{})};
  if (type == "endSymbol")
    return EndSymbolCommand{json.value("key", std::string{})};
  if (type == "placeSymbol")
    return PlaceSymbolCommand{
        json.value("key", std::string{}),
        CanvasTransformFromJson(json.value("transform", nlohmann::json{}))};
  if (type == "symbolInstance")
    return SymbolInstanceCommand{
        json.value("symbolId", 0u),
        Transform2DFromJson(json.value("transform", nlohmann::json{}))};
  return std::nullopt;
}

// Serializes a command buffer into JSON.
nlohmann::json CommandBufferToJson(const CommandBuffer &buffer) {
  nlohmann::json commands = nlohmann::json::array();
  for (const auto &command : buffer.commands)
    commands.push_back(CommandToJson(command));
  nlohmann::json metadata = nlohmann::json::array();
  for (const auto &entry : buffer.metadata) {
    metadata.push_back(
        {{"hasStroke", entry.hasStroke}, {"hasFill", entry.hasFill}});
  }
  return {{"commands", commands},
          {"sources", buffer.sources},
          {"metadata", metadata},
          {"currentSourceKey", buffer.currentSourceKey}};
}

// Parses a command buffer from JSON.
CommandBuffer CommandBufferFromJson(const nlohmann::json &json) {
  CommandBuffer buffer;
  if (!json.is_object())
    return buffer;
  for (const auto &entry : json.value("commands", nlohmann::json::array())) {
    if (auto command = CommandFromJson(entry))
      buffer.commands.push_back(std::move(*command));
  }
  buffer.sources = json.value("sources", std::vector<std::string>{});
  for (const auto &entry : json.value("metadata", nlohmann::json::array())) {
    CommandMetadata metadata;
    metadata.hasStroke = entry.value("hasStroke", false);
    metadata.hasFill = entry.value("hasFill", false);
    buffer.metadata.push_back(metadata);
  }
  buffer.currentSourceKey =
      json.value("currentSourceKey", std::string("unknown"));
  return buffer;
}

// Serializes a captured view state into JSON.
nlohmann::json ViewStateToJson(const Viewer2DViewState &state) {
  return {{"offsetPixelsX", state.offsetPixelsX},
          {"offsetPixelsY", state.offsetPixelsY},
          {"zoom", state.zoom},
          {"viewportWidth", state.viewportWidth},
          {"viewportHeight", state.viewportHeight},
          {"view", static_cast<int>(state.view)}};
}

// Parses a captured view state from JSON.
Viewer2DViewState ViewStateFromJson(const nlohmann::json &json) {
  Viewer2DViewState state;
  if (!json.is_object())
    return state;
  state.offsetPixelsX = json.value("offsetPixelsX", 0.0f);
  state.offsetPixelsY = json.value("offsetPixelsY", 0.0f);
  state.zoom = json.value("zoom", 1.0f);
  state.viewportWidth = json.value("viewportWidth", 0);
  state.viewportHeight = json.value("viewportHeight", 0);
  state.view = static_cast<Viewer2DView>(json.value("view", 0));
  return state;
}

// Serializes the reusable 2D render state into JSON.
nlohmann::json RenderStateToJson(const viewer2d::Viewer2DState &state) {
  layouts::Layout2DViewDefinition view = viewer2d::ToLayoutDefinition(state);
  return ViewDefinitionToHashJson(view);
}

// Copies a JSON array into a fixed-size float array when present.
template <size_t N>
void ReadFloatArray(const nlohmann::json &json, const char *key,
                    std::array<float, N> &out) {
  const auto value = json.value(key, nlohmann::json::array());
  if (!value.is_array())
    return;
  for (size_t i = 0; i < std::min(value.size(), out.size()); ++i)
    out[i] = value[i].get<float>();
}

// Copies a JSON array into a fixed-size boolean array when present.
template <size_t N>
void ReadBoolArray(const nlohmann::json &json, const char *key,
                   std::array<bool, N> &out) {
  const auto value = json.value(key, nlohmann::json::array());
  if (!value.is_array())
    return;
  for (size_t i = 0; i < std::min(value.size(), out.size()); ++i)
    out[i] = value[i].get<bool>();
}

// Parses the reusable 2D render state from JSON.
viewer2d::Viewer2DState RenderStateFromJson(const nlohmann::json &json) {
  layouts::Layout2DViewDefinition view;
  if (json.is_object()) {
    const auto camera = json.value("camera", nlohmann::json::array());
    if (camera.is_array() && camera.size() >= 6) {
      view.camera.offsetPixelsX = camera[0].get<float>();
      view.camera.offsetPixelsY = camera[1].get<float>();
      view.camera.zoom = camera[2].get<float>();
      view.camera.viewportWidth = camera[3].get<int>();
      view.camera.viewportHeight = camera[4].get<int>();
      view.camera.view = camera[5].get<int>();
    }
    const auto options = json.value("renderOptions", nlohmann::json::object());
    if (options.is_object()) {
      view.renderOptions.renderMode = options.value("renderMode", 2);
      view.renderOptions.darkMode = options.value("darkMode", true);
      if (options.contains("forceBottomViewForTopFixtures") &&
          !options["forceBottomViewForTopFixtures"].is_null()) {
        view.renderOptions.forceBottomViewForTopFixtures =
            options["forceBottomViewForTopFixtures"].get<bool>();
      }
      view.renderOptions.showGrid = options.value("showGrid", true);
      view.renderOptions.gridStyle = options.value("gridStyle", 0);
      view.renderOptions.gridColorR = options.value("gridColorR", 0.35f);
      view.renderOptions.gridColorG = options.value("gridColorG", 0.35f);
      view.renderOptions.gridColorB = options.value("gridColorB", 0.35f);
      view.renderOptions.gridDrawAbove = options.value("gridDrawAbove", false);
      view.renderOptions.gridSpacingMeters =
          options.value("gridSpacingMeters", 1.0f);
      view.renderOptions.showRuler = options.value("showRuler", true);
      ReadFloatArray(options, "rulerColorR", view.renderOptions.rulerColorR);
      ReadFloatArray(options, "rulerColorG", view.renderOptions.rulerColorG);
      ReadFloatArray(options, "rulerColorB", view.renderOptions.rulerColorB);
      ReadBoolArray(options, "showLabelName", view.renderOptions.showLabelName);
      ReadBoolArray(options, "showLabelId", view.renderOptions.showLabelId);
      ReadBoolArray(options, "showLabelDmx", view.renderOptions.showLabelDmx);
      view.renderOptions.labelFontSizeName =
          options.value("labelFontSizeName", 3.0f);
      view.renderOptions.labelFontSizeId =
          options.value("labelFontSizeId", 2.0f);
      view.renderOptions.labelFontSizeDmx =
          options.value("labelFontSizeDmx", 4.0f);
      ReadFloatArray(options, "labelOffsetDistance",
                     view.renderOptions.labelOffsetDistance);
      ReadFloatArray(options, "labelOffsetAngle",
                     view.renderOptions.labelOffsetAngle);
    }
    const auto layers = json.value("layers", nlohmann::json::object());
    if (layers.is_object()) {
      view.layers.hiddenLayers =
          layers.value("hiddenLayers", std::vector<std::string>{});
      view.layers.hiddenFixtureTypes =
          layers.value("hiddenFixtureTypes", std::vector<std::string>{});
    }
  }
  return viewer2d::FromLayoutDefinition(view);
}

// Serializes a symbol key into JSON.
nlohmann::json SymbolKeyToJson(const SymbolKey &key) {
  return {{"modelKey", key.modelKey},
          {"viewKind", static_cast<int>(key.viewKind)},
          {"styleVersion", key.styleVersion}};
}

// Parses a symbol key from JSON.
SymbolKey SymbolKeyFromJson(const nlohmann::json &json) {
  SymbolKey key;
  if (json.is_object()) {
    key.modelKey = json.value("modelKey", std::string{});
    key.viewKind = static_cast<SymbolViewKind>(json.value("viewKind", 0));
    key.styleVersion = json.value("styleVersion", 1u);
  }
  return key;
}

// Collects direct symbol instance references from a command buffer.
void CollectDirectSymbolIds(const CommandBuffer &buffer,
                            std::unordered_set<uint32_t> &symbolIds) {
  for (const auto &command : buffer.commands) {
    if (const auto *symbol = std::get_if<SymbolInstanceCommand>(&command))
      symbolIds.insert(symbol->symbolId);
  }
}

// Expands symbol references to include nested symbol-local command buffers.
void ExpandReferencedSymbolIds(const SymbolDefinitionSnapshot &snapshot,
                               std::unordered_set<uint32_t> &symbolIds) {
  bool changed = true;
  while (changed) {
    changed = false;
    std::vector<uint32_t> pending(symbolIds.begin(), symbolIds.end());
    for (uint32_t symbolId : pending) {
      const auto it = snapshot.find(symbolId);
      if (it == snapshot.end())
        continue;
      const size_t before = symbolIds.size();
      CollectDirectSymbolIds(it->second.localCommands, symbolIds);
      changed = changed || symbolIds.size() != before;
    }
  }
}

// Reports whether replay needs symbol definitions.
bool HasSymbolReferences(const CommandBuffer &buffer) {
  std::unordered_set<uint32_t> symbolIds;
  CollectDirectSymbolIds(buffer, symbolIds);
  return !symbolIds.empty();
}

// Counts commands in the main buffer and every referenced symbol definition.
size_t CountPersistentCommands(const CommandBuffer &buffer,
                               const SymbolDefinitionSnapshot *symbols) {
  size_t count = buffer.commands.size();
  if (!symbols)
    return count;
  std::unordered_set<uint32_t> symbolIds;
  CollectDirectSymbolIds(buffer, symbolIds);
  ExpandReferencedSymbolIds(*symbols, symbolIds);
  for (uint32_t symbolId : symbolIds) {
    const auto it = symbols->find(symbolId);
    if (it != symbols->end())
      count += it->second.localCommands.commands.size();
  }
  return count;
}

// Builds the minimal symbol snapshot needed for replay.
std::shared_ptr<const SymbolDefinitionSnapshot> FilterSymbolSnapshotForBuffer(
    const CommandBuffer &buffer,
    const std::shared_ptr<const SymbolDefinitionSnapshot> &snapshot) {
  if (!snapshot)
    return nullptr;
  std::unordered_set<uint32_t> symbolIds;
  CollectDirectSymbolIds(buffer, symbolIds);
  ExpandReferencedSymbolIds(*snapshot, symbolIds);
  auto filtered = std::make_shared<SymbolDefinitionSnapshot>();
  for (uint32_t symbolId : symbolIds) {
    const auto it = snapshot->find(symbolId);
    if (it != snapshot->end())
      (*filtered)[symbolId] = it->second;
  }
  return filtered;
}

// Serializes a symbol snapshot into JSON.
nlohmann::json SymbolSnapshotToJson(
    const std::shared_ptr<const SymbolDefinitionSnapshot> &snapshot) {
  nlohmann::json symbols = nlohmann::json::array();
  if (!snapshot)
    return symbols;
  for (const auto &[id, definition] : *snapshot) {
    symbols.push_back(
        {{"id", id},
         {"key", SymbolKeyToJson(definition.key)},
         {"symbolId", definition.symbolId},
         {"bounds",
          {definition.bounds.min.x, definition.bounds.min.y,
           definition.bounds.max.x, definition.bounds.max.y}},
         {"localCommands", CommandBufferToJson(definition.localCommands)}});
  }
  return symbols;
}

// Parses a symbol snapshot from JSON.
std::shared_ptr<const SymbolDefinitionSnapshot>
SymbolSnapshotFromJson(const nlohmann::json &json) {
  auto snapshot = std::make_shared<SymbolDefinitionSnapshot>();
  if (!json.is_array())
    return snapshot;
  for (const auto &entry : json) {
    if (!entry.is_object())
      continue;
    SymbolDefinition definition;
    definition.key = SymbolKeyFromJson(entry.value("key", nlohmann::json{}));
    definition.symbolId = entry.value("symbolId", entry.value("id", 0u));
    const auto bounds = entry.value("bounds", nlohmann::json::array());
    if (bounds.is_array() && bounds.size() >= 4) {
      definition.bounds.min.x = bounds[0].get<float>();
      definition.bounds.min.y = bounds[1].get<float>();
      definition.bounds.max.x = bounds[2].get<float>();
      definition.bounds.max.y = bounds[3].get<float>();
    }
    definition.localCommands =
        CommandBufferFromJson(entry.value("localCommands", nlohmann::json{}));
    (*snapshot)[entry.value("id", definition.symbolId)] = std::move(definition);
  }
  return snapshot;
}

// Reads a project archive cache entry into a UTF-8 string.
bool ReadCacheEntryFromProject(const std::string &projectPath,
                               std::string &outJson) {
  wxFileInputStream input(wxString::FromUTF8(projectPath));
  if (!input.IsOk())
    return false;
  wxZipInputStream zip(input);
  std::unique_ptr<wxZipEntry> entry;
  while ((entry.reset(zip.GetNextEntry())), entry) {
    if (entry->IsDir())
      continue;
    if (entry->GetName().ToStdString() != kLayoutViewCacheArchiveEntry)
      continue;
    std::array<char, 4096> buffer{};
    while (true) {
      zip.Read(buffer.data(), buffer.size());
      const size_t count = zip.LastRead();
      if (count == 0)
        break;
      outJson.append(buffer.data(), count);
    }
    return true;
  }
  return false;
}
} // namespace

namespace gui::layoutcache {

// Renders a persisted command-buffer cache into RGBA pixels for texture upload.
bool RenderCommandBufferCacheToRgba(const wxSize &renderSize,
                                    const CommandBuffer &buffer,
                                    const Viewer2DViewState &viewState,
                                    const SymbolDefinitionSnapshot *symbols,
                                    double renderZoom,
                                    std::vector<unsigned char> &pixels,
                                    int &width, int &height) {
  pixels.clear();
  width = 0;
  height = 0;
  if (buffer.commands.empty() || renderSize.GetWidth() <= 0 ||
      renderSize.GetHeight() <= 0)
    return false;

  Viewer2DViewState renderViewState = viewState;
  renderViewState.zoom *= static_cast<float>(renderZoom);
  renderViewState.viewportWidth = renderSize.GetWidth();
  renderViewState.viewportHeight = renderSize.GetHeight();

  wxImage image = RenderLayoutViewCommandBufferToImage(
      renderSize, buffer, renderViewState, symbols);
  if (!image.IsOk())
    return false;
  if (!image.HasAlpha())
    image.InitAlpha();

  width = image.GetWidth();
  height = image.GetHeight();
  const unsigned char *rgb = image.GetData();
  const unsigned char *alpha = image.GetAlpha();
  if (!rgb || width <= 0 || height <= 0)
    return false;

  pixels.resize(static_cast<size_t>(width) * static_cast<size_t>(height) * 4);
  for (int i = 0; i < width * height; ++i) {
    pixels[static_cast<size_t>(i) * 4] = rgb[i * 3];
    pixels[static_cast<size_t>(i) * 4 + 1] = rgb[i * 3 + 1];
    pixels[static_cast<size_t>(i) * 4 + 2] = rgb[i * 3 + 2];
    pixels[static_cast<size_t>(i) * 4 + 3] = alpha ? alpha[i] : 255;
  }
  return true;
}

} // namespace gui::layoutcache

// Collects cache data for every reusable 2D view in the active layout.
std::vector<ProjectSession::ArchiveResource>
LayoutViewerPanel::CollectPersistentViewCacheResources() const {
  if (currentLayout.name.empty() || currentLayout.view2dViews.empty())
    return {};

  size_t commandCount = 0;
  nlohmann::json views = nlohmann::json::array();
  for (const auto &view : currentLayout.view2dViews) {
    const auto cacheIt = viewCaches_.find(view.id);
    if (cacheIt == viewCaches_.end())
      continue;
    const ViewCache &cache = cacheIt->second;
    if (!cache.hasCapture || !cache.hasRenderState ||
        !cache.hasCaptureContentHash || cache.buffer.commands.empty())
      continue;

    const auto cacheSymbols =
        FilterSymbolSnapshotForBuffer(cache.buffer, cache.symbols);
    if (HasSymbolReferences(cache.buffer) && !cacheSymbols) {
      Logger::Instance().Log(Logger::Level::Info,
                             "Skipping one persistent layout view cache entry "
                             "because symbol snapshots are unavailable.");
      continue;
    }
    commandCount += CountPersistentCommands(cache.buffer, cacheSymbols.get());
    if (commandCount > kMaxPersistentCommandCount) {
      Logger::Instance().Log(
          Logger::Level::Info,
          "Skipping persistent layout view cache because command_count=" +
              std::to_string(commandCount) +
              " exceeds limit=" + std::to_string(kMaxPersistentCommandCount));
      return {};
    }

    views.push_back({{"viewId", view.id},
                     {"viewHash", StableJsonHash(ViewDefinitionToHashJson(view))},
                     {"legacyCaptureContentHash",
                      std::to_string(cache.captureContentHash)},
                     {"captureVersion", cache.captureVersion},
                     {"commandBuffer", CommandBufferToJson(cache.buffer)},
                     {"viewState", ViewStateToJson(cache.viewState)},
                     {"renderState", RenderStateToJson(cache.renderState)},
                     {"symbols", SymbolSnapshotToJson(cacheSymbols)}});
  }
  if (views.empty()) {
    Logger::Instance().Log(Logger::Level::Info,
                           "Skipping persistent layout view cache because no "
                           "2D view cache entries are ready to save.");
    return {};
  }

  const MvrScene &scene =
      GetDefaultGuiConfigServices().LegacyConfigManager().GetScene();
  const auto sourceAssetHash = ComputeSourceAssetHash(scene);
  if (!sourceAssetHash) {
    Logger::Instance().Log(Logger::Level::Info,
                           "Skipping persistent layout view cache because "
                           "source assets could not be hashed.");
    return {};
  }

  nlohmann::json document;
  document["schemaVersion"] = kLayoutViewCacheSchemaVersion;
  document["symbolGenerationVersion"] =
      symbol_cache::kCurrentPerastageSymbolFormatVersion;
  document["layoutName"] = currentLayout.name;
  document["sceneHash"] = StableJsonHash(SceneToHashJson(scene));
  document["sourceAssetHash"] = *sourceAssetHash;
  document["views"] = views;

  const std::string serialized = document.dump();
  if (serialized.size() > kMaxPersistentCacheBytes) {
    Logger::Instance().Log(
        Logger::Level::Info,
        "Skipping persistent layout view cache because serialized_bytes=" +
            std::to_string(serialized.size()) +
            " exceeds limit=" + std::to_string(kMaxPersistentCacheBytes));
    return {};
  }
  Logger::Instance().Log(
      Logger::Level::Info,
      "Saving persistent layout view cache for layout '" + currentLayout.name +
          "' with view_count=" + std::to_string(views.size()));
  return {{kLayoutViewCacheArchiveEntry,
           std::vector<std::uint8_t>(serialized.begin(), serialized.end())}};
}

// Loads persisted layout cache JSON from the project archive.
void LayoutViewerPanel::LoadPersistentViewCacheFromProject(
    const std::string &projectPath) {
  pendingPersistentViewCacheJson_.clear();
  if (projectPath.empty())
    return;
  std::string jsonText;
  if (ReadCacheEntryFromProject(projectPath, jsonText))
    pendingPersistentViewCacheJson_ = std::move(jsonText);
}

// Hydrates matching persistent cache data into the active layout view.
void LayoutViewerPanel::HydratePendingPersistentViewCache() {
  if (pendingPersistentViewCacheJson_.empty() || currentLayout.name.empty())
    return;
  try {
    const nlohmann::json document =
        nlohmann::json::parse(pendingPersistentViewCacheJson_);
    if (document.value("schemaVersion", 0) != kLayoutViewCacheSchemaVersion ||
        document.value("symbolGenerationVersion", 0) !=
            symbol_cache::kCurrentPerastageSymbolFormatVersion ||
        document.value("layoutName", std::string{}) != currentLayout.name) {
      pendingPersistentViewCacheJson_.clear();
      return;
    }

    const MvrScene &scene =
        GetDefaultGuiConfigServices().LegacyConfigManager().GetScene();
    const auto sourceAssetHash = ComputeSourceAssetHash(scene);
    if (!sourceAssetHash) {
      Logger::Instance().Log(
          Logger::Level::Info,
          "Ignoring layout view cache because source assets could not be hashed.");
      pendingPersistentViewCacheJson_.clear();
      return;
    }
    const std::string sceneHash = StableJsonHash(SceneToHashJson(scene));
    if (document.value("sceneHash", std::string{}) != sceneHash ||
        document.value("sourceAssetHash", std::string{}) != *sourceAssetHash) {
      Logger::Instance().Log(
          Logger::Level::Info,
          "Ignoring layout view cache because source assets or scene data changed.");
      pendingPersistentViewCacheJson_.clear();
      return;
    }

    const auto cachedViews = document.value("views", nlohmann::json::array());
    if (!cachedViews.is_array()) {
      pendingPersistentViewCacheJson_.clear();
      return;
    }

    int hydratedCount = 0;
    for (const auto &cachedView : cachedViews) {
      if (!cachedView.is_object())
        continue;
      const int viewId = cachedView.value("viewId", 0);
      const auto viewIt = std::find_if(
          currentLayout.view2dViews.begin(), currentLayout.view2dViews.end(),
          [viewId](const auto &entry) { return entry.id == viewId; });
      if (viewIt == currentLayout.view2dViews.end())
        continue;
      const std::string viewHash =
          StableJsonHash(ViewDefinitionToHashJson(*viewIt));
      if (cachedView.value("viewHash", std::string{}) != viewHash)
        continue;

      ViewCache &cache = GetViewCache(viewId);
      cache.buffer = CommandBufferFromJson(
          cachedView.value("commandBuffer", nlohmann::json{}));
      if (cache.buffer.commands.empty())
        continue;
      cache.viewState =
          ViewStateFromJson(cachedView.value("viewState", nlohmann::json{}));
      cache.renderState =
          RenderStateFromJson(cachedView.value("renderState", nlohmann::json{}));
      cache.symbols = SymbolSnapshotFromJson(
          cachedView.value("symbols", nlohmann::json::array()));
      cache.hasCapture = true;
      cache.hasRenderState = true;
      cache.captureContentHash = HashViewContent(*viewIt);
      cache.hasCaptureContentHash = true;
      cache.captureVersion = viewRenderVersion;
      cache.restoredFromPersistentCache = true;
      cache.captureInProgress = false;
      cache.renderDirty = true;
      cache.texture = 0;
      cache.pixelUnpackPbo = 0;
      cache.pboBytes = 0;
      cache.textureSize = wxSize(0, 0);
      cache.renderZoom = 0.0;
      ++hydratedCount;
    }

    if (hydratedCount > 0) {
      Logger::Instance().Log(
          Logger::Level::Info,
          "Hydrated persistent layout view cache for layout '" +
              currentLayout.name + "' with view_count=" +
              std::to_string(hydratedCount));
      renderDirty = true;
    } else {
      Logger::Instance().Log(
          Logger::Level::Info,
          "Ignoring layout view cache because no cached views matched the "
          "active layout.");
    }
    pendingPersistentViewCacheJson_.clear();
  } catch (const std::exception &ex) {
    Logger::Instance().Log(Logger::Level::Warn,
                           std::string("Ignoring layout view cache: ") +
                               ex.what());
    pendingPersistentViewCacheJson_.clear();
  }
}
