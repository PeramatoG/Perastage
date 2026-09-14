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
#include "mvr_xml_extension_writer.h"

#include "mvrscene.h"
#include "uuidutils.h"

#include <tinyxml2.h>

#include <algorithm>
#include <cctype>

namespace {

// Trims ASCII whitespace from extension-owned identifier values.
std::string TrimAscii(std::string value) {
  auto isSpace = [](unsigned char ch) { return std::isspace(ch) != 0; };
  value.erase(value.begin(),
              std::find_if(value.begin(), value.end(),
                           [&](unsigned char ch) { return !isSpace(ch); }));
  value.erase(std::find_if(value.rbegin(), value.rend(),
                           [&](unsigned char ch) { return !isSpace(ch); })
                  .base(),
              value.end());
  return value;
}

// Converts provider identifiers to lowercase for case-insensitive ownership
// checks.
std::string ToLowerAscii(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(),
                 [](unsigned char ch) { return std::tolower(ch); });
  return value;
}

// Finds the UserData container suitable for Perastage-owned data.
tinyxml2::XMLElement *FindPerastageUserData(tinyxml2::XMLElement *node) {
  if (!node)
    return nullptr;
  tinyxml2::XMLElement *first = node->FirstChildElement("UserData");
  for (tinyxml2::XMLElement *userData = first; userData;
       userData = userData->NextSiblingElement("UserData")) {
    for (tinyxml2::XMLElement *data = userData->FirstChildElement("Data"); data;
         data = data->NextSiblingElement("Data")) {
      const std::string provider = TrimAscii(
          data->Attribute("provider") ? data->Attribute("provider") : "");
      if (provider.empty() || ToLowerAscii(provider) == "perastage")
        return userData;
    }
  }
  return first;
}

// Validates a Perastage layer color metadata value.
bool IsLayerColor(const std::string &color) {
  return color.size() == 7 && color[0] == '#' &&
         std::all_of(color.begin() + 1, color.end(),
                     [](unsigned char ch) { return std::isxdigit(ch) != 0; });
}

} // namespace

namespace mvr_xml_extension {

// Finds or creates the root Perastage-owned Data element.
tinyxml2::XMLElement *FindOrCreateDataNode(tinyxml2::XMLDocument &document,
                                           tinyxml2::XMLElement *node) {
  tinyxml2::XMLElement *userData = FindPerastageUserData(node);
  if (!userData) {
    userData = document.NewElement("UserData");
    node->InsertEndChild(userData);
  }
  for (tinyxml2::XMLElement *data = userData->FirstChildElement("Data"); data;
       data = data->NextSiblingElement("Data")) {
    const std::string provider = TrimAscii(
        data->Attribute("provider") ? data->Attribute("provider") : "");
    if (provider.empty() || ToLowerAscii(provider) == "perastage")
      return data;
  }
  tinyxml2::XMLElement *data = document.NewElement("Data");
  data->SetAttribute("provider", "Perastage");
  data->SetAttribute("ver", "1.0");
  userData->InsertEndChild(data);
  return data;
}

// Reports whether the scene contains valid Perastage layer color metadata.
bool HasLayerAppearance(const MvrScene &scene) {
  return std::any_of(
      scene.layers.begin(), scene.layers.end(),
      [](const auto &entry) { return IsLayerColor(entry.second.color); });
}

// Appends the Perastage layer appearance map in established scene order.
void AppendLayerAppearance(
    tinyxml2::XMLDocument &document, tinyxml2::XMLElement *perastageData,
    const MvrScene &scene,
    const std::unordered_map<std::string, std::string> &layerUuids) {
  if (!perastageData)
    return;
  tinyxml2::XMLElement *map =
      perastageData->FirstChildElement("LayerAppearanceMap");
  for (const auto &[layerUuid, layer] : scene.layers) {
    if (!IsLayerColor(layer.color))
      continue;
    if (!map)
      map = document.NewElement("LayerAppearanceMap");
    tinyxml2::XMLElement *entry =
        document.NewElement("PerastageLayerAppearance");
    const auto preparedUuid = layerUuids.find(layerUuid);
    const std::string exportUuid =
        preparedUuid != layerUuids.end() ? preparedUuid->second : std::string{};
    if (!exportUuid.empty())
      entry->SetAttribute("uuid", exportUuid.c_str());
    if (!layer.name.empty())
      entry->SetAttribute("name", layer.name.c_str());
    entry->SetAttribute("color", layer.color.c_str());
    map->InsertEndChild(entry);
  }
  if (map && !map->Parent())
    perastageData->InsertEndChild(map);
}

// Appends resolved fixture-type metadata in stable map order.
void AppendFixtureTypes(
    tinyxml2::XMLDocument &document, tinyxml2::XMLElement *perastageData,
    const std::map<std::string, FixtureTypeMetadata> &metadataByType) {
  if (!perastageData || metadataByType.empty())
    return;
  tinyxml2::XMLElement *map = document.NewElement("FixtureTypeInfoMap");
  for (const auto &[key, entry] : metadataByType) {
    (void)key;
    tinyxml2::XMLElement *info = document.NewElement("FixtureTypeInfo");
    info->SetAttribute("key", entry.key.c_str());
    if (!entry.gdtfSpec.empty())
      info->SetAttribute("gdtfSpec", entry.gdtfSpec.c_str());
    if (!entry.gdtfMode.empty())
      info->SetAttribute("gdtfMode", entry.gdtfMode.c_str());
    if (!entry.manufacturer.empty())
      info->SetAttribute("manufacturer", entry.manufacturer.c_str());
    if (!entry.model.empty())
      info->SetAttribute("model", entry.model.c_str());
    auto appendText = [&](const char *name, const std::string &value) {
      if (value.empty())
        return;
      tinyxml2::XMLElement *node = document.NewElement(name);
      node->SetText(value.c_str());
      info->InsertEndChild(node);
    };
    appendText("Category", entry.category);
    if (!entry.category.empty())
      appendText("CategorySource", entry.categorySource);
    appendText("VisualColor", entry.visualColorHex);
    map->InsertEndChild(info);
  }
  perastageData->InsertEndChild(map);
}

// Appends canonical fixture fidelity metadata keyed by instance UUID.
void AppendProjectFixtures(tinyxml2::XMLDocument &document,
                           tinyxml2::XMLElement *perastageData,
                           const MvrScene &scene) {
  if (!perastageData)
    return;
  tinyxml2::XMLElement *map = document.NewElement("ProjectFixtureMetadataMap");
  map->SetAttribute("schemaVersion", "1.0");
  std::map<std::string, const Fixture *> fixturesByUuid;
  for (const auto &[key, fixture] : scene.fixtures) {
    (void)key;
    const std::string uuid = CanonicalizeUuid(fixture.uuid);
    if (!uuid.empty())
      fixturesByUuid[uuid] = &fixture;
  }
  for (const auto &[uuid, fixture] : fixturesByUuid) {
    tinyxml2::XMLElement *entry = document.NewElement("ProjectFixtureMetadata");
    entry->SetAttribute("uuid", uuid.c_str());
    entry->SetAttribute("fixtureId", fixture->fixtureId);
    entry->SetAttribute("fixtureIdNumeric", fixture->fixtureIdNumeric);
    entry->SetAttribute("fixtureIdText", fixture->fixtureIdText.c_str());
    entry->SetAttribute("unitNumber", fixture->unitNumber);
    const std::string color = TrimAscii(fixture->visualColorHex);
    if (fixture->visualColorState != FixtureProjectColorState::Missing ||
        !color.empty()) {
      entry->SetAttribute("hasVisualColorHex",
                          color.empty() ? "false" : "true");
      if (!color.empty())
        entry->SetAttribute("visualColorHex", color.c_str());
    }
    map->InsertEndChild(entry);
  }
  if (map->FirstChild())
    perastageData->InsertEndChild(map);
  else
    document.DeleteNode(map);
}

// Appends resolved primitive geometry compatibility metadata.
void AppendPrimitiveGeometryMap(
    tinyxml2::XMLDocument &document, tinyxml2::XMLElement *perastageData,
    const std::vector<PrimitiveGeometryMetadata> &entries) {
  if (!perastageData || entries.empty())
    return;
  tinyxml2::XMLElement *map = document.NewElement("PrimitiveGeometryMap");
  for (const PrimitiveGeometryMetadata &value : entries) {
    tinyxml2::XMLElement *entry = document.NewElement("Entry");
    entry->SetAttribute("sceneObjectUuid", value.sceneObjectUuid.c_str());
    entry->SetAttribute("fileName", value.fileName.c_str());
    entry->SetAttribute("perastageModelRef", value.perastageModelRef.c_str());
    entry->SetAttribute("geometryIndex",
                        static_cast<unsigned>(value.geometryIndex));
    map->InsertEndChild(entry);
  }
  perastageData->InsertEndChild(map);
}

namespace {

// Appends a non-empty text node to Perastage-owned metadata.
void AppendMetadataText(tinyxml2::XMLDocument &document,
                        tinyxml2::XMLElement *parent, const char *nodeName,
                        const std::string &value) {
  if (value.empty())
    return;
  tinyxml2::XMLElement *node = document.NewElement(nodeName);
  node->SetText(value.c_str());
  parent->InsertEndChild(node);
}

} // namespace

// Appends resolved Perastage TrussInfo metadata in established order.
void AppendTrussInfo(tinyxml2::XMLDocument &document, tinyxml2::XMLElement *map,
                     const TrussInfoMetadata &values) {
  tinyxml2::XMLElement *info = document.NewElement("TrussInfo");
  info->SetAttribute("uuid", values.uuid.c_str());
  if (values.manualLoadKg) {
    tinyxml2::XMLElement *load = document.NewElement("Load");
    load->SetAttribute("unit", "kg");
    load->SetAttribute("source", "Manual");
    load->SetText(std::to_string(*values.manualLoadKg).c_str());
    info->InsertEndChild(load);
  }
  AppendMetadataText(document, info, "Manufacturer", values.manufacturer);
  AppendMetadataText(document, info, "Model", values.model);
  AppendMetadataText(document, info, "Length", values.length);
  AppendMetadataText(document, info, "Width", values.width);
  AppendMetadataText(document, info, "Height", values.height);
  AppendMetadataText(document, info, "Weight", values.weight);
  AppendMetadataText(document, info, "GdtfDescription", values.gdtfDescription);
  AppendMetadataText(document, info, "CrossSectionType",
                     values.crossSectionType);
  AppendMetadataText(document, info, "CrossSection", values.crossSection);
  AppendMetadataText(document, info, "ModelFile", values.modelFile);
  AppendMetadataText(document, info, "PositionName", values.positionName);
  AppendMetadataText(document, info, "HangPos", values.positionName);
  AppendMetadataText(document, info, "Representation", values.representation);
  AppendMetadataText(document, info, "TypeKey", values.typeKey);
  AppendMetadataText(document, info, "AuxGdtf", values.auxGdtf);
  map->InsertEndChild(info);
}

// Appends resolved Perastage HoistInfo metadata in established order.
void AppendHoistInfo(tinyxml2::XMLDocument &document, tinyxml2::XMLElement *map,
                     const HoistInfoMetadata &values) {
  tinyxml2::XMLElement *info = document.NewElement("HoistInfo");
  info->SetAttribute("uuid", values.uuid.c_str());
  auto appendKilograms = [&](const char *name, float value) {
    if (value == 0.0f)
      return;
    tinyxml2::XMLElement *node = document.NewElement(name);
    node->SetAttribute("unit", "kg");
    node->SetText(std::to_string(value).c_str());
    info->InsertEndChild(node);
  };
  appendKilograms("Capacity", values.capacityKg);
  appendKilograms("Weight", values.weightKg);
  if (values.manualLoadKg) {
    tinyxml2::XMLElement *load = document.NewElement("Load");
    load->SetAttribute("unit", "kg");
    load->SetText(std::to_string(*values.manualLoadKg).c_str());
    info->InsertEndChild(load);
  }
  AppendMetadataText(document, info, "RiggingPoint", values.riggingPoint);
  AppendMetadataText(document, info, "MotorName", values.motorName);
  AppendMetadataText(document, info, "MotorManufacturer",
                     values.motorManufacturer);
  AppendMetadataText(document, info, "MotorModel", values.motorModel);
  AppendMetadataText(document, info, "MotorFixtureUuid",
                     values.motorFixtureUuid);
  AppendMetadataText(document, info, "UseMotorDefaults",
                     values.useMotorDefaults);
  AppendMetadataText(document, info, "DummyProfileId", values.dummyProfileId);
  AppendMetadataText(document, info, "DummyPreset", values.dummyPreset);
  AppendMetadataText(document, info, "ValueSource", values.valueSource);
  AppendMetadataText(document, info, "DataSource", values.valueSource);
  AppendMetadataText(document, info, "MotorNameSource", values.motorNameSource);
  AppendMetadataText(document, info, "MotorManufacturerSource",
                     values.motorManufacturerSource);
  AppendMetadataText(document, info, "MotorModelSource",
                     values.motorModelSource);
  AppendMetadataText(document, info, "CapacitySource", values.capacitySource);
  AppendMetadataText(document, info, "WeightSource", values.weightSource);
  AppendMetadataText(document, info, "RiggingPointSource",
                     values.riggingPointSource);
  map->InsertEndChild(info);
}

// Creates the root Perastage TrussInfoMap container.
tinyxml2::XMLElement *CreateTrussInfoMap(tinyxml2::XMLDocument &document) {
  return document.NewElement("TrussInfoMap");
}

// Creates the root Perastage HoistInfoMap container.
tinyxml2::XMLElement *CreateHoistInfoMap(tinyxml2::XMLDocument &document) {
  return document.NewElement("HoistInfoMap");
}

} // namespace mvr_xml_extension
