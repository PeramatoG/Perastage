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
#include "LayoutTemplateSerializer.h"

#include <array>
#include <exception>
#include <filesystem>
#include <unordered_set>

namespace layouts {
namespace {
namespace fs = std::filesystem;

constexpr std::array<const char *, 7> kEventTableFieldKeys = {
    "venue", "location", "date", "stage", "version", "design", "mail"};

constexpr int kFrameMinPosition = -200000;
constexpr int kFrameMaxPosition = 200000;
constexpr int kFrameMinSize = 0;
constexpr int kFrameMaxSize = 200000;
constexpr int kViewportMin = 0;
constexpr int kViewportMax = 200000;
constexpr int kCameraMinView = 0;
constexpr int kCameraMaxView = 2;
constexpr float kZoomMin = 0.01f;
constexpr float kZoomMax = 1000.0f;
constexpr float kColorMin = 0.0f;
constexpr float kColorMax = 1.0f;
constexpr float kFontSizeMin = 0.1f;
constexpr float kFontSizeMax = 1000.0f;
constexpr float kOffsetDistanceMin = -1000.0f;
constexpr float kOffsetDistanceMax = 1000.0f;
constexpr float kOffsetAngleMin = -3600.0f;
constexpr float kOffsetAngleMax = 3600.0f;
constexpr float kAspectRatioMin = 0.01f;
constexpr float kAspectRatioMax = 100.0f;
constexpr const char *kImagePathPolicy = "preserve_original_path";
thread_local LayoutTemplateImportReport *g_activeImportReport = nullptr;

struct ParseContext {
  LayoutTemplateImportReport *report = nullptr;
  std::string location;
};

void AddWarning(const ParseContext &ctx, const std::string &message) {
  if (ctx.report == nullptr)
    return;
  if (ctx.location.empty()) {
    ctx.report->warnings.push_back(message);
    return;
  }
  ctx.report->warnings.push_back(ctx.location + ": " + message);
}

ParseContext ChildContext(const ParseContext &ctx, const std::string &suffix) {
  if (ctx.location.empty())
    return ParseContext{ctx.report, suffix};
  return ParseContext{ctx.report, ctx.location + suffix};
}

template <typename T>
T ClampValue(T value, T minValue, T maxValue, const ParseContext &ctx,
             const char *fieldName) {
  if (value < minValue) {
    AddWarning(ctx, std::string("Clamped '") + fieldName + "' to minimum value.");
    return minValue;
  }
  if (value > maxValue) {
    AddWarning(ctx, std::string("Clamped '") + fieldName + "' to maximum value.");
    return maxValue;
  }
  return value;
}

std::string PageSizeToString(print::PageSize size) {
  switch (size) {
  case print::PageSize::A3:
    return "A3";
  case print::PageSize::A4:
  default:
    return "A4";
  }
}

print::PageSize PageSizeFromString(const std::string &value) {
  if (value == "A3")
    return print::PageSize::A3;
  return print::PageSize::A4;
}

nlohmann::json ToJson(const Layout2DViewDefinition &view) {
  const auto &frame = view.frame;
  const auto &camera = view.camera;
  const auto &options = view.renderOptions;
  const auto &layers = view.layers;
  return {
      {"id", view.id},
      {"zIndex", view.zIndex},
      {"frame",
       {{"x", frame.x}, {"y", frame.y}, {"width", frame.width}, {"height", frame.height}}},
      {"camera",
       {{"offsetPixelsX", camera.offsetPixelsX},
        {"offsetPixelsY", camera.offsetPixelsY},
        {"zoom", camera.zoom},
        {"viewportWidth", camera.viewportWidth},
        {"viewportHeight", camera.viewportHeight},
        {"view", camera.view}}},
      {"renderOptions",
       [&options]() {
         nlohmann::json renderOptions{{"renderMode", options.renderMode},
                                      {"darkMode", options.darkMode},
                                      {"showGrid", options.showGrid},
                                      {"gridStyle", options.gridStyle},
                                      {"gridColorR", options.gridColorR},
                                      {"gridColorG", options.gridColorG},
                                      {"gridColorB", options.gridColorB},
                                      {"gridDrawAbove", options.gridDrawAbove},
                                      {"showRuler", options.showRuler},
                                      {"showLabelName", options.showLabelName},
                                      {"showLabelId", options.showLabelId},
                                      {"showLabelDmx", options.showLabelDmx},
                                      {"labelFontSizeName", options.labelFontSizeName},
                                      {"labelFontSizeId", options.labelFontSizeId},
                                      {"labelFontSizeDmx", options.labelFontSizeDmx},
                                      {"labelOffsetDistance", options.labelOffsetDistance},
                                      {"labelOffsetAngle", options.labelOffsetAngle}};
         if (options.forceBottomViewForTopFixtures.has_value()) {
           renderOptions["forceBottomViewForTopFixtures"] =
               options.forceBottomViewForTopFixtures.value();
         }
         return renderOptions;
       }()},
      {"layers",
       {{"hiddenLayers", layers.hiddenLayers},
        {"hiddenFixtureTypes", layers.hiddenFixtureTypes}}},
  };
}

nlohmann::json ToJson(const LayoutLegendDefinition &legend) {
  const auto &frame = legend.frame;
  return {{"id", legend.id},
          {"zIndex", legend.zIndex},
          {"frame",
           {{"x", frame.x},
            {"y", frame.y},
            {"width", frame.width},
            {"height", frame.height}}}};
}

nlohmann::json ToJson(const LayoutEventTableDefinition &table) {
  const auto &frame = table.frame;
  nlohmann::json fields = nlohmann::json::object();
  for (size_t idx = 0; idx < table.fields.size(); ++idx)
    fields[kEventTableFieldKeys[idx]] = table.fields[idx];

  return {{"id", table.id},
          {"zIndex", table.zIndex},
          {"frame",
           {{"x", frame.x},
            {"y", frame.y},
            {"width", frame.width},
            {"height", frame.height}}},
          {"fields", std::move(fields)}};
}

nlohmann::json ToJson(const LayoutTextDefinition &text) {
  const auto &frame = text.frame;
  nlohmann::json data{
      {"id", text.id},
      {"zIndex", text.zIndex},
      {"frame",
       {{"x", frame.x},
        {"y", frame.y},
        {"width", frame.width},
        {"height", frame.height}}},
      {"text", text.text},
      {"solidBackground", text.solidBackground},
      {"drawFrame", text.drawFrame}};
  if (!text.richText.empty())
    data["richText"] = text.richText;
  return data;
}

nlohmann::json ToJson(const LayoutImageDefinition &image) {
  const auto &frame = image.frame;
  return {{"id", image.id},
          {"zIndex", image.zIndex},
          {"frame",
           {{"x", frame.x},
            {"y", frame.y},
            {"width", frame.width},
            {"height", frame.height}}},
          {"path", image.imagePath},
          {"aspectRatio", image.aspectRatio}};
}

void ReadBoolArray(const nlohmann::json &obj, const char *key,
                   std::array<bool, 3> &out, const ParseContext &ctx) {
  auto it = obj.find(key);
  if (it == obj.end())
    return;
  if (!it->is_array()) {
    AddWarning(ctx, std::string("Ignored '") + key + "' because it is not an array.");
    return;
  }
  if (it->size() != out.size())
    AddWarning(ctx, std::string("Expected '") + key + "' array size 3.");

  for (size_t idx = 0; idx < out.size() && idx < it->size(); ++idx) {
    const auto &entry = (*it)[idx];
    if (entry.is_boolean())
      out[idx] = entry.get<bool>();
  }
}

void ReadFloatArray(const nlohmann::json &obj, const char *key,
                    std::array<float, 3> &out, float minValue,
                    float maxValue, const ParseContext &ctx) {
  auto it = obj.find(key);
  if (it == obj.end())
    return;
  if (!it->is_array()) {
    AddWarning(ctx, std::string("Ignored '") + key + "' because it is not an array.");
    return;
  }
  if (it->size() != out.size())
    AddWarning(ctx, std::string("Expected '") + key + "' array size 3.");

  for (size_t idx = 0; idx < out.size() && idx < it->size(); ++idx) {
    const auto &entry = (*it)[idx];
    if (entry.is_number()) {
      out[idx] = ClampValue(entry.get<float>(), minValue, maxValue, ctx, key);
    }
  }
}

void ReadFrame(const nlohmann::json &obj, Layout2DViewFrame &frame,
               const ParseContext &ctx) {
  if (auto it = obj.find("x"); it != obj.end() && it->is_number_integer())
    frame.x = ClampValue(it->get<int>(), kFrameMinPosition, kFrameMaxPosition,
                         ctx, "frame.x");
  if (auto it = obj.find("y"); it != obj.end() && it->is_number_integer())
    frame.y = ClampValue(it->get<int>(), kFrameMinPosition, kFrameMaxPosition,
                         ctx, "frame.y");
  if (auto it = obj.find("width"); it != obj.end() && it->is_number_integer())
    frame.width = ClampValue(it->get<int>(), kFrameMinSize, kFrameMaxSize, ctx,
                             "frame.width");
  if (auto it = obj.find("height"); it != obj.end() && it->is_number_integer())
    frame.height = ClampValue(it->get<int>(), kFrameMinSize, kFrameMaxSize, ctx,
                              "frame.height");
}

void ReadCamera(const nlohmann::json &obj, Layout2DViewCameraState &camera,
                const ParseContext &ctx) {
  if (auto it = obj.find("offsetPixelsX"); it != obj.end() && it->is_number())
    camera.offsetPixelsX = it->get<float>();
  if (auto it = obj.find("offsetPixelsY"); it != obj.end() && it->is_number())
    camera.offsetPixelsY = it->get<float>();
  if (auto it = obj.find("zoom"); it != obj.end() && it->is_number())
    camera.zoom = ClampValue(it->get<float>(), kZoomMin, kZoomMax, ctx,
                             "camera.zoom");
  if (auto it = obj.find("viewportWidth");
      it != obj.end() && it->is_number_integer())
    camera.viewportWidth = ClampValue(it->get<int>(), kViewportMin,
                                      kViewportMax, ctx, "camera.viewportWidth");
  if (auto it = obj.find("viewportHeight");
      it != obj.end() && it->is_number_integer())
    camera.viewportHeight = ClampValue(it->get<int>(), kViewportMin,
                                       kViewportMax, ctx,
                                       "camera.viewportHeight");
  if (auto it = obj.find("view"); it != obj.end() && it->is_number_integer())
    camera.view = ClampValue(it->get<int>(), kCameraMinView, kCameraMaxView, ctx,
                             "camera.view");
}

void ReadRenderOptions(const nlohmann::json &obj,
                       Layout2DViewRenderOptions &options,
                       const ParseContext &ctx) {
  if (auto it = obj.find("renderMode"); it != obj.end() && it->is_number_integer())
    options.renderMode = it->get<int>();
  if (auto it = obj.find("darkMode"); it != obj.end() && it->is_boolean())
    options.darkMode = it->get<bool>();
  if (auto it = obj.find("forceBottomViewForTopFixtures");
      it != obj.end() && it->is_boolean())
    options.forceBottomViewForTopFixtures = it->get<bool>();
  if (auto it = obj.find("showGrid"); it != obj.end() && it->is_boolean())
    options.showGrid = it->get<bool>();
  if (auto it = obj.find("gridStyle"); it != obj.end() && it->is_number_integer())
    options.gridStyle = it->get<int>();
  if (auto it = obj.find("gridColorR"); it != obj.end() && it->is_number())
    options.gridColorR = ClampValue(it->get<float>(), kColorMin, kColorMax, ctx,
                                    "renderOptions.gridColorR");
  if (auto it = obj.find("gridColorG"); it != obj.end() && it->is_number())
    options.gridColorG = ClampValue(it->get<float>(), kColorMin, kColorMax, ctx,
                                    "renderOptions.gridColorG");
  if (auto it = obj.find("gridColorB"); it != obj.end() && it->is_number())
    options.gridColorB = ClampValue(it->get<float>(), kColorMin, kColorMax, ctx,
                                    "renderOptions.gridColorB");
  if (auto it = obj.find("gridDrawAbove"); it != obj.end() && it->is_boolean())
    options.gridDrawAbove = it->get<bool>();
  if (auto it = obj.find("showRuler"); it != obj.end() && it->is_boolean())
    options.showRuler = it->get<bool>();

  ReadBoolArray(obj, "showLabelName", options.showLabelName, ctx);
  ReadBoolArray(obj, "showLabelId", options.showLabelId, ctx);
  ReadBoolArray(obj, "showLabelDmx", options.showLabelDmx, ctx);

  if (auto it = obj.find("labelFontSizeName"); it != obj.end() && it->is_number())
    options.labelFontSizeName = ClampValue(it->get<float>(), kFontSizeMin,
                                           kFontSizeMax, ctx,
                                           "renderOptions.labelFontSizeName");
  if (auto it = obj.find("labelFontSizeId"); it != obj.end() && it->is_number())
    options.labelFontSizeId = ClampValue(it->get<float>(), kFontSizeMin,
                                         kFontSizeMax, ctx,
                                         "renderOptions.labelFontSizeId");
  if (auto it = obj.find("labelFontSizeDmx"); it != obj.end() && it->is_number())
    options.labelFontSizeDmx = ClampValue(it->get<float>(), kFontSizeMin,
                                          kFontSizeMax, ctx,
                                          "renderOptions.labelFontSizeDmx");

  ReadFloatArray(obj, "labelOffsetDistance", options.labelOffsetDistance,
                 kOffsetDistanceMin, kOffsetDistanceMax, ctx);
  ReadFloatArray(obj, "labelOffsetAngle", options.labelOffsetAngle,
                 kOffsetAngleMin, kOffsetAngleMax, ctx);
}

void ReadLayers(const nlohmann::json &obj, Layout2DViewLayers &layers,
                const ParseContext &ctx) {
  layers.hiddenLayers.clear();
  layers.hiddenFixtureTypes.clear();

  if (auto it = obj.find("hiddenLayers"); it != obj.end()) {
    if (!it->is_array()) {
      AddWarning(ctx, "Ignored 'hiddenLayers' because it is not an array.");
    } else {
      std::unordered_set<std::string> dedupe;
      for (const auto &entry : *it) {
        if (!entry.is_string())
          continue;
        std::string layerName = entry.get<std::string>();
        if (layerName.empty())
          continue;
        if (dedupe.insert(layerName).second)
          layers.hiddenLayers.push_back(std::move(layerName));
      }
    }
  }

  if (auto it = obj.find("hiddenFixtureTypes"); it != obj.end()) {
    if (!it->is_array()) {
      AddWarning(ctx, "Ignored 'hiddenFixtureTypes' because it is not an array.");
    } else {
      std::unordered_set<std::string> dedupe;
      for (const auto &entry : *it) {
        if (!entry.is_string())
          continue;
        std::string typeName = entry.get<std::string>();
        if (typeName.empty())
          continue;
        if (dedupe.insert(typeName).second)
          layers.hiddenFixtureTypes.push_back(std::move(typeName));
      }
    }
  }
}

bool ParseLayout2DView(const nlohmann::json &value, Layout2DViewDefinition &out,
                       const ParseContext &ctx) {
  if (!value.is_object())
    return false;

  if (auto idIt = value.find("id"); idIt != value.end() && idIt->is_number_integer())
    out.id = idIt->get<int>();
  if (auto zIt = value.find("zIndex"); zIt != value.end() && zIt->is_number_integer())
    out.zIndex = zIt->get<int>();

  auto frameIt = value.find("frame");
  if (frameIt != value.end() && frameIt->is_object())
    ReadFrame(*frameIt, out.frame, ctx);

  auto cameraIt = value.find("camera");
  if (cameraIt != value.end() && cameraIt->is_object())
    ReadCamera(*cameraIt, out.camera, ctx);

  auto renderIt = value.find("renderOptions");
  if (renderIt != value.end() && renderIt->is_object())
    ReadRenderOptions(*renderIt, out.renderOptions, ctx);

  auto layersIt = value.find("layers");
  if (layersIt != value.end() && layersIt->is_object())
    ReadLayers(*layersIt, out.layers, ctx);
  return true;
}

bool ParseLayoutLegend(const nlohmann::json &value, LayoutLegendDefinition &out,
                       const ParseContext &ctx) {
  if (!value.is_object())
    return false;

  if (auto idIt = value.find("id"); idIt != value.end() && idIt->is_number_integer())
    out.id = idIt->get<int>();
  if (auto zIt = value.find("zIndex"); zIt != value.end() && zIt->is_number_integer())
    out.zIndex = zIt->get<int>();
  auto frameIt = value.find("frame");
  if (frameIt != value.end() && frameIt->is_object())
    ReadFrame(*frameIt, out.frame, ctx);
  return true;
}

void ReadEventTableFields(const nlohmann::json &obj,
                          std::array<std::string, 7> &out) {
  if (!obj.is_object())
    return;

  for (size_t idx = 0; idx < out.size(); ++idx) {
    const auto *key = kEventTableFieldKeys[idx];
    if (auto it = obj.find(key); it != obj.end() && it->is_string())
      out[idx] = it->get<std::string>();
  }
}

bool ParseLayoutEventTable(const nlohmann::json &value,
                           LayoutEventTableDefinition &out,
                           const ParseContext &ctx) {
  if (!value.is_object())
    return false;

  if (auto idIt = value.find("id"); idIt != value.end() && idIt->is_number_integer())
    out.id = idIt->get<int>();
  if (auto zIt = value.find("zIndex"); zIt != value.end() && zIt->is_number_integer())
    out.zIndex = zIt->get<int>();
  auto frameIt = value.find("frame");
  if (frameIt != value.end() && frameIt->is_object())
    ReadFrame(*frameIt, out.frame, ctx);
  auto fieldsIt = value.find("fields");
  if (fieldsIt != value.end())
    ReadEventTableFields(*fieldsIt, out.fields);
  return true;
}

bool ParseLayoutText(const nlohmann::json &value, LayoutTextDefinition &out,
                     const ParseContext &ctx) {
  if (!value.is_object())
    return false;

  if (auto idIt = value.find("id"); idIt != value.end() && idIt->is_number_integer())
    out.id = idIt->get<int>();
  if (auto zIt = value.find("zIndex"); zIt != value.end() && zIt->is_number_integer())
    out.zIndex = zIt->get<int>();
  auto frameIt = value.find("frame");
  if (frameIt != value.end() && frameIt->is_object())
    ReadFrame(*frameIt, out.frame, ctx);
  if (auto textIt = value.find("text"); textIt != value.end() && textIt->is_string())
    out.text = textIt->get<std::string>();
  if (auto richIt = value.find("richText"); richIt != value.end() && richIt->is_string())
    out.richText = richIt->get<std::string>();
  if (auto bgIt = value.find("solidBackground");
      bgIt != value.end() && bgIt->is_boolean())
    out.solidBackground = bgIt->get<bool>();
  if (auto drawFrameIt = value.find("drawFrame");
      drawFrameIt != value.end() && drawFrameIt->is_boolean())
    out.drawFrame = drawFrameIt->get<bool>();
  return true;
}

bool ParseLayoutImage(const nlohmann::json &value, LayoutImageDefinition &out,
                      const ParseContext &ctx) {
  if (!value.is_object())
    return false;

  if (auto idIt = value.find("id"); idIt != value.end() && idIt->is_number_integer())
    out.id = idIt->get<int>();
  if (auto zIt = value.find("zIndex"); zIt != value.end() && zIt->is_number_integer())
    out.zIndex = zIt->get<int>();
  auto frameIt = value.find("frame");
  if (frameIt != value.end() && frameIt->is_object())
    ReadFrame(*frameIt, out.frame, ctx);
  if (auto pathIt = value.find("path"); pathIt != value.end() && pathIt->is_string()) {
    out.imagePath = pathIt->get<std::string>();
    if (!out.imagePath.empty() && !fs::exists(fs::path(out.imagePath)))
      AddWarning(ctx, "Image path does not exist. Path kept as-is by policy.");
  }
  if (auto ratioIt = value.find("aspectRatio");
      ratioIt != value.end() && ratioIt->is_number())
    out.aspectRatio = ClampValue(ratioIt->get<float>(), kAspectRatioMin,
                                 kAspectRatioMax, ctx, "image.aspectRatio");
  return true;
}

bool ParseLegacySingleView(const nlohmann::json &value, LayoutDefinition &out,
                           const ParseContext &ctx) {
  const auto viewStateIt = value.find("view2dState");
  if (viewStateIt == value.end() || !viewStateIt->is_object())
    return true;

  const auto &viewObj = *viewStateIt;
  Layout2DViewDefinition view;
  Layout2DViewCameraState camera;
  Layout2DViewRenderOptions options;
  Layout2DViewLayers layers;

  ReadCamera(viewObj, camera, ChildContext(ctx, ".view2dState"));
  ReadRenderOptions(viewObj, options, ChildContext(ctx, ".view2dState"));

  if (auto it = viewObj.find("hiddenLayers"); it != viewObj.end() && it->is_array()) {
    for (const auto &entry : *it) {
      if (entry.is_string())
        layers.hiddenLayers.push_back(entry.get<std::string>());
    }
  }
  if (auto it = viewObj.find("hiddenFixtureTypes");
      it != viewObj.end() && it->is_array()) {
    for (const auto &entry : *it) {
      if (entry.is_string())
        layers.hiddenFixtureTypes.push_back(entry.get<std::string>());
    }
  }

  Layout2DViewFrame frame;
  if (auto it = viewObj.find("frameWidth"); it != viewObj.end() && it->is_number_integer())
    frame.width = ClampValue(it->get<int>(), kFrameMinSize, kFrameMaxSize, ctx,
                             "view2dState.frameWidth");
  if (auto it = viewObj.find("frameHeight");
      it != viewObj.end() && it->is_number_integer())
    frame.height = ClampValue(it->get<int>(), kFrameMinSize, kFrameMaxSize, ctx,
                              "view2dState.frameHeight");

  view.frame = frame;
  view.camera = camera;
  view.renderOptions = options;
  view.layers = std::move(layers);
  out.view2dViews.push_back(std::move(view));
  return true;
}

template <typename T, typename ParseFn>
void ParseElementArray(const nlohmann::json &layoutValue, const char *key,
                       std::vector<T> &target,
                       LayoutTemplateImportReport::ElementStats *stats,
                       bool &hasZIndex, const ParseContext &ctx,
                       ParseFn parseFn) {
  target.clear();
  auto entriesIt = layoutValue.find(key);
  if (entriesIt == layoutValue.end())
    return;
  if (!entriesIt->is_array()) {
    AddWarning(ctx, std::string("Ignored '") + key + "' because it is not an array.");
    return;
  }

  for (size_t idx = 0; idx < entriesIt->size(); ++idx) {
    const auto &entry = (*entriesIt)[idx];
    ParseContext entryCtx = ChildContext(ctx, std::string(".") + key + "[" +
                                                  std::to_string(idx) + "]");
    if (entry.is_object() && entry.find("zIndex") != entry.end())
      hasZIndex = true;

    T element;
    if (!parseFn(entry, element, entryCtx)) {
      if (stats != nullptr)
        ++stats->skipped;
      AddWarning(entryCtx, "Skipped element because it is not a valid object.");
      continue;
    }

    if (stats != nullptr)
      ++stats->imported;

    bool replaced = false;
    if (element.id > 0) {
      for (auto &existing : target) {
        if (existing.id == element.id) {
          existing = element;
          replaced = true;
          break;
        }
      }
    }
    if (!replaced)
      target.push_back(std::move(element));
  }
}

} // namespace

nlohmann::json ToJson(const LayoutDefinition &layout) {
  nlohmann::json data{{"name", layout.name},
                      {"pageSetup",
                       {{"pageSize", PageSizeToString(layout.pageSetup.pageSize)},
                        {"landscape", layout.pageSetup.landscape}}},
                      {"resources",
                       {{"imagePathPolicy", kImagePathPolicy}}}};

  if (!layout.view2dViews.empty()) {
    data["view2dViews"] = nlohmann::json::array();
    for (const auto &view : layout.view2dViews)
      data["view2dViews"].push_back(ToJson(view));
  }
  if (!layout.legendViews.empty()) {
    data["legendViews"] = nlohmann::json::array();
    for (const auto &legend : layout.legendViews)
      data["legendViews"].push_back(ToJson(legend));
  }
  if (!layout.eventTables.empty()) {
    data["eventTables"] = nlohmann::json::array();
    for (const auto &table : layout.eventTables)
      data["eventTables"].push_back(ToJson(table));
  }
  if (!layout.textViews.empty()) {
    data["textViews"] = nlohmann::json::array();
    for (const auto &text : layout.textViews)
      data["textViews"].push_back(ToJson(text));
  }
  if (!layout.imageViews.empty()) {
    data["imageViews"] = nlohmann::json::array();
    for (const auto &image : layout.imageViews)
      data["imageViews"].push_back(ToJson(image));
  }
  return data;
}

bool FromJson(const nlohmann::json &value, LayoutDefinition &out,
              std::string *error) {
  try {
    if (!value.is_object()) {
      if (error != nullptr)
        *error = "Layout entry must be an object.";
      return false;
    }

    const auto nameIt = value.find("name");
    if (nameIt == value.end() || !nameIt->is_string()) {
      if (error != nullptr)
        *error = "Layout entry is missing a valid 'name'.";
      return false;
    }

    out.name = nameIt->get<std::string>();
    if (out.name.empty()) {
      if (error != nullptr)
        *error = "Layout name cannot be empty.";
      return false;
    }

    if (const auto pageSetupIt = value.find("pageSetup");
        pageSetupIt != value.end() && pageSetupIt->is_object()) {
      const auto sizeIt = pageSetupIt->find("pageSize");
      if (sizeIt != pageSetupIt->end() && sizeIt->is_string())
        out.pageSetup.pageSize = PageSizeFromString(sizeIt->get<std::string>());
      else
        out.pageSetup.pageSize = print::PageSize::A4;

      const auto landscapeIt = pageSetupIt->find("landscape");
      out.pageSetup.landscape =
          landscapeIt != pageSetupIt->end() && landscapeIt->is_boolean()
              ? landscapeIt->get<bool>()
              : false;
    } else {
      const auto sizeIt = value.find("pageSize");
      out.pageSetup.pageSize =
          (sizeIt != value.end() && sizeIt->is_string())
              ? PageSizeFromString(sizeIt->get<std::string>())
              : print::PageSize::A4;

      const auto landscapeIt = value.find("landscape");
      out.pageSetup.landscape =
          landscapeIt != value.end() && landscapeIt->is_boolean()
              ? landscapeIt->get<bool>()
              : false;
    }

    bool hasZIndex = false;
    ParseContext rootCtx{g_activeImportReport, "layout[" + out.name + "]"};

    ParseElementArray(value, "view2dViews", out.view2dViews, nullptr, hasZIndex,
                      rootCtx, ParseLayout2DView);
    ParseElementArray(value, "legendViews", out.legendViews, nullptr, hasZIndex,
                      rootCtx, ParseLayoutLegend);
    ParseElementArray(value, "eventTables", out.eventTables, nullptr, hasZIndex,
                      rootCtx, ParseLayoutEventTable);
    ParseElementArray(value, "textViews", out.textViews, nullptr, hasZIndex,
                      rootCtx, ParseLayoutText);
    ParseElementArray(value, "imageViews", out.imageViews, nullptr, hasZIndex,
                      rootCtx, ParseLayoutImage);

    if (out.view2dViews.empty()) {
      if (!ParseLegacySingleView(value, out, rootCtx)) {
        if (error != nullptr)
          *error = "Failed to parse legacy 2D view state.";
        return false;
      }
    }

    if (!hasZIndex) {
      int nextZ = 0;
      for (auto &view : out.view2dViews)
        view.zIndex = nextZ++;
      for (auto &legend : out.legendViews)
        legend.zIndex = nextZ++;
      for (auto &table : out.eventTables)
        table.zIndex = nextZ++;
      for (auto &text : out.textViews)
        text.zIndex = nextZ++;
      for (auto &image : out.imageViews)
        image.zIndex = nextZ++;
    }

    return true;
  } catch (const std::exception &ex) {
    if (error != nullptr)
      *error = ex.what();
    return false;
  }
}

nlohmann::json ToTemplateDocument(const std::vector<LayoutDefinition> &layouts) {
  nlohmann::json serializedLayouts = nlohmann::json::array();
  for (const auto &layout : layouts)
    serializedLayouts.push_back(ToJson(layout));

  return {{"schemaVersion", kLayoutTemplateSchemaVersion},
          {"layouts", std::move(serializedLayouts)},
          {"resources", {{"imagePathPolicy", kImagePathPolicy}}}};
}

bool FromTemplateDocument(const nlohmann::json &value,
                          std::vector<LayoutDefinition> &layouts,
                          LayoutTemplateImportReport *report,
                          std::string *error) {
  try {
    const nlohmann::json *layoutArray = nullptr;
    if (value.is_array()) {
      layoutArray = &value;
    } else if (value.is_object()) {
      const auto schemaIt = value.find("schemaVersion");
      if (schemaIt != value.end() &&
          (!schemaIt->is_number_integer() || schemaIt->get<int>() < 1)) {
        if (error != nullptr)
          *error = "Invalid schemaVersion in layout template document.";
        if (report != nullptr)
          report->errors.push_back(*error);
        return false;
      }
      auto layoutsIt = value.find("layouts");
      if (layoutsIt == value.end() || !layoutsIt->is_array()) {
        if (error != nullptr)
          *error = "Layout template document must contain a 'layouts' array.";
        if (report != nullptr)
          report->errors.push_back(*error);
        return false;
      }
      layoutArray = &(*layoutsIt);
    } else {
      if (error != nullptr)
        *error = "Layout template document must be an object or array.";
      if (report != nullptr)
        report->errors.push_back(*error);
      return false;
    }

    std::vector<LayoutDefinition> loaded;
    LayoutTemplateImportReport *previousReport = g_activeImportReport;
    g_activeImportReport = report;
    for (size_t idx = 0; idx < layoutArray->size(); ++idx) {
      const auto &entry = (*layoutArray)[idx];
      LayoutDefinition layout;
      std::string layoutError;
      if (!FromJson(entry, layout, &layoutError)) {
        if (report != nullptr) {
          ++report->layouts.skipped;
          report->errors.push_back("layouts[" + std::to_string(idx) + "]: " +
                                   layoutError);
        }
        continue;
      }

      bool duplicate = false;
      for (const auto &existing : loaded) {
        if (existing.name == layout.name) {
          duplicate = true;
          break;
        }
      }
      if (duplicate) {
        if (report != nullptr) {
          ++report->layouts.skipped;
          report->warnings.push_back("layouts[" + std::to_string(idx) +
                                     "]: Duplicate layout name skipped.");
        }
        continue;
      }

      if (report != nullptr) {
        ++report->layouts.imported;
        report->view2dViews.imported += static_cast<int>(layout.view2dViews.size());
        report->legendViews.imported += static_cast<int>(layout.legendViews.size());
        report->eventTables.imported += static_cast<int>(layout.eventTables.size());
        report->textViews.imported += static_cast<int>(layout.textViews.size());
        report->imageViews.imported += static_cast<int>(layout.imageViews.size());
      }
      loaded.push_back(std::move(layout));
    }

    layouts = std::move(loaded);
    g_activeImportReport = previousReport;
    return true;
  } catch (const std::exception &ex) {
    g_activeImportReport = nullptr;
    if (error != nullptr)
      *error = ex.what();
    if (report != nullptr)
      report->errors.push_back(*error);
    return false;
  }
}

bool FromTemplateDocument(const nlohmann::json &value,
                          std::vector<LayoutDefinition> &layouts,
                          std::string *error) {
  return FromTemplateDocument(value, layouts, nullptr, error);
}

} // namespace layouts
