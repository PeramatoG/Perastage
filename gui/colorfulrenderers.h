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

#include <wx/control.h>
#include <wx/dataview.h>
#include <wx/dc.h>
#include <wx/settings.h>

#include "colorstore.h"

namespace {
inline const ColorfulDataViewListStore *GetColorfulStore(
    const wxDataViewCustomRenderer *renderer) {
  if (!renderer)
    return nullptr;
  wxDataViewColumn *column = renderer->GetOwner();
  if (!column)
    return nullptr;
  wxDataViewCtrl *ctrl = column->GetOwner();
  if (!ctrl)
    return nullptr;
  return dynamic_cast<const ColorfulDataViewListStore *>(ctrl->GetModel());
}

inline wxColour ResolveTextColour(const wxDataViewCustomRenderer *renderer,
                                  int state,
                                  const wxDataViewItemAttr &attr) {
  wxDataViewColumn *column = renderer ? renderer->GetOwner() : nullptr;
  wxDataViewCtrl *ctrl = column ? column->GetOwner() : nullptr;
  wxColour colour =
      ctrl ? ctrl->GetForegroundColour()
           : wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT);
  if (attr.HasColour())
    colour = attr.GetColour();
  const auto *store = GetColorfulStore(renderer);
  if ((state & wxDATAVIEW_CELL_SELECTED) && store &&
      store->selectionForegroundEnabled && !attr.HasColour())
    colour = store->selectionForeground;
  if (ctrl && !ctrl->IsEnabled())
    colour = wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT);
  return colour;
}

inline bool ResolveBackgroundColour(const wxDataViewCustomRenderer *renderer,
                                    int state,
                                    const wxDataViewItemAttr &attr,
                                    wxColour &colour) {
  if (attr.HasBackgroundColour()) {
    colour = attr.GetBackgroundColour();
    return true;
  }
  const auto *store = GetColorfulStore(renderer);
  if ((state & wxDATAVIEW_CELL_SELECTED) && store &&
      store->selectionBackgroundEnabled) {
    colour = store->selectionBackground;
    return true;
  }
  return false;
}

inline void ApplyAttributeFont(wxDC *dc, const wxDataViewItemAttr &attr) {
  if (!dc)
    return;
  if (attr.HasFont())
    dc->SetFont(attr.GetEffectiveFont(dc->GetFont()));
}

inline void DrawTextValue(wxDataViewCustomRenderer *renderer,
                          const wxString &text, wxRect rect, wxDC *dc,
                          int state) {
  if (!renderer || !dc)
    return;

  wxDCClipper clip(*dc, rect);
  const wxDataViewItemAttr &attr = renderer->GetAttr();
  ApplyAttributeFont(dc, attr);
  dc->SetTextForeground(ResolveTextColour(renderer, state, attr));

  wxString displayText = text;
  if (renderer->GetEllipsizeMode() != wxELLIPSIZE_NONE) {
    displayText = wxControl::Ellipsize(displayText, *dc,
                                       renderer->GetEllipsizeMode(),
                                       rect.GetWidth());
  }

  wxSize textSize = dc->GetTextExtent(displayText);
  int align = renderer->GetEffectiveAlignment();
  int x = rect.x + 2;
  if (align & wxALIGN_RIGHT)
    x = rect.x + rect.width - textSize.x - 2;
  else if (align & wxALIGN_CENTER_HORIZONTAL)
    x = rect.x + (rect.width - textSize.x) / 2;

  int y = rect.y + 1;
  if (align & wxALIGN_BOTTOM)
    y = rect.y + rect.height - textSize.y - 1;
  else if (align & wxALIGN_CENTER_VERTICAL)
    y = rect.y + (rect.height - textSize.y) / 2;

  dc->DrawText(displayText, x, y);
}
} // namespace

class ColorfulTextRenderer : public wxDataViewCustomRenderer {
public:
  ColorfulTextRenderer(wxDataViewCellMode mode = wxDATAVIEW_CELL_INERT,
                       int align = wxDVR_DEFAULT_ALIGNMENT)
      : wxDataViewCustomRenderer("string", mode, align) {}

  bool Render(wxRect rect, wxDC *dc, int state) override {
    wxColour background;
    if (ResolveBackgroundColour(this, state, GetAttr(), background)) {
      dc->SetBrush(wxBrush(background));
      dc->SetPen(*wxTRANSPARENT_PEN);
      dc->DrawRectangle(rect);
    }
    DrawTextValue(this, m_text, rect, dc, state);
    return true;
  }

  wxSize GetSize() const override {
    wxSize textSize = GetTextExtent(m_text);
    return wxSize(textSize.x + 4, textSize.y + 2);
  }

  bool SetValue(const wxVariant &value) override {
    m_text = value.MakeString();
    return true;
  }

  bool GetValue(wxVariant &value) const override {
    value = m_text;
    return true;
  }

  bool IsCompatibleVariantType(const wxString &WXUNUSED(type)) const override {
    return true;
  }

#if wxUSE_ACCESSIBILITY
  wxString GetAccessibleDescription() const override { return m_text; }
#endif

private:
  wxString m_text;
};

class ColorfulIconTextRenderer : public wxDataViewCustomRenderer {
public:
  ColorfulIconTextRenderer(wxDataViewCellMode mode = wxDATAVIEW_CELL_INERT,
                           int align = wxDVR_DEFAULT_ALIGNMENT)
      : wxDataViewCustomRenderer("wxDataViewIconText", mode, align) {}

  bool Render(wxRect rect, wxDC *dc, int state) override {
    wxColour background;
    if (ResolveBackgroundColour(this, state, GetAttr(), background)) {
      dc->SetBrush(wxBrush(background));
      dc->SetPen(*wxTRANSPARENT_PEN);
      dc->DrawRectangle(rect);
    }

    wxRect drawRect = rect;
    wxBitmapBundle bundle = m_value.GetBitmapBundle();
    if (bundle.IsOk()) {
      wxBitmap bmp = bundle.GetBitmap(wxDefaultSize);
      if (bmp.IsOk()) {
        int bmpX = drawRect.x + 2;
        int bmpY = drawRect.y + (drawRect.height - bmp.GetHeight()) / 2;
        dc->DrawBitmap(bmp, bmpX, bmpY, true);
        drawRect.x = bmpX + bmp.GetWidth() + 4;
        drawRect.width =
            std::max(0, rect.width - (drawRect.x - rect.x));
      }
    }

    DrawTextValue(this, m_value.GetText(), drawRect, dc, state);
    return true;
  }

  wxSize GetSize() const override {
    wxSize textSize = GetTextExtent(m_value.GetText());
    wxBitmapBundle bundle = m_value.GetBitmapBundle();
    if (!bundle.IsOk())
      return textSize;
    wxBitmap bmp = bundle.GetBitmap(wxDefaultSize);
    if (!bmp.IsOk())
      return textSize;
    int width = textSize.x + bmp.GetWidth() + 8;
    int height = std::max(textSize.y + 2, bmp.GetHeight());
    return wxSize(width, height);
  }

  bool SetValue(const wxVariant &value) override {
    if (value.GetType() == "wxDataViewIconText") {
      m_value << value;
      return true;
    }
    m_value = wxDataViewIconText(value.MakeString());
    return true;
  }

  bool GetValue(wxVariant &value) const override {
    value << m_value;
    return true;
  }

#if wxUSE_ACCESSIBILITY
  wxString GetAccessibleDescription() const override {
    return m_value.GetText();
  }
#endif

private:
  wxDataViewIconText m_value;
};
