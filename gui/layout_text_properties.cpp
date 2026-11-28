// This file is part of Perastage. License: GPL-3.0-or-later.
#include "layout_text_properties.h"
#include "layouttext_legacy_xml.h"
#include "typography/font_resources.h"

#include <algorithm>
#include <array>
#include <wx/log.h>
#include <wx/sstream.h>
#include <wx/xml/xml.h>

namespace layouttext {
namespace {
using Alignment = LayoutTextExportData::Alignment;

bool HasSupportedAttributes(const wxXmlNode &node) {
  const std::array attributes = {
      "version", "xmlns", "partialparagraph", "name", "type", "value",
      "fontface", "fontfamily", "fontpointsize", "fontweight", "fontstyle",
      "fontunderlined", "fontstrikethrough", "textcolor", "bgcolor",
      "alignment", "leftindent", "leftsubindent", "rightindent",
      "parspacingbefore", "parspacingafter", "linespacing",
      "margin-left", "margin-right", "margin-top", "margin-bottom"};
  for (const auto *attr = node.GetAttributes(); attr; attr = attr->GetNext())
    if (std::none_of(attributes.begin(), attributes.end(),
                     [&](const char *name) { return attr->GetName() == name; }))
      return false;
  return true;
}

// Native text and the installed-font reference are the only XML objects this
// editor modifies. Archive comments and unfamiliar document data stay in Edit Text.
bool IsSupportedNode(const wxXmlNode *node) {
  for (; node; node = node->GetNext()) {
    if (node->GetType() == wxXML_TEXT_NODE || node->GetType() == wxXML_CDATA_SECTION_NODE)
      continue;
    if (node->GetType() != wxXML_ELEMENT_NODE) return false;
    const auto &name = node->GetName();
    if (name != "richtext" && name != "paragraphlayout" && name != "paragraph" &&
        name != "text" && name != "symbol" && name != "properties" && name != "property") return false;
    if (name == "property" && node->GetAttribute("name") != kInstalledFontReferenceProperty)
      return false;
    if (!HasSupportedAttributes(*node) || !IsSupportedNode(node->GetChildren())) return false;
  }
  return true;
}

bool CanEditSource(const wxString &source) {
  if (source.empty()) return true;
  const wxLogNull quiet;
  wxStringInputStream input(source);
  wxXmlDocument document;
  return document.Load(input) && IsSupportedNode(document.GetDocumentNode()->GetChildren());
}

template <class T, class Range, class Read>
std::optional<T> Common(const Range &range, Read read) {
  if (range.empty()) return std::nullopt;
  const T value = read(range.front());
  if (std::all_of(range.begin(), range.end(), [&](const auto &item) { return read(item) == value; }))
    return value;
  return std::nullopt;
}

wxTextAttrAlignment ToWx(Alignment a) {
  switch (a) {
  case Alignment::Left: return wxTEXT_ALIGNMENT_LEFT;
  case Alignment::Center: return wxTEXT_ALIGNMENT_CENTRE;
  case Alignment::Right: return wxTEXT_ALIGNMENT_RIGHT;
  case Alignment::Justified: return wxTEXT_ALIGNMENT_JUSTIFIED;
  }
  return wxTEXT_ALIGNMENT_LEFT;
}

bool Matches(const TypographyInspection &style, const TypographyEdit &edit) {
  return std::visit([&](const auto &value) {
    using T = std::decay_t<decltype(value)>;
    if constexpr (std::is_same_v<T, FontEdit>) return style.font == value.reference;
    else if constexpr (std::is_same_v<T, SizeEdit>) return style.size == value.points;
    else if constexpr (std::is_same_v<T, BoldEdit>) return style.bold == value.enabled;
    else if constexpr (std::is_same_v<T, ItalicEdit>) return style.italic == value.enabled;
    else if constexpr (std::is_same_v<T, wxColour>) return style.color == value;
    else return style.alignment == value;
  }, edit);
}
} // namespace

TypographyInspection InspectTypography(const layouts::LayoutTextDefinition &text) {
  TypographyInspection result;
  const auto data = BuildLayoutTextExportData(text);
  std::vector<const LayoutTextExportData::Run *> runs;
  bool first = true;
  for (const auto &line : data.lines) {
    if (!first) result.content += '\n';
    first = false;
    for (const auto &run : line.runs) {
      result.content += run.text;
      runs.push_back(&run);
    }
  }
  result.font = Common<std::string>(runs, [](auto *run) { return run->family; });
  result.size = Common<int>(runs, [](auto *run) { return run->fontSize; });
  result.bold = Common<bool>(runs, [](auto *run) { return run->bold; });
  result.italic = Common<bool>(runs, [](auto *run) { return run->italic; });
  result.color = Common<wxColour>(runs, [](auto *run) {
    return wxColour(run->color[0], run->color[1], run->color[2]);
  });
  result.alignment = Common<Alignment>(data.lines, [&](const auto &line) {
    return line.alignment.value_or(data.alignment);
  });
  result.editable = !data.containsUnsupportedObjects && result.font && result.size &&
      result.bold.has_value() && result.italic.has_value() && result.color && result.alignment &&
      CanEditSource(wxString::FromUTF8(text.richText));
  return result;
}

bool ApplyTypography(layouts::LayoutTextDefinition &text, const TypographyEdit &edit) {
  const auto style = InspectTypography(text);
  if (!style.editable) return false;
  if (Matches(style, edit)) return true;
  if (const auto *size = std::get_if<SizeEdit>(&edit); size && (size->points < 6 || size->points > 72))
    return false;
  const auto source = wxString::FromUTF8(text.richText);
  wxRichTextBuffer buffer;
  if (source.empty()) {
    buffer.Reset();
    buffer.DeleteChildren();
    buffer.AddParagraphs(wxString::FromUTF8(style.content));
  } else {
    if (!LoadRichTextBufferFromString(buffer, source)) return false;
    const auto roundTrip = SaveRichTextBufferToString(buffer);
    if (roundTrip.empty() || !LegacyRichTextTail(source, roundTrip).empty()) return false;
  }
  const auto *ink = std::get_if<wxColour>(&edit);
  if (ink && !ink->IsOk()) return false;
  if (ink && !text.useRichTextColors) NormalizeLegacyTextColors(buffer);

  const auto *alignment = std::get_if<Alignment>(&edit);
  for (size_t p = 0; p < buffer.GetChildCount(); ++p) {
    auto *paragraph = wxDynamicCast(buffer.GetChild(p), wxRichTextParagraph);
    if (!paragraph) return false;
    if (alignment) {
      paragraph->GetAttributes().SetAlignment(ToWx(*alignment));
      continue;
    }
    for (size_t r = 0; r < paragraph->GetChildCount(); ++r) {
      auto *span = wxDynamicCast(paragraph->GetChild(r), wxRichTextPlainText);
      if (!span) return false;
      auto &attr = span->GetAttributes();
      if (const auto *font = std::get_if<FontEdit>(&edit)) {
        auto family = typography::DecodeFontReference(font->reference).family;
        if (family.empty() || family == typography::kDefaultFamily) family = "Noto Sans";
        attr.SetFontFaceName(wxString::FromUTF8(family.data(), family.size()));
        span->GetProperties().Remove(kInstalledFontReferenceProperty);
        if (family != font->reference && !font->reference.empty() &&
            font->reference != typography::kDefaultFamily)
          span->GetProperties().SetProperty(kInstalledFontReferenceProperty,
                                           wxString::FromUTF8(font->reference));
      } else if (const auto *size = std::get_if<SizeEdit>(&edit)) {
        attr.SetFontSize(size->points);
      } else if (const auto *bold = std::get_if<BoldEdit>(&edit)) {
        attr.SetFontWeight(bold->enabled ? wxFONTWEIGHT_BOLD : wxFONTWEIGHT_NORMAL);
      } else if (const auto *italic = std::get_if<ItalicEdit>(&edit)) {
        attr.SetFontStyle(italic->enabled ? wxFONTSTYLE_ITALIC : wxFONTSTYLE_NORMAL);
      } else if (ink) {
        attr.SetTextColour(*ink);
      }
    }
  }
  const auto xml = SaveRichTextBufferToString(buffer);
  if (xml.empty()) return false;
  auto candidate = text;
  candidate.richText = xml.utf8_string();
  candidate.text = style.content;
  candidate.useRichTextColors = text.useRichTextColors || ink != nullptr;
  const auto updated = InspectTypography(candidate);
  if (updated.content != style.content ||
      (!Matches(updated, edit) && !std::holds_alternative<FontEdit>(edit))) return false;
  candidate.wrapText = text.wrapText;
  if (!candidate.wrapText && BuildLayoutTextExportData(candidate).wrap != BuildLayoutTextExportData(text).wrap)
    candidate.wrapText = BuildLayoutTextExportData(text).wrap;
  text = std::move(candidate);
  return true;
}
} // namespace layouttext
