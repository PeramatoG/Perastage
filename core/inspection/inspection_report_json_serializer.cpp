#include "inspection_report_json_serializer.h"

#include "inspection_json_serialization_detail.h"

#include "json.hpp"

#include <stdexcept>
#include <string>

namespace perastage::inspection::serialization {
namespace {

// Returns a stable package kind token.
const char *PackageKindToken(PackageKind kind) {
  return kind == PackageKind::Gdtf ? "gdtf" : "mvr";
}

// Serializes package inventory in its authoritative order.
nlohmann::json
SerializePackage(const std::optional<PackageInventory> &inventory) {
  if (!inventory)
    return nullptr;
  nlohmann::json entries = nlohmann::json::array();
  for (const PackageEntry &entry : inventory->entries) {
    nlohmann::json item{
        {"type", entry.type == PackageEntryType::File ? "file" : "directory"},
        {"display_path", entry.displayPath},
        {"normalized_path", nullptr},
        {"extension", entry.extension},
        {"size", entry.sizeKnown ? nlohmann::json(entry.uncompressedSize)
                                 : nlohmann::json(nullptr)},
        {"path_safe", entry.pathSafe}};
    if (entry.normalizedPath)
      item["normalized_path"] = *entry.normalizedPath;
    entries.push_back(std::move(item));
  }
  return {{"kind", PackageKindToken(inventory->kind)},
          {"canonical_root_document_present",
           inventory->canonicalRootDocumentPresent},
          {"entries", std::move(entries)}};
}

// Returns a stable resource kind token.
const char *ResourceKindToken(ResourceKind kind) {
  switch (kind) {
  case ResourceKind::XmlText:
    return "xml_text";
  case ResourceKind::Text:
    return "text";
  case ResourceKind::Image:
    return "image";
  case ResourceKind::Model:
    return "model";
  case ResourceKind::NestedGdtf:
    return "nested_gdtf";
  case ResourceKind::Binary:
    return "binary";
  }
  throw std::invalid_argument("Unknown inspection resource kind");
}

// Serializes ordered neutral resource descriptors.
nlohmann::json
SerializeResources(const std::vector<ResourceDescriptor> &resources) {
  nlohmann::json serialized = nlohmann::json::array();
  for (const ResourceDescriptor &resource : resources) {
    nlohmann::json item{
        {"display_path", resource.displayPath},
        {"normalized_path", nullptr},
        {"kind", ResourceKindToken(resource.kind)},
        {"entry_type",
         resource.entryType == PackageEntryType::File ? "file" : "directory"},
        {"size", resource.sizeKnown ? nlohmann::json(resource.size)
                                    : nlohmann::json(nullptr)},
        {"path_safe", resource.pathSafe},
        {"raw_read_supported", resource.rawReadSupported},
        {"text_preview_supported", resource.textPreviewSupported}};
    if (resource.normalizedPath)
      item["normalized_path"] = *resource.normalizedPath;
    serialized.push_back(std::move(item));
  }
  return serialized;
}

// Returns a stable validation layer token.
const char *ValidationLayerToken(ValidationLayer layer) {
  switch (layer) {
  case ValidationLayer::XmlWellFormedness:
    return "xml_well_formedness";
  case ValidationLayer::Schema:
    return "schema";
  case ValidationLayer::SemanticInteroperability:
    return "semantic_interoperability";
  }
  throw std::invalid_argument("Unknown validation layer");
}

// Returns a stable validation status token.
const char *ValidationStatusToken(ValidationStatus status) {
  switch (status) {
  case ValidationStatus::NotRun:
    return "not_run";
  case ValidationStatus::Unavailable:
    return "unavailable";
  case ValidationStatus::Valid:
    return "valid";
  case ValidationStatus::Invalid:
    return "invalid";
  }
  throw std::invalid_argument("Unknown validation status");
}

// Serializes validation layers and their uncollapsed findings.
nlohmann::json
SerializeValidation(const std::vector<ValidationResult> &validation) {
  nlohmann::json serialized = nlohmann::json::array();
  for (const ValidationResult &item : validation) {
    nlohmann::json diagnostics = nlohmann::json::array();
    for (const Diagnostic &diagnostic : item.diagnostics)
      diagnostics.push_back(detail::SerializeDiagnostic(diagnostic));
    nlohmann::json layer{{"layer", ValidationLayerToken(item.layer)},
                         {"status", ValidationStatusToken(item.status)},
                         {"diagnostics", std::move(diagnostics)}};
    if (item.schema)
      layer["schema"] = {{"format", item.schema->format},
                         {"format_version", item.schema->formatVersion},
                         {"schema_version", item.schema->schemaVersion},
                         {"provenance", item.schema->provenance},
                         {"source_revision", item.schema->sourceRevision}};
    serialized.push_back(std::move(layer));
  }
  return serialized;
}

// Serializes one MVR scene descriptor.
nlohmann::json SerializeNode(const MvrSceneNodeDescriptor &node) {
  return {{"kind", node.kind},
          {"uuid", node.uuid},
          {"name", node.name},
          {"layer_uuid", node.layerUuid},
          {"layer_name", node.layerName},
          {"parent_group_uuid", node.parentGroupUuid},
          {"resource_reference", node.resourceReference},
          {"child_uuids", node.childUuids}};
}

// Serializes an ordered collection of MVR scene descriptors.
nlohmann::json
SerializeNodes(const std::vector<MvrSceneNodeDescriptor> &nodes) {
  nlohmann::json serialized = nlohmann::json::array();
  for (const auto &node : nodes)
    serialized.push_back(SerializeNode(node));
  return serialized;
}

} // namespace

// Serializes one complete GDTF report from structured inspection data.
std::string
SerializeGdtfReportToJson(const GdtfInspectionResult &result,
                          const std::vector<ResourceDescriptor> &resources) {
  nlohmann::json root = detail::SerializeBase(result.inspection);
  root["format"] = "GDTF";
  const char *status = result.status == GdtfReadStatus::Canonical ? "canonical"
                       : result.status == GdtfReadStatus::CompatibilityAccepted
                           ? "compatibility_accepted"
                           : "unusable";
  root["status"] = status;
  root["package"] = SerializePackage(result.packageInventory);
  root["resources"] = SerializeResources(resources);
  root["validation"] = SerializeValidation(result.validation);
  root["document"] = nullptr;
  if (result.document) {
    const auto &description = result.document->Description();
    const auto &archive = result.document->Archive();
    nlohmann::json revisions = nlohmann::json::array();
    for (const auto &revision : description.revisions)
      revisions.push_back({{"text", revision.text},
                           {"date", revision.date},
                           {"user_id", revision.userId},
                           {"modified_by", revision.modifiedBy}});
    nlohmann::json wheels = nlohmann::json::array();
    for (const auto &wheel : description.wheels) {
      nlohmann::json slots = nlohmann::json::array();
      for (const auto &slot : wheel.slots)
        slots.push_back({{"name", slot.name},
                         {"media_file_name", slot.mediaFileName},
                         {"resource_references", slot.resourceReferences}});
      wheels.push_back({{"name", wheel.name}, {"slots", std::move(slots)}});
    }
    nlohmann::json repeatedFamilies = nlohmann::json::array();
    for (const auto &family : result.document->RepeatedFamilies())
      repeatedFamilies.push_back(
          {{"family_kind", family.familyKind}, {"names", family.names}});
    root["document"] = {
        {"data_version", description.dataVersion},
        {"fixture_type_name", description.fixtureTypeName},
        {"manufacturer", description.manufacturer},
        {"short_name", description.shortName},
        {"long_name", description.longName},
        {"description", description.description},
        {"fixture_type_id", description.fixtureTypeId},
        {"thumbnail", description.thumbnail},
        {"create_date", description.createDate},
        {"revision", description.revision},
        {"weight_kg", description.weightKgPresent
                          ? nlohmann::json(description.weightKg)
                          : nlohmann::json(nullptr)},
        {"power_consumption_w",
         description.powerConsumptionWPresent
             ? nlohmann::json(description.powerConsumptionW)
             : nlohmann::json(nullptr)},
        {"model_color_hex", description.modelColorHex},
        {"truss_cross_section_type", description.trussCrossSectionType},
        {"truss_cross_section", description.trussCrossSection},
        {"dmx_modes", description.dmxModeNames},
        {"valid", result.document->Valid()},
        {"modes", result.document->Modes()},
        {"repeated_families", std::move(repeatedFamilies)},
        {"archive",
         {{"description_entry_path", archive.descriptionEntryPath},
          {"used_compatibility_description_fallback",
           archive.usedCompatibilityDescriptionFallback},
          {"standards_compliant_description_location",
           archive.standardsCompliantDescriptionLocation},
          {"utf8_flag_missing_entry_count",
           archive.utf8FlagMissingEntryCount}}},
        {"revisions", std::move(revisions)},
        {"wheels", std::move(wheels)},
        {"root_xml", result.document->Archive().descriptionXml}};
  }
  return root.dump();
}

// Serializes one complete MVR report from structured inspection data.
std::string
SerializeMvrReportToJson(const MvrInspectionResult &result,
                         const std::vector<ResourceDescriptor> &resources) {
  nlohmann::json root = detail::SerializeBase(result.inspection);
  root["format"] = "MVR";
  root["status"] = result.Success() ? "inspected" : "unusable";
  root["package"] = SerializePackage(result.packageInventory);
  root["resources"] = SerializeResources(resources);
  root["validation"] = SerializeValidation(result.validation);
  root["snapshot"] = nullptr;
  if (result.snapshot) {
    const auto &snapshot = *result.snapshot;
    nlohmann::json references = nlohmann::json::array();
    for (const auto &item : snapshot.referencedResources)
      references.push_back(
          {{"kind", item.kind}, {"archive_path", item.archivePath}});
    nlohmann::json symdefs = nlohmann::json::array();
    for (const auto &item : snapshot.symdefs)
      symdefs.push_back({{"uuid", item.uuid},
                         {"resource_references", item.resourceReferences}});
    nlohmann::json foreign = nlohmann::json::array();
    for (const auto &item : snapshot.foreignUserData)
      foreign.push_back({{"provider", item.provider},
                         {"version", item.version},
                         {"xml", item.xml}});
    nlohmann::json counts = nlohmann::json::array();
    for (const auto &item : snapshot.nodeCounts)
      counts.push_back({{"type", item.type}, {"count", item.count}});
    root["snapshot"] = {
        {"version_major", snapshot.versionMajor},
        {"version_minor", snapshot.versionMinor},
        {"provider", snapshot.provider},
        {"provider_version", snapshot.providerVersion},
        {"scene_description_entry", snapshot.sceneDescriptionEntry},
        {"root_xml", snapshot.sceneDescriptionXml},
        {"embedded_gdtfs", snapshot.embeddedGdtfEntries},
        {"referenced_resources", std::move(references)},
        {"layers", SerializeNodes(snapshot.layers)},
        {"fixtures", SerializeNodes(snapshot.fixtures)},
        {"trusses", SerializeNodes(snapshot.trusses)},
        {"supports", SerializeNodes(snapshot.supports)},
        {"scene_objects", SerializeNodes(snapshot.sceneObjects)},
        {"group_objects", SerializeNodes(snapshot.groupObjects)},
        {"positions", SerializeNodes(snapshot.positions)},
        {"symdefs", std::move(symdefs)},
        {"foreign_user_data", std::move(foreign)},
        {"node_counts", std::move(counts)}};
  }
  return root.dump();
}

} // namespace perastage::inspection::serialization
