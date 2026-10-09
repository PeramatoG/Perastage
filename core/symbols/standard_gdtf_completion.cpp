#include "standard_gdtf_completion.h"

#include "PerastageSvgSymbol.h"
#include "gdtf_archive_reader.h"
#include "gdtf_canonicalizer.h"
#include "gdtf_mutation_audit.h"
#include "gdtf_publication_resources.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <set>
#include <utility>

#include <tinyxml2.h>

namespace symbols {
namespace {

constexpr std::array<SymbolViewKind, 3> kViews = {
    SymbolViewKind::Top, SymbolViewKind::Left, SymbolViewKind::Front};

std::string Lower(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });
  return value;
}

SymbolViewKind NormalizeView(SymbolViewKind view) {
  return view == SymbolViewKind::Right ? SymbolViewKind::Left : view;
}

const char *ViewLabel(SymbolViewKind view) {
  if (view == SymbolViewKind::Top)
    return "Top";
  return view == SymbolViewKind::Front ? "Front" : "Side";
}

std::pair<const char *, const char *> OffsetAttributes(SymbolViewKind view) {
  if (view == SymbolViewKind::Front)
    return {"SVGFrontOffsetX", "SVGFrontOffsetY"};
  if (view == SymbolViewKind::Left)
    return {"SVGSideOffsetX", "SVGSideOffsetY"};
  return {"SVGOffsetX", "SVGOffsetY"};
}

tinyxml2::XMLElement *ResolveModel(tinyxml2::XMLDocument &document,
                                 const std::string &requestedFile,
                                 std::string &error) {
  auto *root = document.FirstChildElement("GDTF");
  if (!root || !root->Attribute("DataVersion") ||
      std::string(root->Attribute("DataVersion")) != "1.2") {
    error = "Standard SVG completion currently targets declared GDTF 1.2 only.";
    return nullptr;
  }
  auto *fixture = root->FirstChildElement("FixtureType");
  auto *models = fixture ? fixture->FirstChildElement("Models") : nullptr;
  tinyxml2::XMLElement *selected = nullptr;
  for (auto *model = models ? models->FirstChildElement("Model") : nullptr;
       model; model = model->NextSiblingElement("Model")) {
    const char *file = model->Attribute("File");
    if (!requestedFile.empty()) {
      if (file && requestedFile == file) {
        if (selected) {
          error = "The requested Model/@File identifies multiple models.";
          return nullptr;
        }
        selected = model;
      }
    } else {
      if (!selected)
        selected = model;
      const char *name = model->Attribute("Name");
      if (name && std::string(name) == "Main") {
        selected = model;
        break;
      }
    }
  }
  const char *file = selected ? selected->Attribute("File") : nullptr;
  if (!file || !*file) {
    error = "Standard SVG completion requires an exact nonempty Model/@File.";
    return nullptr;
  }
  const std::string value = file;
  if (value == "." || value == ".." || value.find_first_of("/\\:") !=
                                        std::string::npos) {
    error = "Model/@File is not a safe standard resource basename.";
    return nullptr;
  }
  return selected;
}

bool HasPrivateMetadata(const tinyxml2::XMLElement *element) {
  for (const auto *attribute = element->FirstAttribute(); attribute;
       attribute = attribute->Next())
    if (Lower(attribute->Name()).rfind("data-perastage-", 0) == 0)
      return true;
  for (const auto *child = element->FirstChildElement(); child;
       child = child->NextSiblingElement())
    if (HasPrivateMetadata(child))
      return true;
  return false;
}

bool ValidateCandidate(const StandardGdtfSvgCandidate &candidate,
                       std::string &error) {
  tinyxml2::XMLDocument svg;
  if (svg.Parse(candidate.svg.c_str(), candidate.svg.size()) !=
          tinyxml2::XML_SUCCESS || !svg.FirstChildElement("svg")) {
    error = "A generated standard SVG is malformed.";
    return false;
  }
  if (HasPrivateMetadata(svg.FirstChildElement("svg"))) {
    error = "A standard GDTF SVG cannot contain private Perastage metadata.";
    return false;
  }
  PerastageSvgSymbolData parsed;
  if (!std::isfinite(candidate.offsetXmm) ||
      !std::isfinite(candidate.offsetYmm) ||
      !ParseFixtureSymbolSvg(candidate.svg, parsed, &error)) {
    if (error.empty())
      error = "A generated standard SVG or its offsets are unusable.";
    return false;
  }
  return true;
}

struct InspectionContext {
  gdtf::ArchiveReadResult archive;
  tinyxml2::XMLDocument document;
  tinyxml2::XMLElement *model = nullptr;
  std::string modelFile;
};

bool Inspect(const std::filesystem::path &sourcePath,
             const std::string &requestedFile, InspectionContext &context,
             StandardGdtfMutationResult &result) {
  context.archive = gdtf::ReadGdtfArchive(sourcePath);
  if (!context.archive.Success()) {
    result.errorMessage = "Could not inspect the standard GDTF SVG source.";
    return false;
  }
  if (context.document.Parse(context.archive.descriptionXml.c_str(),
                             context.archive.descriptionXml.size()) !=
      tinyxml2::XML_SUCCESS) {
    result.errorMessage = "The GDTF description is malformed.";
    return false;
  }
  context.model = ResolveModel(context.document, requestedFile, result.errorMessage);
  if (!context.model)
    return false;
  context.modelFile = context.model->Attribute("File");
  for (const auto view : kViews) {
    StandardGdtfViewInspection inspected;
    inspected.viewKind = view;
    inspected.archivePath = StandardGdtfSvgPath(context.modelFile, view);
    std::vector<std::string> matches;
    for (const auto &entry : context.archive.entries)
      if (!entry.directory && Lower(entry.path) == Lower(inspected.archivePath))
        matches.push_back(entry.path);
    if (!matches.empty()) {
      inspected.state = StandardGdtfViewState::ExistingUnusable;
      if (matches.size() != 1) {
        inspected.diagnostic = "The standard SVG path is ambiguous.";
      } else {
        inspected.archivePath = matches.front();
        const auto payload = gdtf::ReadGdtfArchiveResource(sourcePath, matches.front());
        if (payload.Success() && !payload.filesystemFallback &&
            payload.entryPath == matches.front()) {
          const std::string svg(payload.bytes.begin(), payload.bytes.end());
          PerastageSvgSymbolData parsed;
          if (ParseFixtureSymbolSvg(svg, parsed, &inspected.diagnostic)) {
            inspected.state = StandardGdtfViewState::ExistingUsable;
            inspected.diagnostic.clear();
          }
        }
        if (inspected.state == StandardGdtfViewState::ExistingUnusable &&
            inspected.diagnostic.empty())
          inspected.diagnostic = "The existing standard SVG is empty or unreadable.";
      }
    }
    if (inspected.state == StandardGdtfViewState::ExistingUnusable)
      result.diagnostics.push_back(inspected.archivePath + ": " + inspected.diagnostic);
    result.views.push_back(std::move(inspected));
  }
  result.success = true;
  return true;
}

int RevisionCount(tinyxml2::XMLElement *fixture) {
  const auto *revisions = fixture ? fixture->FirstChildElement("Revisions") : nullptr;
  int count = 0;
  for (const auto *revision = revisions ? revisions->FirstChildElement("Revision") : nullptr;
       revision; revision = revision->NextSiblingElement("Revision"))
    ++count;
  return count;
}

bool CanonicalizeAndAudit(tinyxml2::XMLDocument &document,
                         const std::string &action, bool legacyResources,
                         std::string &error) {
  auto *fixture = GdtfMutationAudit::EnsureFixtureType(document);
  const int previousRevisions = RevisionCount(fixture);
  GdtfCanonicalizer::Options options;
  options.allowPlaceholderFixtureTypeIdRepair = true;
  options.stableIdSeed = "Perastage canonical GDTF publication";
  const auto canonical = GdtfCanonicalizer::CanonicalizeDescription(document, options);
  if (!canonical.success) {
    error = canonical.errors.empty() ? "GDTF canonicalization failed." : canonical.errors.front();
    return false;
  }
  fixture = GdtfMutationAudit::EnsureFixtureType(document);
  // One effective operation includes its necessary standardization repairs.
  auto *revisions = fixture->FirstChildElement("Revisions");
  int index = 0;
  for (auto *revision = revisions ? revisions->FirstChildElement("Revision") : nullptr;
       revision;) {
    auto *next = revision->NextSiblingElement("Revision");
    if (index++ >= previousRevisions)
      revisions->DeleteChild(revision);
    revision = next;
  }
  std::string text = action;
  if (legacyResources || canonical.changed)
    text += "; standardized legacy definition/resources";
  GdtfMutationAudit::AppendRevision(
      fixture, document, text, GdtfMutationAudit::BuildPerastageModifiedBy(), 0, "",
      GdtfMutationAudit::RevisionPolicy::RecordEffectiveChange);
  return true;
}

StandardGdtfMutationResult Mutate(
    const std::filesystem::path &sourcePath,
    const fixture_gdtf::PreparedDerivative &prepared,
    const std::vector<StandardGdtfSvgCandidate> &candidates,
    bool automatic, StandardGdtfReplacementIntent intent,
    const std::string &modelFile, const StandardGdtfPublication &publish) {
  StandardGdtfMutationResult result;
  InspectionContext context;
  auto fail = [&]() {
    result.success = false;
    fixture_gdtf::DiscardPreparedDerivative(prepared);
    return result;
  };
  if (!Inspect(sourcePath, modelFile, context, result))
    return fail();
  std::vector<GdtfCanonicalizer::ResourceMutation> resources;
  std::vector<StandardGdtfSvgCandidate> applied;
  std::set<SymbolViewKind> candidateViews;
  for (const auto &candidate : candidates) {
    const auto view = NormalizeView(candidate.viewKind);
    if (StandardGdtfSvgPath(context.modelFile, view).empty() ||
        !candidateViews.insert(view).second) {
      result.errorMessage = "A standard completion request contains unsupported or duplicate views.";
      return fail();
    }
    const auto inspected = std::find_if(result.views.begin(), result.views.end(),
        [&](const auto &entry) { return entry.viewKind == view; });
    if (automatic && inspected->state != StandardGdtfViewState::Missing)
      continue;
    if (!automatic && (inspected->state == StandardGdtfViewState::Missing ||
        (intent == StandardGdtfReplacementIntent::RepairInvalid &&
         inspected->state != StandardGdtfViewState::ExistingUnusable))) {
      result.errorMessage = "Explicit repair/replacement requires the corresponding existing resource.";
      return fail();
    }
    if (!ValidateCandidate(candidate, result.errorMessage))
      return fail();
    const std::string exactPath = StandardGdtfSvgPath(context.modelFile, view);
    if (!automatic) {
      const auto bytes = gdtf::ReadGdtfArchiveResource(sourcePath, inspected->archivePath);
      const auto attributes = OffsetAttributes(view);
      double x = 0, y = 0;
      context.model->QueryDoubleAttribute(attributes.first, &x);
      context.model->QueryDoubleAttribute(attributes.second, &y);
      if (bytes.Success() && inspected->archivePath == exactPath &&
          std::string(bytes.bytes.begin(), bytes.bytes.end()) == candidate.svg &&
          x == candidate.offsetXmm && y == candidate.offsetYmm)
        continue;
      if (inspected->archivePath != exactPath)
        resources.push_back({inspected->archivePath, std::nullopt});
    }
    resources.push_back({exactPath, candidate.svg});
    applied.push_back(candidate);
    applied.back().viewKind = view;
  }
  if (applied.empty()) {
    fixture_gdtf::DiscardPreparedDerivative(prepared);
    return result;
  }
  std::sort(applied.begin(), applied.end(), [](const auto &left, const auto &right) {
    return std::find(kViews.begin(), kViews.end(), left.viewKind) <
           std::find(kViews.begin(), kViews.end(), right.viewKind);
  });
  if (prepared.workingPath.empty() || prepared.publishedPath.empty() ||
      prepared.publishedReference.empty()) {
    result.errorMessage = "Standard completion requires a prepared owned derivative.";
    return fail();
  }
  std::vector<GdtfCanonicalizer::ResourceMutation> legacy;
  if (!fixture_gdtf::BuildStandardPublicationResourceMutations(sourcePath, legacy,
                                                              result.errorMessage))
    return fail();
  // Explicit standard resource changes take precedence over legacy cleanup.
  legacy.insert(legacy.end(), resources.begin(), resources.end());
  std::string action = automatic ? "Added missing standard SVG " : "Replaced standard SVG ";
  for (std::size_t index = 0; index < applied.size(); ++index) {
    if (index)
      action += index + 1 == applied.size() ? " and " : ", ";
    action += ViewLabel(applied[index].viewKind);
  }
  action += applied.size() == 1 ? " view" : " views";
  if (!automatic)
    action += intent == StandardGdtfReplacementIntent::RepairInvalid
                  ? " after explicit user repair" : " after explicit user replacement";
  const bool legacyChanged = legacy.size() != resources.size();
  const auto rewrite = GdtfCanonicalizer::RewriteArchiveResources(
      sourcePath, prepared.workingPath, legacy, [&](tinyxml2::XMLDocument &document) {
        auto *model = ResolveModel(document, context.modelFile, result.errorMessage);
        if (!model)
          return false;
        for (const auto &candidate : applied) {
          const auto attributes = OffsetAttributes(candidate.viewKind);
          model->SetAttribute(attributes.first, candidate.offsetXmm);
          model->SetAttribute(attributes.second, candidate.offsetYmm);
        }
        return CanonicalizeAndAudit(document, action, legacyChanged, result.errorMessage);
      });
  if (!rewrite.success) {
    if (result.errorMessage.empty())
      result.errorMessage = rewrite.errors.empty() ? "Standard SVG mutation failed." : rewrite.errors.front();
    return fail();
  }
  if (!(publish ? publish(prepared, result.errorMessage)
                : fixture_gdtf::PublishPreparedDerivative(prepared, result.errorMessage)))
    return fail();
  result.changed = true;
  result.publishedPath = prepared.publishedPath;
  return result;
}

} // namespace

StandardGdtfMutationResult InspectStandardGdtfViews(
    const std::filesystem::path &sourcePath, std::string modelFile) {
  StandardGdtfMutationResult result;
  InspectionContext context;
  Inspect(sourcePath, modelFile, context, result);
  return result;
}

StandardGdtfMutationResult CompleteStandardGdtfViews(
    const std::filesystem::path &sourcePath,
    const fixture_gdtf::PreparedDerivative &prepared,
    const std::vector<StandardGdtfSvgCandidate> &candidates,
    gdtf::MutationPolicy policy, std::string modelFile,
    const StandardGdtfPublication &publish) {
  if (!gdtf::AllowsAutomaticCompletion(policy)) {
    auto result = InspectStandardGdtfViews(sourcePath, std::move(modelFile));
    fixture_gdtf::DiscardPreparedDerivative(prepared);
    return result;
  }
  return Mutate(sourcePath, prepared, candidates, true,
                StandardGdtfReplacementIntent::RepairInvalid, modelFile, publish);
}

StandardGdtfMutationResult ReplaceStandardGdtfViews(
    const std::filesystem::path &sourcePath,
    const fixture_gdtf::PreparedDerivative &prepared,
    const std::vector<StandardGdtfSvgCandidate> &candidates,
    StandardGdtfReplacementIntent intent, std::string modelFile,
    const StandardGdtfPublication &publish) {
  return Mutate(sourcePath, prepared, candidates, false, intent, modelFile, publish);
}

} // namespace symbols
