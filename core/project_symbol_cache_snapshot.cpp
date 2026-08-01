#include "project_symbol_cache_snapshot.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <map>
#include <memory>
#include <set>
#include <unordered_map>
#include <unordered_set>

#include <tinyxml2.h>
#include <wx/mstream.h>
#include <wx/zipstrm.h>


namespace symbol_cache {
namespace {

struct FixtureReference {
  std::string uuid;
  std::string gdtfSpec;
};

// Normalizes a portable archive entry and rejects unsafe or ambiguous spellings.
bool NormalizeSafeArchivePath(const std::string &input, std::string &normalized) {
  normalized.clear();
  if (input.empty() || input.find('\\') != std::string::npos ||
      input.front() == '/' || (input.size() >= 2 && input[1] == ':'))
    return false;
  bool priorSlash = false;
  std::string component;
  for (unsigned char ch : input) {
    if (ch < 32 || ch == 127)
      return false;
    if (ch == '/') {
      if (component.empty() || component == "." || component == ".." || priorSlash)
        return false;
      normalized += component + '/';
      component.clear();
      priorSlash = true;
    } else {
      component.push_back(static_cast<char>(ch));
      priorSlash = false;
    }
  }
  if (component.empty() || component == "." || component == "..")
    return false;
  normalized += component;
  return true;
}

// Folds an archive identity for deterministic duplicate detection on every platform.
std::string FoldAscii(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });
  return value;
}

// Reads the current ZIP entry into an immutable byte buffer.
std::vector<std::uint8_t> ReadCurrentEntry(wxZipInputStream &zip) {
  std::vector<std::uint8_t> bytes;
  std::array<char, 8192> buffer{};
  while (true) {
    zip.Read(buffer.data(), buffer.size());
    const std::size_t count = zip.LastRead();
    if (count == 0)
      break;
    const auto *first = reinterpret_cast<const std::uint8_t *>(buffer.data());
    bytes.insert(bytes.end(), first, first + count);
  }
  return bytes;
}

// Reads a ZIP payload while rejecting unsafe, duplicate, and case-colliding entries.
bool ReadArchive(const std::vector<std::uint8_t> &bytes,
                 std::map<std::string, std::vector<std::uint8_t>> &entries,
                 std::string &errorMessage) {
  entries.clear();
  if (bytes.empty()) {
    errorMessage = "Archive payload is empty.";
    return false;
  }
  wxMemoryInputStream memory(bytes.data(), bytes.size());
  wxZipInputStream zip(memory);
  std::unordered_set<std::string> identities;
  while (true) {
    std::unique_ptr<wxZipEntry> entry(zip.GetNextEntry());
    if (!entry)
      break;
    if (entry->IsDir())
      continue;
    std::string path;
    if (!NormalizeSafeArchivePath(entry->GetName().ToStdString(), path)) {
      errorMessage = "Archive contains an unsafe or malformed entry path.";
      return false;
    }
    if (!identities.insert(FoldAscii(path)).second) {
      errorMessage = "Archive contains duplicate or ambiguous entry paths.";
      return false;
    }
    entries.emplace(path, ReadCurrentEntry(zip));
  }
  if (!zip.IsOk() && !zip.Eof()) {
    errorMessage = "Archive payload could not be read completely.";
    return false;
  }
  return true;
}

// Normalizes an MVR UUID for matching scene identities to exported XML nodes.
std::string NormalizeUuid(std::string value) {
  value.erase(std::remove_if(value.begin(), value.end(), [](unsigned char ch) {
                return ch == '{' || ch == '}' || ch == '-' || std::isspace(ch);
              }),
              value.end());
  return FoldAscii(std::move(value));
}

// Recursively collects fixture UUID and GDTFSpec pairs from MVR scene XML.
void CollectFixtureReferences(tinyxml2::XMLElement *element,
                              std::vector<FixtureReference> &references) {
  for (tinyxml2::XMLElement *current = element; current;
       current = current->NextSiblingElement()) {
    tinyxml2::XMLElement *gdtf = current->FirstChildElement("GDTFSpec");
    const char *uuid = current->Attribute("uuid");
    if (uuid && gdtf && gdtf->GetText())
      references.push_back({NormalizeUuid(uuid), gdtf->GetText()});
    CollectFixtureReferences(current->FirstChildElement(), references);
  }
}

// Resolves the preferred GDTF model used by the existing symbol inspection contract.
const tinyxml2::XMLElement *ResolvePreferredModel(
    const tinyxml2::XMLElement *fixtureType) {
  const tinyxml2::XMLElement *models =
      fixtureType ? fixtureType->FirstChildElement("Models") : nullptr;
  const tinyxml2::XMLElement *fallback = nullptr;
  for (const tinyxml2::XMLElement *model =
           models ? models->FirstChildElement("Model") : nullptr;
       model; model = model->NextSiblingElement("Model")) {
    if (!fallback)
      fallback = model;
    const char *name = model->Attribute("Name");
    if (name && std::string(name) == "Main")
      return model;
  }
  return fallback;
}

// Reports whether legacy audit metadata identifies a Perastage mutation.
bool HasPerastageAudit(const tinyxml2::XMLElement *fixtureType) {
  const char *editor = fixtureType ? fixtureType->Attribute("Editor") : nullptr;
  if (editor && (std::string(editor) == "Perastage" ||
                 std::string(editor).rfind("Perastage ", 0) == 0))
    return true;
  const tinyxml2::XMLElement *revisions =
      fixtureType ? fixtureType->FirstChildElement("Revisions") : nullptr;
  for (const tinyxml2::XMLElement *revision =
           revisions ? revisions->FirstChildElement("Revision") : nullptr;
       revision; revision = revision->NextSiblingElement("Revision")) {
    const char *modifiedBy = revision->Attribute("ModifiedBy");
    if (modifiedBy &&
        (std::string(modifiedBy) == "Perastage" ||
         std::string(modifiedBy).rfind("Perastage ", 0) == 0))
      return true;
  }
  return false;
}

// Validates the complete Perastage symbol set and computes its exact semantic hash.
bool ValidatePackagedGdtf(const std::vector<std::uint8_t> &bytes,
                          std::string &fingerprint,
                          std::string &diagnostic) {
  std::map<std::string, std::vector<std::uint8_t>> entries;
  if (!ReadArchive(bytes, entries, diagnostic))
    return false;
  auto descriptionIt = std::find_if(entries.begin(), entries.end(), [](const auto &item) {
    return FoldAscii(item.first) == "description.xml";
  });
  if (descriptionIt == entries.end()) {
    diagnostic = "Packaged GDTF does not contain description.xml.";
    return false;
  }
  tinyxml2::XMLDocument document;
  if (document.Parse(reinterpret_cast<const char *>(descriptionIt->second.data()),
                     descriptionIt->second.size()) != tinyxml2::XML_SUCCESS) {
    diagnostic = "Packaged GDTF description.xml is malformed.";
    return false;
  }
  const tinyxml2::XMLElement *fixtureType = document.FirstChildElement("GDTF");
  fixtureType = fixtureType ? fixtureType->FirstChildElement("FixtureType")
                            : document.FirstChildElement("FixtureType");
  const tinyxml2::XMLElement *audit =
      fixtureType ? fixtureType->FirstChildElement("PerastageMutationAudit") : nullptr;
  int auditVersion = -1;
  const bool hasKnownAudit =
      audit && audit->QueryIntAttribute("SchemaVersion", &auditVersion) ==
                   tinyxml2::XML_SUCCESS &&
      auditVersion == 1;
  const bool hasUnknownAudit = audit && !hasKnownAudit;
  const bool perastageOwned =
      hasKnownAudit || (!audit && HasPerastageAudit(fixtureType));
  const tinyxml2::XMLElement *model = ResolvePreferredModel(fixtureType);
  const char *file = model ? model->Attribute("File") : nullptr;
  const char *name = model ? model->Attribute("Name") : nullptr;
  const std::string base = file && *file ? file : (name && *name ? name : "main");
  const std::set<std::string> required = {
      FoldAscii("models/svg/" + base + ".svg"),
      FoldAscii("models/svg/" + base + "_bottom.svg"),
      FoldAscii("models/svg_front/" + base + ".svg"),
      FoldAscii("models/svg_side/" + base + ".svg")};
  std::set<std::string> present;
  std::vector<GdtfSemanticFingerprintEntry> fingerprintEntries;
  for (const auto &[path, payload] : entries) {
    present.insert(FoldAscii(path));
    fingerprintEntries.push_back({path, payload});
  }
  if (!perastageOwned ||
      !std::includes(present.begin(), present.end(), required.begin(), required.end())) {
    diagnostic = hasUnknownAudit
                     ? "Packaged GDTF uses an unsupported Perastage mutation audit version."
                     : "Packaged GDTF does not contain a proven complete Perastage symbol set.";
    return false;
  }
  fingerprint = ComputeGdtfSemanticFingerprintFromEntries(fingerprintEntries, diagnostic);
  return !fingerprint.empty();
}

// Finds a prior timestamp without accepting prior metadata as validation proof.
std::string ProvenanceTimestamp(const SymbolCacheManifest *manifest,
                                const std::string &fixtureKey) {
  if (manifest) {
    for (const auto &entry : manifest->Entries()) {
      if (entry.fixtureKey == fixtureKey)
        return entry.lastGenerationTimestampUtc;
    }
  }
  return {};
}

} // namespace

// Builds the stable fixture-type key shared by persistence and auto-update planning.
std::string BuildFixtureSymbolCacheKey(const Fixture &fixture) {
  if (!fixture.typeName.empty())
    return fixture.typeName;
  if (!fixture.gdtfSpec.empty()) {
    std::string key = std::filesystem::path(fixture.gdtfSpec)
                          .lexically_normal()
                          .generic_string();
    return key;
  }
  return {};
}

// Collects immutable fixture identities for exact packaged-scene validation.
std::vector<ProjectFixtureSymbolIdentity>
CollectProjectFixtureSymbolIdentities(const MvrScene &scene) {
  std::vector<ProjectFixtureSymbolIdentity> identities;
  identities.reserve(scene.fixtures.size());
  for (const auto &[uuid, fixture] : scene.fixtures) {
    const std::string key = BuildFixtureSymbolCacheKey(fixture);
    if (!uuid.empty() && !key.empty())
      identities.push_back({uuid, key, fixture.typeName});
  }
  return identities;
}

// Builds a manifest snapshot exclusively from GDTFs packaged in the supplied MVR bytes.
ProjectSymbolCacheSnapshotResult BuildProjectSymbolCacheSnapshot(
    const std::vector<std::uint8_t> &sceneMvrBytes,
    const std::vector<ProjectFixtureSymbolIdentity> &fixtureIdentities,
    const SymbolCacheManifest *provenanceManifest,
    const std::string &newEntryTimestampUtc) {
  ProjectSymbolCacheSnapshotResult result;
  std::map<std::string, std::vector<std::uint8_t>> mvrEntries;
  if (!ReadArchive(sceneMvrBytes, mvrEntries, result.errorMessage))
    return result;
  auto sceneIt = std::find_if(mvrEntries.begin(), mvrEntries.end(), [](const auto &item) {
    return FoldAscii(item.first) == "generalscenedescription.xml";
  });
  if (sceneIt == mvrEntries.end()) {
    result.errorMessage = "scene.mvr does not contain GeneralSceneDescription.xml.";
    return result;
  }
  tinyxml2::XMLDocument sceneDocument;
  if (sceneDocument.Parse(reinterpret_cast<const char *>(sceneIt->second.data()),
                          sceneIt->second.size()) != tinyxml2::XML_SUCCESS) {
    result.errorMessage = "scene.mvr GeneralSceneDescription.xml is malformed.";
    return result;
  }
  std::vector<FixtureReference> references;
  CollectFixtureReferences(sceneDocument.RootElement(), references);
  std::unordered_map<std::string, std::string> gdtfByUuid;
  for (const auto &reference : references)
    gdtfByUuid.emplace(reference.uuid, reference.gdtfSpec);

  std::set<std::string> processedKeys;
  for (const auto &identity : fixtureIdentities) {
    if (!processedKeys.insert(identity.fixtureKey).second)
      continue;
    ProjectSymbolSnapshotOutcome outcome;
    outcome.fixtureKey = identity.fixtureKey;
    const auto referenceIt = gdtfByUuid.find(NormalizeUuid(identity.fixtureUuid));
    if (referenceIt == gdtfByUuid.end()) {
      outcome.status = ProjectSymbolSnapshotStatus::MissingReference;
      outcome.diagnostic = "Exported fixture has no identifiable packaged GDTF reference.";
      ++result.missingCount;
      ++result.omittedCount;
      result.warnings.push_back(identity.fixtureKey + ": " + outcome.diagnostic);
      result.outcomes.push_back(std::move(outcome));
      continue;
    }
    std::string archivePath;
    if (!NormalizeSafeArchivePath(referenceIt->second, archivePath)) {
      result.errorMessage = "scene.mvr contains an unsafe fixture GDTF reference.";
      return result;
    }
    const auto archiveIt = mvrEntries.find(archivePath);
    if (archiveIt == mvrEntries.end()) {
      outcome.status = ProjectSymbolSnapshotStatus::MissingArchive;
      outcome.diagnostic = "Referenced GDTF is missing from scene.mvr.";
      ++result.missingCount;
      ++result.omittedCount;
      result.warnings.push_back(identity.fixtureKey + ": " + outcome.diagnostic);
      result.outcomes.push_back(std::move(outcome));
      continue;
    }
    std::string fingerprint;
    if (!ValidatePackagedGdtf(archiveIt->second, fingerprint, outcome.diagnostic)) {
      outcome.status = ProjectSymbolSnapshotStatus::InvalidSymbols;
      ++result.omittedCount;
      result.warnings.push_back(identity.fixtureKey + ": " + outcome.diagnostic);
      result.outcomes.push_back(std::move(outcome));
      continue;
    }
    ValidationRequest request;
    request.fixtureKey = identity.fixtureKey;
    request.fixtureTypeName = identity.fixtureTypeName;
#ifdef _WIN32
    request.gdtfSpec = FoldAscii(archivePath);
#else
    request.gdtfSpec = archivePath;
#endif
    request.gdtfContentHash = fingerprint;
    request.requiredViews = RequiredPerastageSymbolViews();
    std::string timestamp = ProvenanceTimestamp(provenanceManifest, identity.fixtureKey);
    if (timestamp.empty())
      timestamp = newEntryTimestampUtc.empty() ? CurrentUtcTimestamp() : newEntryTimestampUtc;
    result.manifest.MarkFixtureSymbolsValid(request, std::move(timestamp));
    outcome.status = ProjectSymbolSnapshotStatus::Validated;
    ++result.validatedCount;
    result.outcomes.push_back(std::move(outcome));
  }
  result.sceneValid = true;
  return result;
}

// Returns the validation requests that still require automatic inspection or generation.
std::vector<std::size_t> PlanFixtureSymbolCacheMisses(
    const SymbolCacheManifest &manifest,
    const std::vector<ValidationRequest> &requests) {
  std::vector<std::size_t> misses;
  for (std::size_t index = 0; index < requests.size(); ++index) {
    if (!manifest.ValidateFixture(requests[index]).valid)
      misses.push_back(index);
  }
  return misses;
}

} // namespace symbol_cache
