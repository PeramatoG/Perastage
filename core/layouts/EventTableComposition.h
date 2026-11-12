#pragma once
#include "DocumentComposition.h"
#include "typography/composition.h"
#include <array>
#include <string>
#include <vector>

namespace layouts::composition {
// Persisted array order and English presentation are a compatibility contract.
struct EventTableField { const char *id; const char *label; };
inline constexpr std::array<EventTableField, 7> kEventTableFields = {{
    {"venue", "Venue:"}, {"location", "Location:"}, {"date", "Date:"},
    {"stage", "Stage:"}, {"version", "Version:"}, {"design", "Design:"}, {"mail", "Mail:"}}};
inline constexpr const char *kEventTableCompositionVersion = "event-table-v1-perastage-sans-v2";
struct EventTableFonts {
  const typography::TtfFontMetrics &regular;
  const typography::TtfFontMetrics &bold;
};
struct EventTableInput {
  double width = 0, height = 0;
  std::array<std::string, 7> values;
};
struct EventTableCell {
  std::string source; // Complete original UTF-8, including omitted glyphs/text.
  std::string displayed;
  TextRole role = TextRole::Normal;
  Rect clip;
  Point baseline;
  bool overflow = false;
  typography::Composition run;
};
struct EventTableRow { Rect frame; EventTableCell label, value; };
struct EventTablePlan {
  std::string fontIdentity = "PerastageSans-v2";
  Rect clip;
  double borderWidth = 0.5;
  double labelWidth = 0, valueX = 0, fontSize = 0, rowHeight = 0;
  std::array<EventTableRow, 7> rows;
  std::size_t unsupportedGlyphs = 0;
  bool overflow = false;
  std::vector<std::string> diagnostics; // At most 16, stable field/byte identities.
};
// One line per cell; retain the longest supported glyph prefix plus "...".
// Newlines terminate the visible line. No shaping or grapheme clustering is claimed.
// Invalid frames become empty clips; invalid fonts yield an empty, diagnosed plan.
EventTablePlan ComposeEventTable(const EventTableInput &, const EventTableFonts &);
} // namespace layouts::composition
