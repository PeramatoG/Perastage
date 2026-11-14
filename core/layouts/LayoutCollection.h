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

#include <array>
#include <optional>
#include <string>
#include <vector>

#include "PageSetup.h"
#include "PhysicalViewport.h"

namespace layouts {

struct Layout2DViewFrame {
  int x = 0;
  int y = 0;
  int width = 0;
  int height = 0;
  operator ViewportFramePoints() const { return {double(x), double(y), double(width), double(height)}; }
};

struct Layout2DViewCameraState {
  float offsetPixelsX = 0.0f;
  float offsetPixelsY = 0.0f;
  float zoom = 1.0f;
  int viewportWidth = 0;
  int viewportHeight = 0;
  int view = 0;
};

struct Layout2DViewRenderOptions {
  int renderMode = 2;
  bool darkMode = true;
  std::optional<bool> forceBottomViewForTopFixtures;
  bool showGrid = true;
  int gridStyle = 0;
  float gridColorR = 0.35f;
  float gridColorG = 0.35f;
  float gridColorB = 0.35f;
  bool gridDrawAbove = false;
  float gridSpacingMeters = 1.0f;
  bool showRuler = true;
  std::array<float, 3> rulerColorR = {0.0f, 0.0f, 0.0f};
  std::array<float, 3> rulerColorG = {0.0f, 0.0f, 0.0f};
  std::array<float, 3> rulerColorB = {0.0f, 0.0f, 0.0f};

  std::array<bool, 3> showLabelName = {true, true, true};
  std::array<bool, 3> showLabelId = {true, true, true};
  std::array<bool, 3> showLabelDmx = {true, true, true};
  float labelFontSizeName = 3.0f;
  float labelFontSizeId = 2.0f;
  float labelFontSizeDmx = 4.0f;
  std::array<float, 3> labelOffsetDistance = {0.5f, 0.5f, 0.5f};
  std::array<float, 3> labelOffsetAngle = {180.0f, 180.0f, 180.0f};
};

struct Layout2DViewLayers {
  std::vector<std::string> hiddenLayers;
  std::vector<std::string> hiddenFixtureTypes;
};

struct Layout2DViewDefinition {
  int id = 0;
  int zIndex = 0;
  Layout2DViewFrame frame;
  bool drawFrame = true;
  // Fractional point residuals isolate precision from the shared integer frame
  // adapter used by the five existing element interaction paths.
  ViewportFramePoints frameResidual;
  ViewportScaleMode scaleMode = ViewportScaleMode::Free;
  std::optional<PhysicalViewport> physicalScale;
  Layout2DViewCameraState camera;
  Layout2DViewRenderOptions renderOptions;
  Layout2DViewLayers layers;
};

inline ViewportFramePoints ResolveViewportFrame(const Layout2DViewDefinition &v) {
  return {v.frame.x + v.frameResidual.x, v.frame.y + v.frameResidual.y,
          v.frame.width + v.frameResidual.width, v.frame.height + v.frameResidual.height};
}
inline void SetViewportFrame(Layout2DViewDefinition &v, ViewportFramePoints f) {
  ValidateViewportFrame(f);
  v.frame = {static_cast<int>(std::round(f.x)), static_cast<int>(std::round(f.y)),
             static_cast<int>(std::round(f.width)), static_cast<int>(std::round(f.height))};
  v.frameResidual = {f.x-v.frame.x, f.y-v.frame.y,
                      f.width-v.frame.width, f.height-v.frame.height};
}
inline void ValidateViewport(const Layout2DViewDefinition &v) {
  if (v.scaleMode != ViewportScaleMode::Free && v.scaleMode != ViewportScaleMode::Fixed)
    throw std::invalid_argument("Unsupported Layout viewport scale mode.");
  if (v.physicalScale) ValidatePhysicalViewport(*v.physicalScale);
  if (v.scaleMode == ViewportScaleMode::Fixed) {
    if (!v.physicalScale) throw std::invalid_argument("Fixed viewport is missing its drawing scale.");
    if (v.camera.view < 0 || v.camera.view > 2)
      throw std::invalid_argument("Fixed scale supports Top, Front and Side orientations. Select one of these views.");
    ValidateViewportFrame(ResolveViewportFrame(v));
  }
  const auto &r = v.frameResidual;
  if (r.x != 0 || r.y != 0 || r.width != 0 || r.height != 0)
    ValidateViewportFrame(ResolveViewportFrame(v));
  for (double n : {r.x, r.y, r.width, r.height})
    if (!std::isfinite(n) || std::abs(n) > 0.5)
      throw std::invalid_argument("Invalid fractional frame point residual.");
}

struct LayoutLegendDefinition {
  struct ItemSettings {
    std::string typeName;
    bool visible = true;
    bool showBottomSymbol = true;
    bool showFrontSymbol = true;
    bool showSideSymbol = false;
    std::string customName;
  };

  int id = 0;
  int zIndex = 0;
  Layout2DViewFrame frame;
  bool showChannelColumn = true;
  std::vector<ItemSettings> itemSettings;
};

struct LayoutEventTableDefinition {
  int id = 0;
  int zIndex = 0;
  Layout2DViewFrame frame;
  std::array<std::string, 7> fields;
};

struct LayoutTextDefinition {
  int id = 0;
  int zIndex = 0;
  Layout2DViewFrame frame;
  std::string text;
  std::string richText;
  bool solidBackground = true;
  bool drawFrame = true;
};

struct LayoutImageDefinition {
  int id = 0;
  int zIndex = 0;
  Layout2DViewFrame frame;
  std::string imagePath;
  std::string originalImagePath;
  std::string projectResourcePath;
  float aspectRatio = 1.0f;
};

struct LayoutDefinition {
  std::string name;
  print::PageSetup pageSetup;
  std::vector<Layout2DViewDefinition> view2dViews;
  std::vector<LayoutLegendDefinition> legendViews;
  std::vector<LayoutEventTableDefinition> eventTables;
  std::vector<LayoutTextDefinition> textViews;
  std::vector<LayoutImageDefinition> imageViews;
};

class LayoutCollection {
public:
  LayoutCollection();

  const std::vector<LayoutDefinition> &Items() const;
  std::vector<LayoutDefinition> &Items();
  std::size_t Count() const;

  bool AddLayout(const LayoutDefinition &layout);
  bool RenameLayout(const std::string &currentName,
                    const std::string &newName);
  bool RemoveLayout(const std::string &name);
  bool SetLayoutOrientation(const std::string &name, bool landscape);
  bool UpdateLayout2DView(const std::string &name,
                          const Layout2DViewDefinition &view);
  bool RemoveLayout2DView(const std::string &name, int viewId);
  bool MoveLayout2DView(const std::string &name, int viewId, bool toFront);
  bool UpdateLayoutLegend(const std::string &name,
                          const LayoutLegendDefinition &legend);
  bool RemoveLayoutLegend(const std::string &name, int legendId);
  bool MoveLayoutLegend(const std::string &name, int legendId, bool toFront);
  bool UpdateLayoutEventTable(const std::string &name,
                              const LayoutEventTableDefinition &table);
  bool RemoveLayoutEventTable(const std::string &name, int tableId);
  bool MoveLayoutEventTable(const std::string &name, int tableId,
                            bool toFront);
  bool UpdateLayoutText(const std::string &name,
                        const LayoutTextDefinition &text);
  bool RemoveLayoutText(const std::string &name, int textId);
  bool MoveLayoutText(const std::string &name, int textId, bool toFront);
  bool UpdateLayoutImage(const std::string &name,
                         const LayoutImageDefinition &image);
  bool RemoveLayoutImage(const std::string &name, int imageId);
  bool MoveLayoutImage(const std::string &name, int imageId, bool toFront);

  void ReplaceAll(std::vector<LayoutDefinition> layouts);

private:
  static LayoutDefinition DefaultLayout();
  bool NameExists(const std::string &name,
                  const std::string &ignoreName = {}) const;

  std::vector<LayoutDefinition> layouts;
};

} // namespace layouts
