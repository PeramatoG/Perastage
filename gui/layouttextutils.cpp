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
#include <wx/tokenzr.h>

#include "layoutviewerpanel_shared.h"

#ifndef wxTEXT_ATTR_PARAGRAPH_SPACING_BEFORE
#define wxTEXT_ATTR_PARAGRAPH_SPACING_BEFORE 0
#endif

#ifndef wxTEXT_ATTR_PARAGRAPH_SPACING_AFTER
#define wxTEXT_ATTR_PARAGRAPH_SPACING_AFTER 0
#endif

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

bool LoadRichTextBufferFromString(wxRichTextBuffer &buffer,
                                  const wxString &content) {
  if (content.empty())
    return false;
  EnsureRichTextHandlers();
  wxStringInputStream xmlInput(content);
  if (buffer.LoadFile(xmlInput, wxRICHTEXT_TYPE_XML))
    return true;
  wxStringInputStream richInput(content);
#if defined(wxRICHTEXT_TYPE_RICHTEXT)
  return buffer.LoadFile(richInput, wxRICHTEXT_TYPE_RICHTEXT);
#else
  return buffer.LoadFile(richInput, wxRICHTEXT_TYPE_TEXT);
#endif
}

wxString SaveRichTextBufferToString(wxRichTextBuffer &buffer) {
  EnsureRichTextHandlers();
  wxStringOutputStream output;
  if (buffer.SaveFile(output, wxRICHTEXT_TYPE_XML))
    return output.GetString();
  wxStringOutputStream richOutput;
#if defined(wxRICHTEXT_TYPE_RICHTEXT)
  if (buffer.SaveFile(richOutput, wxRICHTEXT_TYPE_RICHTEXT))
#else
  if (buffer.SaveFile(richOutput, wxRICHTEXT_TYPE_TEXT))
#endif
    return richOutput.GetString();
  return buffer.GetText();
}

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
    dc.SetBackground(wxBrush(wxColour(255, 255, 255, 255)));
  } else {
    dc.SetBackground(wxBrush(wxColour(255, 255, 255, 0)));
  }
  dc.Clear();
  dc.SetTextForeground(wxColour(0, 0, 0));

  wxRichTextBuffer buffer;
  bool loaded = false;
  if (!text.richText.empty()) {
    loaded = LoadRichTextBufferFromString(
        buffer, wxString::FromUTF8(text.richText));
  }
  if (!loaded) {
    wxString fallback =
        text.text.empty() ? wxString("Light Plot")
                          : wxString::FromUTF8(text.text);
    wxString normalized = fallback;
    normalized.Replace("\r\n", "\n");
    normalized.Replace("\r", "\n");
    wxStringTokenizer tokenizer(normalized, "\n", wxTOKEN_RET_EMPTY_ALL);
    bool wroteParagraph = false;
    while (tokenizer.HasMoreTokens()) {
      buffer.AddParagraph(tokenizer.GetNextToken());
      wroteParagraph = true;
    }
    if (!wroteParagraph) {
      buffer.AddParagraph(wxString());
    }
  }

  if (!loaded) {
    wxRichTextAttr baseStyle = buffer.GetDefaultStyle();
    wxString faceName = layoutviewerpanel::detail::ResolveSharedFontFaceName();
    if (!faceName.empty())
      baseStyle.SetFontFaceName(faceName);
    baseStyle.SetTextColour(*wxBLACK);
    baseStyle.SetParagraphSpacingBefore(0);
    baseStyle.SetParagraphSpacingAfter(0);
    baseStyle.SetFontSize(layoutviewerpanel::detail::kTextDefaultFontSize);
    buffer.SetDefaultStyle(baseStyle);
    buffer.SetBasicStyle(baseStyle);
    if (buffer.GetRange().GetLength() > 0) {
      wxRichTextAttr overrideStyle;
      long flags = wxTEXT_ATTR_TEXT_COLOUR |
                   wxTEXT_ATTR_PARAGRAPH_SPACING_BEFORE |
                   wxTEXT_ATTR_PARAGRAPH_SPACING_AFTER |
                   wxTEXT_ATTR_FONT_SIZE;
      if (!faceName.empty()) {
        overrideStyle.SetFontFaceName(faceName);
        flags |= wxTEXT_ATTR_FONT_FACE;
      }
      overrideStyle.SetTextColour(*wxBLACK);
      overrideStyle.SetParagraphSpacingBefore(0);
      overrideStyle.SetParagraphSpacingAfter(0);
      overrideStyle.SetFontSize(layoutviewerpanel::detail::kTextDefaultFontSize);
      overrideStyle.SetFlags(flags);
      buffer.SetStyle(buffer.GetRange(), overrideStyle);
    }
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
  wxImage image = bitmap.ConvertToImage();
  if (!image.HasAlpha())
    image.InitAlpha();
  if (text.solidBackground) {
    unsigned char *alpha = image.GetAlpha();
    if (alpha) {
      const size_t pixelCount =
          static_cast<size_t>(image.GetWidth()) *
          static_cast<size_t>(image.GetHeight());
      std::fill(alpha, alpha + pixelCount, 255);
    }
  }
  return image;
}

} // namespace layouttext
