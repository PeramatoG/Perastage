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
#include "mvr_scene_node_reader_detail.h"

#include "dummyprofilelibrary.h"
#include "gdtf_fixture_category.h"
#include "support.h"
#include "uuidutils.h"

#include <tinyxml2.h>

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <sstream>

namespace mvr::scene_reader_detail {

// Trims surrounding ASCII whitespace from imported text.
std::string Trim(const std::string &value) {
  const char *whitespace = " \t\r\n";
  const size_t start = value.find_first_not_of(whitespace);
  if (start == std::string::npos)
    return {};
  const size_t end = value.find_last_not_of(whitespace);
  return value.substr(start, end - start + 1);
}

// Converts imported ASCII text to lowercase.
std::string ToLowerCopy(std::string value) {
  std::transform(
      value.begin(), value.end(), value.begin(),
      [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
  return value;
}

// Parses one finite floating-point metadata value.
bool TryParseFloat(const std::string &text, float &out) {
  if (text.empty())
    return false;
  const auto first =
      std::find_if_not(text.begin(), text.end(),
                       [](unsigned char ch) { return std::isspace(ch); });
  if (first == text.end())
    return false;
  const auto last =
      std::find_if_not(text.rbegin(), text.rend(), [](unsigned char ch) {
        return std::isspace(ch);
      }).base();
  const std::string value(first, last);
  errno = 0;
  char *end = nullptr;
  const double parsed = std::strtod(value.c_str(), &end);
  if (end != value.c_str() + value.size() || errno == ERANGE ||
      !std::isfinite(parsed) || !std::isfinite(static_cast<float>(parsed)))
    return false;
  out = static_cast<float>(parsed);
  return true;
}

// Describes one truss for structured fallback diagnostics.
std::string DescribeTruss(const Truss &truss) {
  const std::string displayName = truss.name.empty() ? "(unnamed)" : truss.name;
  std::ostringstream message;
  message << "uuid='" << truss.uuid << "', name='" << displayName
          << "', model='" << truss.model << "', modelFile='" << truss.modelFile
          << "', gdtfSpec='" << truss.gdtfSpec << "', symbolFile='"
          << truss.symbolFile << "'";
  return message.str();
}

// Parses the Perastage truss representation extension value.
Truss::GeometryRepresentation
ParseTrussRepresentation(const std::string &value) {
  const std::string lower = ToLowerCopy(Trim(value));
  if (lower == "symbolsymdef")
    return Truss::GeometryRepresentation::SymbolSymdef;
  if (lower == "geometry3d")
    return Truss::GeometryRepresentation::Geometry3D;
  if (lower == "publicgdtf")
    return Truss::GeometryRepresentation::PublicGdtf;
  if (lower == "nativeperastage")
    return Truss::GeometryRepresentation::NativePerastage;
  return Truss::GeometryRepresentation::Unknown;
}

// Reports whether a truss geometry path names a supported renderable format.
bool IsRenderableTrussGeometry(const std::string &path) {
  if (path.empty())
    return false;
  std::string extension = std::filesystem::path(path).extension().string();
  extension = ToLowerCopy(extension);
  return extension == ".3ds" || extension == ".glb" || extension == ".gltf";
}

// Applies defaulted and normalized values to imported Support hoist metadata.
void ApplySupportDefaults(Support &support) {
  if (support.dummyProfileId.empty() && !support.dummyPreset.empty()) {
    const auto profile =
        DummyProfileLibrary::FindByDisplayName(support.dummyPreset);
    if (profile.has_value())
      support.dummyProfileId = profile->id;
  }

  support.hoistFunction = NormalizeHoistFunction(
      support.hoistFunction.empty() ? support.function : support.hoistFunction);
  support.hoistDataSource = NormalizeHoistDataSource(support.hoistDataSource);
  support.motorNameSource = ResolveHoistFieldDataSource(
      support.motorNameSource, support.hoistDataSource);
  support.motorManufacturerSource = ResolveHoistFieldDataSource(
      support.motorManufacturerSource, support.hoistDataSource);
  support.motorModelSource = ResolveHoistFieldDataSource(
      support.motorModelSource, support.hoistDataSource);
  support.capacitySource = ResolveHoistFieldDataSource(support.capacitySource,
                                                       support.hoistDataSource);
  support.weightSource = ResolveHoistFieldDataSource(support.weightSource,
                                                     support.hoistDataSource);
  support.hoistFunctionSource = ResolveHoistFieldDataSource(
      support.hoistFunctionSource, support.hoistDataSource);
  if (support.function.empty())
    support.function = support.hoistFunction;
}

// Reads legacy per-fixture category metadata from older Perastage MVR exports.
void ReadFixtureCategory(tinyxml2::XMLElement *fixtureNode, Fixture &fixture) {
  if (!fixtureNode)
    return;

  tinyxml2::XMLElement *ud = fixtureNode->FirstChildElement("UserData");
  if (!ud)
    return;

  for (tinyxml2::XMLElement *data = ud->FirstChildElement("Data"); data;
       data = data->NextSiblingElement("Data")) {
    tinyxml2::XMLElement *info = data->FirstChildElement("FixtureInfo");
    if (!info)
      continue;

    if (tinyxml2::XMLElement *categoryNode =
            info->FirstChildElement("Category")) {
      if (const char *txt = categoryNode->GetText())
        fixture.category = GdtfFixtureCategory::NormalizeCategory(Trim(txt));
    }

    if (tinyxml2::XMLElement *sourceNode =
            info->FirstChildElement("CategorySource")) {
      if (const char *txt = sourceNode->GetText())
        fixture.categorySource = Trim(txt);
    }
    if (tinyxml2::XMLElement *reasonNode =
            info->FirstChildElement("CategoryReason")) {
      if (const char *txt = reasonNode->GetText())
        fixture.categorySourceReason = Trim(txt);
    }

    if (!fixture.category.empty() && fixture.categorySource.empty())
      fixture.categorySource = GdtfFixtureCategory::kManualSource;
    if (fixture.categorySource == GdtfFixtureCategory::kManualSource)
      fixture.categorySourceReason.clear();
    return;
  }
}

// Reads legacy Perastage fixture identity fields used by older MVR exports.
SceneReadLegacyFixtureIdentity
ReadLegacyFixtureIdentity(tinyxml2::XMLElement *fixtureNode) {
  SceneReadLegacyFixtureIdentity identity;
  if (!fixtureNode)
    return identity;

  tinyxml2::XMLElement *ud = fixtureNode->FirstChildElement("UserData");
  if (!ud)
    return identity;

  for (tinyxml2::XMLElement *data = ud->FirstChildElement("Data"); data;
       data = data->NextSiblingElement("Data")) {
    tinyxml2::XMLElement *info = data->FirstChildElement("FixtureInfo");
    if (!info)
      continue;

    auto readText = [&](const char *name) -> std::string {
      if (tinyxml2::XMLElement *el = info->FirstChildElement(name)) {
        if (const char *txt = el->GetText())
          return Trim(txt);
      }
      return {};
    };

    identity.instanceName = readText("InstanceName");
    identity.stableId = readText("StableId");
    return identity;
  }
  return identity;
}

// Reads one Perastage hoist metadata element into a support.
void ReadSupportHoistInfo(tinyxml2::XMLElement *info, Support &support,
                          std::vector<MvrImportDiagnostic> &diagnostics) {
  if (!info)
    return;

  auto readFloat = [&](const char *name, float &out) {
    if (tinyxml2::XMLElement *el = info->FirstChildElement(name)) {
      if (const char *txt = el->GetText()) {
        float parsed = 0.0f;
        if (TryParseFloat(txt, parsed)) {
          out = parsed;
        } else {
          diagnostics.push_back({"invalid_hoist_numeric_field",
                                 "Support '" + support.uuid + "' has invalid " +
                                     name + " metadata."});
        }
      }
    }
  };
  auto readText = [&](const char *name) -> std::string {
    if (tinyxml2::XMLElement *el = info->FirstChildElement(name)) {
      if (const char *txt = el->GetText())
        return Trim(txt);
    }
    return {};
  };

  readFloat("Capacity", support.capacityKg);
  readFloat("Weight", support.weightKg);
  if (tinyxml2::XMLElement *load = info->FirstChildElement("Load")) {
    float parsed = 0.0f;
    if (load->GetText() && TryParseFloat(load->GetText(), parsed)) {
      support.loadKg = parsed;
      support.loadSource = "Manual";
    } else {
      diagnostics.push_back(
          {"invalid_hoist_numeric_field",
           "Support '" + support.uuid + "' has invalid Load metadata."});
    }
  }

  std::string hoistFunction = readText("RiggingPoint");
  if (hoistFunction.empty())
    hoistFunction = readText("Function");
  if (!hoistFunction.empty())
    support.hoistFunction = NormalizeHoistFunction(hoistFunction);

  const std::string motorName = readText("MotorName");
  if (!motorName.empty())
    support.motorName = motorName;
  const std::string manufacturer = readText("MotorManufacturer");
  if (!manufacturer.empty())
    support.motorManufacturer = manufacturer;
  const std::string model = readText("MotorModel");
  if (!model.empty())
    support.motorModel = model;
  const std::string fixtureUuid = readText("MotorFixtureUuid");
  if (!fixtureUuid.empty()) {
    const std::string canonicalFixtureUuid = CanonicalizeUuid(fixtureUuid);
    if (canonicalFixtureUuid.empty()) {
      diagnostics.push_back(
          {"invalid_motor_fixture_uuid",
           "Support '" + support.uuid + "' has a malformed MotorFixtureUuid."});
    } else {
      support.motorFixtureUuid = canonicalFixtureUuid;
    }
  }

  const std::string useDefaults = ToLowerCopy(readText("UseMotorDefaults"));
  if (!useDefaults.empty()) {
    if (useDefaults == "true" || useDefaults == "1" || useDefaults == "yes")
      support.useMotorDefaults = true;
    else if (useDefaults == "false" || useDefaults == "0" ||
             useDefaults == "no")
      support.useMotorDefaults = false;
    else
      diagnostics.push_back({"invalid_use_motor_defaults",
                             "Support '" + support.uuid +
                                 "' has invalid UseMotorDefaults metadata."});
  }

  const std::string dummyPreset = readText("DummyPreset");
  if (!dummyPreset.empty())
    support.dummyPreset = dummyPreset;
  const std::string dummyProfileId = readText("DummyProfileId");
  if (!dummyProfileId.empty())
    support.dummyProfileId = dummyProfileId;

  std::string source = readText("ValueSource");
  if (source.empty())
    source = readText("DataSource");
  if (!source.empty())
    support.hoistDataSource = NormalizeHoistDataSource(source);

  const std::string motorNameSource = readText("MotorNameSource");
  if (!motorNameSource.empty())
    support.motorNameSource = NormalizeHoistDataSource(motorNameSource);

  const std::string motorManufacturerSource =
      readText("MotorManufacturerSource");
  if (!motorManufacturerSource.empty()) {
    support.motorManufacturerSource =
        NormalizeHoistDataSource(motorManufacturerSource);
  }

  const std::string motorModelSource = readText("MotorModelSource");
  if (!motorModelSource.empty())
    support.motorModelSource = NormalizeHoistDataSource(motorModelSource);

  const std::string capacitySource = readText("CapacitySource");
  if (!capacitySource.empty())
    support.capacitySource = NormalizeHoistDataSource(capacitySource);

  const std::string weightSource = readText("WeightSource");
  if (!weightSource.empty())
    support.weightSource = NormalizeHoistDataSource(weightSource);

  std::string hoistFunctionSource = readText("RiggingPointSource");
  if (hoistFunctionSource.empty())
    hoistFunctionSource = readText("FunctionSource");
  if (!hoistFunctionSource.empty()) {
    support.hoistFunctionSource = NormalizeHoistDataSource(hoistFunctionSource);
  }
}

// Reads legacy Support/UserData hoist metadata into a support.
void ReadSupportHoistUserData(tinyxml2::XMLElement *supportNode,
                              Support &support,
                              std::vector<MvrImportDiagnostic> &diagnostics) {
  for (tinyxml2::XMLElement *ud = supportNode->FirstChildElement("UserData");
       ud; ud = ud->NextSiblingElement("UserData")) {
    for (tinyxml2::XMLElement *data = ud->FirstChildElement("Data"); data;
         data = data->NextSiblingElement("Data")) {
      const std::string provider = ToLowerCopy(
          Trim(data->Attribute("provider") ? data->Attribute("provider") : ""));
      if (provider != "perastage")
        continue;
      const std::string version =
          Trim(data->Attribute("ver") ? data->Attribute("ver") : "");
      if (version.empty()) {
        diagnostics.push_back(
            {"legacy_perastage_metadata_missing_version",
             "Accepted legacy Support metadata without a schema version."});
      } else if (version != "1.0") {
        diagnostics.push_back({"unsupported_perastage_metadata_version",
                               "Ignored legacy Support metadata with "
                               "unsupported schema version '" +
                                   version + "'."});
        continue;
      }
      tinyxml2::XMLElement *info = data->FirstChildElement("HoistInfo");
      if (!info)
        info = data->FirstChildElement("MotorInfo");
      ReadSupportHoistInfo(info, support, diagnostics);
    }
  }
}

} // namespace mvr::scene_reader_detail
