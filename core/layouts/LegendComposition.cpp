#include "LegendComposition.h"

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
// A valid UTF-8 input is split only at codepoint boundaries. Combining sequences
// and shaping remain the metric/backend's responsibility, as in legacy Legends.
std::size_t NextCodepoint(const std::string &text, std::size_t offset) {
  ++offset;
  while (offset < text.size() &&
         (static_cast<unsigned char>(text[offset]) & 0xc0) == 0x80)
    ++offset;
  return offset;
}
std::array<std::string, 2> Wrap(const std::string &text, double width,
    const std::function<double(std::string_view)> &measure) {
  if (width <= 0) return {};
  if (measure(text) <= width) return {text, ""};
  std::size_t split = 0;
  for (std::size_t pos = text.size(); pos-- > 1;) {
    if (text[pos] == ' ' && measure(std::string_view(text).substr(0, pos)) <= width) {
      split = pos;
      break;
    }
  }
  if (!split) {
    for (std::size_t end = NextCodepoint(text, 0); end <= text.size();) {
      if (measure(std::string_view(text).substr(0, end)) > width) break;
      split = end;
      if (end == text.size()) break;
      end = NextCodepoint(text, end);
    }
    if (!split && !text.empty()) split = NextCodepoint(text, 0);
  }
  std::string remainder = text.substr(split);
  const auto first = remainder.find_first_not_of(' ');
  if (first == std::string::npos) remainder.clear();
  else remainder = remainder.substr(first, remainder.find_last_not_of(' ') - first + 1);
  // Legacy keeps the complete second line; the frame clips it. Never discard
  // source data or silently append an ellipsis that was not in the document.
  return {text.substr(0, split), remainder};
}
} // namespace

LegendCompositionPlan ComposeLegend(const LegendCompositionInput &in,
                                    const TextMetrics &metrics) {
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
  const bool preview = in.style.metrics == LegacyLegendMetrics::Preview;
  const double gap = Nonnegative(in.style.columnGap);
  const double separator = Nonnegative(in.style.separatorGap);
  const double totalRows = static_cast<double>(visible.size()) + 1;
  const double availableHeight = Nonnegative(plan.clip.height - separator);
  const double candidateRowHeight = availableHeight / totalRows;
  double fontSize = std::clamp(candidateRowHeight - 2, 6.0, 14.0) * kFontScale;
  fontSize *= Nonnegative(in.style.typographyScale);
  if (preview) fontSize = std::max(1.0, std::round(fontSize));
  const double initialFontScale = std::clamp(fontSize / (14 * kFontScale), 0.0, 1.0);
  auto measure = [&](std::string_view text, double size, TextRole role) {
    auto extent = metrics(text, size, role);
    extent.width = Nonnegative(extent.width);
    extent.height = Nonnegative(extent.height);
    extent.baseline = Nonnegative(extent.baseline);
    return extent;
  };
  const double maxRowHeight = preview ? std::max(1.0, std::floor(candidateRowHeight))
                                      : candidateRowHeight;
  const double maxTextHeight = preview ? maxRowHeight
      : Nonnegative((candidateRowHeight - separator) / 2);
  auto extent = measure("Hg", fontSize, TextRole::Normal);
  if (preview) {
    while (extent.height > maxTextHeight && fontSize > 1) {
      --fontSize;
      extent = measure("Hg", fontSize, TextRole::Normal);
    }
  } else if (maxTextHeight > 0 && extent.height > maxTextHeight) {
    fontSize *= maxTextHeight / extent.height;
    extent = measure("Hg", fontSize, TextRole::Normal);
  }
  plan.fontSize = fontSize;
  plan.rowHeight = maxRowHeight;
  const auto width = [&](std::string_view text) {
    return measure(text, fontSize, TextRole::Normal).width;
  };
  double countWidth = width("Count");
  double channelWidth = in.showChannelColumn ? width("Ch") : 0;
  for (const auto i : visible) {
    countWidth = std::max(countWidth, width(in.rows[i].count));
    if (in.showChannelColumn) channelWidth = std::max(channelWidth, width(in.rows[i].channels));
  }
  if (in.showChannelColumn) channelWidth += width("0");

  const double fontScale = preview ? std::clamp(fontSize / (14 * kFontScale), 0.0, 1.0)
                                   : initialFontScale;
  const double symbolSize = std::max(4.0, preview ? std::round(44.0 * fontScale) : 44.8 * fontScale);
  const double maxColumn = std::max(4.0, std::floor(std::max(4.0, std::round(symbolSize * 1.45)) * 2 / 5));
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
  if (hasSvg && !hasCommands) { symbolGap = preview ? 3 : 2.5; outerMargin = 2; }
  // Legacy vector Legends had two slots. Add their third gap only when Side
  // actually has geometry, retaining ordinary two-view PDF column positions.
  const std::size_t lastSymbolColumn = !preview && columnWidths[2] == 0 ? 1 : 2;
  double x = outerMargin;
  for (std::size_t view = 0; view < 3; ++view) {
    double w = std::clamp(std::ceil(std::min(columnWidths[view], maxColumn)), 0.0, maxColumn);
    if (preview && hasSvg && !hasCommands && svgColumns[view])
      w = std::max(w, std::max(4.0, std::round(symbolSize * 0.55)));
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
  const auto legacyOffset = [&](double value) {
    return Nonnegative(preview ? std::floor(value) : value);
  };
  const double singleOffset = legacyOffset((plan.rowHeight - extent.height) / 2);
  const double typeOffset = legacyOffset((plan.rowHeight - (2 * extent.height + separator)) / 2);
  auto append = [&](const std::string &text, TextRole role, double tx, double ty, double maxWidth) {
    const auto measured = measure(text, fontSize, role);
    const bool overflow = measured.width > maxWidth + 1e-6 ||
        tx + measured.width > plan.clip.width + 1e-6 || ty + measured.height > plan.clip.height + 1e-6;
    plan.text.push_back({text, role, {tx, ty}, {tx, ty + measured.baseline}, measured.width, overflow});
    plan.overflow = plan.overflow || overflow;
  };
  append("Count", TextRole::Bold, plan.countColumn.x, singleOffset, countWidth);
  append("Type", TextRole::Bold, plan.typeColumn.x, singleOffset, plan.typeColumn.width);
  if (in.showChannelColumn) append("Ch", TextRole::Bold, plan.channelColumn.x, singleOffset, channelWidth);
  double y = plan.rowHeight + separator;
  for (const auto i : visible) {
    if (y + plan.rowHeight > plan.clip.height + 1e-6) { ++plan.clippedRows; continue; }
    const auto &row = in.rows[i];
    const auto lines = Wrap(row.name, plan.typeColumn.width, width);
    const bool twoLines = !lines[1].empty();
    const auto firstText = plan.text.size();
    append(row.count, TextRole::Normal, plan.countColumn.x, y + singleOffset, countWidth);
    append(lines[0], TextRole::Normal, plan.typeColumn.x, y + (twoLines ? typeOffset : singleOffset), plan.typeColumn.width);
    if (twoLines) append(lines[1], TextRole::Normal, plan.typeColumn.x,
        y + typeOffset + extent.height + (preview ? separator : 0), plan.typeColumn.width);
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
      // Retain legacy size and command-origin alignment. Oversized fallback
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
} // namespace layouts::composition
