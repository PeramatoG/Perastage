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
#include "layoutviewerpanel.h"

#include <algorithm>
#include <functional>
#include <vector>

// Include GLEW or other OpenGL loader first if present
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#ifdef __APPLE__
#include <OpenGL/gl.h>
#include <OpenGL/glu.h>
#else
#include <GL/gl.h>
#include <GL/glu.h>
#endif

#include "LayoutManager.h"
#include "configmanager.h"
#include "guiconfigservices.h"
#include "layoutlegendeditdialog.h"
#include "layoutlegenditems.h"
#include "layoutlegendrenderer.h"
#include "layoutlegendcachepolicy.h"
#include "mainwindow.h"
#include "project_cache_validation.h"


layouts::LayoutLegendDefinition *LayoutViewerPanel::GetSelectedLegend() {
  if (currentLayout.legendViews.empty())
    return nullptr;
  if (selectionState_.Current().kind == LayoutElementKind::Legend &&
      selectionState_.Current().id >= 0) {
    for (auto &legend : currentLayout.legendViews) {
      if (legend.id == selectionState_.Current().id)
        return &legend;
    }
  }
  return &currentLayout.legendViews.front();
}

const layouts::LayoutLegendDefinition *
LayoutViewerPanel::GetSelectedLegend() const {
  if (currentLayout.legendViews.empty())
    return nullptr;
  if (selectionState_.Current().kind == LayoutElementKind::Legend &&
      selectionState_.Current().id >= 0) {
    for (const auto &legend : currentLayout.legendViews) {
      if (legend.id == selectionState_.Current().id)
        return &legend;
    }
  }
  if (!currentLayout.legendViews.empty())
    return &currentLayout.legendViews.front();
  return nullptr;
}

bool LayoutViewerPanel::GetLegendFrameById(
    int legendId, layouts::Layout2DViewFrame &frame) const {
  if (legendId <= 0)
    return false;
  for (const auto &legend : currentLayout.legendViews) {
    if (legend.id == legendId) {
      frame = legend.frame;
      return true;
    }
  }
  return false;
}

// Updates legend geometry and invalidates only the legend cache when resizing
// completes.
void LayoutViewerPanel::UpdateLegendFrame(
    const layouts::Layout2DViewFrame &frame, bool updatePosition) {
  layouts::LayoutLegendDefinition *legend = GetSelectedLegend();
  if (!legend)
    return;
  legend->frame.width = frame.width;
  legend->frame.height = frame.height;
  if (updatePosition) {
    legend->frame.x = frame.x;
    legend->frame.y = frame.y;
  }
  if (updatePosition) {
    pendingFrameCommit_ = true;
    Refresh();
    return;
  }
  if (!currentLayout.name.empty()) {
    layouts::LayoutManager::Get().UpdateLayoutLegend(currentLayout.name,
                                                     *legend);
  }
  LegendCache &cache = GetLegendCache(legend->id);
  cache.renderDirty = true;
  renderDirty = true;
  RequestRenderRebuild();
  Refresh();
}

void LayoutViewerPanel::OnDeleteLegend(wxCommandEvent &) {
  if (selectionState_.Current().kind != LayoutElementKind::Legend)
    return;
  const layouts::LayoutLegendDefinition *legend = GetSelectedLegend();
  if (!legend)
    return;
  const int legendId = legend->id;
  if (!currentLayout.name.empty()) {
    if (layouts::LayoutManager::Get().RemoveLayoutLegend(currentLayout.name,
                                                         legendId)) {
      auto &legends = currentLayout.legendViews;
      legends.erase(std::remove_if(legends.begin(), legends.end(),
                                   [legendId](const auto &entry) {
                                     return entry.id == legendId;
                                   }),
                    legends.end());
      InvalidateSelectionIndexCache();
      if (selectionState_.Current().id == legendId) {
        if (!currentLayout.view2dViews.empty()) {
          selectionState_.Select(LayoutElementKind::View2D,
                                 currentLayout.view2dViews.front().id);
        } else if (!legends.empty()) {
          selectionState_.Select(LayoutElementKind::Legend, legends.front().id);
        } else if (!currentLayout.textViews.empty()) {
          selectionState_.Select(LayoutElementKind::Text,
                                 currentLayout.textViews.front().id);
        } else if (!currentLayout.eventTables.empty()) {
          selectionState_.Select(LayoutElementKind::EventTable,
                                 currentLayout.eventTables.front().id);
        } else if (!currentLayout.imageViews.empty()) {
          selectionState_.Select(LayoutElementKind::Image,
                                 currentLayout.imageViews.front().id);
        } else {
          selectionState_.Clear();
        }
      }
    }
  }
  auto cacheIt = legendCaches_.find(legendId);
  if (cacheIt != legendCaches_.end()) {
    ClearCachedTexture(cacheIt->second);
    legendCaches_.erase(cacheIt);
  }
  Refresh();
}

void LayoutViewerPanel::OnEditLegend(wxCommandEvent &) {
  if (selectionState_.Current().kind != LayoutElementKind::Legend)
    return;
  layouts::LayoutLegendDefinition *legend = GetSelectedLegend();
  if (!legend)
    return;

  const std::vector<SharedLayoutLegendItem> availableItems =
      BuildSharedLayoutLegendItems();
  LayoutLegendEditDialog dialog(this, *legend, availableItems);
  if (dialog.ShowModal() != wxID_OK)
    return;

  legend->showChannelColumn = dialog.GetShowChannelColumn();
  legend->itemSettings = dialog.GetItemSettings();
  layouts::LayoutManager::Get().UpdateLayoutLegend(currentLayout.name, *legend);
  legendDataDirty_ = true;
  RefreshLegendData();
  RequestRenderRebuild();
  Refresh();
}

void LayoutViewerPanel::DrawLegendElement(
    const layouts::LayoutLegendDefinition &legend, int activeLegendId) {
  LegendCache &cache = GetLegendCache(legend.id);
  wxRect frameRect;
  if (!GetFrameRect(legend.frame, frameRect))
    return;
  const int frameRight = frameRect.GetLeft() + frameRect.GetWidth();
  const int frameBottom = frameRect.GetTop() + frameRect.GetHeight();

  const wxSize renderSize = GetFrameSizeForZoom(legend.frame, cache.renderZoom);
  if (ShouldDeferMissingElementTexture(cache.renderDirty, cache.texture,
                                       cache.textureSize, renderSize)) {
    return;
  }
  if (cache.texture != 0 && renderSize.GetWidth() > 0 &&
      renderSize.GetHeight() > 0 && cache.textureSize == renderSize) {
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, cache.texture);
    glColor4ub(255, 255, 255, 255);
    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 1.0f);
    glVertex2f(static_cast<float>(frameRect.GetLeft()),
               static_cast<float>(frameRect.GetTop()));
    glTexCoord2f(1.0f, 1.0f);
    glVertex2f(static_cast<float>(frameRight),
               static_cast<float>(frameRect.GetTop()));
    glTexCoord2f(1.0f, 0.0f);
    glVertex2f(static_cast<float>(frameRight), static_cast<float>(frameBottom));
    glTexCoord2f(0.0f, 0.0f);
    glVertex2f(static_cast<float>(frameRect.GetLeft()),
               static_cast<float>(frameRect.GetBottom()));
    glEnd();
    glDisable(GL_TEXTURE_2D);
  } else {
    glColor4ub(245, 245, 245, 255);
    glBegin(GL_QUADS);
    glVertex2f(static_cast<float>(frameRect.GetLeft()),
               static_cast<float>(frameRect.GetTop()));
    glVertex2f(static_cast<float>(frameRect.GetRight()),
               static_cast<float>(frameRect.GetTop()));
    glVertex2f(static_cast<float>(frameRight), static_cast<float>(frameBottom));
    glVertex2f(static_cast<float>(frameRect.GetLeft()),
               static_cast<float>(frameRect.GetBottom()));
    glEnd();
  }

  if (legend.id == activeLegendId) {
    glColor4ub(60, 160, 240, 255);
    glLineWidth(2.0f);
  } else {
    glColor4ub(160, 160, 160, 255);
    glLineWidth(1.0f);
  }
  glBegin(GL_LINE_LOOP);
  glVertex2f(static_cast<float>(frameRect.GetLeft()),
             static_cast<float>(frameRect.GetTop()));
  glVertex2f(static_cast<float>(frameRight),
             static_cast<float>(frameRect.GetTop()));
  glVertex2f(static_cast<float>(frameRight), static_cast<float>(frameBottom));
  glVertex2f(static_cast<float>(frameRect.GetLeft()),
             static_cast<float>(frameRect.GetBottom()));
  glEnd();

  if (legend.id == activeLegendId)
    DrawSelectionHandles(frameRect);
}

// Rebuilds cached legend items and marks legend textures dirty when legend
// content changes.
void LayoutViewerPanel::RefreshLegendData() {
  if (auto *mw = MainWindow::Instance();
      mw && mw->IsMvrImportPipelineActive()) {
    return;
  }
  if (!legendDataDirty_)
    return;
  if (currentLayout.legendViews.empty()) {
    legendItems_.clear();
    legendDataHash = 0;
    legendDataDirty_ = false;
    return;
  }
  if (gui::InvalidateChangedLegendRasters(currentLayout.legendViews,
      legendCaches_, [this](const auto &legend) {
        return ComputeLegendContentHash(legend);
      })) {
    renderDirty = true;
    RequestRenderRebuild();
  }
  const auto *selectedLegend =
      static_cast<const LayoutViewerPanel *>(this)->GetSelectedLegend();
  std::vector<LegendItem> items = BuildLegendItems(selectedLegend);
  size_t newHash = HashLegendItems(items, selectedLegend);
  if (newHash == legendDataHash) {
    legendDataDirty_ = false;
    return;
  }
  legendItems_ = std::move(items);
  legendDataHash = newHash;
  legendDataDirty_ = false;
}

// Builds the visible Legend items for the specified Legend definition.
std::vector<LayoutViewerPanel::LegendItem> LayoutViewerPanel::BuildLegendItems(
    const layouts::LayoutLegendDefinition *legend) const {
  if (!legend) legend = GetSelectedLegend();
  return layouts::ResolveLegendItems(BuildSharedLayoutLegendItems(), legend);
}

// Hashes the ordered semantic inputs that determine a Legend raster.
size_t LayoutViewerPanel::HashLegendItems(
    const std::vector<LegendItem> &items,
    const layouts::LayoutLegendDefinition *legend) const {
  project_cache::FingerprintAccumulator hash;
  auto appendString = [&hash](const std::string &value) {
    hash.Update(value.data(), value.size());
    const char separator = '\0';
    hash.Update(&separator, 1);
  };
  auto appendInt = [&appendString](int value) {
    appendString(std::to_string(value));
  };
  // Old persistent rasters predate logical composition and symbol-slot clips.
  appendString("legend-composition-v1");
  appendInt(static_cast<int>(items.size()));
  for (const auto &item : items) {
    appendString(item.typeName);
    appendString(item.displayName);
    appendInt(item.count);
    appendInt(item.channelCount.value_or(-1));
    appendString(item.persistentSymbolIdentity);
    appendString(item.symbolFillHex.value_or(""));
    appendInt(item.showBottomSymbol ? 1 : 0);
    appendInt(item.showFrontSymbol ? 1 : 0);
    appendInt(item.showSideSymbol ? 1 : 0);
  }
  if (!legend)
    legend = GetSelectedLegend();
  appendInt((legend && legend->showChannelColumn) ? 1 : 0);
  const std::string fingerprint = hash.Finish();
  return static_cast<size_t>(
      std::stoull(fingerprint.substr(fingerprint.find(':') + 1), nullptr, 16));
}

// Computes the deterministic content identity for one Legend definition.
size_t LayoutViewerPanel::ComputeLegendContentHash(
    const layouts::LayoutLegendDefinition &legend) const {
  return HashLegendItems(BuildLegendItems(&legend), &legend);
}

// Refreshes Legend semantics before persistent cache validation can inspect
// them.
void LayoutViewerPanel::EnsureLegendDataCurrentForCacheValidation() {
  if (!legendDataDirty_ &&
      (currentLayout.legendViews.empty() || legendDataHash != 0))
    return;
  RefreshLegendData();
}

wxImage LayoutViewerPanel::BuildLegendImage(
    const wxSize &size, const wxSize &logicalSize, double renderZoom,
    const std::vector<LegendItem> &items,
    const layouts::LayoutLegendDefinition &legend,
    const SymbolDefinitionSnapshot *symbols) const {
  return gui::RenderLegendImage(size, logicalSize, renderZoom, items, legend,
                                symbols);
}
