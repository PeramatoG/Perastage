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
#include <wx/log.h>
#include <wx/mstream.h>
#include <wx/richtext/richtextbuffer.h>
#include <wx/richtext/richtextxml.h>
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
enum class RichTextOpStatus { kSuccess, kNoHandler, kFailure };

const char *FormatName(int format) {
  switch (static_cast<wxRichTextFileType>(format)) {
    case wxRICHTEXT_TYPE_XML:
      return "XML";
#if defined(wxRICHTEXT_TYPE_RICHTEXT)
    case wxRICHTEXT_TYPE_RICHTEXT:
      return "RICHTEXT";
#endif
    case wxRICHTEXT_TYPE_TEXT:
      return "TEXT";
    default:
      return "UNKNOWN";
  }
}

RichTextOpStatus LoadBufferFromUtf8(wxRichTextBuffer &buffer,
                                   const wxString &content, int format) {
  wxCharBuffer utf8 = content.ToUTF8();
  const char *data = utf8.data();
  if (!data)
    return RichTextOpStatus::kFailure;
  const size_t size = utf8.length();
  if (size == 0)
    return RichTextOpStatus::kFailure;
  wxMemoryInputStream input(data, size);
  wxRichTextFileHandler *handler =
      wxRichTextBuffer::FindHandler(static_cast<wxRichTextFileType>(format));
  if (!handler)
    return RichTextOpStatus::kNoHandler;
  if (!handler->LoadFile(&buffer, input))
    return RichTextOpStatus::kFailure;
  return RichTextOpStatus::kSuccess;
}

struct RichTextSaveResult {
  wxString data;
  RichTextOpStatus status;
};

RichTextSaveResult SaveBufferToUtf8(wxRichTextBuffer &buffer, int format) {
  wxMemoryOutputStream output;
  wxRichTextFileHandler *handler =
      wxRichTextBuffer::FindHandler(static_cast<wxRichTextFileType>(format));
  if (!handler)
    return {wxEmptyString, RichTextOpStatus::kNoHandler};
  if (!handler->SaveFile(&buffer, output))
    return {wxEmptyString, RichTextOpStatus::kFailure};
  const size_t size = output.GetSize();
  if (size == 0)
    return {wxEmptyString, RichTextOpStatus::kFailure};
  wxStreamBuffer *streamBuffer = output.GetOutputStreamBuffer();
  if (!streamBuffer)
    return {wxEmptyString, RichTextOpStatus::kFailure};
  const char *data =
      static_cast<const char *>(streamBuffer->GetBufferStart());
  return {wxString::FromUTF8(data, size), RichTextOpStatus::kSuccess};
}

void EnsureRichTextHandlers() {
  static bool initialized = false;
  if (initialized)
    return;
  wxRichTextBuffer::InitStandardHandlers();
  if (!wxRichTextBuffer::FindHandler(wxRICHTEXT_TYPE_XML)) {
    wxRichTextBuffer::AddHandler(new wxRichTextXMLHandler);
  }
  initialized = true;
}
} // namespace

bool LoadRichTextBufferFromString(wxRichTextBuffer &buffer,
                                  const wxString &content) {
  if (content.empty())
    return false;
  EnsureRichTextHandlers();
  RichTextOpStatus status =
      LoadBufferFromUtf8(buffer, content, wxRICHTEXT_TYPE_XML);
  if (status == RichTextOpStatus::kSuccess)
    return true;
  if (status == RichTextOpStatus::kNoHandler) {
    wxLogWarning("No rich text handler found for %s format.",
                 FormatName(wxRICHTEXT_TYPE_XML));
  } else {
    wxLogWarning("Failed to load rich text buffer using %s format.",
                 FormatName(wxRICHTEXT_TYPE_XML));
  }
#if defined(wxRICHTEXT_TYPE_RICHTEXT)
  status = LoadBufferFromUtf8(buffer, content, wxRICHTEXT_TYPE_RICHTEXT);
  if (status == RichTextOpStatus::kSuccess)
    return true;
  if (status == RichTextOpStatus::kNoHandler) {
    wxLogWarning("No rich text handler found for %s format.",
                 FormatName(wxRICHTEXT_TYPE_RICHTEXT));
  } else {
    wxLogWarning("Failed to load rich text buffer using %s format.",
                 FormatName(wxRICHTEXT_TYPE_RICHTEXT));
  }
#endif
  status = LoadBufferFromUtf8(buffer, content, wxRICHTEXT_TYPE_TEXT);
  if (status == RichTextOpStatus::kSuccess)
    return true;
  if (status == RichTextOpStatus::kNoHandler) {
    wxLogWarning("No rich text handler found for %s format.",
                 FormatName(wxRICHTEXT_TYPE_TEXT));
  } else {
    wxLogWarning("Failed to load rich text buffer using %s format.",
                 FormatName(wxRICHTEXT_TYPE_TEXT));
  }
  return false;
}

wxString SaveRichTextBufferToString(wxRichTextBuffer &buffer) {
  EnsureRichTextHandlers();
  RichTextSaveResult output = SaveBufferToUtf8(buffer, wxRICHTEXT_TYPE_XML);
  if (output.status == RichTextOpStatus::kSuccess)
    return output.data;
  if (output.status == RichTextOpStatus::kNoHandler) {
    wxLogWarning("No rich text handler found for %s format.",
                 FormatName(wxRICHTEXT_TYPE_XML));
  } else {
    wxLogWarning("Failed to save rich text buffer using %s format.",
                 FormatName(wxRICHTEXT_TYPE_XML));
  }
#if defined(wxRICHTEXT_TYPE_RICHTEXT)
  output = SaveBufferToUtf8(buffer, wxRICHTEXT_TYPE_RICHTEXT);
  if (output.status == RichTextOpStatus::kSuccess)
    return output.data;
  if (output.status == RichTextOpStatus::kNoHandler) {
    wxLogWarning("No rich text handler found for %s format.",
                 FormatName(wxRICHTEXT_TYPE_RICHTEXT));
  } else {
    wxLogWarning("Failed to save rich text buffer using %s format.",
                 FormatName(wxRICHTEXT_TYPE_RICHTEXT));
  }
#endif
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
  wxString fallbackText;
  if (!text.richText.empty()) {
    loaded = LoadRichTextBufferFromString(
        buffer, wxString::FromUTF8(text.richText.data(),
                                   text.richText.size()));
  }
  if (!loaded) {
    fallbackText = text.text.empty() ? wxString("Light Plot")
                                     : wxString::FromUTF8(text.text.data(),
                                                          text.text.size());
  }

  wxRichTextAttr baseStyle = buffer.GetDefaultStyle();
  wxString faceName = layoutviewerpanel::detail::ResolveSharedFontFaceName();
  if (!faceName.empty())
    baseStyle.SetFontFaceName(faceName);
  baseStyle.SetTextColour(*wxBLACK);
  baseStyle.SetFontFamily(wxFONTFAMILY_SWISS);
  baseStyle.SetParagraphSpacingBefore(0);
  baseStyle.SetParagraphSpacingAfter(0);
  if (!loaded || baseStyle.GetFontSize() <= 0)
    baseStyle.SetFontSize(layoutviewerpanel::detail::kTextDefaultFontSize);
  buffer.SetDefaultStyle(baseStyle);
  buffer.SetBasicStyle(baseStyle);

  auto normalizeNewlines = [](wxString value) {
    value.Replace("\r\n", "\n");
    value.Replace("\r", "\n");
    return value;
  };

  auto rebuildBufferFromPlainText = [&](const wxString &plainText,
                                        const wxRichTextAttr *style) {
    buffer.Clear();
    buffer.SetDefaultStyle(baseStyle);
    buffer.SetBasicStyle(baseStyle);
    if (plainText.empty()) {
      buffer.AddParagraph(wxString());
    } else {
      wxStringTokenizer tokenizer(plainText, "\n", wxTOKEN_RET_EMPTY_ALL);
      while (tokenizer.HasMoreTokens()) {
        buffer.AddParagraph(tokenizer.GetNextToken());
      }
    }
    if (style && buffer.GetRange().GetLength() > 0) {
      buffer.SetStyle(buffer.GetRange(), *style);
    }
  };

  if (loaded && buffer.GetParagraphCount() == 1) {
    wxString plainText = normalizeNewlines(buffer.GetText());
    wxString fallbackPlain;
    if (!text.text.empty()) {
      fallbackPlain =
          normalizeNewlines(wxString::FromUTF8(text.text.data(),
                                               text.text.size()));
    }
    if (plainText.Find('\n') == wxNOT_FOUND &&
        fallbackPlain.Find('\n') != wxNOT_FOUND) {
      plainText = fallbackPlain;
    }
    if (plainText.Find('\n') != wxNOT_FOUND) {
      wxRichTextAttr firstStyle;
      const bool hasStyle = buffer.GetRange().GetLength() > 0 &&
                            buffer.GetStyle(0, firstStyle);
      rebuildBufferFromPlainText(plainText, hasStyle ? &firstStyle : nullptr);
    }
  }

  if (!loaded) {
    wxString plainText = normalizeNewlines(fallbackText);
    rebuildBufferFromPlainText(plainText, nullptr);
  }
  if (buffer.GetRange().GetLength() > 0) {
    wxRichTextAttr overrideStyle;
    long flags = wxTEXT_ATTR_TEXT_COLOUR;
    if (!faceName.empty()) {
      overrideStyle.SetFontFaceName(faceName);
      flags |= wxTEXT_ATTR_FONT_FACE;
    }
    overrideStyle.SetTextColour(*wxBLACK);
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
  buffer.Layout(dc, context, logicalRect, logicalRect, wxRICHTEXT_FIXED_WIDTH);
  dc.SetClippingRegion(logicalRect);
  buffer.Draw(dc, context, buffer.GetRange(), selection, logicalRect, 0, 0);
  dc.DestroyClippingRegion();

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
