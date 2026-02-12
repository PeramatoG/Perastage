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

// Include GLEW or other OpenGL loader first if present
#ifdef __APPLE__
#  include <OpenGL/gl.h>
#  include <OpenGL/glu.h>
#else
#  include <GL/gl.h>
#  include <GL/glu.h>
#endif

#include <wx/log.h>
#include <wx/richtext/richtextbuffer.h>

#include "layouttextdialog.h"
#include "layouttextutils.h"
#include "layoutviewerpanel_shared.h"
#include "LayoutManager.h"

layouts::LayoutTextDefinition *LayoutViewerPanel::GetSelectedText() {
  if (currentLayout.textViews.empty())
    return nullptr;
  if (selectedElementType == SelectedElementType::Text &&
      selectedElementId >= 0) {
    for (auto &text : currentLayout.textViews) {
      if (text.id == selectedElementId)
        return &text;
    }
  }
  selectedElementType = SelectedElementType::Text;
  selectedElementId = currentLayout.textViews.front().id;
  return &currentLayout.textViews.front();
}

const layouts::LayoutTextDefinition *LayoutViewerPanel::GetSelectedText() const {
  if (currentLayout.textViews.empty())
    return nullptr;
  if (selectedElementType == SelectedElementType::Text &&
      selectedElementId >= 0) {
    for (const auto &text : currentLayout.textViews) {
      if (text.id == selectedElementId)
        return &text;
    }
  }
  if (!currentLayout.textViews.empty())
    return &currentLayout.textViews.front();
  return nullptr;
}

bool LayoutViewerPanel::GetTextFrameById(
    int textId, layouts::Layout2DViewFrame &frame) const {
  if (textId <= 0)
    return false;
  for (const auto &text : currentLayout.textViews) {
    if (text.id == textId) {
      frame = text.frame;
      return true;
    }
  }
  return false;
}

void LayoutViewerPanel::UpdateTextFrame(const layouts::Layout2DViewFrame &frame,
                                        bool updatePosition) {
  layouts::LayoutTextDefinition *text = GetSelectedText();
  if (!text)
    return;
  text->frame.width = frame.width;
  text->frame.height = frame.height;
  if (updatePosition) {
    text->frame.x = frame.x;
    text->frame.y = frame.y;
  }
  if (!currentLayout.name.empty()) {
    layouts::LayoutManager::Get().UpdateLayoutText(currentLayout.name, *text);
  }
  InvalidateRenderIfFrameChanged();
  if (NeedsRenderRebuild()) {
    RequestRenderRebuild();
  }
  Refresh();
}

void LayoutViewerPanel::OnEditText(wxCommandEvent &) {
  if (selectedElementType != SelectedElementType::Text)
    return;
  layouts::LayoutTextDefinition *text = GetSelectedText();
  if (!text)
    return;
  const wxString richText =
      wxString::FromUTF8(text->richText.data(), text->richText.size());
  const wxString fallbackText =
      text->text.empty() ? wxString("Light Plot")
                         : wxString::FromUTF8(text->text.data(),
                                              text->text.size());
  LayoutTextDialog dialog(this, richText, fallbackText, text->solidBackground,
                          text->drawFrame);
  if (dialog.ShowModal() != wxID_OK)
    return;
  wxString updatedRichText = dialog.GetRichText();
  wxString updatedPlainText = dialog.GetPlainText();
  const bool hadRichText = !text->richText.empty();
  wxScopedCharBuffer richBuf = updatedRichText.ToUTF8();
  wxScopedCharBuffer plainBuf = updatedPlainText.ToUTF8();
  if (updatedRichText.IsEmpty() && hadRichText) {
    wxLogWarning(
        "LayoutViewerPanel::OnEditText received empty rich text, "
        "preserving previous rich text to avoid losing formatting.");
  } else {
    text->richText.assign(richBuf.data() ? richBuf.data() : "",
                          richBuf.length());
  }
  text->text.assign(plainBuf.data() ? plainBuf.data() : "",
                    plainBuf.length());
  text->solidBackground = dialog.GetSolidBackground();
  text->drawFrame = dialog.GetDrawFrame();
  if (!currentLayout.name.empty()) {
    layouts::LayoutManager::Get().UpdateLayoutText(currentLayout.name, *text);
  }
  TextCache &cache = GetTextCache(text->id);
  cache.renderDirty = true;
  RequestRenderRebuild();
  Refresh();
}

void LayoutViewerPanel::OnDeleteText(wxCommandEvent &) {
  if (selectedElementType != SelectedElementType::Text)
    return;
  const layouts::LayoutTextDefinition *text = GetSelectedText();
  if (!text)
    return;
  const int textId = text->id;
  if (!currentLayout.name.empty()) {
    if (layouts::LayoutManager::Get().RemoveLayoutText(currentLayout.name,
                                                       textId)) {
      auto &texts = currentLayout.textViews;
      texts.erase(std::remove_if(texts.begin(), texts.end(),
                                 [textId](const auto &entry) {
                                   return entry.id == textId;
                                 }),
                  texts.end());
      if (selectedElementId == textId) {
        if (!currentLayout.view2dViews.empty()) {
          selectedElementType = SelectedElementType::View2D;
          selectedElementId = currentLayout.view2dViews.front().id;
        } else if (!currentLayout.legendViews.empty()) {
          selectedElementType = SelectedElementType::Legend;
          selectedElementId = currentLayout.legendViews.front().id;
        } else if (!currentLayout.eventTables.empty()) {
          selectedElementType = SelectedElementType::EventTable;
          selectedElementId = currentLayout.eventTables.front().id;
        } else if (!currentLayout.imageViews.empty()) {
          selectedElementType = SelectedElementType::Image;
          selectedElementId = currentLayout.imageViews.front().id;
        } else if (!texts.empty()) {
          selectedElementType = SelectedElementType::Text;
          selectedElementId = texts.front().id;
        } else {
          selectedElementType = SelectedElementType::None;
          selectedElementId = -1;
        }
      }
    }
  }
  auto cacheIt = textCaches_.find(textId);
  if (cacheIt != textCaches_.end()) {
    ClearCachedTexture(cacheIt->second);
    textCaches_.erase(cacheIt);
  }
  Refresh();
}

void LayoutViewerPanel::DrawTextElement(
    const layouts::LayoutTextDefinition &text, int activeTextId) {
  TextCache &cache = GetTextCache(text.id);
  wxRect frameRect;
  if (!GetFrameRect(text.frame, frameRect))
    return;
  const int frameRight = frameRect.GetLeft() + frameRect.GetWidth();
  const int frameBottom = frameRect.GetTop() + frameRect.GetHeight();

  const wxSize renderSize = GetFrameSizeForZoom(text.frame, cache.renderZoom);
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
  } else if (text.solidBackground) {
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

  if (text.drawFrame || text.id == activeTextId) {
    if (text.id == activeTextId) {
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
  }

  if (text.id == activeTextId)
    DrawSelectionHandles(frameRect);
}

size_t LayoutViewerPanel::HashTextContent(
    const layouts::LayoutTextDefinition &text) const {
  std::hash<std::string> strHasher;
  std::string content = text.richText.empty() ? text.text : text.richText;
  content += text.solidBackground ? "|bg1" : "|bg0";
  return strHasher(content);
}

wxImage LayoutViewerPanel::BuildTextImage(
    const wxSize &size, const wxSize &logicalSize, double renderZoom,
    const layouts::LayoutTextDefinition &text) const {
  if (size.GetWidth() <= 0 || size.GetHeight() <= 0 || renderZoom <= 0.0)
    return wxImage();
  return layouttext::RenderTextImage(text, size, logicalSize, renderZoom);
}
