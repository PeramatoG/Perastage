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

wxFont MakeRenderFont(int sizePx, bool bold, bool italic,
                      const wxString &faceName) {
  const wxFontWeight weight =
      bold ? wxFONTWEIGHT_BOLD : wxFONTWEIGHT_NORMAL;
  const wxFontStyle style =
      italic ? wxFONTSTYLE_ITALIC : wxFONTSTYLE_NORMAL;
  wxFont font;
  if (!faceName.empty()) {
    font = wxFont(sizePx, wxFONTFAMILY_SWISS, style, weight, false, faceName);
  } else {
    font = wxFont(sizePx, wxFONTFAMILY_SWISS, style, weight);
  }
  font.SetEncoding(wxFONTENCODING_UTF8);
  return font;
}

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

LayoutTextExportData BuildLayoutTextExportData(
    const layouts::LayoutTextDefinition &text, double scaleX, double scaleY) {
  LayoutTextExportData data;
  layouts::Layout2DViewFrame frame = text.frame;
  frame.x = static_cast<int>(std::lround(frame.x * scaleX));
  frame.y = static_cast<int>(std::lround(frame.y * scaleY));
  frame.width = static_cast<int>(std::lround(frame.width * scaleX));
  frame.height = static_cast<int>(std::lround(frame.height * scaleY));
  data.frame = frame;
  data.zIndex = text.zIndex;
  data.solidBackground = text.solidBackground;
  data.drawFrame = text.drawFrame;

  wxRichTextBuffer buffer;
  bool loaded = false;
  if (!text.richText.empty()) {
    loaded = LoadRichTextBufferFromString(
        buffer, wxString::FromUTF8(text.richText.data(),
                                   text.richText.size()));
  }

  wxString plainText;
  if (loaded) {
    plainText = buffer.GetText();
  }
  if (plainText.empty()) {
    plainText = text.text.empty() ? wxString("Light Plot")
                                  : wxString::FromUTF8(text.text.data(),
                                                       text.text.size());
  }
  plainText.Replace("\r\n", "\n");
  plainText.Replace("\r", "\n");

  wxRichTextAttr style;
  if (loaded && buffer.GetRange().GetLength() > 0) {
    buffer.GetStyle(0, style);
  }
  const int fontSize = style.GetFontSize() > 0
                           ? style.GetFontSize()
                           : layoutviewerpanel::detail::kTextDefaultFontSize;
  data.fontSize = fontSize;
  data.bold = style.GetFontWeight() >= wxFONTWEIGHT_BOLD;
  data.italic = style.GetFontStyle() == wxFONTSTYLE_ITALIC ||
                style.GetFontStyle() == wxFONTSTYLE_SLANT;
  switch (style.GetAlignment()) {
  case wxTEXT_ALIGNMENT_CENTRE:
    data.alignment = LayoutTextExportData::Alignment::Center;
    break;
  case wxTEXT_ALIGNMENT_RIGHT:
    data.alignment = LayoutTextExportData::Alignment::Right;
    break;
  case wxTEXT_ALIGNMENT_JUSTIFIED:
    data.alignment = LayoutTextExportData::Alignment::Justified;
    break;
  case wxTEXT_ALIGNMENT_LEFT:
  default:
    data.alignment = LayoutTextExportData::Alignment::Left;
    break;
  }

  wxStringTokenizer tokenizer(plainText, "\n", wxTOKEN_RET_EMPTY_ALL);
  if (loaded) {
    wxString content = buffer.GetText();
    if (content.empty()) {
      content = plainText;
    }
    const size_t length = static_cast<size_t>(content.length());
    LayoutTextExportData::Line currentLine;
    LayoutTextExportData::Run currentRun;
    bool hasRun = false;
    auto finalizeRun = [&]() {
      if (!hasRun)
        return;
      if (!currentRun.text.empty()) {
        currentLine.runs.push_back(std::move(currentRun));
      }
      currentRun = LayoutTextExportData::Run{};
      hasRun = false;
    };
    auto finalizeLine = [&]() {
      finalizeRun();
      data.lines.push_back(currentLine);
      currentLine = LayoutTextExportData::Line{};
    };
    for (size_t i = 0; i < length; ++i) {
      const wxChar ch = content[i];
      if (ch == '\r')
        continue;
      if (ch == '\n') {
        finalizeLine();
        continue;
      }
      wxRichTextAttr runStyle;
      if (!buffer.GetStyle(static_cast<long>(i), runStyle)) {
        runStyle = buffer.GetDefaultStyle();
      }
      const int runFontSize =
          runStyle.GetFontSize() > 0
              ? runStyle.GetFontSize()
              : layoutviewerpanel::detail::kTextDefaultFontSize;
      const bool runBold =
          runStyle.GetFontWeight() >= wxFONTWEIGHT_BOLD;
      const bool runItalic =
          runStyle.GetFontStyle() == wxFONTSTYLE_ITALIC ||
          runStyle.GetFontStyle() == wxFONTSTYLE_SLANT;
      if (!hasRun || currentRun.fontSize != runFontSize ||
          currentRun.bold != runBold || currentRun.italic != runItalic) {
        finalizeRun();
        currentRun.fontSize = runFontSize;
        currentRun.bold = runBold;
        currentRun.italic = runItalic;
        hasRun = true;
      }
      wxCharBuffer utf8 = wxString(ch).ToUTF8();
      if (utf8.data())
        currentRun.text.append(utf8.data(), utf8.length());
    }
    finalizeLine();
  } else {
    bool wroteLine = false;
    while (tokenizer.HasMoreTokens()) {
      wxCharBuffer utf8 = tokenizer.GetNextToken().ToUTF8();
      LayoutTextExportData::Line line;
      LayoutTextExportData::Run run;
      run.text = utf8.data() ? utf8.data() : "";
      run.fontSize = data.fontSize;
      run.bold = data.bold;
      run.italic = data.italic;
      line.runs.push_back(std::move(run));
      data.lines.push_back(std::move(line));
      wroteLine = true;
    }
    if (!wroteLine) {
      data.lines.emplace_back();
    }
  }
  return data;
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

  const LayoutTextExportData data =
      BuildLayoutTextExportData(text, 1.0, 1.0);
  wxString faceName = layoutviewerpanel::detail::ResolveSharedFontFaceName();

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
  const double availableHeight =
      static_cast<double>(logicalRect.GetHeight());
  double usedHeight = 0.0;
  const int defaultFontSize =
      layoutviewerpanel::detail::kTextDefaultFontSize;
  for (const auto &line : data.lines) {
    double lineFontSize =
        data.fontSize > 0 ? static_cast<double>(data.fontSize)
                          : static_cast<double>(defaultFontSize);
    for (const auto &run : line.runs) {
      const double runSize =
          run.fontSize > 0 ? static_cast<double>(run.fontSize) : lineFontSize;
      lineFontSize = std::max(lineFontSize, runSize);
    }
    const double lineHeight = lineFontSize * 1.2;
    if (usedHeight + lineHeight > availableHeight && usedHeight > 0.0)
      break;

    double lineWidth = 0.0;
    for (const auto &run : line.runs) {
      const int runSize =
          run.fontSize > 0 ? run.fontSize : static_cast<int>(lineFontSize);
      dc.SetFont(MakeRenderFont(runSize, run.bold, run.italic, faceName));
      const wxString runText = wxString::FromUTF8(run.text);
      int runWidth = 0;
      int runHeight = 0;
      dc.GetTextExtent(runText, &runWidth, &runHeight);
      lineWidth += static_cast<double>(runWidth);
    }

    double x = static_cast<double>(logicalRect.GetX());
    if (data.alignment == LayoutTextExportData::Alignment::Center) {
      x += std::max(0.0, (logicalRect.GetWidth() - lineWidth) * 0.5);
    } else if (data.alignment == LayoutTextExportData::Alignment::Right) {
      x += logicalRect.GetWidth() - lineWidth;
    }
    const double y = static_cast<double>(logicalRect.GetY()) + usedHeight;
    double cursorX = x;
    if (line.runs.empty()) {
      usedHeight += lineHeight;
      continue;
    }
    for (const auto &run : line.runs) {
      const int runSize =
          run.fontSize > 0 ? run.fontSize : static_cast<int>(lineFontSize);
      dc.SetFont(MakeRenderFont(runSize, run.bold, run.italic, faceName));
      const wxString runText = wxString::FromUTF8(run.text);
      int runWidth = 0;
      int runHeight = 0;
      dc.GetTextExtent(runText, &runWidth, &runHeight);
      dc.DrawText(runText, cursorX, y);
      cursorX += static_cast<double>(runWidth);
    }
    usedHeight += lineHeight;
  }

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
