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
#include "layouteventtabledialog.h"
#include "layoutviewerpanel_shared.h"
#include "layout_event_table_renderer.h"
#include "layout_event_table_cache_policy.h"

layouts::LayoutEventTableDefinition *
LayoutViewerPanel::GetSelectedEventTable() {
  if (currentLayout.eventTables.empty())
    return nullptr;
  if (selectionState_.Current().kind == LayoutElementKind::EventTable &&
      selectionState_.Current().id >= 0) {
    for (auto &table : currentLayout.eventTables) {
      if (table.id == selectionState_.Current().id)
        return &table;
    }
  }
  return &currentLayout.eventTables.front();
}

const layouts::LayoutEventTableDefinition *
LayoutViewerPanel::GetSelectedEventTable() const {
  if (currentLayout.eventTables.empty())
    return nullptr;
  if (selectionState_.Current().kind == LayoutElementKind::EventTable &&
      selectionState_.Current().id >= 0) {
    for (const auto &table : currentLayout.eventTables) {
      if (table.id == selectionState_.Current().id)
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
  if (updatePosition) {
    pendingFrameCommit_ = true;
    Refresh();
    return;
  }
  if (!currentLayout.name.empty()) {
    layouts::LayoutManager::Get().UpdateLayoutEventTable(currentLayout.name,
                                                         *table);
  }
  InvalidateRenderIfFrameChanged(false);
  if (NeedsRenderRebuild()) {
    RequestRenderRebuild();
  }
  Refresh();
}

void LayoutViewerPanel::OnEditEventTable(wxCommandEvent &) {
  if (selectionState_.Current().kind != LayoutElementKind::EventTable)
    return;
  layouts::LayoutEventTableDefinition *table = GetSelectedEventTable();
  if (!table)
    return;
  LayoutEventTableDialog dialog(this, *table);
  if (dialog.ShowModal() != wxID_OK)
    return;
  const auto editedFields = dialog.GetFields();
  if (table->fields == editedFields) {
    return;
  }

  table->fields = editedFields;
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
  if (selectionState_.Current().kind != LayoutElementKind::EventTable)
    return;
  const layouts::LayoutEventTableDefinition *table = GetSelectedEventTable();
  if (!table)
    return;
  const int tableId = table->id;
  if (!currentLayout.name.empty()) {
    if (layouts::LayoutManager::Get().RemoveLayoutEventTable(currentLayout.name,
                                                             tableId)) {
      auto &tables = currentLayout.eventTables;
      tables.erase(std::remove_if(tables.begin(), tables.end(),
                                  [tableId](const auto &entry) {
                                    return entry.id == tableId;
                                  }),
                   tables.end());
      InvalidateSelectionIndexCache();
      if (selectionState_.Current().id == tableId) {
        if (!currentLayout.view2dViews.empty()) {
          selectionState_.Select(LayoutElementKind::View2D,
                                 currentLayout.view2dViews.front().id);
        } else if (!currentLayout.legendViews.empty()) {
          selectionState_.Select(LayoutElementKind::Legend,
                                 currentLayout.legendViews.front().id);
        } else if (!currentLayout.textViews.empty()) {
          selectionState_.Select(LayoutElementKind::Text,
                                 currentLayout.textViews.front().id);
        } else if (!currentLayout.imageViews.empty()) {
          selectionState_.Select(LayoutElementKind::Image,
                                 currentLayout.imageViews.front().id);
        } else if (!tables.empty()) {
          selectionState_.Select(LayoutElementKind::EventTable,
                                 tables.front().id);
        } else {
          selectionState_.Clear();
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

  if (table.id == selectionState_.Current().id &&
      selectionState_.Current().kind == LayoutElementKind::EventTable) {
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

  if (table.id == selectionState_.Current().id &&
      selectionState_.Current().kind == LayoutElementKind::EventTable) {
    DrawSelectionHandles(frameRect);
  }
}

size_t LayoutViewerPanel::HashEventTableFields(
    const layouts::LayoutEventTableDefinition &table) const {
  return gui::EventTableContentHash(table);
}

wxImage LayoutViewerPanel::BuildEventTableImage(
    const wxSize &size, const wxSize &logicalSize, double renderZoom,
    const layouts::LayoutEventTableDefinition &table) const {
  (void)logicalSize;
  return gui::RenderEventTableImage(size, renderZoom, table);
}
