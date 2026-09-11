#include "mvr_scene_node_reader.h"
#include "mvr_scene_node_reader_detail.h"

#include "filesystem_path_utils.h"
#include "fixture_visual_color.h"
#include "gdtf_import_matching.h"
#include "geometry_bounds_resolver.h"
#include "groupobject.h"
#include "layer_service.h"
#include "matrixutils.h"
#include "scene_grouping.h"
#include "sceneobject.h"
#include "support.h"
#include "truss_dimension_resolution.h"
#include "utf8_utils.h"
#include "uuidutils.h"

#include <tinyxml2.h>

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <iomanip>
#include <sstream>
#include <system_error>

namespace fs = std::filesystem;

namespace mvr {

using scene_reader_detail::ToLowerCopy;
using scene_reader_detail::Trim;
using scene_reader_detail::TryParseFloat;

namespace {

// Reports whether extension metadata names a portable root archive file.
bool IsPortableRootArchiveFileName(const std::string &value) {
  if (value.empty() || value == "." || value == "..")
    return false;
  if (std::any_of(value.begin(), value.end(),
                  [](unsigned char ch) { return ch < 32 || ch == 127; }))
    return false;
  if (value.find('/') != std::string::npos ||
      value.find('\\') != std::string::npos ||
      value.find(':') != std::string::npos)
    return false;
  const fs::path path = PathUtils::PathFromUtf8(value);
  return !path.is_absolute() && !path.has_root_name() &&
         path.filename().generic_string() == value;
}

// Converts one MVR CIE color value to an RGB hexadecimal string.
std::string CieToHex(const std::string &cie) {
  std::string value = cie;
  std::replace(value.begin(), value.end(), ',', ' ');
  std::stringstream stream(value);
  double x = 0.0, y = 0.0, luminance = 0.0;
  if (!(stream >> x >> y >> luminance) || y <= 0.0)
    return {};
  double red = 3.2406 * x * (luminance / y) - 1.5372 * luminance -
               0.4986 * (1.0 - x - y) * (luminance / y);
  double green = -0.9689 * x * (luminance / y) + 1.8758 * luminance +
                 0.0415 * (1.0 - x - y) * (luminance / y);
  double blue = 0.0557 * x * (luminance / y) - 0.2040 * luminance +
                1.0570 * (1.0 - x - y) * (luminance / y);
  auto gamma = [](double channel) {
    channel = std::max(0.0, channel);
    return channel <= 0.0031308 ? 12.92 * channel
                                : 1.055 * std::pow(channel, 1.0 / 2.4) - 0.055;
  };
  const int redByte =
      static_cast<int>(std::round(std::clamp(gamma(red), 0.0, 1.0) * 255.0));
  const int greenByte =
      static_cast<int>(std::round(std::clamp(gamma(green), 0.0, 1.0) * 255.0));
  const int blueByte =
      static_cast<int>(std::round(std::clamp(gamma(blue), 0.0, 1.0) * 255.0));
  std::ostringstream result;
  result << '#' << std::uppercase << std::hex << std::setfill('0')
         << std::setw(2) << redByte << std::setw(2) << greenByte << std::setw(2)
         << blueByte;
  return result.str();
}

// Converts a UTF-8 filesystem string without changing its byte sequence.
std::string ToString(const std::u8string &value) {
  return std::string(value.begin(), value.end());
}

// Builds a stable key for one SceneObject geometry instance.
std::string BuildSceneObjectGeometryInstanceKey(
    const std::string &parentUuid, const std::string &childKind,
    size_t childIndex, const std::string &symdefUuid = {}) {
  std::ostringstream key;
  key << parentUuid << '/' << childKind << '/' << childIndex;
  if (!symdefUuid.empty())
    key << '/' << symdefUuid;
  return key.str();
}

} // namespace

// Reads supported logical scene nodes while preserving hierarchy and
// transforms.
void ReadMvrSceneNodes(tinyxml2::XMLElement *sceneNode, MvrScene &scene,
                       MvrImportResult &importResult,
                       const MvrImportOptions &options,
                       const MvrSceneReadServices &services,
                       const MvrSceneReadMetadata &metadata,
                       MvrSceneReadState &state, MvrSceneReadMetrics &metrics) {

  auto &fixtureUuidRemap = state.fixtures.uuidRemap;
  const auto &textOf = services.textOf;
  const auto &intOf = services.intOf;
  const auto &fixtureIdOf = services.fixtureIdOf;
  auto parseMatrixOrIdentity =
      [&](tinyxml2::XMLElement *parent, const char *elementName,
          const std::string &context, Matrix &out, bool inspectScale = false) {
        services.parseMatrixOrIdentity(parent, elementName, context, out,
                                       inspectScale);
      };
  const auto &remapArchivePathIfNeeded = services.remapArchivePath;
  const auto &buildFixtureTypeInfoKey = services.buildFixtureTypeInfoKey;
  auto resolveStableUuid = [&](const char *kind, tinyxml2::XMLElement *node,
                               const std::string &layer,
                               const Matrix &transform,
                               const std::string &legacyId = std::string{}) {
    return services.resolveStableUuid(kind, node, layer, transform, legacyId);
  };
  const auto &referenceUuidForNode = services.referenceUuid;
  const auto &ensurePositionEntry = services.ensurePosition;
  const auto &normalizeGdtfSpecForScene = services.normalizeGdtfSpec;
  const auto &normalizeSupportGdtfSpec = services.normalizeSupportGdtfSpec;
  const auto &resolveGdtfPathCached = services.resolveGdtfPath;
  const auto &getFixtureMetadata = services.fixtureMetadata;
  const auto &resolveExistingGdtfModeCached = services.resolveGdtfMode;
  const auto &getGdtfModeChannelCountCached = services.gdtfModeChannelCount;
  const auto &getDictionaryEntryCached = services.dictionaryEntry;
  const auto &loadTrussDefinitionCached = services.loadTrussDefinition;
  const auto &resolveSymdefReference = services.resolveSymdef;
  const auto &normalizeAndResolveGeometryFileName =
      services.normalizeGeometryFile;
  auto appendGeometryInstance = [&](std::vector<GeometryInstance> &instances,
                                    const std::string &fileName,
                                    const Matrix &localTransform,
                                    const std::string &instanceKey,
                                    const std::string &sourceSymbolUuid = {},
                                    const std::string &sourceSymdefUuid = {}) {
    services.appendGeometry(instances, fileName, localTransform, instanceKey,
                            sourceSymbolUuid, sourceSymdefUuid);
  };
  const auto &reportProgress = services.reportProgress;
  const auto &ReadLegacyFixtureIdentityFromUserData =
      scene_reader_detail::ReadLegacyFixtureIdentity;
  const auto &ReadFixtureCategoryFromUserData =
      scene_reader_detail::ReadFixtureCategory;
  const auto &ParseTrussRepresentation =
      scene_reader_detail::ParseTrussRepresentation;
  const auto &IsRenderableTrussGeometry =
      scene_reader_detail::IsRenderableTrussGeometry;
  const auto &DescribeTrussForLog = scene_reader_detail::DescribeTruss;
  const auto &ReadSupportHoistInfoElement =
      scene_reader_detail::ReadSupportHoistInfo;
  const auto &ReadSupportHoistInfoFromUserData =
      scene_reader_detail::ReadSupportHoistUserData;
  const auto &ApplySupportHoistInfoDefaults =
      scene_reader_detail::ApplySupportDefaults;
  auto &pendingGdtfConflictByType = state.fixtures.pendingGdtfConflicts;
  auto &categoryByTypeKey = state.fixtures.categoriesByType;
  auto &categoryInferenceByResolvedPath = state.fixtures.categoryInferences;
  const auto &rootFixtureTypeInfoByKey = metadata.fixtures.types;
  const auto &projectFixtureIdentifiersByUuid = metadata.fixtures.identifiers;
  const auto &projectFixtureColorsByUuid = metadata.fixtures.colors;
  const auto &projectFixtureColorMetadataUuids =
      metadata.fixtures.colorMetadataUuids;
  auto &consumedProjectFixtureColorUuids = state.fixtures.consumedColors;
  const auto &rootTrussInfoByUuid = metadata.rigging.trussInfo;
  auto &consumedRootTrussInfoUuids = state.rigging.consumedTrussInfo;
  const auto &rootHoistInfoByUuid = metadata.rigging.hoistInfo;
  auto &consumedRootHoistInfoUuids = state.rigging.consumedHoistInfo;
  const auto &perastageTypeToGdtfPath = metadata.rigging.trussGdtfByType;
  const auto &perastageInstanceToTypeKey = metadata.rigging.trussTypeByInstance;
  const auto &rootPrimitiveModelRefsBySceneObjectAndFile =
      metadata.geometry.primitiveModels;
  const auto &legacyPositionIdToCanonical = metadata.geometry.legacyPositions;
  const auto &layerColorByUuid = metadata.layers.colorsByUuid;
  const auto &layerColorByName = metadata.layers.colorsByName;
  auto isHexRgb = [](const std::string &color) {
    return color.size() == 7 && color[0] == '#' &&
           std::all_of(color.begin() + 1, color.end(),
                       [](unsigned char ch) { return std::isxdigit(ch) != 0; });
  };
  auto &trussSymbolSymdefPreservedCount =
      metrics.trussSymbolSymdefPreservedCount;
  auto &trussSymbolSymdefPreservedBySymdef =
      metrics.trussSymbolSymdefPreservedBySymdef;
  int &preservedGroupObjectCount = metrics.preservedGroupObjectCount;
  std::function<void(tinyxml2::XMLElement *, const std::string &,
                     const Matrix &, const std::string &)>
      parseChildList;

  // Parses a Fixture XML node into scene data while preserving its original
  // matching identity.
  std::function<void(tinyxml2::XMLElement *, const std::string &,
                     const Matrix &, const Matrix &, const std::string &)>
      parseFixture = [&](tinyxml2::XMLElement *node,
                         const std::string &layerName,
                         const Matrix &nodeTransform,
                         const Matrix &localTransform,
                         const std::string &parentGroupUuid) {
        Fixture fixture;
        const SceneReadLegacyFixtureIdentity legacyIdentity =
            ReadLegacyFixtureIdentityFromUserData(node);
        const char *rawUuidAttr = node->Attribute("uuid");
        const std::string rawFixtureUuid =
            rawUuidAttr ? Trim(rawUuidAttr) : std::string{};
        fixture.uuid = resolveStableUuid(
            "Fixture", node, layerName, nodeTransform, legacyIdentity.stableId);
        if (!rawFixtureUuid.empty() && rawFixtureUuid != fixture.uuid)
          fixtureUuidRemap[rawFixtureUuid] = fixture.uuid;
        fixture.layer = layerName;
        fixture.transform = nodeTransform;
        fixture.localTransform = localTransform;
        fixture.hasLocalTransform = true;
        fixture.parentGroupUuid = parentGroupUuid;

        const char *nameAttr = node->Attribute("name");
        const std::string rawFixtureNodeName =
            nameAttr ? Trim(nameAttr) : std::string{};
        if (!rawFixtureNodeName.empty())
          fixture.instanceName = rawFixtureNodeName;
        else if (!legacyIdentity.instanceName.empty())
          fixture.instanceName = legacyIdentity.instanceName;

        fixtureIdOf(node, fixture.fixtureIdText, fixture.fixtureIdNumeric);
        fixture.fixtureId = fixture.fixtureIdNumeric;
        intOf(node, "UnitNumber", fixture.unitNumber);
        const auto projectIdentifiersIt =
            projectFixtureIdentifiersByUuid.find(fixture.uuid);
        if (projectIdentifiersIt != projectFixtureIdentifiersByUuid.end()) {
          fixture.fixtureId = projectIdentifiersIt->second.fixtureId;
          fixture.fixtureIdNumeric =
              projectIdentifiersIt->second.fixtureIdNumeric;
          fixture.fixtureIdText = projectIdentifiersIt->second.fixtureIdText;
          fixture.unitNumber = projectIdentifiersIt->second.unitNumber;
        }
        intOf(node, "CustomId", fixture.customId);
        intOf(node, "CustomIdType", fixture.customIdType);

        fixture.gdtfSpec = textOf(node, "GDTFSpec");
        const std::string rawGdtfSpec = fixture.gdtfSpec;
        fixture.originalMvrGdtfSpec = rawGdtfSpec;
        fixture.gdtfMode = textOf(node, "GDTFMode");
        fixture.requestedFixtureName =
            mvr::gdtf_import_matching::SelectRequestedFixtureName(
                rawFixtureNodeName, rawGdtfSpec);
        fixture.focus = textOf(node, "Focus");
        fixture.function = textOf(node, "Function");
        fixture.position = CanonicalizeUuid(textOf(node, "Position"));
        if (fixture.position.empty())
          fixture.position = textOf(node, "Position");
        fixture.positionName = ensurePositionEntry(fixture.position);
        auto fixturePosIt = legacyPositionIdToCanonical.find(fixture.position);
        if (fixturePosIt != legacyPositionIdToCanonical.end())
          fixture.position = fixturePosIt->second;
        if (tinyxml2::XMLElement *colorNode =
                node->FirstChildElement("Color")) {
          if (const char *txt = colorNode->GetText())
            fixture.mvrFixtureColorHex = CieToHex(txt);
        }
        float legacyPowerConsumptionW = 0.0f;
        bool hasLegacyPowerConsumption = false;
        if (tinyxml2::XMLElement *pcNode =
                node->FirstChildElement("PowerConsumption")) {
          if (const char *txt = pcNode->GetText()) {
            float parsed = 0.0f;
            if (TryParseFloat(txt, parsed)) {
              legacyPowerConsumptionW = parsed;
              hasLegacyPowerConsumption = true;
            }
          }
        }
        float legacyWeightKg = 0.0f;
        bool hasLegacyWeight = false;
        if (tinyxml2::XMLElement *wNode = node->FirstChildElement("Weight")) {
          if (const char *txt = wNode->GetText()) {
            float parsed = 0.0f;
            if (TryParseFloat(txt, parsed)) {
              legacyWeightKg = parsed;
              hasLegacyWeight = true;
            }
          }
        }
        std::string resolvedGdtfPathForFixture;
        std::string resolvedGdtfManufacturer;
        std::string resolvedFixtureTypeId;
        if (!fixture.gdtfSpec.empty()) {
          fixture.gdtfSpec = remapArchivePathIfNeeded(fixture.gdtfSpec);
          const std::string &resolvedGdtfPath =
              resolveGdtfPathCached(fixture.gdtfSpec);
          resolvedGdtfPathForFixture = resolvedGdtfPath;
          fixture.gdtfSpec = normalizeGdtfSpecForScene(fixture.gdtfSpec);
          if (fixture.gdtfSpec.empty() && !rawGdtfSpec.empty())
            fixture.gdtfSpec = remapArchivePathIfNeeded(rawGdtfSpec);
          const auto &metadata = getFixtureMetadata(resolvedGdtfPath);
          resolvedGdtfManufacturer = metadata.manufacturer;
          resolvedFixtureTypeId = metadata.fixtureTypeId;
          fixture.typeName = metadata.fixtureName;
          if (fixture.typeName.empty()) {
            fixture.typeName =
                mvr::gdtf_import_matching::SelectFallbackFixtureTypeName(
                    rawFixtureNodeName, rawGdtfSpec);
          }
          if (metadata.hasProperties) {
            if (metadata.weightKg > 0.0f)
              fixture.weightKg = metadata.weightKg;
            if (metadata.powerW > 0.0f)
              fixture.powerConsumptionW = metadata.powerW;
            fixture.physicalPropertiesSource =
                FixturePhysicalPropertiesSource::Gdtf;
            fixture.physicalPropertiesDirty = false;
          }
        }
        if (fixture.weightKg <= 0.0f && hasLegacyWeight) {
          fixture.weightKg = legacyWeightKg;
          fixture.physicalPropertiesSource =
              FixturePhysicalPropertiesSource::LegacyMvrFixtureNode;
        }
        if (fixture.powerConsumptionW <= 0.0f && hasLegacyPowerConsumption) {
          fixture.powerConsumptionW = legacyPowerConsumptionW;
          fixture.physicalPropertiesSource =
              FixturePhysicalPropertiesSource::LegacyMvrFixtureNode;
        }
        fixture.physicalPropertiesDirty = false;

        ReadFixtureCategoryFromUserData(node, fixture);
        const std::string fixtureTypeInfoKey = buildFixtureTypeInfoKey(
            rawGdtfSpec, fixture.gdtfMode, fixture.typeName);
        auto rootTypeInfoIt = rootFixtureTypeInfoByKey.find(fixtureTypeInfoKey);
        if (rootTypeInfoIt != rootFixtureTypeInfoByKey.end()) {
          if (!rootTypeInfoIt->second.category.empty()) {
            fixture.category = rootTypeInfoIt->second.category;
            fixture.categorySource = rootTypeInfoIt->second.categorySource;
            fixture.categorySourceReason.clear();
          }
          fixture.automaticVisualColorHex =
              rootTypeInfoIt->second.visualColorHex;
        }
        const auto projectColorIt =
            projectFixtureColorsByUuid.find(fixture.uuid);
        if (projectColorIt != projectFixtureColorsByUuid.end()) {
          const auto normalizedProjectColor =
              NormalizeFixtureVisualColor(projectColorIt->second);
          fixture.visualColorHex =
              normalizedProjectColor.value_or(std::string{});
          fixture.visualColorState =
              fixture.visualColorHex.empty()
                  ? FixtureProjectColorState::ExplicitEmpty
                  : FixtureProjectColorState::Present;
          consumedProjectFixtureColorUuids.insert(projectColorIt->first);
        } else if (options.sourceKind == MvrImportSourceKind::ProjectRestore &&
                   !projectFixtureColorMetadataUuids.contains(fixture.uuid)) {
          const FixtureVisualColorResult recovered =
              ResolveFixtureVisualColor({{},
                                         fixture.mvrFixtureColorHex,
                                         fixture.automaticVisualColorHex,
                                         FixtureProjectColorState::Missing,
                                         true});
          if (recovered.source == FixtureVisualColorSource::LegacyMvrRecovery &&
              recovered.colorHex) {
            fixture.visualColorHex = *recovered.colorHex;
            fixture.visualColorState = FixtureProjectColorState::Present;
          }
        }
        const std::optional<GdtfDictionary::Entry> &dictionaryEntry =
            getDictionaryEntryCached(fixture.typeName);
        auto posIt = scene.positions.find(fixture.position);
        if (posIt != scene.positions.end())
          fixture.positionName = posIt->second;

        auto boolOf = [&](const char *name, bool &out) {
          tinyxml2::XMLElement *n = node->FirstChildElement(name);
          if (n && n->GetText()) {
            std::string v = n->GetText();
            out = (v == "true" || v == "1");
          }
        };

        boolOf("DMXInvertPan", fixture.dmxInvertPan);
        boolOf("DMXInvertTilt", fixture.dmxInvertTilt);

        if (tinyxml2::XMLElement *addresses =
                node->FirstChildElement("Addresses")) {
          tinyxml2::XMLElement *addr = addresses->FirstChildElement("Address");
          if (addr) {
            const char *breakAttr = addr->Attribute("break");
            int breakNum = breakAttr ? std::atoi(breakAttr) : 0;
            const char *txt = addr->GetText();
            if (txt) {
              std::string t = txt;
              std::string normalized;
              if (t.find('.') == std::string::npos) {
                int value = std::atoi(t.c_str());
                int universe = breakNum + 1;
                if (value > 512) {
                  universe += (value - 1) / 512;
                  value = (value - 1) % 512 + 1;
                }
                normalized =
                    std::to_string(universe) + "." + std::to_string(value);
              } else {
                normalized = t;
              }
              fixture.address = normalized;
            }
          }
        }

        if (tinyxml2::XMLElement *matrix = node->FirstChildElement("Matrix")) {
          if (const char *txt = matrix->GetText())
            fixture.matrixRaw = txt;
        }

        if (options.applyDictionary && dictionaryEntry &&
            !fixture.typeName.empty()) {
          int footprint = 0;
          if (!resolvedGdtfPathForFixture.empty() &&
              !fixture.gdtfMode.empty()) {
            footprint = getGdtfModeChannelCountCached(
                resolvedGdtfPathForFixture, fixture.gdtfMode);
          }
          pendingGdtfConflictByType.try_emplace(
              fixture.typeName,
              SceneReadGdtfConflict{
                  fixture.typeName, fixture.requestedFixtureName,
                  fixture.gdtfSpec, dictionaryEntry->path,
                  resolvedGdtfManufacturer, fixture.typeName,
                  resolvedFixtureTypeId, fixture.gdtfMode, footprint, true});
        }

        scene.fixtures[fixture.uuid] = fixture;
      };

  // Applies Perastage TrussInfo metadata as the effective edited truss state.
  auto applyTrussInfo = [&](tinyxml2::XMLElement *info, Truss &truss,
                            bool hasGdtfMetadataAuthority) {
    if (!info)
      return;
    auto readNumeric = [&](const char *field, float &target) {
      tinyxml2::XMLElement *element = info->FirstChildElement(field);
      if (!element)
        return;
      float parsed = 0.0f;
      if (element->GetText() && TryParseFloat(element->GetText(), parsed)) {
        target = parsed;
      } else {
        importResult.diagnostics.push_back(
            {"invalid_truss_numeric_field",
             "Truss '" + truss.uuid + "' has invalid " + field + " metadata."});
      }
    };
    if (tinyxml2::XMLElement *m = info->FirstChildElement("Manufacturer"))
      if (m->GetText())
        truss.manufacturer = Trim(m->GetText());
    if (tinyxml2::XMLElement *mo = info->FirstChildElement("Model"))
      if (mo->GetText())
        truss.model = Trim(mo->GetText());
    if (!hasGdtfMetadataAuthority) {
      readNumeric("Length", truss.lengthMm);
      readNumeric("Width", truss.widthMm);
      readNumeric("Height", truss.heightMm);
      truss.dimensionSource = Truss::DimensionSource::PerastageMetadata;
    }
    readNumeric("Weight", truss.weightKg);
    if (tinyxml2::XMLElement *desc = info->FirstChildElement("GdtfDescription"))
      if (desc->GetText())
        truss.gdtfDescription = desc->GetText();
    if (tinyxml2::XMLElement *cst = info->FirstChildElement("CrossSectionType"))
      if (cst->GetText()) {
        const std::string value = Trim(cst->GetText());
        truss.crossSectionType = (value == "Tube") ? "Tube" : "TrussFramework";
      }
    if (tinyxml2::XMLElement *cs = info->FirstChildElement("CrossSection"))
      if (cs->GetText())
        truss.crossSection = Trim(cs->GetText());
    if (tinyxml2::XMLElement *load = info->FirstChildElement("Load"))
      if (load->GetText()) {
        float parsed = 0.0f;
        if (TryParseFloat(Trim(load->GetText()), parsed)) {
          truss.manualLoadKg = parsed;
          truss.hasManualLoadOverride = true;
        } else {
          importResult.diagnostics.push_back(
              {"invalid_truss_numeric_field",
               "Truss '" + truss.uuid + "' has invalid Load metadata."});
        }
      }
    if (tinyxml2::XMLElement *mf = info->FirstChildElement("ModelFile"))
      if (mf->GetText())
        truss.modelFile = mf->GetText();
    if (tinyxml2::XMLElement *hp = info->FirstChildElement("PositionName"))
      if (hp->GetText())
        truss.positionName = Trim(hp->GetText());
    if (truss.positionName.empty())
      if (tinyxml2::XMLElement *hp = info->FirstChildElement("HangPos"))
        if (hp->GetText())
          truss.positionName = Trim(hp->GetText());
    if (tinyxml2::XMLElement *rep = info->FirstChildElement("Representation"))
      if (rep->GetText())
        truss.sourceRepresentation = ParseTrussRepresentation(rep->GetText());
    if (tinyxml2::XMLElement *tk = info->FirstChildElement("TypeKey"))
      if (tk->GetText())
        truss.perastageTypeKey = Trim(tk->GetText());
    if (tinyxml2::XMLElement *ag = info->FirstChildElement("AuxGdtf"))
      if (ag->GetText()) {
        const std::string archiveName = Trim(ag->GetText());
        if (!IsPortableRootArchiveFileName(archiveName)) {
          importResult.diagnostics.push_back(
              {"unsafe_truss_aux_gdtf_path",
               "Ignored unsafe TrussInfo AuxGdtf path '" + archiveName + "'."});
        } else {
          const std::string remapped = remapArchivePathIfNeeded(archiveName);
          const fs::path resolved =
              std::filesystem::path(services.resolveScenePath(remapped));
          std::error_code existsEc;
          if (fs::is_regular_file(resolved, existsEc) && !existsEc) {
            truss.perastageAuxGdtfArchivePath = remapped;
          } else {
            importResult.diagnostics.push_back(
                {"missing_truss_aux_gdtf",
                 "Ignored missing TrussInfo AuxGdtf archive entry '" +
                     archiveName + "'."});
          }
        }
      }
  };

  std::function<void(tinyxml2::XMLElement *, const std::string &,
                     const Matrix &, const Matrix &, const std::string &)>
      parseTruss = [&](tinyxml2::XMLElement *node, const std::string &layerName,
                       const Matrix &nodeTransform,
                       const Matrix &localTransform,
                       const std::string &parentGroupUuid) {
        Truss truss;
        truss.uuid = resolveStableUuid("Truss", node, layerName, nodeTransform);
        truss.layer = layerName;
        truss.transform = nodeTransform;
        truss.localTransform = localTransform;
        truss.hasLocalTransform = true;
        truss.parentGroupUuid = parentGroupUuid;
        truss.sourceSymbolMatrix = MatrixUtils::Identity();
        truss.sourceGeometryMatrix = MatrixUtils::Identity();
        if (const char *nameAttr = node->Attribute("name"))
          truss.name = nameAttr;

        intOf(node, "UnitNumber", truss.unitNumber);
        intOf(node, "CustomId", truss.customId);
        intOf(node, "CustomIdType", truss.customIdType);

        truss.gdtfSpec = textOf(node, "GDTFSpec");
        truss.gdtfMode = textOf(node, "GDTFMode");
        truss.function = textOf(node, "Function");
        truss.position = CanonicalizeUuid(textOf(node, "Position"));
        if (truss.position.empty())
          truss.position = textOf(node, "Position");
        truss.positionName = ensurePositionEntry(truss.position);
        auto trussPosIt = legacyPositionIdToCanonical.find(truss.position);
        if (trussPosIt != legacyPositionIdToCanonical.end())
          truss.position = trussPosIt->second;

        bool gdtfLoadFailed = false;
        if (!truss.gdtfSpec.empty()) {
          truss.sourceRepresentation =
              Truss::GeometryRepresentation::PublicGdtf;
          truss.gdtfSpec = remapArchivePathIfNeeded(truss.gdtfSpec);
          const std::string trussGdtfPath =
              resolveGdtfPathCached(truss.gdtfSpec);
          truss.gdtfSpec = normalizeGdtfSpecForScene(truss.gdtfSpec);
          Truss gdtfTruss;
          if (loadTrussDefinitionCached(trussGdtfPath, gdtfTruss)) {
            truss.modelFile = gdtfTruss.modelFile;
            if (!gdtfTruss.symbolFile.empty())
              truss.symbolFile = gdtfTruss.symbolFile;
            if (!gdtfTruss.manufacturer.empty())
              truss.manufacturer = gdtfTruss.manufacturer;
            if (!gdtfTruss.model.empty())
              truss.model = gdtfTruss.model;
            if (!gdtfTruss.name.empty() && truss.name.empty())
              truss.name = gdtfTruss.name;
            if (gdtfTruss.lengthMm > 0.0f)
              truss.lengthMm = gdtfTruss.lengthMm;
            if (gdtfTruss.widthMm > 0.0f)
              truss.widthMm = gdtfTruss.widthMm;
            if (gdtfTruss.heightMm > 0.0f)
              truss.heightMm = gdtfTruss.heightMm;
            truss.localGeometryBounds = gdtfTruss.localGeometryBounds;
            truss.dimensionSource = gdtfTruss.dimensionSource;
            if (gdtfTruss.weightKg > 0.0f)
              truss.weightKg = gdtfTruss.weightKg;
            if (truss.gdtfMode.empty())
              truss.gdtfMode =
                  gdtfTruss.gdtfMode.empty() ? "Default" : gdtfTruss.gdtfMode;
          } else {
            gdtfLoadFailed = true;
          }
        }

        if (tinyxml2::XMLElement *geos =
                node->FirstChildElement("Geometries")) {
          if (tinyxml2::XMLElement *g3d =
                  geos->FirstChildElement("Geometry3D")) {
            truss.sourceRepresentation =
                Truss::GeometryRepresentation::Geometry3D;
            const char *file = g3d->Attribute("fileName");
            if (file)
              truss.symbolFile = normalizeAndResolveGeometryFileName(file);
            Matrix geoMatrix = MatrixUtils::Identity();
            parseMatrixOrIdentity(g3d, "Matrix", "Truss/Geometry3D", geoMatrix,
                                  true);
            truss.sourceGeometryMatrix = geoMatrix;
            if (const char *type = g3d->Attribute("geometryType"))
              truss.sourceGeometryType = Trim(type);
            truss.transform = MatrixUtils::Multiply(nodeTransform, geoMatrix);
          } else if (tinyxml2::XMLElement *sym =
                         geos->FirstChildElement("Symbol")) {
            truss.sourceRepresentation =
                Truss::GeometryRepresentation::SymbolSymdef;
            std::vector<SymdefGeometry> symGeometries;
            std::string symType;
            Matrix symMatrix = MatrixUtils::Identity();
            resolveSymdefReference(sym, symGeometries, symType, symMatrix);
            if (const char *symbolUuid = sym->Attribute("uuid"))
              truss.sourceSymbolUuid = CanonicalizeUuid(Trim(symbolUuid));
            if (const char *symdef = sym->Attribute("symdef"))
              truss.sourceSymdefUuid = Trim(symdef);
            truss.sourceSymbolMatrix = symMatrix;
            truss.sourceGeometryType = symType;
            Matrix symLocal = symMatrix;
            if (!symGeometries.empty()) {
              truss.symbolFile = normalizeAndResolveGeometryFileName(
                  symGeometries.front().file);
              symLocal = MatrixUtils::Multiply(symMatrix,
                                               symGeometries.front().transform);
            }
            truss.transform = MatrixUtils::Multiply(nodeTransform, symLocal);
            ++trussSymbolSymdefPreservedCount;
            const std::string symdefKey = truss.sourceSymdefUuid.empty()
                                              ? std::string{"(empty)"}
                                              : truss.sourceSymdefUuid;
            ++trussSymbolSymdefPreservedBySymdef[symdefKey];
          }
        }

        const bool hasGdtfMetadataAuthority =
            !truss.gdtfSpec.empty() && !gdtfLoadFailed;

        auto rootTrussInfoIt = rootTrussInfoByUuid.find(truss.uuid);
        if (rootTrussInfoIt != rootTrussInfoByUuid.end()) {
          consumedRootTrussInfoUuids.insert(rootTrussInfoIt->first);
          applyTrussInfo(rootTrussInfoIt->second, truss,
                         hasGdtfMetadataAuthority);
        } else if (tinyxml2::XMLElement *ud =
                       node->FirstChildElement("UserData")) {
          for (tinyxml2::XMLElement *data = ud->FirstChildElement("Data"); data;
               data = data->NextSiblingElement("Data")) {
            const std::string provider = ToLowerCopy(
                Trim(data->Attribute("provider") ? data->Attribute("provider")
                                                 : ""));
            if (provider != "perastage")
              continue;
            const std::string version =
                Trim(data->Attribute("ver") ? data->Attribute("ver") : "");
            if (version.empty()) {
              importResult.diagnostics.push_back(
                  {"legacy_perastage_metadata_missing_version",
                   "Accepted legacy Truss metadata without a schema version."});
            } else if (version != "1.0") {
              importResult.diagnostics.push_back(
                  {"unsupported_perastage_metadata_version",
                   "Ignored legacy Truss metadata with unsupported schema "
                   "version '" +
                       version + "'."});
              continue;
            }
            if (tinyxml2::XMLElement *info =
                    data->FirstChildElement("TrussInfo")) {
              applyTrussInfo(info, truss, hasGdtfMetadataAuthority);
              break;
            }
          }
        }

        auto manifestIt = perastageInstanceToTypeKey.find(truss.uuid);
        if (manifestIt != perastageInstanceToTypeKey.end()) {
          truss.perastageTypeKey = manifestIt->second;
          auto typeIt = perastageTypeToGdtfPath.find(truss.perastageTypeKey);
          if (typeIt != perastageTypeToGdtfPath.end()) {
            truss.perastageAuxGdtfArchivePath = typeIt->second;
            fs::path auxPath =
                scene.basePath.empty()
                    ? PathUtils::PathFromUtf8(typeIt->second)
                    : PathUtils::PathFromUtf8(scene.basePath) /
                          PathUtils::PathFromUtf8(typeIt->second);
            Truss sidecar;
            if (loadTrussDefinitionCached(PathUtils::PathToUtf8(auxPath),
                                          sidecar)) {
              if (truss.manufacturer.empty())
                truss.manufacturer = sidecar.manufacturer;
              if (truss.model.empty())
                truss.model = sidecar.model;
              if (truss.lengthMm <= 0.0f)
                truss.lengthMm = sidecar.lengthMm;
              if (truss.widthMm <= 0.0f)
                truss.widthMm = sidecar.widthMm;
              if (truss.heightMm <= 0.0f)
                truss.heightMm = sidecar.heightMm;
              if (truss.weightKg <= 0.0f)
                truss.weightKg = sidecar.weightKg;
            }
          }
        }

        const fs::path resolvedSymbolPath =
            std::filesystem::path(services.resolveScenePath(truss.symbolFile));
        const bool symbolRenderable =
            IsRenderableTrussGeometry(truss.symbolFile);
        std::error_code symbolExistsEc;
        const bool symbolExists =
            symbolRenderable &&
            fs::exists(resolvedSymbolPath, symbolExistsEc) && !symbolExistsEc;
        if (symbolExists) {
          std::string boundsDiagnostic;
          truss.localGeometryBounds = GeometryBoundsResolver::Resolve(
              resolvedSymbolPath, &boundsDiagnostic);
          if (truss.localGeometryBounds) {
            const bool legacyMetadataContext =
                !hasGdtfMetadataAuthority &&
                (truss.sourceRepresentation ==
                     Truss::GeometryRepresentation::SymbolSymdef ||
                 truss.sourceRepresentation ==
                     Truss::GeometryRepresentation::Geometry3D);
            ResolveTrussDimensionsFromGeometry(truss, legacyMetadataContext);
          } else if (!boundsDiagnostic.empty()) {
            importResult.diagnostics.push_back(
                {"truss_geometry_bounds_unavailable",
                 "Truss '" + truss.uuid +
                     "' geometry bounds could not be measured: " +
                     boundsDiagnostic});
          }
        }
        if (!symbolExists) {
          std::ostringstream reason;
          if (truss.symbolFile.empty()) {
            reason << "symbolFile is empty";
          } else if (!symbolRenderable) {
            reason << "symbolFile extension is not .3ds/.glb";
          } else {
            reason << "symbolFile does not exist on disk (checked path='"
                   << ToString(resolvedSymbolPath.u8string()) << "')";
          }
          if (gdtfLoadFailed)
            reason << "; LoadTrussDefinition(gdtfSpec) returned false";

          std::ostringstream msg;
          msg << "MVR import truss fallback to dummy box: "
              << DescribeTrussForLog(truss) << ". Reason: " << reason.str();
          services.logWarning(msg.str());
        }

        scene.trusses[truss.uuid] = truss;
      };

  // Parses a Support XML node into scene data while preserving group-local
  // transforms.
  std::function<void(tinyxml2::XMLElement *, const std::string &,
                     const Matrix &, const Matrix &, const std::string &)>
      parseSupport = [&](tinyxml2::XMLElement *node,
                         const std::string &layerName,
                         const Matrix &nodeTransform,
                         const Matrix &localTransform,
                         const std::string &parentGroupUuid) {
        Support support;
        support.uuid =
            resolveStableUuid("Support", node, layerName, nodeTransform);
        support.layer = layerName;
        support.transform = nodeTransform;
        support.localTransform = localTransform;
        support.hasLocalTransform = true;
        support.parentGroupUuid = parentGroupUuid;

        if (const char *nameAttr = node->Attribute("name"))
          support.name = nameAttr;

        tinyxml2::XMLElement *childList = node->FirstChildElement("ChildList");
        auto readText = [&](const char *name) -> std::string {
          tinyxml2::XMLElement *parent = childList ? childList : node;
          if (!parent)
            return {};
          if (tinyxml2::XMLElement *el = parent->FirstChildElement(name)) {
            if (const char *txt = el->GetText())
              return Trim(txt);
          }
          return {};
        };

        support.gdtfSpec = remapArchivePathIfNeeded(readText("GDTFSpec"));
        if (!support.gdtfSpec.empty())
          support.gdtfSpec = normalizeSupportGdtfSpec(support.gdtfSpec);
        support.gdtfMode = readText("GDTFMode");
        support.function = readText("Function");
        support.hoistFunction = NormalizeHoistFunction(support.function);
        std::string chainText = readText("ChainLength");
        if (!chainText.empty()) {
          float parsed = 0.0f;
          if (TryParseFloat(chainText, parsed))
            support.chainLength = parsed;
          else
            support.chainLength = 0.0f;
        }

        support.position = CanonicalizeUuid(readText("Position"));
        if (support.position.empty())
          support.position = readText("Position");
        support.positionName = ensurePositionEntry(support.position);
        auto supportPosIt = legacyPositionIdToCanonical.find(support.position);
        if (supportPosIt != legacyPositionIdToCanonical.end())
          support.position = supportPosIt->second;

        if (tinyxml2::XMLElement *geos =
                node->FirstChildElement("Geometries")) {
          for (tinyxml2::XMLElement *g3d =
                   geos->FirstChildElement("Geometry3D");
               g3d; g3d = g3d->NextSiblingElement("Geometry3D")) {
            const char *file = g3d->Attribute("fileName");
            if (!file)
              continue;
            Matrix geoMatrix = MatrixUtils::Identity();
            parseMatrixOrIdentity(g3d, "Matrix", "Support/Geometry3D",
                                  geoMatrix, true);
            appendGeometryInstance(support.geometries, Trim(file), geoMatrix,
                                   BuildSceneObjectGeometryInstanceKey(
                                       support.uuid, "support-geometry3d",
                                       support.geometries.size()));
          }

          size_t symbolIndex = 0;
          for (tinyxml2::XMLElement *sym = geos->FirstChildElement("Symbol");
               sym; sym = sym->NextSiblingElement("Symbol"), ++symbolIndex) {
            std::vector<SymdefGeometry> symGeometries;
            Matrix symMatrix = MatrixUtils::Identity();
            std::string symGeometryType;
            const std::string sourceSymbolUuid =
                Trim(sym->Attribute("uuid") ? sym->Attribute("uuid") : "");
            const std::string sourceSymdefUuid =
                Trim(sym->Attribute("symdef") ? sym->Attribute("symdef") : "");
            resolveSymdefReference(sym, symGeometries, symGeometryType,
                                   symMatrix);
            size_t symGeometryIndex = 0;
            for (const auto &geo : symGeometries) {
              appendGeometryInstance(
                  support.geometries, geo.file,
                  MatrixUtils::Multiply(symMatrix, geo.transform),
                  BuildSceneObjectGeometryInstanceKey(
                      support.uuid, "support-symbol", symbolIndex,
                      sourceSymdefUuid + "/" +
                          std::to_string(symGeometryIndex)),
                  sourceSymbolUuid, sourceSymdefUuid);
              ++symGeometryIndex;
            }
          }
        }
        if (!support.geometries.empty())
          support.modelFile = support.geometries.front().modelFile;

        auto rootHoistInfoIt = rootHoistInfoByUuid.find(support.uuid);
        if (rootHoistInfoIt != rootHoistInfoByUuid.end()) {
          consumedRootHoistInfoUuids.insert(rootHoistInfoIt->first);
          ReadSupportHoistInfoElement(rootHoistInfoIt->second, support,
                                      importResult.diagnostics);
        } else {
          ReadSupportHoistInfoFromUserData(node, support,
                                           importResult.diagnostics);
        }
        ApplySupportHoistInfoDefaults(support);
        auto posIt = scene.positions.find(support.position);
        if (posIt != scene.positions.end())
          support.positionName = posIt->second;

        scene.supports[support.uuid] = support;
      };

  // Parses a SceneObject XML node into scene data while preserving group-local
  // transforms.
  std::function<void(tinyxml2::XMLElement *, const std::string &,
                     const Matrix &, const Matrix &, const std::string &)>
      parseSceneObj = [&](tinyxml2::XMLElement *node,
                          const std::string &layerName,
                          const Matrix &nodeTransform,
                          const Matrix &localTransform,
                          const std::string &parentGroupUuid) {
        SceneObject obj;
        obj.uuid =
            resolveStableUuid("SceneObject", node, layerName, nodeTransform);
        obj.layer = layerName;
        obj.transform = nodeTransform;
        obj.localTransform = localTransform;
        obj.hasLocalTransform = true;
        obj.parentGroupUuid = parentGroupUuid;
        if (const char *nameAttr = node->Attribute("name"))
          obj.name = nameAttr;
        fixtureIdOf(node, obj.fixtureIdText, obj.fixtureIdNumeric);

        std::string geometryType;
        std::unordered_map<std::string, std::string>
            primitiveModelRefByArchiveFile;

        for (tinyxml2::XMLElement *ud = node->FirstChildElement("UserData"); ud;
             ud = ud->NextSiblingElement("UserData")) {
          for (tinyxml2::XMLElement *data = ud->FirstChildElement("Data"); data;
               data = data->NextSiblingElement("Data")) {
            const std::string provider = ToLowerCopy(
                Trim(data->Attribute("provider") ? data->Attribute("provider")
                                                 : ""));
            if (provider != "perastage")
              continue;
            if (tinyxml2::XMLElement *map =
                    data->FirstChildElement("PrimitiveGeometryMap")) {
              for (tinyxml2::XMLElement *entry =
                       map->FirstChildElement("Entry");
                   entry; entry = entry->NextSiblingElement("Entry")) {
                const char *fileName = entry->Attribute("fileName");
                const char *modelRef = entry->Attribute("perastageModelRef");
                if (!modelRef)
                  modelRef = entry->Attribute("modelRef");
                if (!fileName || !modelRef)
                  continue;
                primitiveModelRefByArchiveFile[ToLowerCopy(Trim(fileName))] =
                    Trim(modelRef);
              }
            }
          }
        }

        if (const char *typeAttr = node->Attribute("geometryType"))
          geometryType = Trim(typeAttr);

        if (tinyxml2::XMLElement *geos =
                node->FirstChildElement("Geometries")) {
          for (tinyxml2::XMLElement *g3d =
                   geos->FirstChildElement("Geometry3D");
               g3d; g3d = g3d->NextSiblingElement("Geometry3D")) {
            const char *file = g3d->Attribute("fileName");
            if (!file)
              continue;

            if (const char *type = g3d->Attribute("geometryType"))
              geometryType = Trim(type);

            Matrix geoMatrix = MatrixUtils::Identity();
            parseMatrixOrIdentity(g3d, "Matrix", "SceneObject/Geometry3D",
                                  geoMatrix, true);
            std::string fileName = Trim(file);
            const std::string rawSceneObjectUuid =
                Trim(node->Attribute("uuid") ? node->Attribute("uuid") : "");
            const std::string normalizedFileName = ToLowerCopy(fileName);
            const std::string rootPrimitiveKey =
                obj.uuid + "|" + normalizedFileName;
            auto rootMappedModelRefIt =
                rootPrimitiveModelRefsBySceneObjectAndFile.find(
                    rootPrimitiveKey);
            if (rootMappedModelRefIt ==
                    rootPrimitiveModelRefsBySceneObjectAndFile.end() &&
                !rawSceneObjectUuid.empty()) {
              rootMappedModelRefIt =
                  rootPrimitiveModelRefsBySceneObjectAndFile.find(
                      rawSceneObjectUuid + "|" + normalizedFileName);
            }
            auto mappedModelRefIt =
                primitiveModelRefByArchiveFile.find(ToLowerCopy(fileName));
            const std::string *mappedModelRef = nullptr;
            if (rootMappedModelRefIt !=
                    rootPrimitiveModelRefsBySceneObjectAndFile.end() &&
                !rootMappedModelRefIt->second.empty()) {
              if (rootMappedModelRefIt->second.size() > 1) {
                services.logWarning(
                    "MVR import found ambiguous root PrimitiveGeometryMap "
                    "entries for SceneObject " +
                    obj.uuid + " and file " + fileName +
                    "; using the first entry");
              }
              mappedModelRef = &rootMappedModelRefIt->second.front();
            } else if (mappedModelRefIt !=
                       primitiveModelRefByArchiveFile.end()) {
              mappedModelRef = &mappedModelRefIt->second;
            }
            if (mappedModelRef) {
              GeometryInstance instance;
              instance.modelFile = *mappedModelRef;
              instance.instanceKey = BuildSceneObjectGeometryInstanceKey(
                  obj.uuid, "geometry3d", obj.geometries.size());
              instance.localTransform = geoMatrix;
              obj.geometries.push_back(std::move(instance));
            } else {
              const std::string instanceKey =
                  BuildSceneObjectGeometryInstanceKey(obj.uuid, "geometry3d",
                                                      obj.geometries.size());
              appendGeometryInstance(obj.geometries, fileName, geoMatrix,
                                     instanceKey);
            }
          }

          size_t symbolIndex = 0;
          for (tinyxml2::XMLElement *sym = geos->FirstChildElement("Symbol");
               sym; sym = sym->NextSiblingElement("Symbol"), ++symbolIndex) {
            std::vector<SymdefGeometry> symGeometries;
            Matrix symMatrix = MatrixUtils::Identity();
            std::string symGeometryType;
            const std::string sourceSymbolUuid =
                Trim(sym->Attribute("uuid") ? sym->Attribute("uuid") : "");
            const std::string sourceSymdefUuid =
                Trim(sym->Attribute("symdef") ? sym->Attribute("symdef") : "");
            resolveSymdefReference(sym, symGeometries, symGeometryType,
                                   symMatrix);
            if (!symGeometryType.empty())
              geometryType = symGeometryType;

            size_t symGeometryIndex = 0;
            for (const auto &geo : symGeometries) {
              Matrix localTransform =
                  MatrixUtils::Multiply(symMatrix, geo.transform);
              const std::string instanceKey =
                  BuildSceneObjectGeometryInstanceKey(
                      obj.uuid, "symbol", symbolIndex,
                      sourceSymdefUuid + "/" +
                          std::to_string(symGeometryIndex));
              appendGeometryInstance(obj.geometries, geo.file, localTransform,
                                     instanceKey, sourceSymbolUuid,
                                     sourceSymdefUuid);
              std::ostringstream geometryLog;
              geometryLog << "SceneObject geometry resolved: sceneObject='"
                          << obj.name << "' sceneUuid=" << obj.uuid
                          << " symbolUuid=" << sourceSymbolUuid
                          << " symdef=" << sourceSymdefUuid
                          << " file=" << geo.file
                          << " instanceKey=" << instanceKey
                          << " localTransform="
                          << MatrixUtils::FormatMatrix(localTransform);
              services.logDebug(geometryLog.str());
              if (!geo.geometryType.empty())
                geometryType = geo.geometryType;
              ++symGeometryIndex;
            }
          }
        }

        if (!obj.geometries.empty()) {
          obj.modelFile = obj.geometries.front().modelFile;
          obj.transform = nodeTransform;
        }

        std::ostringstream importedLog;
        importedLog << "Imported SceneObject " << obj.uuid << " with "
                    << obj.geometries.size() << " geometry parts";
        services.logDebug(importedLog.str());

        auto typeLower = geometryType;
        std::transform(typeLower.begin(), typeLower.end(), typeLower.begin(),
                       [](unsigned char c) { return std::tolower(c); });

        if (typeLower == "support") {
          Support support;
          support.uuid = obj.uuid;
          support.name = obj.name;
          support.layer = obj.layer;
          support.transform = obj.transform;
          support.localTransform = obj.localTransform;
          support.hasLocalTransform = obj.hasLocalTransform;
          support.parentGroupUuid = obj.parentGroupUuid;
          support.modelFile = obj.modelFile;
          support.geometries = obj.geometries;
          for (tinyxml2::XMLElement *ud = node->FirstChildElement("UserData");
               ud; ud = ud->NextSiblingElement("UserData")) {
            for (tinyxml2::XMLElement *data = ud->FirstChildElement("Data");
                 data; data = data->NextSiblingElement("Data")) {
              const std::string provider = ToLowerCopy(
                  Trim(data->Attribute("provider") ? data->Attribute("provider")
                                                   : ""));
              if (provider != "perastage")
                continue;
              if (tinyxml2::XMLElement *info =
                      data->FirstChildElement("SupportInfo")) {
                if (tinyxml2::XMLElement *n =
                        info->FirstChildElement("GDTFSpec");
                    n && n->GetText()) {
                  support.gdtfSpec =
                      remapArchivePathIfNeeded(Trim(n->GetText()));
                }
                if (tinyxml2::XMLElement *n =
                        info->FirstChildElement("GDTFMode");
                    n && n->GetText()) {
                  support.gdtfMode = Trim(n->GetText());
                }
                if (tinyxml2::XMLElement *n =
                        info->FirstChildElement("Function");
                    n && n->GetText()) {
                  support.function = Trim(n->GetText());
                }
                if (tinyxml2::XMLElement *n =
                        info->FirstChildElement("HoistFunction");
                    n && n->GetText()) {
                  support.hoistFunction =
                      NormalizeHoistFunction(Trim(n->GetText()));
                }
                if (tinyxml2::XMLElement *n =
                        info->FirstChildElement("ChainLength");
                    n && n->GetText()) {
                  float parsed = 0.0f;
                  if (TryParseFloat(Trim(n->GetText()), parsed))
                    support.chainLength = parsed;
                }
                if (tinyxml2::XMLElement *n =
                        info->FirstChildElement("Position");
                    n && n->GetText()) {
                  support.position = Trim(n->GetText());
                }
                if (tinyxml2::XMLElement *n =
                        info->FirstChildElement("PositionName");
                    n && n->GetText()) {
                  support.positionName = Trim(n->GetText());
                }
                services.logInfo("MVR import reconstructed Support from "
                                 "SceneObject fallback "
                                 "uuid='" +
                                 support.uuid + "'");
              }
            }
          }
          if (!support.position.empty()) {
            auto posIt = legacyPositionIdToCanonical.find(support.position);
            if (posIt != legacyPositionIdToCanonical.end())
              support.position = posIt->second;
            support.positionName = ensurePositionEntry(support.position);
          } else if (!support.positionName.empty()) {
            for (const auto &[positionUuid, positionName] : scene.positions) {
              if (positionName == support.positionName) {
                support.position = positionUuid;
                break;
              }
            }
          }
          auto rootHoistInfoIt = rootHoistInfoByUuid.find(support.uuid);
          if (rootHoistInfoIt != rootHoistInfoByUuid.end()) {
            consumedRootHoistInfoUuids.insert(rootHoistInfoIt->first);
            ReadSupportHoistInfoElement(rootHoistInfoIt->second, support,
                                        importResult.diagnostics);
          } else {
            ReadSupportHoistInfoFromUserData(node, support,
                                             importResult.diagnostics);
          }
          ApplySupportHoistInfoDefaults(support);
          scene.supports[support.uuid] = support;
        } else {
          scene.sceneObjects[obj.uuid] = obj;
        }
      };

  tinyxml2::XMLElement *layersNode = sceneNode->FirstChildElement("Layers");
  if (!layersNode)
    return;

  std::function<int(tinyxml2::XMLElement *)> countImportSceneNodes =
      [&](tinyxml2::XMLElement *childList) {
        if (!childList)
          return 0;
        int count = 0;
        for (tinyxml2::XMLElement *child = childList->FirstChildElement();
             child; child = child->NextSiblingElement()) {
          const char *name = child->Name();
          if (!name)
            continue;
          const std::string nodeName = name;
          if (nodeName == "Fixture" || nodeName == "Truss" ||
              nodeName == "Support" || nodeName == "SceneObject" ||
              nodeName == "GroupObject") {
            ++count;
          }
          if (tinyxml2::XMLElement *inner =
                  child->FirstChildElement("ChildList"))
            count += countImportSceneNodes(inner);
        }
        return count;
      };

  int totalImportNodes = 0;
  for (tinyxml2::XMLElement *cl = layersNode->FirstChildElement("ChildList");
       cl; cl = cl->NextSiblingElement("ChildList")) {
    totalImportNodes += countImportSceneNodes(cl);
  }
  for (tinyxml2::XMLElement *layer = layersNode->FirstChildElement("Layer");
       layer; layer = layer->NextSiblingElement("Layer")) {
    totalImportNodes +=
        countImportSceneNodes(layer->FirstChildElement("ChildList"));
  }

  int importedNodes = 0;
  auto reportNodeProgress = [&](const char *nodeKind) {
    ++importedNodes;
    if (totalImportNodes <= 0)
      return;
    constexpr int kReportEveryNodes = 25;
    if (importedNodes == 1 || importedNodes == totalImportNodes ||
        importedNodes % kReportEveryNodes == 0) {
      reportProgress(std::string("Importing scene objects (") + nodeKind + ")",
                     importedNodes, totalImportNodes);
    }
  };

  parseChildList = [&](tinyxml2::XMLElement *cl, const std::string &layerName,
                       const Matrix &parentTransform,
                       const std::string &parentGroupUuid) {
    for (tinyxml2::XMLElement *child = cl->FirstChildElement(); child;
         child = child->NextSiblingElement()) {
      const char *name = child->Name();
      if (!name)
        continue;

      Matrix local = MatrixUtils::Identity();
      parseMatrixOrIdentity(child, "Matrix", std::string("Child/") + name,
                            local, true);
      Matrix nodeTransform = MatrixUtils::Multiply(parentTransform, local);

      std::string nodeName = name;
      if (nodeName == "Fixture") {
        parseFixture(child, layerName, nodeTransform, local, parentGroupUuid);
        reportNodeProgress("Fixture");
        if (!parentGroupUuid.empty()) {
          scene.groupObjects[parentGroupUuid].children.push_back(
              {MvrNodeType::Fixture,
               referenceUuidForNode("Fixture", child, layerName,
                                    nodeTransform)});
        }
      } else if (nodeName == "Truss") {
        parseTruss(child, layerName, nodeTransform, local, parentGroupUuid);
        reportNodeProgress("Truss");
        if (!parentGroupUuid.empty()) {
          scene.groupObjects[parentGroupUuid].children.push_back(
              {MvrNodeType::Truss,
               referenceUuidForNode("Truss", child, layerName, nodeTransform)});
        }
      } else if (nodeName == "Support") {
        parseSupport(child, layerName, nodeTransform, local, parentGroupUuid);
        reportNodeProgress("Support");
        if (!parentGroupUuid.empty()) {
          scene.groupObjects[parentGroupUuid].children.push_back(
              {MvrNodeType::Support,
               referenceUuidForNode("Support", child, layerName,
                                    nodeTransform)});
        }
      } else if (nodeName == "SceneObject") {
        parseSceneObj(child, layerName, nodeTransform, local, parentGroupUuid);
        reportNodeProgress("SceneObject");
        if (!parentGroupUuid.empty()) {
          scene.groupObjects[parentGroupUuid].children.push_back(
              {MvrNodeType::SceneObject,
               referenceUuidForNode("SceneObject", child, layerName,
                                    nodeTransform)});
        }
      } else if (nodeName == "GroupObject") {
        GroupObject group;
        group.uuid =
            resolveStableUuid("GroupObject", child, layerName, nodeTransform);
        group.layer = layerName;
        group.transform = nodeTransform;
        group.localTransform = local;
        group.parentGroupUuid = parentGroupUuid;
        if (const char *nameAttr = child->Attribute("name"))
          group.name = nameAttr;
        scene.groupObjects[group.uuid] = group;
        reportNodeProgress("GroupObject");
        if (!parentGroupUuid.empty()) {
          scene.groupObjects[parentGroupUuid].children.push_back(
              {MvrNodeType::GroupObject, group.uuid});
        }
        ++preservedGroupObjectCount;
        if (tinyxml2::XMLElement *inner = child->FirstChildElement("ChildList"))
          parseChildList(inner, layerName, nodeTransform, group.uuid);
        continue;
      }

      if (tinyxml2::XMLElement *inner = child->FirstChildElement("ChildList"))
        parseChildList(inner, layerName, nodeTransform, parentGroupUuid);
    }
  };
  for (tinyxml2::XMLElement *cl = layersNode->FirstChildElement("ChildList");
       cl; cl = cl->NextSiblingElement("ChildList")) {
    parseChildList(cl, DEFAULT_LAYER_NAME, MatrixUtils::Identity(), "");
  }

  for (tinyxml2::XMLElement *layer = layersNode->FirstChildElement("Layer");
       layer; layer = layer->NextSiblingElement("Layer")) {
    const char *layerName = layer->Attribute("name");
    std::string layerStr = layerName ? layerName : "";
    if (!IsValidUtf8(layerStr)) {
      const auto repairedLayerName = RepairWindows1252AsUtf8(layerStr);
      if (repairedLayerName) {
        services.logWarning(
            "Repaired legacy Windows-1252 layer name bytes at Layer uuid=" +
            std::string(layer->Attribute("uuid") ? layer->Attribute("uuid")
                                                 : "") +
            " offset=" + std::to_string(ValidateUtf8(layerStr).errorOffset));
        layerStr = *repairedLayerName;
      } else {
        services.logError(
            "Rejected invalid UTF-8 layer name at Layer uuid=" +
            std::string(layer->Attribute("uuid") ? layer->Attribute("uuid")
                                                 : "") +
            " offset=" + std::to_string(ValidateUtf8(layerStr).errorOffset));
        layerStr.clear();
      }
    }
    bool isDefaultLayer = layerStr.empty();

    tinyxml2::XMLElement *childList = layer->FirstChildElement("ChildList");
    if (childList)
      parseChildList(childList, isDefaultLayer ? DEFAULT_LAYER_NAME : layerStr,
                     MatrixUtils::Identity(), "");

    if (!isDefaultLayer) {
      Layer l;
      const char *uuidAttr = layer->Attribute("uuid");
      if (uuidAttr)
        l.uuid = uuidAttr;
      l.name = layerStr;
      auto colorByUuid = layerColorByUuid.find(CanonicalizeUuid(l.uuid));
      if (colorByUuid != layerColorByUuid.end()) {
        l.color = colorByUuid->second;
      } else {
        auto colorByName = layerColorByName.find(l.name);
        if (colorByName != layerColorByName.end()) {
          l.color = colorByName->second;
        } else if (tinyxml2::XMLElement *colorNode =
                       layer->FirstChildElement("Color")) {
          if (const char *txt = colorNode->GetText()) {
            const std::string legacyColor = Trim(txt);
            l.color =
                isHexRgb(legacyColor) ? legacyColor : CieToHex(legacyColor);
          }
        }
      }
      scene.layers[l.uuid] = l;
    }
  }

  const auto reconcileResult = layerdomain::ReconcileLegacyLayers(scene);
  if (reconcileResult.status == layerdomain::LayerStatus::Success) {
    services.logWarning("Reconciled legacy layer metadata: " +
                        reconcileResult.message);
  }

  if (preservedGroupObjectCount > 0) {
    services.logInfo("MVR import preserved GroupObject count=" +
                     std::to_string(preservedGroupObjectCount));
  }
  const std::size_t repairedGroupLayerCount =
      scene_grouping::SynchronizeGroupObjectLayerOwnership(scene);
  if (repairedGroupLayerCount > 0) {
    services.logInfo("MVR import repaired " +
                     std::to_string(repairedGroupLayerCount) +
                     " GroupObject child layer assignment(s)");
  }
}

} // namespace mvr
