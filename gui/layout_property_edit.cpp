// This file is part of Perastage. License: GPL-3.0-or-later.
#include "layout_properties_provider.h"

#include "LayoutDocumentEdit.h"
#include "LayoutManager.h"
#include "layout_text_properties.h"

namespace gui::layoutproperties {
namespace {
using namespace properties;
using layouts::LayoutElementKind;
using layouts::LayoutElementRef;
using layouts::ViewportFramePoints;
constexpr double kMmPerPoint = print::kMillimetersPerPoint;
constexpr std::array kFrameFields = {&ViewportFramePoints::x, &ViewportFramePoints::y,
                                      &ViewportFramePoints::width, &ViewportFramePoints::height};

bool EditFrame(layouts::LayoutManager &manager, const layouts::LayoutDefinition &page,
               LayoutElementRef ref, Property property, double mm) {
  if (!std::isfinite(mm)) return false;
  try {
    auto frame = layouts::ResolveElementFrame(page, ref);
    const auto field = kFrameFields[property - X];
    // Match the displayed millimetre precision without rewriting hidden residuals.
    if (std::round(mm * 1000) == std::round(frame.*field * kMmPerPoint * 1000))
      return true;
    frame.*field = mm / kMmPerPoint;
    if (ref.kind == LayoutElementKind::Image && (property == Width || property == Height)) {
      layouts::VisitElement(page, ref, [&](const auto &item) {
        if constexpr (std::is_same_v<std::decay_t<decltype(item)>, layouts::LayoutImageDefinition>) {
          if (item.aspectRatio > 0 && std::isfinite(item.aspectRatio)) {
            if (property == Width) frame.height = frame.width / item.aspectRatio;
            else frame.width = frame.height * item.aspectRatio;
          }
        }
      });
    }
    return manager.SetElementFrame(page.name, ref, frame);
  } catch (const std::invalid_argument &) {
    return false;
  }
}

std::optional<layouttext::TypographyEdit> TextEdit(Property property, const Value &value) {
  using namespace layouttext;
  switch (property) {
  case Font:
    if (const auto *font = std::get_if<properties::Font>(&value)) return FontEdit{font->reference};
    break;
  case Size:
    if (const auto *size = std::get_if<double>(&value);
        size && *size >= 6 && *size <= 72 && std::trunc(*size) == *size) return SizeEdit{int(*size)};
    break;
  case Bold:
    if (const auto *bold = std::get_if<bool>(&value)) return BoldEdit{*bold};
    break;
  case Italic:
    if (const auto *italic = std::get_if<bool>(&value)) return ItalicEdit{*italic};
    break;
  case Ink:
    if (const auto *ink = std::get_if<wxColour>(&value)) return *ink;
    break;
  case Alignment:
    if (const auto *choice = std::get_if<Choice>(&value); choice && choice->selected >= 0 && choice->selected <= 3)
      return static_cast<LayoutTextExportData::Alignment>(choice->selected);
    break;
  default: break;
  }
  return std::nullopt;
}
} // namespace

bool Edit(layouts::LayoutManager &manager, const Context &context,
          Property property, const properties::Value &value) {
  if (context.revision != manager.Revision()) return false;
  const auto *page = FindPage(manager.GetLayouts(), context.page);
  if (!page) return false;
  if (context.selection.empty()) {
    if (property == Name) {
      if (const auto *name = std::get_if<wxString>(&value)) {
        auto trimmed = *name;
        trimmed.Trim().Trim(false);
        return manager.RenameLayout(context.page, trimmed.utf8_string());
      }
    } else if (const auto *choice = std::get_if<Choice>(&value);
               choice && choice->selected >= 0 && choice->selected <= 1) {
      if (property == Paper) return manager.SetLayoutPageSize(context.page, static_cast<print::PageSize>(choice->selected));
      if (property == Orientation) return manager.SetLayoutOrientation(context.page, choice->selected == 1);
    }
    return false;
  }
  if (context.selection.size() != 1) return false;
  const auto ref = context.selection.front();
  if (property >= X && property <= Height) {
    const auto *amount = std::get_if<double>(&value);
    return amount && EditFrame(manager, *page, ref, property, *amount);
  }
  bool applied = false;
  const auto *flag = std::get_if<bool>(&value);
  layouts::VisitElement(*page, ref, [&](const auto &item) {
    using T = std::decay_t<decltype(item)>;
    if constexpr (std::is_same_v<T, layouts::LayoutTextDefinition>) {
      auto candidate = item;
      if (flag && property == Frame) candidate.drawFrame = *flag;
      else if (flag && property == Background) candidate.solidBackground = *flag;
      else if (const auto style = TextEdit(property, value)) {
        if (!layouttext::ApplyTypography(candidate, *style)) return;
      } else return;
      applied = manager.UpdateLayoutText(context.page, candidate);
    } else if constexpr (std::is_same_v<T, layouts::Layout2DViewDefinition>) {
      if (!flag || property != Frame) return;
      auto candidate = item;
      candidate.drawFrame = *flag;
      applied = manager.UpdateLayout2DView(context.page, candidate);
    } else if constexpr (std::is_same_v<T, layouts::LayoutLegendDefinition>) {
      if (!flag || property != ChannelColumn) return;
      auto candidate = item;
      candidate.showChannelColumn = *flag;
      applied = manager.UpdateLayoutLegend(context.page, candidate);
    }
  });
  return applied;
}
} // namespace gui::layoutproperties
