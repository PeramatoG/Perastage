#include "layouts/EventTableComposition.h"
#include "layouts/PageOutputMapping.h"
#include "typography/font_resources.h"
#include <cassert>
#include <limits>
using namespace layouts::composition;
int main() {
  for (auto paper : {std::pair{595.,842.},std::pair{842.,1191.}})
    for (auto output : {std::pair{595.,842.},std::pair{842.,595.},std::pair{1191.,842.}}) {
      auto mapping = UniformPageFit(output.first/paper.first, output.second/paper.second);
      auto frame = mapping.Map(Rect{10,20,300,100});
      assert(std::abs(frame.width/frame.height - 3) < 1e-12);
      assert(frame.x == 10*mapping.scale && frame.y == 20*mapping.scale);
      auto page = mapping.Map(Point{paper.first,paper.second});
      assert(page.x <= output.first+1e-9 && page.y <= output.second+1e-9);
    }
  assert(UniformPageFit(-1,1).scale == 0);
  const typography::FontPair fonts(PERASTAGE_TEST_RESOURCE_DIR);
  EventTableInput input{300, 100, {"Venue", "Málaga", "Date", "Stage", "Version", "Design", "Mail"}};
  auto plan = ComposeEventTable(input, {fonts.regular, fonts.bold});
  assert(!plan.overflow && plan.unsupportedGlyphs == 0);
  assert(std::abs(plan.labelWidth - 22.7032) < 1e-4);
  assert(std::abs(plan.rows[0].label.baseline.y - 14.1998) < 1e-4);
  assert(plan.rowHeight == 88.0 / 7 && plan.fontSize > 0);
  for (std::size_t i = 0; i < 7; ++i) {
    const auto &row = plan.rows[i];
    assert(row.label.source == kEventTableFields[i].label);
    assert(row.value.displayed == input.values[i]);
    assert(row.label.role == TextRole::Bold);
    assert(row.value.role == (i == 0 ? TextRole::Bold : TextRole::Normal));
    assert(row.value.baseline.x == plan.valueX);
    assert(row.value.clip.y == row.frame.y);
    assert(row.value.baseline.y > row.frame.y);
    assert(row.value.clip.x + row.value.clip.width <= input.width);
  }
  input.values[0] = std::string(1000, 'W');
  input.values[1] = "ÉclairageSüdÉclairageSüdÉclairageSüdÉclairageSüd";
  input.values[2] = "É灯😀";
  input.values[3] = "First\nSecond";
  input.values[4] = "    ";
  for (double width : {0., -1., 1., 20., 60., 120., 300., 600.}) {
    for (double height : {0., -1., 1., 12., 100., 900.}) {
      input.width = width; input.height = height;
      auto p = ComposeEventTable(input, {fonts.regular, fonts.bold});
      assert(p.unsupportedGlyphs == 2 && p.diagnostics.size() <= 16);
      assert(p.clip.width >= 0 && p.clip.height >= 0);
      assert(p.rows[0].value.source == input.values[0]);
      for (const auto &row : p.rows) for (const auto *cell : {&row.label, &row.value}) {
        assert(cell->run.lines.size() == 1);
        assert(cell->run.lines[0].width <= cell->clip.width + 1e-8);
        assert(cell->clip.height >= 0 && cell->clip.width >= 0);
        assert(std::isfinite(cell->baseline.y));
      }
    }
  }
  input = {300,100,{"First\nSecond", std::string("invalid ") + char(0xff)}};
  auto p = ComposeEventTable(input, {fonts.regular, fonts.bold});
  assert(p.rows[0].value.displayed == "First...");
  assert(p.unsupportedGlyphs == 1);
  assert(p.rows[1].value.source == input.values[1]);
  input.width = std::numeric_limits<double>::quiet_NaN();
  p = ComposeEventTable(input, {fonts.regular, fonts.bold});
  assert(p.clip.width == 0);
  typography::TtfFontMetrics missing;
  p = ComposeEventTable(input, {missing, missing});
  assert(p.overflow && !p.diagnostics.empty() && p.rows[0].value.source == input.values[0]);
  input = {300,100,{}};
  p = ComposeEventTable(input, {fonts.regular,fonts.bold});
  assert(!p.overflow && p.rows[0].value.displayed.empty());
}
