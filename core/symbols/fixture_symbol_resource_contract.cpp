#include "fixture_symbol_resource_contract.h"

#include "gdtf_fixture_type_vocabulary.h"

namespace {

// Legacy revisions identify only the views actually replaced by that action.
unsigned int LegacyRevisionViews(std::string_view modifiedBy,
                                std::string_view text,
                                bool internalAction = false) {
  if (!gdtf::IsLegacyPerastageEditorValue(modifiedBy))
    return 0;
  constexpr std::array<std::string_view, 4> labels = {
      "top", "side", "front", "bottom"};
  for (unsigned int mask = 1; mask < 16; ++mask) {
    std::string action = internalAction
                             ? "Applied Perastage fixture SVG symbol views ("
                             : "Applied fixture SVG symbol views (";
    bool first = true;
    for (unsigned int index = 0; index < labels.size(); ++index) {
      if ((mask & (1u << index)) == 0)
        continue;
      if (!first)
        action += ", ";
      action += labels[index];
      first = false;
    }
    action += ')';
    if (text == action)
      return mask;
  }
  return 0;
}

} // namespace

bool IsPerastageFixtureSymbolRevision(std::string_view modifiedBy,
                                     std::string_view text) {
  return LegacyRevisionViews(modifiedBy, text) != 0 ||
         LegacyRevisionViews(modifiedBy, text, true) != 0;
}

bool IsPerastageFixtureSymbolRevisionForView(std::string_view modifiedBy,
                                            std::string_view text,
                                            SymbolViewKind view) {
  unsigned int mask = 0;
  switch (view) {
  case SymbolViewKind::Top:
    mask = 1;
    break;
  case SymbolViewKind::Left:
  case SymbolViewKind::Right:
    mask = 2;
    break;
  case SymbolViewKind::Front:
    mask = 4;
    break;
  case SymbolViewKind::Bottom:
    mask = 8;
    break;
  case SymbolViewKind::Back:
    break;
  }
  return (LegacyRevisionViews(modifiedBy, text) & mask) != 0;
}

std::string BuildPerastageFixtureSymbolPath(std::string_view modelSvgBasename,
                                           SymbolViewKind view) {
  const char *name = "top";
  if (view == SymbolViewKind::Bottom)
    name = "bottom";
  else if (view == SymbolViewKind::Front)
    name = "front";
  else if (view == SymbolViewKind::Left || view == SymbolViewKind::Right)
    name = "side";
  return "perastage/symbols/" + std::string(modelSvgBasename) + "/" + name +
         ".svg";
}
