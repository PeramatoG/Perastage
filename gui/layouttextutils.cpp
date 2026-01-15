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
#include <cmath>
#include <cstring>

#include <wx/dcgraph.h>
#include <wx/dcmemory.h>
#include <wx/mstream.h>
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
bool LoadBufferFromUtf8(wxRichTextBuffer &buffer, const wxString &content,
                        int format) {
  wxCharBuffer utf8 = content.ToUTF8();
  const char *data = utf8.data();
  if (!data)
    return false;
  const size_t size = std::strlen(data);
  wxMemoryInputStream input(data, size);
  wxRichTextFileHandler *handler = wxRichTextBuffer::FindHandler(format);
  if (!handler)
    return false;
  return buffer.LoadFile(input, handler);
}

wxString SaveBufferToUtf8(wxRichTextBuffer &buffer, int format) {
  wxMemoryOutputStream output;
  wxRichTextFileHandler *handler = wxRichTextBuffer::FindHandler(format);
  if (!handler)
    return wxEmptyString;
  if (!buffer.SaveFile(output, handler))
    return wxEmptyString;
  const size_t size = output.GetSize();
  if (size == 0)
    return wxEmptyString;
  wxStreamBuffer *streamBuffer = output.GetOutputStreamBuffer();
  if (!streamBuffer)
    return wxEmptyString;
  const char *data =
      static_cast<const char *>(streamBuffer->GetBufferStart());
  return wxString::FromUTF8(data, size);
}

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
  if (LoadBufferFromUtf8(buffer, content, wxRICHTEXT_TYPE_XML))
    return true;
#if defined(wxRICHTEXT_TYPE_RICHTEXT)
  if (LoadBufferFromUtf8(buffer, content, wxRICHTEXT_TYPE_RICHTEXT))
    return true;
#endif
  return LoadBufferFromUtf8(buffer, content, wxRICHTEXT_TYPE_TEXT);
}

wxString SaveRichTextBufferToString(wxRichTextBuffer &buffer) {
  EnsureRichTextHandlers();
  wxString output = SaveBufferToUtf8(buffer, wxRICHTEXT_TYPE_XML);
  if (!output.empty())
    return output;
#if defined(wxRICHTEXT_TYPE_RICHTEXT)
  output = SaveBufferToUtf8(buffer, wxRICHTEXT_TYPE_RICHTEXT);
#else
  output = SaveBufferToUtf8(buffer, wxRICHTEXT_TYPE_TEXT);
#endif
  if (!output.empty())
    return output;
  return wxEmptyString;
}

wxImage RenderTextImage(const layouts::LayoutTextDefinition &text,
                        const wxSize &renderSize, const wxSize &logicalSize,
                        double renderScale) {
  if (renderSize.GetWidth() <= 0 || renderSize.GetHeight() <= 0 ||
      renderScale <= 0.0) {
    return wxImage();
  }

  const double adjustedScale =
      renderScale / layoutviewerpanel::detail::kTextRenderScale;
  if (adjustedScale <= 0.0) {
    return wxImage();
  }

  EnsureRichTextHandlers();

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
    buffer.AddParagraph(fallback);
  }

  wxString plainText = buffer.GetText();
  if (buffer.GetParagraphCount() <= 1 &&
      (plainText.Find('\n') != wxNOT_FOUND ||
       plainText.Find('\r') != wxNOT_FOUND)) {
    for (long i = static_cast<long>(plainText.length()) - 1; i >= 0; --i) {
      if (plainText[i] == '\n') {
        long deleteStart = i;
        if (i > 0 && plainText[i - 1] == '\r') {
          deleteStart = i - 1;
          --i;
        }
        buffer.DeleteRange(wxRichTextRange(deleteStart, i));
        buffer.InsertNewlineWithUndo(deleteStart, nullptr);
      } else if (plainText[i] == '\r') {
        buffer.DeleteRange(wxRichTextRange(i, i));
        buffer.InsertNewlineWithUndo(i, nullptr);
      }
    }
  }

  wxRichTextAttr baseStyle = buffer.GetDefaultStyle();
  wxString faceName = layoutviewerpanel::detail::ResolveSharedFontFaceName();
  if (!faceName.empty())
    baseStyle.SetFontFaceName(faceName);
  baseStyle.SetTextColour(*wxBLACK);
  baseStyle.SetFontEncoding(wxFONTENCODING_DEFAULT);
  baseStyle.SetFontFamily(wxFONTFAMILY_SWISS);
  baseStyle.SetParagraphSpacingBefore(0);
  baseStyle.SetParagraphSpacingAfter(0);
  if (!loaded || baseStyle.GetFontSize() <= 0)
    baseStyle.SetFontSize(layoutviewerpanel::detail::kTextDefaultFontSize);
  buffer.SetDefaultStyle(baseStyle);
  buffer.SetBasicStyle(baseStyle);
  if (buffer.GetRange().GetLength() > 0) {
    wxRichTextAttr overrideStyle;
    long flags = wxTEXT_ATTR_TEXT_COLOUR |
                 wxTEXT_ATTR_FONT_ENCODING |
                 wxTEXT_ATTR_PARAGRAPH_SPACING_BEFORE |
                 wxTEXT_ATTR_PARAGRAPH_SPACING_AFTER;
    if (!faceName.empty()) {
      overrideStyle.SetFontFaceName(faceName);
      flags |= wxTEXT_ATTR_FONT_FACE;
    }
    overrideStyle.SetTextColour(*wxBLACK);
    overrideStyle.SetFontEncoding(wxFONTENCODING_DEFAULT);
    overrideStyle.SetParagraphSpacingBefore(0);
    overrideStyle.SetParagraphSpacingAfter(0);
    overrideStyle.SetFlags(flags);
    buffer.SetStyle(buffer.GetRange(), overrideStyle);
  }

  const int padding = 4;
  const double logicalScale = layoutviewerpanel::detail::kTextRenderScale;
  const int logicalWidth = std::max(
      0, static_cast<int>(std::lround(logicalSize.GetWidth() * logicalScale)) -
             padding * 2);
  const int logicalHeight =
      std::max(0, static_cast<int>(std::lround(logicalSize.GetHeight() *
                                               logicalScale)) -
                     padding * 2);
  wxRect logicalRect(padding, padding, logicalWidth, logicalHeight);

  dc.SetUserScale(adjustedScale, adjustedScale);
  wxRichTextDrawingContext context(&buffer);
  wxRichTextSelection selection;
  buffer.Layout(dc, context, logicalRect, logicalRect,
                wxRICHTEXT_FIXED_WIDTH | wxRICHTEXT_FIXED_HEIGHT);
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
