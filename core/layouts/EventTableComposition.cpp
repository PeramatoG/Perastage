#include "EventTableComposition.h"
#include "typography/font_resources.h"
#include <algorithm>
#include <cmath>
#include <map>

namespace layouts::composition {
namespace {
double Nonnegative(double v) { return std::isfinite(v) ? std::max(0.0, v) : 0; }
}
EventTablePlan ComposeEventTable(const EventTableInput &in, const EventTableFonts &fonts) {
  EventTablePlan plan;
  plan.clip = {0, 0, Nonnegative(in.width), Nonnegative(in.height)};
  auto diagnose = [&](const std::string &message) {
    if (plan.diagnostics.size() < 16 &&
        std::find(plan.diagnostics.begin(), plan.diagnostics.end(), message) == plan.diagnostics.end())
      plan.diagnostics.push_back(message);
  };
  const bool valid = fonts.catalog ?
      fonts.catalog->Resolve(fonts.family, false).face && fonts.catalog->Resolve(fonts.family, true).face :
      fonts.regular.valid && fonts.bold.valid && fonts.regular.unitsPerEm > 0 && fonts.bold.unitsPerEm > 0;
  if (!valid) diagnose("Event Table bundled PerastageSans metrics unavailable; text hidden.");
  const double padX = std::min(6.0, plan.clip.width / 2);
  const double padY = std::min(6.0, plan.clip.height / 2);
  plan.rowHeight = (plan.clip.height - 2 * padY) / 7;
  plan.fontSize = std::clamp(plan.rowHeight - 2, 6.0, 14.0) * (2.0 / 3.0) * 0.7;
  auto compose = [&](const std::string &text, double size, TextRole role) {
    const typography::TextStyle style{size, typography::TextUnit::PaperPoints,
                                      role == TextRole::Bold, 0, 0};
    if (!fonts.catalog) return typography::Compose(text, style, style.bold ? fonts.bold : fonts.regular);
    auto run = typography::Compose(text, style, *fonts.catalog, fonts.family);
    if (run.fontIssue != typography::FontIssue::None)
      diagnose(typography::DescribeFontIssue(*fonts.catalog, fonts.family, style.bold, run.fontIssue));
    return run;
  };
  if (valid) {
    plan.fontIdentity = std::string(compose({}, plan.fontSize, TextRole::Normal).resourceId) +
                        "/" + std::string(compose({}, plan.fontSize, TextRole::Bold).resourceId);
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
      if (!full.unsupportedBytes.empty())
        diagnose("Some event table characters cannot be rendered.");
      const auto &line = full.lines.front();
      out.overflow = full.lines.size() > 1 || line.width > width + 1e-9 ||
          (!source.empty() && full.ascent + full.descent > out.clip.height + 1e-9);
      const auto dots = compose("...", size, out.role);
      const double suffixWidth = out.overflow ? dots.lines.front().width : 0;
      std::map<size_t, std::pair<size_t, double>> clusters;
      for (size_t j = line.first; j < line.first + line.count; ++j) {
        const auto &glyph = full.glyphs[j];
        auto &cluster = clusters[glyph.sourceBegin];
        cluster.first = glyph.sourceEnd;
        cluster.second += glyph.advance;
      }
      std::vector<size_t> fitting{0};
      double used = 0;
      for (const auto &[begin, cluster] : clusters) {
        used += cluster.second;
        if (used + suffixWidth > width + 1e-9) break;
        fitting.push_back(cluster.first);
      }
      const std::string suffix = out.overflow && suffixWidth <= width ? "..." : "";
      do {
        out.displayed = source.substr(0, fitting.back()) + suffix;
        out.run = compose(out.displayed, size, out.role);
        fitting.pop_back();
      } while (!fitting.empty() && out.run.lines.front().width > width + 1e-9);
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
