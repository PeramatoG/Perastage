#include "project_fixture_gdtf_consolidator.h"

#include "filesystem_path_utils.h"
#include "fixture_gdtf_derivative_contract.h"
#include "mvrscene.h"

#include <wx/wfstream.h>
#include <wx/zipstrm.h>

#include <tinyxml2.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <unordered_map>

namespace project_gdtf {
namespace {
namespace fs = std::filesystem;

// Normalizes a ZIP entry path for deterministic comparisons.
std::string NormalizeEntryPath(std::string path) {
  std::replace(path.begin(), path.end(), '\\', '/');
  while (path.rfind("./", 0) == 0)
    path.erase(0, 2);
  std::transform(path.begin(), path.end(), path.begin(),
                 [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
  return path;
}

// Returns whether a revision is exclusively the known fixture-symbol operation.
bool IsFixtureSymbolRevision(const tinyxml2::XMLElement *revision) {
  const char *modifiedBy = revision ? revision->Attribute("ModifiedBy") : nullptr;
  const char *text = revision ? revision->Attribute("Text") : nullptr;
  if (!modifiedBy || !text)
    return false;
  const std::string author = modifiedBy;
  const std::string action = text;
  return (author == "Perastage" || author.rfind("Perastage ", 0) == 0) &&
         action.rfind("Applied fixture SVG symbol views (", 0) == 0 &&
         !action.empty() && action.back() == ')';
}

// Resolves the model mutated by fixture-symbol application.
tinyxml2::XMLElement *ResolveSymbolModel(tinyxml2::XMLElement *fixtureType) {
  tinyxml2::XMLElement *models =
      fixtureType ? fixtureType->FirstChildElement("Models") : nullptr;
  tinyxml2::XMLElement *first = nullptr;
  for (tinyxml2::XMLElement *model =
           models ? models->FirstChildElement("Model") : nullptr;
       model; model = model->NextSiblingElement("Model")) {
    if (!first)
      first = model;
    const char *name = model->Attribute("Name");
    if (name && std::string(name) == "Main")
      return model;
  }
  return first;
}

// Normalizes only recognized fixture-symbol mutations in description.xml.
bool NormalizeDescription(std::vector<unsigned char> &bytes,
                          std::set<std::string> &derivedEntries,
                          std::string &errorMessage) {
  tinyxml2::XMLDocument doc;
  if (doc.Parse(reinterpret_cast<const char *>(bytes.data()), bytes.size()) !=
      tinyxml2::XML_SUCCESS) {
    errorMessage = "Could not parse GDTF description.xml.";
    return false;
  }
  tinyxml2::XMLElement *fixtureType = doc.FirstChildElement("GDTF");
  if (fixtureType)
    fixtureType = fixtureType->FirstChildElement("FixtureType");
  if (!fixtureType)
    fixtureType = doc.FirstChildElement("FixtureType");
  if (!fixtureType) {
    errorMessage = "GDTF description.xml has no FixtureType.";
    return false;
  }
  if (tinyxml2::XMLElement *audit =
          fixtureType->FirstChildElement("PerastageMutationAudit")) {
    int version = -1;
    if (audit->QueryIntAttribute("SchemaVersion", &version) !=
            tinyxml2::XML_SUCCESS ||
        version != 1) {
      errorMessage = "Unsupported Perastage mutation audit metadata.";
      return false;
    }
    fixtureType->DeleteChild(audit);
  }

  bool removedRevision = false;
  tinyxml2::XMLElement *revisions = fixtureType->FirstChildElement("Revisions");
  for (tinyxml2::XMLElement *revision =
           revisions ? revisions->FirstChildElement("Revision") : nullptr;
       revision;) {
    tinyxml2::XMLElement *next = revision->NextSiblingElement("Revision");
    if (IsFixtureSymbolRevision(revision)) {
      revisions->DeleteChild(revision);
      removedRevision = true;
    }
    revision = next;
  }
  if (revisions && !revisions->FirstChildElement())
    fixtureType->DeleteChild(revisions);

  tinyxml2::XMLElement *model = ResolveSymbolModel(fixtureType);
  if (model) {
    const char *file = model->Attribute("File");
    const char *name = model->Attribute("Name");
    const std::string base =
        file && *file ? file : (name && *name ? name : "main");
    derivedEntries = {NormalizeEntryPath("models/svg/" + base + ".svg"),
                      NormalizeEntryPath("models/svg/" + base + "_bottom.svg"),
                      NormalizeEntryPath("models/svg_front/" + base + ".svg"),
                      NormalizeEntryPath("models/svg_side/" + base + ".svg")};
    if (removedRevision) {
      static constexpr const char *kOffsets[] = {
          "SVGOffsetX", "SVGOffsetY", "SVGSideOffsetX", "SVGSideOffsetY",
          "SVGFrontOffsetX", "SVGFrontOffsetY"};
      for (const char *attribute : kOffsets)
        model->DeleteAttribute(attribute);
    }
  }
  tinyxml2::XMLPrinter printer(nullptr, true);
  doc.Print(&printer);
  const char *normalized = printer.CStr();
  bytes.assign(normalized, normalized + printer.CStrSize() - 1);
  return true;
}

// Updates an FNV-1a digest with an unambiguous byte sequence.
void UpdateHash(std::uint64_t &hash, const void *data, size_t size) {
  const auto *bytes = static_cast<const unsigned char *>(data);
  for (size_t index = 0; index < size; ++index) {
    hash ^= bytes[index];
    hash *= 1099511628211ull;
  }
}

// Resolves a project-relative fixture archive to its extracted file.
std::string ResolveProjectPath(const MvrScene &scene, const std::string &spec) {
  fs::path path = PathUtils::PathFromUtf8(spec);
  if (path.is_relative())
    path = PathUtils::PathFromUtf8(scene.basePath) / path;
  std::error_code ec;
  const fs::path canonical = fs::weakly_canonical(path, ec);
  return PathUtils::PathToUtf8(ec ? path.lexically_normal() : canonical);
}

// Returns whether a filename is the canonical unsuffixed derivative.
bool IsUnsuffixed(const std::string &spec) {
  const std::string stem = fs::path(spec).stem().string();
  const size_t underscore = stem.rfind('_');
  return underscore == std::string::npos ||
         !std::all_of(stem.begin() + static_cast<std::ptrdiff_t>(underscore + 1),
                      stem.end(), [](unsigned char ch) { return std::isdigit(ch); });
}

} // namespace

// Computes a fingerprint that excludes only recognized Perastage symbol output.
std::string ComputeBaseGdtfFingerprint(const std::string &path,
                                       std::string &errorMessage) {
  wxFileInputStream input(wxString::FromUTF8(path));
  if (!input.IsOk()) {
    errorMessage = "Could not open project GDTF.";
    return {};
  }
  wxZipInputStream zip(input);
  std::map<std::string, std::vector<unsigned char>> entries;
  std::unique_ptr<wxZipEntry> entry;
  while ((entry.reset(zip.GetNextEntry())), entry) {
    if (entry->IsDir())
      continue;
    std::vector<unsigned char> bytes;
    std::array<char, 8192> buffer{};
    while (true) {
      zip.Read(buffer.data(), buffer.size());
      const size_t count = zip.LastRead();
      if (!count)
        break;
      bytes.insert(bytes.end(), buffer.begin(), buffer.begin() + count);
    }
    entries[NormalizeEntryPath(entry->GetName().ToStdString())] =
        std::move(bytes);
  }
  auto description = entries.find("description.xml");
  if (description == entries.end()) {
    errorMessage = "Project GDTF has no description.xml.";
    return {};
  }
  std::set<std::string> derivedEntries;
  if (!NormalizeDescription(description->second, derivedEntries, errorMessage))
    return {};
  for (const std::string &derived : derivedEntries)
    entries.erase(derived);

  std::uint64_t hash = 1469598103934665603ull;
  constexpr char version[] = "gdtf-base-fnv1a64-v1\n";
  UpdateHash(hash, version, sizeof(version) - 1);
  std::uint64_t payloadSize = 0;
  for (const auto &[name, bytes] : entries) {
    UpdateHash(hash, name.data(), name.size());
    const char separator = '\0';
    UpdateHash(hash, &separator, 1);
    if (!bytes.empty())
      UpdateHash(hash, bytes.data(), bytes.size());
    UpdateHash(hash, &separator, 1);
    payloadSize += name.size() + bytes.size();
  }
  std::ostringstream result;
  result << "gdtfbasefnv1a64v1:" << std::hex << std::setw(16)
         << std::setfill('0') << hash << ':' << std::dec << entries.size()
         << ':' << payloadSize;
  errorMessage.clear();
  return result.str();
}

// Builds a deterministic and inspectable project-resource consolidation plan.
ConsolidationPlan BuildConsolidationPlan(const MvrScene &scene) {
  struct Candidate {
    std::string spec;
    std::string path;
    std::string mode;
    std::string fingerprint;
    bool validSymbols = false;
    std::vector<std::string> fixtureUuids;
  };
  std::map<std::string, Candidate> candidates;
  for (const auto &[uuid, fixture] : scene.fixtures) {
    if (fixture.gdtfSpec.empty())
      continue;
    const std::string key = fixture.gdtfSpec + "\n" + fixture.gdtfMode;
    Candidate &candidate = candidates[key];
    candidate.spec = fixture.gdtfSpec;
    candidate.mode = fixture.gdtfMode;
    candidate.fixtureUuids.push_back(uuid);
  }
  ConsolidationPlan plan;
  for (auto &[key, candidate] : candidates) {
    (void)key;
    std::sort(candidate.fixtureUuids.begin(), candidate.fixtureUuids.end());
    candidate.path = ResolveProjectPath(scene, candidate.spec);
    std::string error;
    candidate.fingerprint =
        ComputeBaseGdtfFingerprint(candidate.path, error);
    if (candidate.fingerprint.empty()) {
      plan.complete = false;
      plan.diagnostics.push_back(
          "GDTF legacy migration skipped archive='" +
          fs::path(candidate.spec).filename().string() + "' reason='" +
          error + "'");
    }
    std::string contractError;
    candidate.validSymbols =
        !candidate.fingerprint.empty() &&
        fixture_gdtf::ValidatePublishedDerivative(candidate.path,
                                                  contractError);
  }
  std::map<std::string, std::vector<Candidate *>> groups;
  for (auto &[key, candidate] : candidates) {
    (void)key;
    if (!candidate.fingerprint.empty())
      groups[candidate.fingerprint + "\n" + candidate.mode].push_back(&candidate);
  }

  for (auto &[groupKey, groupCandidates] : groups) {
    if (groupCandidates.size() < 2)
      continue;
    std::sort(groupCandidates.begin(), groupCandidates.end(),
              [](const Candidate *left, const Candidate *right) {
                if (left->validSymbols != right->validSymbols)
                  return left->validSymbols > right->validSymbols;
                if (IsUnsuffixed(left->spec) != IsUnsuffixed(right->spec))
                  return IsUnsuffixed(left->spec);
                return left->spec < right->spec;
              });
    const Candidate &survivor = *groupCandidates.front();
    ConsolidationGroup group;
    group.baseFingerprint = survivor.fingerprint;
    group.mode = survivor.mode;
    group.survivorGdtfSpec = survivor.spec;
    for (const Candidate *candidate : groupCandidates) {
      group.candidateGdtfSpecs.push_back(candidate->spec);
      for (const std::string &uuid : candidate->fixtureUuids) {
        if (candidate->spec != survivor.spec)
          group.rebindings.push_back({uuid, candidate->spec, survivor.spec});
      }
    }
    std::sort(group.candidateGdtfSpecs.begin(),
              group.candidateGdtfSpecs.end());
    std::sort(group.rebindings.begin(), group.rebindings.end(),
              [](const Rebind &left, const Rebind &right) {
                return left.fixtureUuid < right.fixtureUuid;
              });
    plan.diagnostics.push_back(
        "GDTF consolidation mode='" + group.mode + "' candidates=" +
        std::to_string(group.candidateGdtfSpecs.size()) + " survivor='" +
        fs::path(group.survivorGdtfSpec).filename().string() + "' rebound=" +
        std::to_string(group.rebindings.size()));
    plan.groups.push_back(std::move(group));
  }
  return plan;
}

// Applies a fully validated plan atomically to fixture GDTF references.
bool ApplyConsolidationPlan(MvrScene &scene, const ConsolidationPlan &plan,
                            std::string &errorMessage) {
  for (const ConsolidationGroup &group : plan.groups) {
    for (const Rebind &rebind : group.rebindings) {
      const auto fixture = scene.fixtures.find(rebind.fixtureUuid);
      if (fixture == scene.fixtures.end() ||
          fixture->second.gdtfSpec != rebind.oldGdtfSpec) {
        errorMessage = "GDTF consolidation plan no longer matches the scene.";
        return false;
      }
    }
  }
  for (const ConsolidationGroup &group : plan.groups)
    for (const Rebind &rebind : group.rebindings)
      scene.fixtures.at(rebind.fixtureUuid).gdtfSpec = rebind.newGdtfSpec;
  errorMessage.clear();
  return true;
}

} // namespace project_gdtf
