#include "symbols/fixture_symbol_state.h"

#include "gdtf_archive_reader.h"
#include "gdtf_fixture_type_vocabulary.h"
#include "gdtf_mutation_audit.h"
#include "symbols/fixture_symbol_availability.h"

#include <tinyxml2.h>

namespace symbols {

bool InspectFixtureSymbolState(const std::string &gdtfPath,
                               FixtureSymbolState &state,
                               std::string &errorMessage) {
  state = {};
  const auto archive = gdtf::ReadGdtfArchive(gdtfPath);
  if (!archive.Success()) {
    errorMessage = archive.diagnostics.empty()
                       ? "Could not read the fixture GDTF archive."
                       : archive.diagnostics.front().message;
    return false;
  }
  tinyxml2::XMLDocument document;
  if (document.Parse(archive.descriptionXml.c_str(),
                     archive.descriptionXml.size()) != tinyxml2::XML_SUCCESS) {
    errorMessage = "Could not parse the fixture GDTF description.";
    return false;
  }
  auto *fixtureType = document.FirstChildElement("GDTF");
  if (fixtureType)
    fixtureType = fixtureType->FirstChildElement("FixtureType");
  if (!fixtureType)
    fixtureType = document.FirstChildElement("FixtureType");
  if (!fixtureType) {
    errorMessage = "The GDTF has no FixtureType description.";
    return false;
  }
  const auto compatibility = GdtfMutationAudit::InspectCompatibility(fixtureType);
  state.warningMessage = compatibility.warning;
  bool perastageRevision = false;
  const auto *revisions = fixtureType->FirstChildElement("Revisions");
  for (const auto *revision = revisions ? revisions->FirstChildElement("Revision")
                                       : nullptr;
       revision; revision = revision->NextSiblingElement("Revision")) {
    const char *modifiedBy = revision->Attribute("ModifiedBy");
    if (modifiedBy && (std::string(modifiedBy) == "Perastage" ||
                       std::string(modifiedBy).rfind("Perastage ", 0) == 0))
      perastageRevision = true;
  }
  const bool legacyEditor = fixtureType->Attribute("Editor") &&
      gdtf::IsLegacyPerastageEditorValue(fixtureType->Attribute("Editor"));
  state.editorIsPerastage =
      compatibility.mode == GdtfMutationAudit::CompatibilityMode::KnownPerastageVersion ||
      (compatibility.mode == GdtfMutationAudit::CompatibilityMode::LegacyFallback &&
       (perastageRevision || legacyEditor));
  const auto availability = symbol_cache::InspectFixtureSymbolAvailability(gdtfPath);
  state.hasValidSvgSymbolSet = availability.storedSvgUsable;
  state.requiresSymbolGeneration = !state.hasValidSvgSymbolSet;
  if (state.requiresSymbolGeneration && state.warningMessage.empty())
    state.warningMessage = availability.diagnostic;
  return true;
}

} // namespace symbols
