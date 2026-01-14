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
#include "layouttextutils.h"

#include <algorithm>

#include <wx/dcgraph.h>
#include <wx/dcmemory.h>
#include <wx/richtext/richtextbuffer.h>
#include <wx/sstream.h>

#include "layoutviewerpanel_shared.h"

namespace layouttext {
namespace {
void EnsureRichTextHandlers() {
  static bool initialized = false;
  if (initialized)
    return;
  wxRichTextBuffer::InitStandardHandlers();
  initialized = true;
}
} // namespace

wxImage RenderTextImage(const layouts::LayoutTextDefinition &text,
                        const wxSize &renderSize, const wxSize &logicalSize,
                        double renderScale) {
  if (renderSize.GetWidth() <= 0 || renderSize.GetHeight() <= 0 ||
      renderScale <= 0.0) {
    return wxImage();
  }

  wxBitmap bitmap(renderSize.GetWidth(), renderSize.GetHeight(), 32);
  bitmap.UseAlpha();
  wxMemoryDC memoryDc(bitmap);
  wxGCDC dc(memoryDc);

  if (text.solidBackground) {
    dc.SetBackground(wxBrush(wxColour(255, 255, 255)));
  } else {
    dc.SetBackground(wxBrush(wxColour(255, 255, 255, 0)));
  }
  dc.Clear();
  dc.SetTextForeground(wxColour(0, 0, 0));

  EnsureRichTextHandlers();
  wxRichTextBuffer buffer;
  bool loaded = false;
  if (!text.richText.empty()) {
    wxStringInputStream input(wxString::FromUTF8(text.richText));
    loaded = buffer.LoadFile(input, wxRICHTEXT_TYPE_XML);
  }
  if (!loaded) {
    wxString fallback =
        text.text.empty() ? wxString("Light Plot")
                          : wxString::FromUTF8(text.text);
    buffer.AddParagraph(fallback);
  }

  wxRichTextAttr baseStyle = buffer.GetDefaultStyle();
  wxString faceName = layoutviewerpanel::detail::ResolveSharedFontFaceName();
  if (!faceName.empty()) {
    baseStyle.SetFontFaceName(faceName);
    buffer.SetDefaultStyle(baseStyle);
  }

  const int padding = 4;
  const int logicalWidth = std::max(0, logicalSize.GetWidth() - padding * 2);
  const int logicalHeight = std::max(0, logicalSize.GetHeight() - padding * 2);
  wxRect logicalRect(padding, padding, logicalWidth, logicalHeight);

  dc.SetUserScale(renderScale, renderScale);
  wxRichTextDrawingContext context(&buffer);
  wxRichTextSelection selection;
  buffer.Layout(dc, context, logicalRect, logicalRect, wxRICHTEXT_FIXED_WIDTH);
  buffer.Draw(dc, context, buffer.GetRange(), selection, logicalRect, 0, 0);

  memoryDc.SelectObject(wxNullBitmap);
  return bitmap.ConvertToImage();
}

} // namespace layouttext
