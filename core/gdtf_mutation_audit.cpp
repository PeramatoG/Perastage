#include "gdtf_mutation_audit.h"

#include "app_version.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace GdtfMutationAudit {
namespace {

std::string BuildIsoTimestampUtcNow() {
  using Clock = std::chrono::system_clock;
  const auto now = Clock::now();
  const std::time_t nowTime = Clock::to_time_t(now);

  std::tm utcTm{};
#if defined(_WIN32)
  gmtime_s(&utcTm, &nowTime);
#else
  gmtime_r(&nowTime, &utcTm);
#endif

  std::ostringstream stamp;
  stamp << std::put_time(&utcTm, "%Y-%m-%dT%H:%M:%SZ");
  return stamp.str();
}

} // namespace

CompatibilityDecision
InspectCompatibility(const tinyxml2::XMLElement *fixtureType) {
  CompatibilityDecision decision;
  if (!fixtureType)
    return decision;

  const tinyxml2::XMLElement *auditNode =
      fixtureType->FirstChildElement("PerastageMutationAudit");
  if (!auditNode)
    return decision;

  int schemaVersion = -1;
  if (auditNode->QueryIntAttribute("SchemaVersion", &schemaVersion) !=
      tinyxml2::XML_SUCCESS) {
    decision.mode = CompatibilityMode::SafeFallbackUnknownVersion;
    decision.warning =
        "PerastageMutationAudit metadata is present but SchemaVersion is "
        "missing or invalid. Using safe fallback compatibility mode.";
    return decision;
  }

  if (schemaVersion == kPerastageGdtfMutationSchemaVersion) {
    decision.mode = CompatibilityMode::KnownPerastageVersion;
    return decision;
  }

  decision.mode = CompatibilityMode::SafeFallbackUnknownVersion;
  decision.warning =
      "PerastageMutationAudit SchemaVersion=" + std::to_string(schemaVersion) +
      " is not supported by this Perastage build. Using safe fallback "
      "compatibility mode.";
  return decision;
}

tinyxml2::XMLElement *EnsureFixtureType(tinyxml2::XMLDocument &doc) {
  tinyxml2::XMLElement *root = doc.FirstChildElement("GDTF");
  if (root) {
    tinyxml2::XMLElement *fixtureType = root->FirstChildElement("FixtureType");
    if (!fixtureType) {
      fixtureType = doc.NewElement("FixtureType");
      root->InsertEndChild(fixtureType);
    }
    return fixtureType;
  }

  tinyxml2::XMLElement *fixtureType = doc.FirstChildElement("FixtureType");
  if (fixtureType)
    return fixtureType;

  root = doc.NewElement("GDTF");
  doc.InsertEndChild(root);
  fixtureType = doc.NewElement("FixtureType");
  root->InsertEndChild(fixtureType);
  return fixtureType;
}

tinyxml2::XMLElement *EnsureRevisionsNode(tinyxml2::XMLElement *fixtureType,
                                          tinyxml2::XMLDocument &doc) {
  if (!fixtureType)
    return nullptr;

  tinyxml2::XMLElement *revisions = fixtureType->FirstChildElement("Revisions");
  if (!revisions) {
    revisions = doc.NewElement("Revisions");
    fixtureType->InsertEndChild(revisions);
  }
  return revisions;
}

std::string BuildPerastageModifiedBy() {
  return std::string("Perastage ") + app::kVersion;
}

void AppendRevision(tinyxml2::XMLElement *fixtureType,
                    tinyxml2::XMLDocument &doc,
                    const std::string &text,
                    const std::string &modifiedBy,
                    int userId,
                    const std::string &dateUtcIso8601) {
  if (!fixtureType)
    return;

  tinyxml2::XMLElement *revisions = EnsureRevisionsNode(fixtureType, doc);
  if (!revisions)
    return;

  tinyxml2::XMLElement *revision = doc.NewElement("Revision");
  const std::string timestamp =
      dateUtcIso8601.empty() ? BuildIsoTimestampUtcNow() : dateUtcIso8601;
  revision->SetAttribute("Date", timestamp.c_str());
  revision->SetAttribute("ModifiedBy",
                         modifiedBy.empty() ? BuildPerastageModifiedBy().c_str()
                                            : modifiedBy.c_str());
  revision->SetAttribute("Text", text.c_str());
  revision->SetAttribute("UserID", userId);
  revisions->InsertEndChild(revision);
}

void StampPerastageMutationMetadata(tinyxml2::XMLElement *fixtureType,
                                    tinyxml2::XMLDocument &doc) {
  if (!fixtureType)
    return;

  fixtureType->SetAttribute("Editor", "Perastage");

  tinyxml2::XMLElement *auditNode =
      fixtureType->FirstChildElement("PerastageMutationAudit");
  if (!auditNode) {
    auditNode = doc.NewElement("PerastageMutationAudit");
    fixtureType->InsertEndChild(auditNode);
  }

  auditNode->SetAttribute("SchemaVersion", kPerastageGdtfMutationSchemaVersion);
  auditNode->SetAttribute("PerastageVersion", app::kVersion);
  auditNode->SetAttribute("PerastageVersionDisplay", app::kVersionDisplay);
  auditNode->SetAttribute("LastMutationDateUtc", BuildIsoTimestampUtcNow().c_str());
}

tinyxml2::XMLElement *EnsurePhysicalPropertiesNode(
    tinyxml2::XMLElement *fixtureType, tinyxml2::XMLDocument &doc) {
  if (!fixtureType)
    return nullptr;

  tinyxml2::XMLElement *physicalDescriptions =
      fixtureType->FirstChildElement("PhysicalDescriptions");
  if (!physicalDescriptions) {
    physicalDescriptions = doc.NewElement("PhysicalDescriptions");
    fixtureType->InsertEndChild(physicalDescriptions);
  }

  tinyxml2::XMLElement *properties =
      physicalDescriptions->FirstChildElement("Properties");
  if (!properties) {
    properties = doc.NewElement("Properties");
    physicalDescriptions->InsertEndChild(properties);
  }
  return properties;
}

bool ApplyPhysicalProperties(tinyxml2::XMLElement *fixtureType,
                             tinyxml2::XMLDocument &doc,
                             const std::optional<float> &weightKg,
                             const std::optional<float> &powerConsumptionW) {
  if (!fixtureType)
    return false;

  if (!weightKg.has_value() && !powerConsumptionW.has_value())
    return false;

  tinyxml2::XMLElement *properties = EnsurePhysicalPropertiesNode(fixtureType, doc);
  if (!properties)
    return false;

  bool mutated = false;
  if (weightKg.has_value()) {
    tinyxml2::XMLElement *weightNode = properties->FirstChildElement("Weight");
    if (!weightNode)
      weightNode = properties->InsertNewChildElement("Weight");
    weightNode->SetAttribute("Value", *weightKg);
    mutated = true;
  }

  if (powerConsumptionW.has_value()) {
    tinyxml2::XMLElement *powerNode =
        properties->FirstChildElement("PowerConsumption");
    if (!powerNode)
      powerNode = properties->InsertNewChildElement("PowerConsumption");
    powerNode->SetAttribute("Value", *powerConsumptionW);
    mutated = true;
  }

  return mutated;
}

bool ApplyPhysicalPropertiesWithAudit(
    tinyxml2::XMLElement *fixtureType, tinyxml2::XMLDocument &doc,
    const std::optional<float> &weightKg,
    const std::optional<float> &powerConsumptionW, const std::string &revisionText,
    const std::string &modifiedBy) {
  const bool mutated =
      ApplyPhysicalProperties(fixtureType, doc, weightKg, powerConsumptionW);
  if (!mutated)
    return false;

  StampPerastageMutationMetadata(fixtureType, doc);
  AppendRevision(fixtureType, doc, revisionText, modifiedBy);
  return true;
}

} // namespace GdtfMutationAudit
