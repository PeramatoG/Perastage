// This file is part of Perastage. License: GPL-3.0-or-later.
#include "layout_properties_provider.h"
#include "layout_text_properties.h"
#include "support/legacy_layout_text_fixture.h"
#include "typography/font_reference.h"

#include <cassert>
#include <wx/app.h>
#include <wx/frame.h>
#include <wx/textctrl.h>

namespace {
using namespace gui::layoutproperties;
using gui::properties::Row;
using gui::properties::Sheet;
using K = layouts::LayoutElementKind;

const Row &FindRow(const Sheet &sheet, Property id) {
  for (const auto &section : sheet.sections)
    for (const auto &row : section.rows)
      if (row.id == id) return row;
  std::abort();
}

void TestModelAndTypography() {
  layouts::LayoutDefinition page;
  page.name = "Plan";
  page.view2dViews.resize(1);
  page.legendViews.resize(1);
  page.eventTables.resize(1);
  page.textViews.resize(1);
  page.imageViews.resize(1);
  for (auto ref : {K::View2D, K::Legend, K::EventTable, K::Text, K::Image}) {
    const layouts::LayoutElementRef element{ref, 0};
    const auto snapshot = Build(&page, {}, {page.name, {element}, 12});
    assert(snapshot.generation == 12 && FindRow(snapshot, X).editable);
    assert(std::holds_alternative<gui::properties::Action>(FindRow(snapshot, Advanced).value));
  }
  auto &text = page.textViews.front();
  text.text = "Producción\n第二行";
  text.frame = {72, 14, 220, 100};
  page.imageViews.front().frame.y = 14;
  auto sheet = Build(&page, {}, {page.name, {}, 12});
  assert(FindRow(sheet, Name).editable && FindRow(sheet, Paper).editable);
  assert(Build(nullptr, {}, {}).sections.empty());
  assert(FindRow(Build(&page, {}, {page.name, {{K::Image, 99}}, 12}), Name).editable);
  sheet = Build(&page, {}, {page.name, {{K::Text, 0}}, 12});
  assert(std::get<double>(FindRow(sheet, X).value) == 25.4);
  assert(FindRow(sheet, Size).editable && text.richText.empty());
  sheet = Build(&page, {}, {page.name, {{K::Text, 0}, {K::Image, 0}}, 12});
  assert(!FindRow(sheet, X).editable && std::get<wxString>(FindRow(sheet, X).value) == "Mixed");
  assert(std::get<double>(FindRow(sheet, Y).value) == 14 * (25.4 / 72));

  assert(layouttext::ApplyTypography(text, layouttext::SizeEdit{18}));
  assert(layouttext::ApplyTypography(text, layouttext::BoldEdit{true}));
  assert(layouttext::ApplyTypography(text, layouttext::ItalicEdit{true}));
  assert(layouttext::ApplyTypography(text, layouttext::FontEdit{"Source Sans 3"}));
  assert(layouttext::ApplyTypography(text, *wxWHITE));
  assert(layouttext::ApplyTypography(text, LayoutTextExportData::Alignment::Center));
  const auto font = layouttext::InspectTypography(text);
  assert(font.editable && font.content == "Producción\n第二行");
  assert(font.font == "Source Sans 3" && font.size == 18 && font.bold == true && font.italic == true);
  assert(font.color == *wxWHITE && text.useRichTextColors);
  assert(font.alignment == LayoutTextExportData::Alignment::Center);
  const auto xml = text.richText;
  assert(layouttext::ApplyTypography(text, layouttext::SizeEdit{18}) && xml == text.richText);
  assert(!layouttext::ApplyTypography(text, layouttext::SizeEdit{0}) && xml == text.richText);
  const auto printable = layouttext::BuildLayoutTextExportData(text);
  assert(printable.lines.size() == 2 && printable.lines.front().runs.front().fontSize == 18);
  assert((printable.lines.front().runs.front().color == std::array<uint8_t, 3>{255, 255, 255}));
  text.solidBackground = false;
  const auto raster = layouttext::RenderTextImage(text, {440, 200}, {220, 100}, 2);
  assert(raster.IsOk() && raster.HasAlpha());
  bool hasWhiteInk = false;
  for (int y = 0; y < raster.GetHeight(); ++y)
    for (int x = 0; x < raster.GetWidth(); ++x)
      hasWhiteInk |= raster.GetAlpha(x, y) > 224 && raster.GetRed(x, y) == 255 &&
          raster.GetGreen(x, y) == 255 && raster.GetBlue(x, y) == 255;
  assert(hasWhiteInk && raster.GetAlpha(0, 0) == 0);
  typography::FontReference installed{"Test installed family"};
  installed.postScriptNames[0] = "TestInstalled-Regular";
  installed.fingerprints[0] = "test-exact-regular";
  const auto reference = typography::EncodeFontReference(installed);
  assert(layouttext::ApplyTypography(text, layouttext::FontEdit{reference}));
  assert(layouttext::ApplyTypography(text, wxColour(28, 70, 178)));
  const auto external = layouttext::InspectTypography(text);
  assert(external.font == reference && external.color == wxColour(28, 70, 178));
  assert(text.useRichTextColors);

  text.richText = kLegacyLayoutText;
  text.useRichTextColors = false;
  const auto legacy = layouttext::InspectTypography(text);
  assert(!legacy.editable && !legacy.bold && legacy.color == *wxBLACK);
  assert(!layouttext::ApplyTypography(text, layouttext::BoldEdit{true}) &&
         text.richText == kLegacyLayoutText);
  sheet = Build(&page, {}, {page.name, {{K::Text, 0}}, 12});
  assert(!FindRow(sheet, Bold).editable && FindRow(sheet, Background).editable);
  assert(text.richText == kLegacyLayoutText);
}

void TestPropertyControls() {
  using namespace gui::properties;
  auto *frame = new wxFrame(nullptr, wxID_ANY, "Properties test");
  unsigned generation = 1;
  unsigned commits = 0;
  double coordinate = 8;
  auto *panel = new Panel(frame, [&] {
    return Sheet{generation, "Text box", {{"Transform", {{0, "Coordinate", coordinate, true}}}}};
  }, [&](int id, const Value &value) {
    if (id != 0 || !std::holds_alternative<double>(value)) return false;
    coordinate = std::get<double>(value);
    ++commits;
    ++generation;
    return true;
  });
  frame->SetClientSize(320, 420);
  frame->Show();
  panel->RequestRefresh();
  const auto flush = [] { wxTheApp->ProcessPendingEvents(); wxYield(); };
  const auto control = [&] {
    auto *input = dynamic_cast<wxTextCtrl *>(wxWindow::FindWindowByName("Coordinate", panel));
    assert(input);
    return input;
  };
  const auto enter = [](wxTextCtrl *input) {
    wxCommandEvent e(wxEVT_TEXT_ENTER, input->GetId());
    input->ProcessWindowEvent(e);
  };
  flush();
  enter(control());
  assert(commits == 0);
  auto *input = control();
  input->ChangeValue("15");
  wxKeyEvent escape(wxEVT_CHAR_HOOK);
  escape.m_keyCode = WXK_ESCAPE;
  input->ProcessWindowEvent(escape);
  assert(input->GetValue() == "8" && commits == 0);
  input->ChangeValue("invalid");
  enter(input);
  flush();
  assert(control()->GetValue() == "8" && commits == 0);
  control()->ChangeValue("12");
  enter(control());
  flush();
  assert(control()->GetValue() == "12" && commits == 1);
  frame->Destroy();
  flush();
}
} // namespace

void TestLayoutProperties() {
  TestModelAndTypography();
  TestPropertyControls();
}
