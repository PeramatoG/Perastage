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
#include "document_text_renderer.h"
#include "projectutils.h"
#include "typography/font_resources.h"
#include "typography/glyph_outline.h"
#include <wx/graphics.h>

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

namespace layouttext {
namespace {
enum class RichTextOpStatus { kSuccess, kNoHandler, kFailure };

wxString FormatName(int format) {
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
    wxLogWarning("No rich text handler found for " +
                 FormatName(wxRICHTEXT_TYPE_XML) + " format.");
  } else {
    wxLogWarning("Failed to load rich text buffer using " +
                 FormatName(wxRICHTEXT_TYPE_XML) + " format.");
  }
#if defined(wxRICHTEXT_TYPE_RICHTEXT)
  status = LoadBufferFromUtf8(buffer, content, wxRICHTEXT_TYPE_RICHTEXT);
  if (status == RichTextOpStatus::kSuccess)
    return true;
  if (status == RichTextOpStatus::kNoHandler) {
    wxLogWarning("No rich text handler found for " +
                 FormatName(wxRICHTEXT_TYPE_RICHTEXT) + " format.");
  } else {
    wxLogWarning("Failed to load rich text buffer using " +
                 FormatName(wxRICHTEXT_TYPE_RICHTEXT) + " format.");
  }
#endif
  status = LoadBufferFromUtf8(buffer, content, wxRICHTEXT_TYPE_TEXT);
  if (status == RichTextOpStatus::kSuccess)
    return true;
  if (status == RichTextOpStatus::kNoHandler) {
    wxLogWarning("No rich text handler found for " +
                 FormatName(wxRICHTEXT_TYPE_TEXT) + " format.");
  } else {
    wxLogWarning("Failed to load rich text buffer using " +
                 FormatName(wxRICHTEXT_TYPE_TEXT) + " format.");
  }
  return false;
}

wxString SaveRichTextBufferToString(wxRichTextBuffer &buffer) {
  EnsureRichTextHandlers();
  RichTextSaveResult output = SaveBufferToUtf8(buffer, wxRICHTEXT_TYPE_XML);
  if (output.status == RichTextOpStatus::kSuccess)
    return output.data;
  if (output.status == RichTextOpStatus::kNoHandler) {
    wxLogWarning("No rich text handler found for " +
                 FormatName(wxRICHTEXT_TYPE_XML) + " format.");
  } else {
    wxLogWarning("Failed to save rich text buffer using " +
                 FormatName(wxRICHTEXT_TYPE_XML) + " format.");
  }
#if defined(wxRICHTEXT_TYPE_RICHTEXT)
  output = SaveBufferToUtf8(buffer, wxRICHTEXT_TYPE_RICHTEXT);
  if (output.status == RichTextOpStatus::kSuccess)
    return output.data;
  if (output.status == RichTextOpStatus::kNoHandler) {
    wxLogWarning("No rich text handler found for " +
                 FormatName(wxRICHTEXT_TYPE_RICHTEXT) + " format.");
  } else {
    wxLogWarning("Failed to save rich text buffer using " +
                 FormatName(wxRICHTEXT_TYPE_RICHTEXT) + " format.");
  }
#endif
  return wxEmptyString;
}

LayoutTextExportData BuildLayoutTextExportData(
    const layouts::LayoutTextDefinition &text) {
  LayoutTextExportData data;
  data.frame = text.frame;
  data.zIndex = text.zIndex;
  data.solidBackground = text.solidBackground;
  data.drawFrame = text.drawFrame;
  data.wrap = text.wrapText.value_or(false);

  wxRichTextBuffer buffer;
  const bool loaded = !text.richText.empty() && LoadRichTextBufferFromString(
      buffer, wxString::FromUTF8(text.richText.data(), text.richText.size()));
  using Alignment = LayoutTextExportData::Alignment;
  const auto alignment = [](wxTextAttrAlignment value) {
    switch (value) {
    case wxTEXT_ALIGNMENT_CENTRE: return Alignment::Center;
    case wxTEXT_ALIGNMENT_RIGHT: return Alignment::Right;
    case wxTEXT_ALIGNMENT_JUSTIFIED: return Alignment::Justified;
    default: return Alignment::Left;
    }
  };
  const auto toUtf8 = [](const wxString &value) {
    const auto bytes = value.ToUTF8();
    return bytes.data() ? std::string(bytes.data(), bytes.length()) : std::string{};
  };
  const auto makeRun = [&](const wxString &value, const wxRichTextAttr &style,
                           const wxRichTextProperties *properties = nullptr) {
    LayoutTextExportData::Run run;
    run.text = toUtf8(value);
    run.fontSize = style.GetFontSize() > 0 ? style.GetFontSize() : data.fontSize;
    run.bold = style.GetFontWeight() >= wxFONTWEIGHT_BOLD;
    run.italic = style.GetFontStyle() == wxFONTSTYLE_ITALIC || style.GetFontStyle() == wxFONTSTYLE_SLANT;
    run.family = toUtf8(style.GetFontFaceName());
    if (properties) {
      const auto reference = toUtf8(properties->GetPropertyString(kInstalledFontReferenceProperty));
      if (!reference.empty() && typography::DecodeFontReference(reference).family == run.family)
        run.family = reference;
    }
    // Preserve historical defaults independently of the installed font catalog.
    if (!text.wrapText && typography::FontCatalog::LegacyWrapDefault(run.family)) data.wrap = true;
    if (style.GetTextColour().IsOk()) {
      const auto ink = style.GetTextColour();
      run.color = {ink.Red(), ink.Green(), ink.Blue()};
    }
    run.underline = style.GetFontUnderlined();
    run.strikethrough = style.GetFontStrikethrough();
    return run;
  };

  if (loaded && buffer.GetChildCount() > 0) {
    const auto defaults = buffer.GetBasicStyle();
    constexpr double tenthsMmToPoints = 72.0 / 254;
    for (size_t p = 0; p < buffer.GetChildCount(); ++p) {
      const auto *paragraph = wxDynamicCast(buffer.GetChild(p), wxRichTextParagraph);
      if (!paragraph) { data.containsUnsupportedObjects = true; continue; }
      auto style = defaults;
      style.Apply(paragraph->GetAttributes());
      LayoutTextExportData::Line line;
      line.alignment = alignment(style.GetAlignment());
      line.spaceBefore = style.GetParagraphSpacingBefore() * tenthsMmToPoints;
      line.spaceAfter = style.GetParagraphSpacingAfter() * tenthsMmToPoints;
      line.lineSpacing = style.GetLineSpacing() > 0 ? style.GetLineSpacing() / 10.0 : 1.0;
      line.leftIndent = (style.GetLeftIndent() + style.GetLeftSubIndent()) * tenthsMmToPoints;
      line.firstLineIndent = -style.GetLeftSubIndent() * tenthsMmToPoints;
      line.rightIndent = style.GetRightIndent() * tenthsMmToPoints;
      for (size_t r = 0; r < paragraph->GetChildCount(); ++r) {
        const auto *span = wxDynamicCast(paragraph->GetChild(r), wxRichTextPlainText);
        if (!span) { data.containsUnsupportedObjects = true; continue; }
        auto runStyle = style;
        runStyle.Apply(span->GetAttributes());
        wxString part = span->GetText();
        part.Replace(wxString(wxRichTextLineBreakChar), "\n");
        part.Replace("\r\n", "\n");
        part.Replace("\r", "\n");
        while (part.Contains("\n")) {
          const auto split = part.Find('\n');
          line.runs.push_back(makeRun(part.Left(split), runStyle, &span->GetProperties()));
          auto next = line;
          next.runs.clear();
          next.spaceBefore = 0;
          next.firstLineIndent = 0;
          line.spaceAfter = 0;
          data.lines.push_back(std::move(line));
          line = std::move(next);
          part = part.Mid(split + 1);
        }
        line.runs.push_back(makeRun(part, runStyle, &span->GetProperties()));
      }
      if (line.runs.empty()) line.runs.push_back(makeRun({}, style));
      data.lines.push_back(std::move(line));
    }
    return data;
  }

  auto plain = text.text.empty() ? wxString("Light Plot") : wxString::FromUTF8(text.text);
  plain.Replace("\r\n", "\n");
  plain.Replace("\r", "\n");
  wxStringTokenizer tokenizer(plain, "\n", wxTOKEN_RET_EMPTY_ALL);
  while (tokenizer.HasMoreTokens())
    data.lines.push_back({{makeRun(tokenizer.GetNextToken(), {})}});
  return data;
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
  const auto data = BuildLayoutTextExportData(text);
  const auto catalog = typography::GetFontCatalog(ProjectUtils::GetResourceRoot());
  const auto plan = typography::ComposeTextBlock(data, logicalSize.x, logicalSize.y, *catalog);
  for (const auto &message : plan.diagnostics) gui::ReportDocumentTypographyDiagnostic(message);
  if (auto *gc = dc.GetGraphicsContext()) {
    gc->PushState();
    gc->Scale(renderScale, renderScale);
    for (const auto &entry : plan.glyphs) {
      const auto outline = typography::ResolveGlyphOutline(*catalog, entry.glyph.resourceId, entry.glyph.glyphId);
      if (!outline || outline->substituted)
        gui::ReportDocumentTypographyDiagnostic("A document glyph contour is unavailable in the approved face.");
      if (!outline) continue;
      gc->SetBrush(wxBrush(wxColour(entry.color[0], entry.color[1], entry.color[2])));
      gc->FillPath(gui::BuildDocumentGlyphPath(*gc, *outline, entry.x, entry.baseline, entry.size),
                   wxWINDING_RULE);
    }
    gc->SetPen(*wxTRANSPARENT_PEN);
    for (const auto &line : plan.decorations) {
      gc->SetBrush(wxBrush(wxColour(line.color[0], line.color[1], line.color[2])));
      gc->DrawRectangle(line.x, line.y, line.width, line.height);
    }
    gc->PopState();
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
