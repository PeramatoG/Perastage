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
#include <array>
#include <functional>
#include <unordered_map>

// Include GLEW or other OpenGL loader first if present
#ifdef __APPLE__
#  include <OpenGL/gl.h>
#  include <OpenGL/glu.h>
#else
#  include <GL/gl.h>
#  include <GL/glu.h>
#endif

#include "layouteventtabledialog.h"
#include "layoutviewerpanel_shared.h"
#include "LayoutManager.h"
#include "guiconfigservices.h"
#include "configmanager.h"
#include <wx/dcgraph.h>

namespace {
constexpr std::array<const char *, 7> kEventTableLabels = {
    "Venue:", "Location:", "Date:", "Stage:",
    "Version:", "Design:", "Mail:"};
} // namespace

layouts::LayoutEventTableDefinition *
LayoutViewerPanel::GetSelectedEventTable() {
  if (currentLayout.eventTables.empty())
    return nullptr;
  if (selectedElementType == SelectedElementType::EventTable &&
      selectedElementId >= 0) {
    for (auto &table : currentLayout.eventTables) {
      if (table.id == selectedElementId)
        return &table;
    }
  }
  selectedElementType = SelectedElementType::EventTable;
  selectedElementId = currentLayout.eventTables.front().id;
  return &currentLayout.eventTables.front();
}

const layouts::LayoutEventTableDefinition *
LayoutViewerPanel::GetSelectedEventTable() const {
  if (currentLayout.eventTables.empty())
    return nullptr;
  if (selectedElementType == SelectedElementType::EventTable &&
      selectedElementId >= 0) {
    for (const auto &table : currentLayout.eventTables) {
      if (table.id == selectedElementId)
        return &table;
    }
  }
  if (!currentLayout.eventTables.empty())
    return &currentLayout.eventTables.front();
  return nullptr;
}

bool LayoutViewerPanel::GetEventTableFrameById(
    int tableId, layouts::Layout2DViewFrame &frame) const {
  if (tableId <= 0)
    return false;
  for (const auto &table : currentLayout.eventTables) {
    if (table.id == tableId) {
      frame = table.frame;
      return true;
    }
  }
  return false;
}

void LayoutViewerPanel::UpdateEventTableFrame(
    const layouts::Layout2DViewFrame &frame, bool updatePosition) {
  layouts::LayoutEventTableDefinition *table = GetSelectedEventTable();
  if (!table)
    return;
  table->frame.width = frame.width;
  table->frame.height = frame.height;
  if (updatePosition) {
    table->frame.x = frame.x;
    table->frame.y = frame.y;
  }
  if (!currentLayout.name.empty()) {
    layouts::LayoutManager::Get().UpdateLayoutEventTable(currentLayout.name,
                                                         *table);
  }
  InvalidateRenderIfFrameChanged();
  if (NeedsRenderRebuild()) {
    RequestRenderRebuild();
  }
  Refresh();
}

void LayoutViewerPanel::OnEditEventTable(wxCommandEvent &) {
  if (selectedElementType != SelectedElementType::EventTable)
    return;
  layouts::LayoutEventTableDefinition *table = GetSelectedEventTable();
  if (!table)
    return;
  LayoutEventTableDialog dialog(this, *table);
  if (dialog.ShowModal() != wxID_OK)
    return;
  table->fields = dialog.GetFields();
  if (!currentLayout.name.empty()) {
    layouts::LayoutManager::Get().UpdateLayoutEventTable(currentLayout.name,
                                                         *table);
  }
  EventTableCache &cache = GetEventTableCache(table->id);
  cache.renderDirty = true;
  RequestRenderRebuild();
  Refresh();
}

void LayoutViewerPanel::OnDeleteEventTable(wxCommandEvent &) {
  if (selectedElementType != SelectedElementType::EventTable)
    return;
  const layouts::LayoutEventTableDefinition *table = GetSelectedEventTable();
  if (!table)
    return;
  const int tableId = table->id;
  if (!currentLayout.name.empty()) {
    auto &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
    cfg.PushUndoState("delete layout event table");
    if (layouts::LayoutManager::Get().RemoveLayoutEventTable(
            currentLayout.name, tableId)) {
      auto &tables = currentLayout.eventTables;
      tables.erase(std::remove_if(tables.begin(), tables.end(),
                                  [tableId](const auto &entry) {
                                    return entry.id == tableId;
                                  }),
                   tables.end());
      if (selectedElementId == tableId) {
        if (!currentLayout.view2dViews.empty()) {
          selectedElementType = SelectedElementType::View2D;
          selectedElementId = currentLayout.view2dViews.front().id;
        } else if (!currentLayout.legendViews.empty()) {
          selectedElementType = SelectedElementType::Legend;
          selectedElementId = currentLayout.legendViews.front().id;
        } else if (!currentLayout.textViews.empty()) {
          selectedElementType = SelectedElementType::Text;
          selectedElementId = currentLayout.textViews.front().id;
        } else if (!currentLayout.imageViews.empty()) {
          selectedElementType = SelectedElementType::Image;
          selectedElementId = currentLayout.imageViews.front().id;
        } else if (!tables.empty()) {
          selectedElementType = SelectedElementType::EventTable;
          selectedElementId = tables.front().id;
        } else {
          selectedElementType = SelectedElementType::None;
          selectedElementId = -1;
        }
      }
    }
  }
  auto cacheIt = eventTableCaches_.find(tableId);
  if (cacheIt != eventTableCaches_.end()) {
    ClearCachedTexture(cacheIt->second);
    eventTableCaches_.erase(cacheIt);
  }
  Refresh();
}

void LayoutViewerPanel::DrawEventTableElement(
    const layouts::LayoutEventTableDefinition &table) {
  EventTableCache &cache = GetEventTableCache(table.id);
  wxRect frameRect;
  if (!GetFrameRect(table.frame, frameRect))
    return;
  const int frameRight = frameRect.GetLeft() + frameRect.GetWidth();
  const int frameBottom = frameRect.GetTop() + frameRect.GetHeight();

  const wxSize renderSize = GetFrameSizeForZoom(table.frame, cache.renderZoom);
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
    glVertex2f(static_cast<float>(frameRight),
               static_cast<float>(frameBottom));
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
    glVertex2f(static_cast<float>(frameRight),
               static_cast<float>(frameBottom));
    glVertex2f(static_cast<float>(frameRect.GetLeft()),
               static_cast<float>(frameRect.GetBottom()));
    glEnd();
  }

  if (table.id == selectedElementId &&
      selectedElementType == SelectedElementType::EventTable) {
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
  glVertex2f(static_cast<float>(frameRight),
             static_cast<float>(frameBottom));
  glVertex2f(static_cast<float>(frameRect.GetLeft()),
             static_cast<float>(frameRect.GetBottom()));
  glEnd();

  if (table.id == selectedElementId &&
      selectedElementType == SelectedElementType::EventTable) {
    DrawSelectionHandles(frameRect);
  }
}

size_t LayoutViewerPanel::HashEventTableFields(
    const layouts::LayoutEventTableDefinition &table) const {
  size_t hash = table.fields.size();
  std::hash<std::string> strHasher;
  for (const auto &field : table.fields) {
    hash ^= strHasher(field) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
  }
  return hash;
}

wxImage LayoutViewerPanel::BuildEventTableImage(
    const wxSize &size, const wxSize &logicalSize, double renderZoom,
    const layouts::LayoutEventTableDefinition &table) const {
  if (size.GetWidth() <= 0 || size.GetHeight() <= 0 || renderZoom <= 0.0)
    return wxImage();
  wxBitmap bitmap(size.GetWidth(), size.GetHeight(), 32);
  wxMemoryDC memoryDc(bitmap);
  wxGCDC dc(memoryDc);
  dc.SetBackground(wxBrush(wxColour(255, 255, 255)));
  dc.Clear();
  dc.SetTextForeground(wxColour(20, 20, 20));
  dc.SetPen(*wxTRANSPARENT_PEN);

  const int paddingLeft = 6;
  const int paddingRight = 6;
  const int paddingTop = 6;
  const int paddingBottom = 6;
  const int columnGap = 10;
  const int totalRows = static_cast<int>(kEventTableLabels.size());
  const int baseHeight = logicalSize.GetHeight() > 0 ? logicalSize.GetHeight()
                                                     : size.GetHeight();
  const double availableHeight =
      static_cast<double>(baseHeight) - paddingTop - paddingBottom;
  double fontSize =
      totalRows > 0 ? (availableHeight / totalRows) - 2.0 : 10.0;
  fontSize = std::clamp(fontSize, 6.0, 14.0);
  fontSize *= kLegendFontScale;
  const int fontSizePx =
      std::max(1, static_cast<int>(std::lround(fontSize * renderZoom)));
  const int emphasizedFontSizePx = std::max(
      fontSizePx + 1,
      static_cast<int>(std::lround(fontSizePx * 1.1)));

  wxFont baseFont =
      layoutviewerpanel::detail::MakeSharedFont(fontSizePx,
                                                wxFONTWEIGHT_NORMAL);
  wxFont labelFont =
      layoutviewerpanel::detail::MakeSharedFont(fontSizePx,
                                                wxFONTWEIGHT_BOLD);
  wxFont emphasizedFont =
      layoutviewerpanel::detail::MakeSharedFont(emphasizedFontSizePx,
                                                wxFONTWEIGHT_BOLD);

  std::unordered_map<wxString, wxSize, wxStringHash, wxStringEqual>
      labelTextExtentCache;
  std::unordered_map<wxString, wxSize, wxStringHash, wxStringEqual>
      baseTextExtentCache;
  std::unordered_map<wxString, wxSize, wxStringHash, wxStringEqual>
      emphasizedTextExtentCache;

  auto measureTextExtent =
      [&](const wxString &text,
          std::unordered_map<wxString, wxSize, wxStringHash, wxStringEqual>
              &cache) {
        auto cacheIt = cache.find(text);
        if (cacheIt != cache.end())
          return cacheIt->second;
        int width = 0;
        int height = 0;
        dc.GetTextExtent(text, &width, &height);
        wxSize sizeMeasured(width, height);
        cache.emplace(text, sizeMeasured);
        return sizeMeasured;
      };

  dc.SetFont(labelFont);
  int maxLabelWidth = 0;
  for (const auto &label : kEventTableLabels) {
    const wxSize labelSize =
        measureTextExtent(wxString::FromUTF8(label), labelTextExtentCache);
    maxLabelWidth = std::max(maxLabelWidth, labelSize.GetWidth());
  }

  const int paddingLeftPx =
      std::max(0, static_cast<int>(std::lround(paddingLeft * renderZoom)));
  const int paddingRightPx =
      std::max(0, static_cast<int>(std::lround(paddingRight * renderZoom)));
  const int paddingTopPx =
      std::max(0, static_cast<int>(std::lround(paddingTop * renderZoom)));
  const int paddingBottomPx =
      std::max(0, static_cast<int>(std::lround(paddingBottom * renderZoom)));
  const int columnGapPx =
      std::max(0, static_cast<int>(std::lround(columnGap * renderZoom)));
  const int rowHeightPx = std::max(
      1, static_cast<int>(std::lround(
             (availableHeight / std::max(1, totalRows)) * renderZoom)));
  const int labelX = paddingLeftPx;
  const int valueX = labelX + maxLabelWidth + columnGapPx;
  const int maxValueWidth =
      std::max(0, size.GetWidth() - paddingRightPx - valueX);

  auto trimTextToWidth =
      [&](const wxString &text, int maxWidth,
          std::unordered_map<wxString, wxSize, wxStringHash, wxStringEqual>
              &cache) {
    if (maxWidth <= 0)
      return wxString();
    int textWidth = measureTextExtent(text, cache).GetWidth();
    if (textWidth <= maxWidth)
      return text;
    wxString ellipsis = "...";
    const int ellipsisWidth = measureTextExtent(ellipsis, cache).GetWidth();
    if (ellipsisWidth >= maxWidth)
      return ellipsis.Left(1);
    wxString trimmed = text;
    while (!trimmed.empty()) {
      textWidth = measureTextExtent(trimmed, cache).GetWidth();
      if (textWidth + ellipsisWidth <= maxWidth)
        break;
      trimmed.RemoveLast();
    }
    return trimmed + ellipsis;
  };

  for (size_t idx = 0; idx < kEventTableLabels.size(); ++idx) {
    const int rowTop = paddingTopPx + static_cast<int>(idx) * rowHeightPx;
    wxString labelText = wxString::FromUTF8(kEventTableLabels[idx]);
    dc.SetFont(labelFont);
    const wxSize labelSize = measureTextExtent(labelText, labelTextExtentCache);
    const int labelHeight = labelSize.GetHeight();
    int labelY = rowTop + (rowHeightPx - labelHeight) / 2;
    dc.DrawText(labelText, labelX, labelY);

    wxString valueText;
    if (idx < table.fields.size()) {
      valueText = wxString::FromUTF8(table.fields[idx]);
    }
    if (idx == 0) {
      dc.SetFont(emphasizedFont);
    } else {
      dc.SetFont(baseFont);
    }
    auto &valueTextExtentCache =
        (idx == 0) ? emphasizedTextExtentCache : baseTextExtentCache;
    wxString trimmed = trimTextToWidth(valueText, maxValueWidth,
                                       valueTextExtentCache);
    const int valueHeight =
        measureTextExtent(trimmed, valueTextExtentCache).GetHeight();
    int valueY = rowTop + (rowHeightPx - valueHeight) / 2;
    dc.DrawText(trimmed, valueX, valueY);
  }

  return bitmap.ConvertToImage();
}
