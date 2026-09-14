/*
 * This file is part of Perastage.
 * Copyright (C) 2026 Luisma Peramato
 *
 * Perastage is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Perastage is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Perastage. If not, see <https://www.gnu.org/licenses/>.
 */
#pragma once

#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

class MvrScene;
namespace tinyxml2 {
class XMLDocument;
class XMLElement;
} // namespace tinyxml2

namespace mvr_xml_extension {

struct FixtureTypeMetadata {
  std::string key;
  std::string gdtfSpec;
  std::string gdtfMode;
  std::string manufacturer;
  std::string model;
  std::string category;
  std::string categorySource;
  std::string visualColorHex;
};

struct PrimitiveGeometryMetadata {
  std::string sceneObjectUuid;
  std::string fileName;
  std::string perastageModelRef;
  size_t geometryIndex = 0;
};

struct TrussInfoMetadata {
  std::string uuid;
  std::optional<float> manualLoadKg;
  std::string manufacturer, model, length, width, height, weight;
  std::string gdtfDescription, crossSectionType, crossSection, modelFile;
  std::string positionName, representation, typeKey, auxGdtf;
};

struct HoistInfoMetadata {
  std::string uuid;
  float capacityKg = 0.0f, weightKg = 0.0f;
  std::optional<float> manualLoadKg;
  std::string riggingPoint, motorName, motorManufacturer, motorModel;
  std::string motorFixtureUuid, useMotorDefaults, dummyProfileId, dummyPreset;
  std::string valueSource, motorNameSource, motorManufacturerSource;
  std::string motorModelSource, capacitySource, weightSource;
  std::string riggingPointSource;
};

// Finds or creates the root Perastage-owned Data element.
tinyxml2::XMLElement *FindOrCreateDataNode(tinyxml2::XMLDocument &document,
                                           tinyxml2::XMLElement *node);

// Reports whether the scene contains Perastage layer appearance metadata.
bool HasLayerAppearance(const MvrScene &scene);

// Appends Perastage layer appearance metadata using prepared layer UUIDs.
void AppendLayerAppearance(
    tinyxml2::XMLDocument &document, tinyxml2::XMLElement *perastageData,
    const MvrScene &scene,
    const std::unordered_map<std::string, std::string> &layerUuids);

// Appends resolved fixture-type metadata to Perastage-owned UserData.
void AppendFixtureTypes(
    tinyxml2::XMLDocument &document, tinyxml2::XMLElement *perastageData,
    const std::map<std::string, FixtureTypeMetadata> &metadataByType);

// Appends project fixture fidelity metadata to Perastage-owned UserData.
void AppendProjectFixtures(tinyxml2::XMLDocument &document,
                           tinyxml2::XMLElement *perastageData,
                           const MvrScene &scene);

// Appends the resolved primitive geometry map to Perastage-owned UserData.
void AppendPrimitiveGeometryMap(
    tinyxml2::XMLDocument &document, tinyxml2::XMLElement *perastageData,
    const std::vector<PrimitiveGeometryMetadata> &entries);

// Appends resolved Perastage TrussInfo metadata in established order.
void AppendTrussInfo(tinyxml2::XMLDocument &document, tinyxml2::XMLElement *map,
                     const TrussInfoMetadata &values);

// Appends resolved Perastage HoistInfo metadata in established order.
void AppendHoistInfo(tinyxml2::XMLDocument &document, tinyxml2::XMLElement *map,
                     const HoistInfoMetadata &values);

// Creates the root Perastage TrussInfoMap container.
tinyxml2::XMLElement *CreateTrussInfoMap(tinyxml2::XMLDocument &document);

// Creates the root Perastage HoistInfoMap container.
tinyxml2::XMLElement *CreateHoistInfoMap(tinyxml2::XMLDocument &document);

} // namespace mvr_xml_extension
