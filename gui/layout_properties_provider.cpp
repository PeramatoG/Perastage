// This file is part of Perastage. License: GPL-3.0-or-later.
#include "layout_properties_provider.h"

#include "LayoutDocumentEdit.h"
#include "layout_text_properties.h"
#include "typography/font_resources.h"
#include "viewer2dcommandrenderer.h"
#include "viewer2dpanel.h"

#include <array>
#include <wx/intl.h>

namespace gui::layoutproperties {
namespace {
using namespace properties;
using layouts::LayoutElementKind;
using layouts::LayoutElementRef;
using layouts::ViewportFramePoints;
constexpr double kMmPerPoint = print::kMillimetersPerPoint;
constexpr std::array kFrameFields = {&ViewportFramePoints::x, &ViewportFramePoints::y,
                                      &ViewportFramePoints::width, &ViewportFramePoints::height};

wxString ElementName(LayoutElementKind kind) {
  switch (kind) {
  case LayoutElementKind::View2D: return _("2D View");
  case LayoutElementKind::Legend: return _("Legend");
  case LayoutElementKind::EventTable: return _("Event Table");
  case LayoutElementKind::Text: return _("Text Box");
  case LayoutElementKind::Image: return _("Image");
  default: return {};
  }
}

wxString FontName(std::string_view reference) {
  auto name = typography::DecodeFontReference(reference).family;
  if (name.empty() || name == "Noto Sans") name = typography::kDefaultFamily;
  return wxString::FromUTF8(name.data(), name.size());
}

void AddText(Sheet &sheet, const layouts::LayoutTextDefinition &text) {
  const auto data = layouttext::InspectTypography(text);
  const auto editable = data.editable;
  sheet.sections.push_back({_("Content"), {
      {Info, _("Text"), wxString::FromUTF8(data.content), false, true},
      {Advanced, _("Edit Text..."), Action{}},
  }});
  auto &type = sheet.sections.emplace_back(Section{_("Typography"), {}}).rows;
  const auto add = [&](Property id, const wxString &label, std::optional<Value> value) {
    type.push_back({id, label, value.value_or(Value{_("Mixed")}), editable && value.has_value()});
  };
  add(Font, _("Font family"), data.font ? std::optional<Value>{
       editable ? Value{properties::Font{*data.font, *data.bold, *data.italic}}
                : Value{FontName(*data.font)}} : std::nullopt);
  add(Size, _("Size (pt)"), data.size ? std::optional<Value>{double(*data.size)} : std::nullopt);
  add(Bold, _("Bold"), data.bold ? std::optional<Value>{*data.bold} : std::nullopt);
  add(Italic, _("Italic"), data.italic ? std::optional<Value>{*data.italic} : std::nullopt);
  add(Alignment, _("Alignment"), data.alignment ? std::optional<Value>{Choice{
      {_("Align start"), _("Align center"), _("Align end"), _("Justify")}, int(*data.alignment)}} : std::nullopt);
  add(Ink, _("Color"), data.color ? std::optional<Value>{*data.color} : std::nullopt);
  sheet.sections.push_back({_("Appearance"), {
      {Background, _("Solid background"), text.solidBackground, true},
      {Frame, _("Show outline"), text.drawFrame, true},
  }});
}

void AddViewport(Sheet &sheet, const layouts::Layout2DViewDefinition &view,
                 const layouts::FixtureLabelProfiles &profiles) {
  const std::array orientations = {_("Top"), _("Front"), _("Side"), _("Bottom")};
  const int orientation = view.camera.view;
  const bool fixed = view.scaleMode == layouts::ViewportScaleMode::Fixed;
  auto &rows = sheet.sections.emplace_back(Section{_("Viewport properties"), {}}).rows;
  rows.push_back({Info, _("Orientation"),
      orientation >= 0 && orientation < int(orientations.size()) ? orientations[orientation] : wxString{}});
  rows.push_back({Info, _("Scale mode"), fixed ? _("Fixed") : _("Free / Variable")});
  if (fixed && view.physicalScale) {
    rows.push_back({Info, _("Drawing scale 1:"), view.physicalScale->denominator});
    rows.push_back({Info, _("Center horizontal (m)"), view.physicalScale->centerX});
    rows.push_back({Info, _("Center vertical (m)"), view.physicalScale->centerY});
  } else {
    Viewer2DViewState state;
    state.viewportWidth = view.camera.viewportWidth;
    state.viewportHeight = view.camera.viewportHeight;
    state.zoom = view.camera.zoom;
    state.offsetPixelsX = view.camera.offsetPixelsX;
    state.offsetPixelsY = view.camera.offsetPixelsY;
    state.view = static_cast<Viewer2DView>(orientation);
    viewer2d::Viewer2DViewBounds bounds;
    if (viewer2d::ComputeViewBounds(state, bounds)) {
      rows.push_back({Info, _("Center horizontal (m)"),
                      layouts::ViewportHorizontalSign(orientation) * (bounds.minX + bounds.maxX) / 2});
      rows.push_back({Info, _("Center vertical (m)"), (bounds.minY + bounds.maxY) / 2});
    }
  }
  const auto profile = profiles.find(view.labelPresentation.profileId);
  rows.push_back({Info, _("Label profile"),
      profile != profiles.end() ? wxString::FromUTF8(profile->second.name) : _("Project default")});
  rows.push_back({Frame, _("Show outline"), view.drawFrame, true});
  rows.push_back({Advanced, _("Edit View..."), Action{}});
  rows.push_back({Labels, _("Fixture label settings..."), Action{}});
}

} // namespace

properties::Sheet Build(const layouts::LayoutDefinition *page,
                        const layouts::FixtureLabelProfiles &profiles, const Context &context) {
  Sheet sheet{context.revision, _("No Layout selected"), {}};
  if (!page) return sheet;
  std::vector<layouts::ElementFrame> selected;
  for (auto ref : context.selection)
    layouts::VisitElement(*page, ref, [&](const auto &item) {
      selected.push_back({ref, layouts::ElementFramePoints(item)});
    });
  if (selected.empty()) {
    const auto count = page->view2dViews.size() + page->legendViews.size() +
        page->eventTables.size() + page->textViews.size() + page->imageViews.size();
    sheet.title = _("Document");
    sheet.sections.push_back({_("Page setup"), {
      {Name, _("Layout name"), wxString::FromUTF8(page->name), true},
      {Paper, _("Page size"), Choice{{_("A3"), _("A4")}, int(page->pageSetup.pageSize)}, true},
      {Orientation, _("Orientation"), Choice{{_("Portrait"), _("Landscape")}, int(page->pageSetup.landscape)}, true},
      {Info, _("Width (mm)"), page->pageSetup.PageWidthPt() * kMmPerPoint},
      {Info, _("Height (mm)"), page->pageSetup.PageHeightPt() * kMmPerPoint},
      {Info, _("Elements"), wxString::Format("%zu", count)},
    }});
    return sheet;
  }

  const auto ref = selected.front().element;
  const bool single = selected.size() == 1;
  sheet.title = single ? wxString::Format("%s #%d", ElementName(ref.kind), ref.id)
      : wxString::Format(wxPLURAL("%zu element selected", "%zu elements selected",
                                 selected.size()), selected.size());
  auto &geometry = sheet.sections.emplace_back(Section{_("Transform"), {}}).rows;
  const std::array labels = {_("X (mm)"), _("Y (mm)"), _("Width (mm)"), _("Height (mm)")};
  for (size_t n = 0; n < kFrameFields.size(); ++n) {
    const auto field = kFrameFields[n];
    const double amount = selected.front().frame.*field;
    const bool same = std::all_of(selected.begin(), selected.end(),
                                  [&](const auto &item) { return item.frame.*field == amount; });
    geometry.push_back({int(X) + int(n), labels[n],
                        same ? Value{amount * kMmPerPoint} : Value{_("Mixed")}, single});
  }
  if (!single) {
    sheet.sections.push_back({_("Alignment"), {
        {Info, _("Use the Alignment toolbar for the selection."), wxString{}},
    }});
    return sheet;
  }

  layouts::VisitElement(*page, ref, [&](const auto &item) {
    using T = std::decay_t<decltype(item)>;
    if constexpr (std::is_same_v<T, layouts::LayoutTextDefinition>) AddText(sheet, item);
    else if constexpr (std::is_same_v<T, layouts::Layout2DViewDefinition>)
      AddViewport(sheet, item, profiles);
    else if constexpr (std::is_same_v<T, layouts::LayoutLegendDefinition>) {
      sheet.sections.push_back({_("Legend"), {
          {ChannelColumn, _("Show channel column"), item.showChannelColumn, true},
          {Info, _("Font family"), FontName(item.fontFamily)},
          {Advanced, _("Edit Legend and Symbols..."), Action{}},
      }});
    } else if constexpr (std::is_same_v<T, layouts::LayoutEventTableDefinition>) {
      sheet.sections.push_back({_("Event Table"), {{Advanced, _("Edit Event Table..."), Action{}}}});
    } else {
      sheet.sections.push_back({_("Image"), {{Advanced, _("Edit Image..."), Action{}}}});
    }
    sheet.sections.push_back({_("Stacking order"), {
        {Info, _("Z-order"), wxString::Format("%d", item.zIndex)},
        {BringFront, _("Bring to Front"), Action{}},
        {SendBack, _("Send to Back"), Action{}},
    }});
  });
  return sheet;
}

} // namespace gui::layoutproperties
