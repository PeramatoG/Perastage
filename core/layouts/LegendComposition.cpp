#include "LegendComposition.h"
#include "typography/font_resources.h"

#include <algorithm>
#include <cmath>

namespace layouts::composition {
namespace {
constexpr double kFontScale = (2.0 / 3.0) * 0.7;
double Nonnegative(double value) {
  return std::isfinite(value) ? std::max(0.0, value) : 0.0;
}
bool HasGeometry(const LegendSymbolGeometry &symbol) {
  return symbol.kind != SymbolGeometryKind::Missing &&
      std::isfinite(symbol.bounds.x) && std::isfinite(symbol.bounds.y) &&
      std::isfinite(symbol.bounds.width) && std::isfinite(symbol.bounds.height) &&
      symbol.bounds.width > 0 && symbol.bounds.height > 0;
}
std::array<std::string, 2> Wrap(const std::string &text, double width,
    const std::function<double(std::string_view)> &measure,
    const std::function<size_t(const std::string &, double)> &firstBreak) {
  if (width <= 0) return {};
  if (const auto newline = text.find('\n'); newline != std::string::npos)
    return {text.substr(0,newline), text.substr(newline+1)};
  if (measure(text) <= width) return {text, ""};
  std::size_t split = 0;
  for (std::size_t pos = text.size(); pos-- > 1;) {
    if (text[pos] == ' ' && measure(std::string_view(text).substr(0, pos)) <= width) {
      split = pos;
      break;
    }
  }
  if (!split) split = firstBreak(text, width);
  std::string remainder = text.substr(split);
  const auto first = remainder.find_first_not_of(' ');
  if (first == std::string::npos) remainder.clear();
  else remainder = remainder.substr(first, remainder.find_last_not_of(' ') - first + 1);
  // Legacy keeps the complete second line; the frame clips it. Never discard
  // source data or silently append an ellipsis that was not in the document.
  return {text.substr(0, split), remainder};
}
} // namespace

static LegendCompositionPlan ComposeGeometry(const LegendCompositionInput &in,
                                    const TextMetrics &metrics,
                                    const std::function<size_t(const std::string &, double, double)> &firstBreak) {
  LegendCompositionPlan plan;
  plan.clip = {0, 0, Nonnegative(in.width), Nonnegative(in.height)};
  std::vector<std::size_t> visible;
  for (std::size_t i = 0; i < in.rows.size(); ++i) {
    if (in.rows[i].visible) visible.push_back(i);
    else ++plan.hiddenRows;
  }
  if (!metrics || plan.clip.width == 0 || plan.clip.height == 0) {
    plan.clippedRows = visible.size();
    plan.overflow = !visible.empty();
    return plan;
  }
  const double gap = Nonnegative(in.style.columnGap);
  const double separator = Nonnegative(in.style.separatorGap);
  const double totalRows = static_cast<double>(visible.size()) + 1;
  const double availableHeight = Nonnegative(plan.clip.height - separator);
  const double candidateRowHeight = availableHeight / totalRows;
  double fontSize = std::clamp(candidateRowHeight - 2, 6.0, 14.0) * kFontScale;
  auto measure = [&](std::string_view text, double size, TextRole role) {
    auto extent = metrics(text, size, role);
    extent.width = Nonnegative(extent.width);
    extent.height = Nonnegative(extent.height);
    extent.baseline = Nonnegative(extent.baseline);
    return extent;
  };
  const double maxRowHeight = candidateRowHeight;
  const double maxTextHeight = Nonnegative((candidateRowHeight - separator) / 2);
  auto extent = measure("Hg", fontSize, TextRole::Normal);
  if (maxTextHeight > 0 && extent.height > maxTextHeight) {
    fontSize *= maxTextHeight / extent.height;
    extent = measure("Hg", fontSize, TextRole::Normal);
  }
  plan.fontSize = fontSize;
  plan.rowHeight = maxRowHeight;
  const auto width = [&](std::string_view text) {
    return measure(text, fontSize, TextRole::Normal).width;
  };
  double countWidth = measure("Count", fontSize, TextRole::Bold).width;
  double channelWidth = in.showChannelColumn ? measure("Ch", fontSize, TextRole::Bold).width : 0;
  for (const auto i : visible) {
    countWidth = std::max(countWidth, width(in.rows[i].count));
    if (in.showChannelColumn) channelWidth = std::max(channelWidth, width(in.rows[i].channels));
  }
  if (in.showChannelColumn) channelWidth += width("0");

  const double fontScale = std::clamp(fontSize / (14 * kFontScale), 0.0, 1.0);
  const double symbolSize = std::max(4.0, 44.0 * fontScale);
  const double maxColumn = std::max(4.0, symbolSize * 1.45 * 2 / 5);
  std::array<double, 3> columnWidths{};
  std::array<bool, 3> svgColumns{};
  bool hasSvg = false, hasCommands = false;
  auto symbolScale = [&](const LegendSymbolGeometry &symbol) {
    const double size = std::max(4.0, symbolSize *
        (symbol.kind == SymbolGeometryKind::Svg ? 0.4 : 2.0));
    return std::min(size / symbol.bounds.width, size / symbol.bounds.height);
  };
  for (const auto i : visible) {
    for (std::size_t view = 0; view < 3; ++view) {
      const auto &symbol = in.rows[i].symbols[view];
      if (!HasGeometry(symbol)) continue;
      if (symbol.kind == SymbolGeometryKind::Svg) hasSvg = svgColumns[view] = true;
      else hasCommands = true;
      columnWidths[view] = std::max(columnWidths[view], symbol.bounds.width * symbolScale(symbol));
    }
  }
  double symbolGap = 0, outerMargin = 0;
  if (hasSvg && !hasCommands) { symbolGap = 3; outerMargin = 2; }
  std::size_t lastSymbolColumn = 0;
  for (std::size_t view = 0; view < 3; ++view)
    if (columnWidths[view] > 0) lastSymbolColumn = view;
  double x = outerMargin;
  for (std::size_t view = 0; view < 3; ++view) {
    double w = std::clamp(columnWidths[view], 0.0, maxColumn);
    if (hasSvg && !hasCommands && svgColumns[view])
      w = std::max(w, std::max(4.0, symbolSize * 0.55));
    plan.symbolColumns[view] = {x, w};
    x += w + (view < lastSymbolColumn ? symbolGap : 0);
  }
  plan.countColumn = {x + gap, countWidth};
  plan.typeColumn.x = plan.countColumn.x + countWidth + gap;
  plan.channelColumn = {in.showChannelColumn
      ? std::max(plan.clip.width - channelWidth, plan.typeColumn.x + gap)
      : plan.clip.width, channelWidth};
  plan.typeColumn.width = Nonnegative(plan.channelColumn.x - plan.typeColumn.x -
                                      (in.showChannelColumn ? gap : 0));
  plan.separatorStart = {outerMargin, plan.rowHeight};
  plan.separatorEnd = {plan.clip.width, plan.rowHeight};
  const double singleOffset = Nonnegative((plan.rowHeight - extent.height) / 2);
  const double typeOffset = Nonnegative((plan.rowHeight - (2 * extent.height + separator)) / 2);
  auto append = [&](const std::string &text, TextRole role, double tx, double ty, double maxWidth) {
    const auto measured = measure(text, fontSize, role);
    const bool overflow = measured.width > maxWidth + 1e-6 ||
        tx + measured.width > plan.clip.width + 1e-6 || ty + measured.height > plan.clip.height + 1e-6;
    plan.text.push_back({text, role, {tx, ty}, {tx, ty + measured.baseline}, measured.width, overflow});
    plan.text.back().clip = {tx, ty, Nonnegative(maxWidth), measured.height};
    plan.overflow = plan.overflow || overflow;
  };
  append("Count", TextRole::Bold, plan.countColumn.x, singleOffset, countWidth);
  append("Type", TextRole::Bold, plan.typeColumn.x, singleOffset, plan.typeColumn.width);
  if (in.showChannelColumn) append("Ch", TextRole::Bold, plan.channelColumn.x, singleOffset, channelWidth);
  double y = plan.rowHeight + separator;
  for (const auto i : visible) {
    if (y + plan.rowHeight > plan.clip.height + 1e-6) { ++plan.clippedRows; continue; }
    const auto &row = in.rows[i];
    const auto lines = Wrap(row.name, plan.typeColumn.width, width,
        [&](const auto &name, double available) { return firstBreak(name, fontSize, available); });
    const bool twoLines = !lines[1].empty();
    const auto firstText = plan.text.size();
    append(row.count, TextRole::Normal, plan.countColumn.x, y + singleOffset, countWidth);
    append(lines[0], TextRole::Normal, plan.typeColumn.x, y + (twoLines ? typeOffset : singleOffset), plan.typeColumn.width);
    if (twoLines) append(lines[1], TextRole::Normal, plan.typeColumn.x,
        y + typeOffset + extent.height + separator, plan.typeColumn.width);
    if (in.showChannelColumn) append(row.channels, TextRole::Normal, plan.channelColumn.x, y + singleOffset, channelWidth);
    bool rowOverflow = (plan.typeColumn.width == 0 && !row.name.empty()) ||
        extent.height > plan.rowHeight ||
        (twoLines && 2 * extent.height + separator > plan.rowHeight);
    for (auto j = firstText; j < plan.text.size(); ++j) rowOverflow |= plan.text[j].overflow;
    plan.rows.push_back({i, row.id, {0, y, plan.clip.width, plan.rowHeight}, lines, rowOverflow});
    plan.overflow |= rowOverflow;
    for (std::size_t view = 0; view < 3; ++view) {
      const auto &symbol = row.symbols[view];
      if (!HasGeometry(symbol)) continue;
      const auto column = plan.symbolColumns[view];
      const double scale = symbolScale(symbol);
      const double w = symbol.bounds.width * scale, h = symbol.bounds.height * scale;
      // Keep command-origin alignment. Oversized fallback
      // geometry is diagnosed and clipped to its slot by both drawing adapters.
      const double left = symbol.kind == SymbolGeometryKind::Svg
          ? column.x + Nonnegative((column.width - w) / 2)
          : column.x + column.width / 2 + symbol.bounds.x * scale;
      const Rect slot{column.x, y, column.width, plan.rowHeight};
      const Rect draw{left, y + (plan.rowHeight - h) / 2, w, h};
      plan.symbols.push_back({i, static_cast<LegendView>(view), symbol, slot, draw,
                             scale, column.x + column.width / 2});
      plan.overflow |= draw.x < slot.x || draw.x + w > slot.x + slot.width || h > slot.height;
    }
    y += plan.rowHeight;
  }
  plan.overflow |= plan.clippedRows > 0;
  return plan;
}
LegendCompositionPlan ComposeLegend(const LegendCompositionInput &input,
                                    const LegendFonts &fonts) {
  const auto compose = [&](std::string_view text, double size, TextRole role, double wrapWidth = 0) {
    const typography::TextStyle style{size, typography::TextUnit::PaperPoints,
                                      role == TextRole::Bold, wrapWidth, 0};
    if (fonts.catalog) return typography::Compose(std::string(text), style, *fonts.catalog, fonts.family, false, fonts.usage);
    return typography::Compose(std::string(text), style, style.bold ? fonts.bold : fonts.regular);
  };
  const auto measure = [&](std::string_view text, double size, TextRole role) {
    const auto run = compose(text, size, role);
    double width = 0;
    for (const auto &line : run.lines) width = std::max(width, line.width);
    const auto extraLines = run.lines.empty() ? 0 : run.lines.size() - 1;
    return TextMeasurement{width, run.ascent + run.descent + extraLines * run.lineHeight, run.ascent};
  };
  auto plan = ComposeGeometry(input, measure, [&](const std::string &text, double size, double width) {
    const auto wrapped = compose(text, size, TextRole::Normal, width);
    return wrapped.lines.empty() ? text.size() : wrapped.lines.front().sourceEnd;
  });
  plan.fontIdentity = std::string(compose({}, plan.fontSize, TextRole::Normal).resourceId) +
                      "/" + std::string(compose({}, plan.fontSize, TextRole::Bold).resourceId);
  for (auto &line : plan.text) {
    line.glyphRun = compose(line.text, plan.fontSize, line.role);
    if (fonts.catalog && line.glyphRun.fontIssue != typography::FontIssue::None) {
      const auto message = typography::DescribeFontIssue(*fonts.catalog, fonts.family,
          line.role == TextRole::Bold, line.glyphRun.fontIssue, false, fonts.usage);
      if (std::find(plan.diagnostics.begin(), plan.diagnostics.end(), message) == plan.diagnostics.end())
        plan.diagnostics.push_back(message);
    }
    plan.unsupportedGlyphs += line.glyphRun.unsupportedBytes.size();
  }
  if (plan.unsupportedGlyphs) plan.diagnostics.push_back("Some legend characters cannot be rendered.");
  return plan;
}
} // namespace layouts::composition
