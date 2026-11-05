#include "EventTableComposition.h"
#include <algorithm>
#include <cmath>

namespace layouts::composition {
namespace {
double Nonnegative(double v) { return std::isfinite(v) ? std::max(0.0, v) : 0; }
}
EventTablePlan ComposeEventTable(const EventTableInput &in, const EventTableFonts &fonts) {
  EventTablePlan plan;
  plan.clip = {0, 0, Nonnegative(in.width), Nonnegative(in.height)};
  auto diagnose = [&](const std::string &message) {
    if (plan.diagnostics.size() < 16) plan.diagnostics.push_back(message);
  };
  const bool valid = fonts.regular.valid && fonts.bold.valid &&
      fonts.regular.unitsPerEm > 0 && fonts.bold.unitsPerEm > 0;
  if (!valid) diagnose("Event Table bundled PerastageSans metrics unavailable; text hidden.");
  const double padX = std::min(6.0, plan.clip.width / 2);
  const double padY = std::min(6.0, plan.clip.height / 2);
  plan.rowHeight = (plan.clip.height - 2 * padY) / 7;
  plan.fontSize = std::clamp(plan.rowHeight - 2, 6.0, 14.0) * (2.0 / 3.0) * 0.7;
  auto compose = [&](const std::string &text, double size, TextRole role) {
    return typography::Compose(text, {size, typography::TextUnit::PaperPoints,
        role == TextRole::Bold, 0, 0}, role == TextRole::Bold ? fonts.bold : fonts.regular);
  };
  if (valid) {
    for (const auto &field : kEventTableFields)
      plan.labelWidth = std::max(plan.labelWidth,
          compose(field.label, plan.fontSize, TextRole::Bold).lines.front().width);
  }
  plan.labelWidth = std::min(plan.labelWidth, Nonnegative(plan.clip.width - 2 * padX));
  plan.valueX = std::min(plan.clip.width - padX, padX + plan.labelWidth + 10);
  for (std::size_t i = 0; i < plan.rows.size(); ++i) {
    auto &row = plan.rows[i];
    row.frame = {0, padY + i * plan.rowHeight, plan.clip.width, plan.rowHeight};
    auto cell = [&](EventTableCell &out, const std::string &source, bool bold,
                    double x, double width, double size) {
      out.source = source;
      out.role = bold ? TextRole::Bold : TextRole::Normal;
      out.clip = {x, row.frame.y, width, row.frame.height};
      if (!valid) { out.overflow = !source.empty(); plan.overflow |= out.overflow; return; }
      const auto full = compose(source, size, out.role);
      plan.unsupportedGlyphs += full.unsupportedBytes.size();
      for (auto offset : full.unsupportedBytes)
        diagnose(std::string("Event Table ") + kEventTableFields[i].id +
            " unsupported UTF-8 at byte " + std::to_string(offset) +
            "; glyph omitted on preview and PDF; source retained.");
      const auto &line = full.lines.front();
      out.overflow = full.lines.size() > 1 || line.width > width + 1e-9 ||
          (!source.empty() && full.ascent + full.descent > out.clip.height + 1e-9);
      const auto dots = compose("...", size, out.role);
      const double suffixWidth = out.overflow ? dots.lines.front().width : 0;
      double used = 0;
      // Glyphs carry complete decoded UTF-8 sequences; never slice source bytes.
      for (std::size_t j = line.first; j < line.first + line.count; ++j) {
        const auto &g = full.glyphs[j];
        if (used + g.advance + suffixWidth > width + 1e-9) break;
        out.displayed += g.text; used += g.advance;
      }
      if (out.overflow && suffixWidth <= width) out.displayed += "...";
      out.run = compose(out.displayed, size, out.role);
      out.baseline = {x, row.frame.y + (row.frame.height - full.ascent - full.descent) / 2 + full.ascent};
      if (out.overflow) diagnose(std::string("Event Table ") + kEventTableFields[i].id +
          " cell overflow; hidden text retained in source.");
      plan.overflow |= out.overflow;
    };
    cell(row.label, kEventTableFields[i].label, true, padX, plan.labelWidth, plan.fontSize);
    cell(row.value, in.values[i], i == 0, plan.valueX,
         Nonnegative(plan.clip.width - padX - plan.valueX),
         i == 0 ? std::max(plan.fontSize + 1, plan.fontSize * 1.1) : plan.fontSize);
  }
  return plan;
}
} // namespace layouts::composition
