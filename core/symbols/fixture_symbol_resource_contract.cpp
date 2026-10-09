#include "fixture_symbol_resource_contract.h"

#include "gdtf_fixture_type_vocabulary.h"

#include <algorithm>

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

constexpr std::string_view kStandardizationSuffix =
    "; standardized legacy definition/resources";

unsigned int StandardRevisionViews(std::string_view modifiedBy,
                                   std::string_view text) {
  if (!gdtf::IsLegacyPerastageEditorValue(modifiedBy))
    return 0;
  if (text.ends_with(kStandardizationSuffix))
    text.remove_suffix(kStandardizationSuffix.size());
  constexpr std::array<std::string_view, 3> labels = {"Top", "Side", "Front"};
  for (unsigned mask = 1; mask < 8; ++mask) {
    std::vector<std::string_view> views;
    for (unsigned index = 0; index < labels.size(); ++index)
      if (mask & (1u << index))
        views.push_back(labels[index]);
    // Current output is canonical; compatible historical actions may list the
    // same distinct views in another order. Recognize only this finite grammar.
    std::sort(views.begin(), views.end());
    do {
      std::string phrase;
      for (std::size_t index = 0; index < views.size(); ++index) {
        if (index)
          phrase += index + 1 == views.size() ? " and " : ", ";
        phrase += views[index];
      }
      phrase += views.size() == 1 ? " view" : " views";
      if (text == "Added missing standard SVG " + phrase ||
          text == "Replaced standard SVG " + phrase + " after explicit user repair" ||
          text == "Replaced standard SVG " + phrase + " after explicit user replacement")
        return mask;
    } while (std::next_permutation(views.begin(), views.end()));
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

bool IsPerastageStandardSvgMutationRevision(std::string_view modifiedBy,
                                           std::string_view text) {
  return StandardRevisionViews(modifiedBy, text) != 0;
}

bool IsPerastageStandardSvgMutationRevisionForView(std::string_view modifiedBy,
                                                  std::string_view text,
                                                  SymbolViewKind view) {
  const unsigned mask = view == SymbolViewKind::Top ? 1
                        : view == SymbolViewKind::Front ? 4
                        : view == SymbolViewKind::Left || view == SymbolViewKind::Right ? 2 : 0;
  return (StandardRevisionViews(modifiedBy, text) & mask) != 0;
}

bool IsPerastageStandardSvgCleanupRevision(std::string_view modifiedBy,
                                          std::string_view text) {
  constexpr std::string_view documentCleanupSuffix =
      "; standardized legacy private symbol resources";
  return (gdtf::IsLegacyPerastageEditorValue(modifiedBy) &&
          (text == "Removed legacy Perastage private symbol extensions" ||
           (text.size() > documentCleanupSuffix.size() &&
            text.ends_with(documentCleanupSuffix)))) ||
         (text.ends_with(kStandardizationSuffix) &&
          StandardRevisionViews(modifiedBy, text) != 0);
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
