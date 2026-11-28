// This file is part of Perastage. License: GPL-3.0-or-later.
#pragma once

#include "layouttextutils.h"
#include <optional>
#include <variant>

namespace layouttext {
struct FontEdit { std::string reference; };
struct SizeEdit { int points; };
struct BoldEdit { bool enabled; };
struct ItalicEdit { bool enabled; };
using TypographyEdit = std::variant<FontEdit, SizeEdit, BoldEdit, ItalicEdit,
                                    wxColour, LayoutTextExportData::Alignment>;
struct TypographyInspection {
  std::string content;
  std::optional<std::string> font;
  std::optional<int> size;
  std::optional<bool> bold, italic;
  std::optional<wxColour> color;
  std::optional<LayoutTextExportData::Alignment> alignment;
  bool editable = false;
};
TypographyInspection InspectTypography(const layouts::LayoutTextDefinition &text);
bool ApplyTypography(layouts::LayoutTextDefinition &text, const TypographyEdit &edit);
} // namespace layouttext
