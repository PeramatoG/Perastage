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
  std::string gdtfMode;
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

// Converts a wx archive name to UTF-8 without consulting the process locale.
std::string ArchiveNameToUtf8(const wxArchiveEntry &entry) {
  const wxString name = entry.GetName(wxPATH_UNIX);
  const wxScopedCharBuffer utf8 = name.utf8_str();
  return utf8.data() ? std::string(utf8.data()) : std::string{};
}

// Reads a little-endian 16-bit value from ZIP metadata.
std::uint16_t ReadLe16(const std::uint8_t *bytes) {
  return static_cast<std::uint16_t>(bytes[0]) |
         (static_cast<std::uint16_t>(bytes[1]) << 8);
}

// Reads a little-endian 32-bit value from ZIP metadata.
std::uint32_t ReadLe32(const std::uint8_t *bytes) {
  return static_cast<std::uint32_t>(bytes[0]) |
         (static_cast<std::uint32_t>(bytes[1]) << 8) |
         (static_cast<std::uint32_t>(bytes[2]) << 16) |
         (static_cast<std::uint32_t>(bytes[3]) << 24);
}

// Preflights raw central-directory names before wxWidgets presents normalized paths.
bool ValidateRawArchiveNames(const std::vector<std::uint8_t> &bytes,
                             std::string &errorMessage) {
  constexpr std::uint32_t kEndSignature = 0x06054b50;
  constexpr std::uint32_t kCentralSignature = 0x02014b50;
  if (bytes.size() < 22) {
    errorMessage = "Archive central directory is malformed.";
    return false;
  }
  const std::size_t searchStart =
      bytes.size() > 65557 ? bytes.size() - 65557 : 0;
  std::size_t endOffset = bytes.size() - 22;
  bool foundEnd = false;
  while (true) {
    if (ReadLe32(bytes.data() + endOffset) == kEndSignature &&
        endOffset + 22ULL + ReadLe16(bytes.data() + endOffset + 20) ==
            bytes.size()) {
      foundEnd = true;
      break;
    }
    if (endOffset == searchStart)
      break;
    --endOffset;
  }
  if (!foundEnd) {
    errorMessage = "Archive central directory is malformed.";
    return false;
  }
  const std::uint16_t entryCount = ReadLe16(bytes.data() + endOffset + 10);
  const std::uint32_t centralSize = ReadLe32(bytes.data() + endOffset + 12);
  const std::uint32_t centralOffset = ReadLe32(bytes.data() + endOffset + 16);
  if (centralOffset > endOffset || centralSize > endOffset - centralOffset) {
    errorMessage = "Archive central directory is malformed.";
    return false;
  }
  std::size_t cursor = centralOffset;
  std::unordered_set<std::string> identities;
  for (std::uint16_t index = 0; index < entryCount; ++index) {
    if (cursor > endOffset || endOffset - cursor < 46 ||
        ReadLe32(bytes.data() + cursor) != kCentralSignature) {
      errorMessage = "Archive central directory is malformed.";
      return false;
    }
    const std::uint16_t nameLength = ReadLe16(bytes.data() + cursor + 28);
    const std::uint16_t extraLength = ReadLe16(bytes.data() + cursor + 30);
    const std::uint16_t commentLength = ReadLe16(bytes.data() + cursor + 32);
    const std::uint32_t localOffset = ReadLe32(bytes.data() + cursor + 42);
    const std::size_t recordSize =
        46ULL + nameLength + extraLength + commentLength;
    if (nameLength == 0 || recordSize > endOffset - cursor) {
      errorMessage = "Archive central directory is malformed.";
      return false;
    }
    std::string rawName(reinterpret_cast<const char *>(bytes.data() + cursor + 46),
                        nameLength);
    if (localOffset >= centralOffset || centralOffset - localOffset < 30 ||
        ReadLe32(bytes.data() + localOffset) != 0x04034b50) {
      errorMessage = "Archive local entry header is malformed.";
      return false;
    }
    const std::uint16_t localNameLength = ReadLe16(bytes.data() + localOffset + 26);
    const std::uint16_t localExtraLength = ReadLe16(bytes.data() + localOffset + 28);
    if (30ULL + localNameLength + localExtraLength > centralOffset - localOffset) {
      errorMessage = "Archive local entry header is malformed.";
      return false;
    }
    const std::string localName(
        reinterpret_cast<const char *>(bytes.data() + localOffset + 30),
        localNameLength);
    if (localName != rawName) {
      errorMessage = "Archive local and central entry names do not match.";
      return false;
    }
    const wxString decoded = wxString::FromUTF8(rawName.data(), rawName.size());
    const wxScopedCharBuffer encoded = decoded.utf8_str();
    if (encoded.data() == nullptr || std::string(encoded.data()) != rawName) {
      errorMessage = "Archive contains a malformed UTF-8 entry name.";
      return false;
    }
    const bool directory = rawName.back() == '/';
    if (directory)
      rawName.pop_back();
    std::string normalized;
    if (!NormalizeSafeArchivePath(rawName, normalized)) {
      errorMessage = "Archive contains an unsafe or malformed entry path.";
      return false;
    }
    if (!identities.insert(FoldAscii(normalized)).second) {
      errorMessage = "Archive contains duplicate or ambiguous entry paths.";
      return false;
    }
    cursor += recordSize;
  }
  if (cursor != static_cast<std::size_t>(centralOffset) + centralSize) {
    errorMessage = "Archive central directory is malformed.";
    return false;
  }
  return true;
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
  if (!ValidateRawArchiveNames(bytes, errorMessage))
    return false;
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
    if (!NormalizeSafeArchivePath(ArchiveNameToUtf8(*entry), path)) {
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
    tinyxml2::XMLElement *mode = current->FirstChildElement("GDTFMode");
    if (uuid && gdtf && gdtf->GetText())
      references.push_back({NormalizeUuid(uuid), gdtf->GetText(),
                            mode && mode->GetText() ? mode->GetText() : ""});
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
                          std::string &fixtureTypeName,
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
  const char *fixtureName = fixtureType ? fixtureType->Attribute("Name") : nullptr;
  fixtureTypeName = fixtureName ? fixtureName : "";
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
                                const std::string &generationIdentityKey) {
  if (manifest) {
    for (const auto &entry : manifest->Entries()) {
      if (entry.generationIdentity.key == generationIdentityKey)
        return entry.lastGenerationTimestampUtc;
    }
  }
  return {};
}

} // namespace

// Collects immutable fixture identities for exact packaged-scene validation.
std::vector<ProjectFixtureSymbolIdentity>
CollectProjectFixtureSymbolIdentities(const MvrScene &scene) {
  std::vector<ProjectFixtureSymbolIdentity> identities;
  identities.reserve(scene.fixtures.size());
  for (const auto &[uuid, fixture] : scene.fixtures) {
    if (!uuid.empty())
      identities.push_back({uuid, fixture.typeName, fixture.gdtfMode});
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
  std::unordered_map<std::string, FixtureReference> gdtfByUuid;
  for (const auto &reference : references)
    gdtfByUuid.emplace(reference.uuid, reference);

  std::set<std::string> validatedKeys;
  for (const auto &identity : fixtureIdentities) {
    ProjectSymbolSnapshotOutcome outcome;
    const auto referenceIt = gdtfByUuid.find(NormalizeUuid(identity.fixtureUuid));
    if (referenceIt == gdtfByUuid.end()) {
      outcome.status = ProjectSymbolSnapshotStatus::MissingReference;
      outcome.diagnostic = "Exported fixture has no identifiable packaged GDTF reference.";
      ++result.missingCount;
      ++result.omittedCount;
      result.warnings.push_back(identity.fixtureTypeName + ": " + outcome.diagnostic);
      result.outcomes.push_back(std::move(outcome));
      continue;
    }
    std::string archivePath;
    if (!NormalizeSafeArchivePath(referenceIt->second.gdtfSpec, archivePath)) {
      result.errorMessage = "scene.mvr contains an unsafe fixture GDTF reference.";
      return result;
    }
    const auto archiveIt = mvrEntries.find(archivePath);
    if (archiveIt == mvrEntries.end()) {
      outcome.status = ProjectSymbolSnapshotStatus::MissingArchive;
      outcome.diagnostic = "Referenced GDTF is missing from scene.mvr.";
      ++result.missingCount;
      ++result.omittedCount;
      result.warnings.push_back(identity.fixtureTypeName + ": " + outcome.diagnostic);
      result.outcomes.push_back(std::move(outcome));
      continue;
    }
    std::string fingerprint;
    std::string packagedFixtureTypeName;
    if (!ValidatePackagedGdtf(archiveIt->second, fingerprint,
                              packagedFixtureTypeName, outcome.diagnostic)) {
      outcome.status = ProjectSymbolSnapshotStatus::InvalidSymbols;
      ++result.omittedCount;
      result.warnings.push_back(identity.fixtureTypeName + ": " + outcome.diagnostic);
      result.outcomes.push_back(std::move(outcome));
      continue;
    }
    FixtureSymbolGenerationIdentity generationIdentity;
    std::string identityError;
    const std::string mode = referenceIt->second.gdtfMode;
    if (!BuildFixtureSymbolGenerationIdentity(
            archivePath, mode, kCurrentPerastageSymbolFormatVersion, fingerprint,
            identity.fixtureTypeName, generationIdentity, identityError)) {
      outcome.status = ProjectSymbolSnapshotStatus::Failed;
      outcome.diagnostic = identityError;
      ++result.failedCount;
      ++result.omittedCount;
      result.warnings.push_back(identity.fixtureTypeName + ": " + identityError);
      result.outcomes.push_back(std::move(outcome));
      continue;
    }
    outcome.generationIdentityKey = generationIdentity.key;
    if (!validatedKeys.insert(generationIdentity.key).second)
      continue;
    ValidationRequest request;
    request.generationIdentity = generationIdentity;
    request.fixtureTypeName = packagedFixtureTypeName.empty()
                                  ? identity.fixtureTypeName
                                  : packagedFixtureTypeName;
    request.gdtfSpec = archivePath;
    request.gdtfContentHash = fingerprint;
    request.requiredViews = RequiredPerastageSymbolViews();
    std::string timestamp =
        ProvenanceTimestamp(provenanceManifest, generationIdentity.key);
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
