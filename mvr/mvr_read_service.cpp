/*
 * This file is part of Perastage.
 * Copyright (C) 2026 Luisma Peramato
 *
 * Perastage is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#include "mvr_read_service.h"

#include "filesystem_path_utils.h"
#include "fixture_visual_color.h"
#include "gdtf_fixture_category.h"
#include "layer_service.h"
#include "matrixutils.h"
#include "mvr_import_reference_resolver.h"
#include "mvr_scene_node_reader.h"
#include "primitive_model_resources.h"
#include "uuidutils.h"

#include <tinyxml2.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <optional>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace {

enum class ReadLogLevel { Debug, Info, Warn, Error };

// Converts a UTF-8 filesystem string without changing its bytes.
std::string ToString(const std::u8string &value) {
  return {value.begin(), value.end()};
}

// Removes surrounding ASCII whitespace.
std::string Trim(const std::string &value) {
  const size_t first = value.find_first_not_of(" \t\r\n");
  return first == std::string::npos
             ? std::string{}
             : value.substr(first,
                            value.find_last_not_of(" \t\r\n") - first + 1);
}

// Lowercases ASCII text used for provider comparisons.
std::string ToLowerCopy(std::string value) {
  std::transform(
      value.begin(), value.end(), value.begin(),
      [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
  return value;
}

// Reports whether floating-point values are nearly equal by scale.
bool IsNearlyEqualRelative(float a, float b, float epsilon) {
  if (!std::isfinite(a) || !std::isfinite(b))
    return false;
  return std::fabs(a - b) <=
         epsilon * std::max({std::fabs(a), std::fabs(b), 1.0e-6f});
}

// Reports whether a matrix belongs to geometry content.
bool IsGeometryMatrixContext(const std::string &context) {
  return context.find("Geometry3D") != std::string::npos ||
         context == "Symbol" || context == "Truss/Geometry3D";
}

// Builds a stable child-geometry identity.
std::string
BuildSceneObjectGeometryInstanceKey(const std::string &parentUuid,
                                    const std::string &kind, size_t index,
                                    const std::string &symdef = {}) {
  std::ostringstream key;
  key << parentUuid << '/' << kind << '/' << index;
  if (!symdef.empty())
    key << '/' << symdef;
  return key.str();
}

// Converts a CIE xyY triplet to an RGB display color.
std::string CieToHex(const std::string &cie) {
  std::string t = cie;
  std::replace(t.begin(), t.end(), ',', ' ');
  std::stringstream ss(t);
  double x = 0.0, y = 0.0, Yv = 0.0;
  if (!(ss >> x >> y >> Yv) || y <= 0.0)
    return {};
  double X = x * (Yv / y);
  double Z = (1.0 - x - y) * (Yv / y);
  double r = 3.2406 * X - 1.5372 * Yv - 0.4986 * Z;
  double g = -0.9689 * X + 1.8758 * Yv + 0.0415 * Z;
  double b = 0.0557 * X - 0.2040 * Yv + 1.0570 * Z;
  auto gamma = [](double c) {
    c = std::max(0.0, c);
    return c <= 0.0031308 ? 12.92 * c : 1.055 * std::pow(c, 1.0 / 2.4) - 0.055;
  };
  r = gamma(r);
  g = gamma(g);
  b = gamma(b);
  r = std::clamp(r, 0.0, 1.0);
  g = std::clamp(g, 0.0, 1.0);
  b = std::clamp(b, 0.0, 1.0);
  int R = static_cast<int>(std::round(r * 255.0));
  int G = static_cast<int>(std::round(g * 255.0));
  int B = static_cast<int>(std::round(b * 255.0));
  std::ostringstream os;
  os << '#' << std::uppercase << std::hex << std::setfill('0') << std::setw(2)
     << R << std::setw(2) << G << std::setw(2) << B;
  return os.str();
}

// Discards optional application logging at the standalone read boundary.
void LogMessage(ReadLogLevel, const std::string &) {}

// Discards optional informational logging at the standalone read boundary.
void LogMessage(const std::string &) {}

// Applies package path remapping while retaining authored archive spelling.
std::string RemapPackagePath(const mvr::ImportPackage &package,
                             const std::string &path) {
  const std::string normalized = mvr::NormalizeImportArchivePath(path);
  const auto found = package.pathRemap.find(normalized);
  return found == package.pathRemap.end() ? path : found->second;
}

} // namespace

namespace mvr {

// Parses one acquired package without applying it to application state.
bool ReadAcquiredMvrPackage(const ImportPackage &package,
                            MvrImportResult &importResult,
                            const MvrImportOptions &options,
                            MvrReadProgressCallback progressCallback) {
  const std::string sceneXmlPath = PathUtils::PathToUtf8(package.sceneXmlPath);
  auto reportProgress = [&](std::string stage, int completed = 0,
                            int total = 0) {
    if (!progressCallback)
      return;
    progressCallback(std::move(stage), completed, total);
  };

  tinyxml2::XMLDocument doc;
  tinyxml2::XMLError result = doc.LoadFile(sceneXmlPath.c_str());
  if (result != tinyxml2::XML_SUCCESS) {
    LogMessage("Failed to load XML: " + sceneXmlPath);
    return false;
  }

  tinyxml2::XMLElement *root = doc.FirstChildElement("GeneralSceneDescription");
  if (!root) {
    LogMessage("Missing GeneralSceneDescription node");
    return false;
  }

  MvrScene &scene = importResult.scene;
  scene.Clear();
  mvr::MvrImportReferenceResolver referenceResolver(
      [](const std::string &message) {
        LogMessage(ReadLogLevel::Warn, message);
      });
  scene.basePath =
      ToString(PathUtils::PathFromUtf8(sceneXmlPath).parent_path().u8string());
  LogMessage(
      ReadLogLevel::Info,
      std::string("MVR import mode: source=") + "read-service" +
          ", promptConflicts=" + (options.promptConflicts ? "true" : "false") +
          ", applyDictionary=" + (options.applyDictionary ? "true" : "false") +
          ", basePath='" + scene.basePath + "'.");

  root->QueryIntAttribute("verMajor", &scene.versionMajor);
  root->QueryIntAttribute("verMinor", &scene.versionMinor);

  // Warn if the MVR file uses a newer version than we officially support.
  // The importer still attempts to parse the file so that documents with a
  // higher minor version (e.g. 1.5) remain usable.
  constexpr int SUPPORTED_MAJOR = 1;
  constexpr int SUPPORTED_MINOR = 6;
  if (scene.versionMajor != SUPPORTED_MAJOR ||
      scene.versionMinor > SUPPORTED_MINOR) {
    LogMessage("Warning: unsupported MVR version " +
               std::to_string(scene.versionMajor) + "." +
               std::to_string(scene.versionMinor) +
               ". Results may be incomplete.");
  }

  const char *provider = root->Attribute("provider");
  const char *version = root->Attribute("providerVersion");

  if (provider)
    scene.provider = provider;
  if (version)
    scene.providerVersion = version;

  int rootUserDataCount = 0;
  for (tinyxml2::XMLElement *userData = root->FirstChildElement("UserData");
       userData; userData = userData->NextSiblingElement("UserData"))
    ++rootUserDataCount;
  if (rootUserDataCount > 1) {
    importResult.diagnostics.push_back(
        {"multiple_root_userdata",
         "Ignored additional root UserData elements beyond the first."});
  }

  // Preserves validated foreign provider blocks without interpreting their
  // schema.
  if (tinyxml2::XMLElement *userData = root->FirstChildElement("UserData")) {
    for (tinyxml2::XMLElement *data = userData->FirstChildElement(); data;
         data = data->NextSiblingElement()) {
      if (std::string(data->Name()) != "Data") {
        importResult.diagnostics.push_back(
            {"invalid_root_userdata_child",
             "Ignored a root UserData child that is not a Data element."});
        continue;
      }
      const std::string dataProvider =
          Trim(data->Attribute("provider") ? data->Attribute("provider") : "");
      if (dataProvider.empty()) {
        importResult.diagnostics.push_back(
            {"missing_userdata_provider",
             "Ignored a root UserData Data block without a provider."});
        continue;
      }
      if (ToLowerCopy(dataProvider) == "perastage")
        continue;
      tinyxml2::XMLPrinter printer(nullptr, true);
      data->Accept(&printer);
      MvrOpaqueUserDataBlock block{
          dataProvider,
          Trim(data->Attribute("ver") ? data->Attribute("ver") : ""),
          printer.CStr()};
      if (std::find(scene.opaqueUserDataBlocks.begin(),
                    scene.opaqueUserDataBlocks.end(),
                    block) == scene.opaqueUserDataBlocks.end())
        scene.opaqueUserDataBlocks.push_back(std::move(block));
    }
  }

  tinyxml2::XMLElement *sceneNode = root->FirstChildElement("Scene");
  if (!sceneNode) {
    LogMessage("No Scene node found in GeneralSceneDescription");
    return true;
  }

  auto textOf = [](tinyxml2::XMLElement *parent,
                   const char *name) -> std::string {
    tinyxml2::XMLElement *n = parent->FirstChildElement(name);
    if (n && n->GetText())
      return Trim(n->GetText());
    return {};
  };

  auto intOf = [](tinyxml2::XMLElement *parent, const char *name, int &out) {
    tinyxml2::XMLElement *n = parent->FirstChildElement(name);
    if (n && n->GetText())
      out = std::atoi(n->GetText());
  };

  auto fixtureIdOf = [&](tinyxml2::XMLElement *parent, std::string &textOut,
                         int &numericOut) {
    textOut = textOf(parent, "FixtureID");
    intOf(parent, "FixtureIDNumeric", numericOut);
    if (numericOut <= 0 && !textOut.empty())
      numericOut = std::atoi(textOut.c_str());
  };

  std::unordered_map<std::string, std::string> perastageTypeToGdtfPath;
  std::unordered_map<std::string, std::string> perastageInstanceToTypeKey;
  auto parsePerastageManifest = [&](tinyxml2::XMLElement *userDataNode,
                                    const char *originLabel) {
    if (!userDataNode)
      return false;
    bool found = false;
    for (tinyxml2::XMLElement *data = userDataNode->FirstChildElement("Data");
         data; data = data->NextSiblingElement("Data")) {
      const std::string provider = ToLowerCopy(
          Trim(data->Attribute("provider") ? data->Attribute("provider") : ""));
      if (provider != "perastage")
        continue;
      if (tinyxml2::XMLElement *manifest =
              data->FirstChildElement("TrussSidecarManifest")) {
        found = true;
        for (tinyxml2::XMLElement *type = manifest->FirstChildElement("Type");
             type; type = type->NextSiblingElement("Type")) {
          const char *key = type->Attribute("key");
          const char *path = type->Attribute("gdtf");
          if (key && path)
            perastageTypeToGdtfPath[Trim(key)] =
                RemapPackagePath(package, path);
        }
        for (tinyxml2::XMLElement *inst =
                 manifest->FirstChildElement("Instance");
             inst; inst = inst->NextSiblingElement("Instance")) {
          const char *uuid = inst->Attribute("uuid");
          const char *key = inst->Attribute("typeKey");
          if (uuid && key)
            perastageInstanceToTypeKey[CanonicalizeUuid(Trim(uuid))] =
                Trim(key);
        }
      }
    }
    if (found) {
      LogMessage(
          ReadLogLevel::Info,
          std::string("MVR import loaded Perastage sidecar manifest from ") +
              originLabel);
    }
    return found;
  };

  const bool hasRootManifest = parsePerastageManifest(
      root->FirstChildElement("UserData"), "GeneralSceneDescription/UserData");
  if (!hasRootManifest &&
      parsePerastageManifest(sceneNode->FirstChildElement("UserData"),
                             "legacy Scene/UserData")) {
    LogMessage(ReadLogLevel::Warn, "MVR import used legacy Scene/UserData "
                                   "fallback for Perastage sidecar manifest");
  }

  std::unordered_map<std::string, std::string> layerColorByUuid;
  std::unordered_map<std::string, std::string> layerColorByName;
  auto isHexRgb = [](const std::string &color) {
    if (color.size() != 7 || color[0] != '#')
      return false;
    return std::all_of(color.begin() + 1, color.end(),
                       [](unsigned char ch) { return std::isxdigit(ch) != 0; });
  };
  auto parseLayerAppearanceMap = [&](tinyxml2::XMLElement *userDataNode) {
    if (!userDataNode)
      return;
    for (tinyxml2::XMLElement *data = userDataNode->FirstChildElement("Data");
         data; data = data->NextSiblingElement("Data")) {
      const std::string provider = ToLowerCopy(
          Trim(data->Attribute("provider") ? data->Attribute("provider") : ""));
      if (provider != "perastage")
        continue;
      for (tinyxml2::XMLElement *map =
               data->FirstChildElement("LayerAppearanceMap");
           map; map = map->NextSiblingElement("LayerAppearanceMap")) {
        auto parseAppearanceEntry = [&](tinyxml2::XMLElement *entry) {
          const std::string color =
              Trim(entry->Attribute("color") ? entry->Attribute("color") : "");
          if (!isHexRgb(color))
            return;
          const std::string uuid = CanonicalizeUuid(
              Trim(entry->Attribute("uuid") ? entry->Attribute("uuid") : ""));
          const std::string name =
              Trim(entry->Attribute("name") ? entry->Attribute("name") : "");
          if (!uuid.empty())
            layerColorByUuid[uuid] = color;
          if (!name.empty())
            layerColorByName[name] = color;
        };

        for (tinyxml2::XMLElement *entry =
                 map->FirstChildElement("PerastageLayerAppearance");
             entry;
             entry = entry->NextSiblingElement("PerastageLayerAppearance"))
          parseAppearanceEntry(entry);
        for (tinyxml2::XMLElement *entry = map->FirstChildElement("Layer");
             entry; entry = entry->NextSiblingElement("Layer"))
          parseAppearanceEntry(entry);
      }
    }
  };
  parseLayerAppearanceMap(root->FirstChildElement("UserData"));

  using RootFixtureTypeInfo = mvr::SceneReadFixtureTypeInfo;
  std::unordered_map<std::string, RootFixtureTypeInfo> rootFixtureTypeInfoByKey;

  // Builds the root UserData fixture type key used by Perastage exports.
  auto buildFixtureTypeInfoKey = [](const std::string &gdtfSpec,
                                    const std::string &gdtfMode,
                                    const std::string &typeName) {
    std::ostringstream key;
    key << Trim(gdtfSpec) << '|' << Trim(gdtfMode);
    if (Trim(gdtfSpec).empty())
      key << '|' << Trim(typeName);
    std::string value = key.str();
    for (char &ch : value) {
      const unsigned char uch = static_cast<unsigned char>(ch);
      if (uch < 32 || ch == '/' || ch == '\\')
        ch = '_';
    }
    return Trim(value);
  };

  // Collects root-level Perastage fixture type category metadata.
  auto parseRootFixtureTypeInfoMap = [&](tinyxml2::XMLElement *userDataNode) {
    if (!userDataNode)
      return;
    for (tinyxml2::XMLElement *data = userDataNode->FirstChildElement("Data");
         data; data = data->NextSiblingElement("Data")) {
      const std::string provider = ToLowerCopy(
          Trim(data->Attribute("provider") ? data->Attribute("provider") : ""));
      if (provider != "perastage")
        continue;
      for (tinyxml2::XMLElement *map =
               data->FirstChildElement("FixtureTypeInfoMap");
           map; map = map->NextSiblingElement("FixtureTypeInfoMap")) {
        for (tinyxml2::XMLElement *info =
                 map->FirstChildElement("FixtureTypeInfo");
             info; info = info->NextSiblingElement("FixtureTypeInfo")) {
          std::string key =
              Trim(info->Attribute("key") ? info->Attribute("key") : "");
          if (key.empty()) {
            key = buildFixtureTypeInfoKey(
                info->Attribute("gdtfSpec") ? info->Attribute("gdtfSpec") : "",
                info->Attribute("gdtfMode") ? info->Attribute("gdtfMode") : "",
                info->Attribute("model") ? info->Attribute("model") : "");
          }
          if (key.empty())
            continue;
          RootFixtureTypeInfo typeInfo;
          if (tinyxml2::XMLElement *category =
                  info->FirstChildElement("Category")) {
            if (const char *txt = category->GetText())
              typeInfo.category =
                  GdtfFixtureCategory::NormalizeCategory(Trim(txt));
          }
          if (tinyxml2::XMLElement *source =
                  info->FirstChildElement("CategorySource")) {
            if (const char *txt = source->GetText())
              typeInfo.categorySource = Trim(txt);
          }
          if (!typeInfo.category.empty() && typeInfo.categorySource.empty())
            typeInfo.categorySource = GdtfFixtureCategory::kManualSource;
          if (tinyxml2::XMLElement *visualColor =
                  info->FirstChildElement("VisualColor")) {
            if (const char *txt = visualColor->GetText()) {
              const std::string value = Trim(txt);
              if (isHexRgb(value))
                typeInfo.visualColorHex = value;
            }
          }
          if (!typeInfo.category.empty() || !typeInfo.visualColorHex.empty())
            rootFixtureTypeInfoByKey[key] = typeInfo;
        }
      }
    }
  };
  parseRootFixtureTypeInfoMap(root->FirstChildElement("UserData"));

  std::unordered_set<tinyxml2::XMLElement *> diagnosedUnsupportedData;
  auto isSupportedPerastageMetadata = [&](tinyxml2::XMLElement *data) {
    const std::string version =
        Trim(data->Attribute("ver") ? data->Attribute("ver") : "");
    if (version == "1.0")
      return true;
    if (diagnosedUnsupportedData.insert(data).second) {
      importResult.diagnostics.push_back(
          {"unsupported_perastage_metadata_version",
           "Ignored Perastage root metadata with unsupported schema version '" +
               version + "'."});
    }
    return false;
  };

  std::unordered_map<std::string, std::string> projectFixtureColorsByUuid;
  using ProjectFixtureIdentifiers = mvr::SceneReadFixtureIdentifiers;
  std::unordered_map<std::string, ProjectFixtureIdentifiers>
      projectFixtureIdentifiersByUuid;
  std::unordered_set<std::string> projectFixtureColorMetadataUuids;
  std::unordered_set<std::string> consumedProjectFixtureColorUuids;
  // Collects canonical Perastage fixture fidelity metadata for every import
  // mode.
  auto parseProjectFixtureMetadata = [&](tinyxml2::XMLElement *userDataNode) {
    if (!userDataNode)
      return;
    for (tinyxml2::XMLElement *data = userDataNode->FirstChildElement("Data");
         data; data = data->NextSiblingElement("Data")) {
      const std::string provider = ToLowerCopy(
          Trim(data->Attribute("provider") ? data->Attribute("provider") : ""));
      if (provider != "perastage" || !isSupportedPerastageMetadata(data))
        continue;
      for (tinyxml2::XMLElement *map =
               data->FirstChildElement("ProjectFixtureMetadataMap");
           map; map = map->NextSiblingElement("ProjectFixtureMetadataMap")) {
        const std::string schemaVersion = Trim(
            map->Attribute("schemaVersion") ? map->Attribute("schemaVersion")
                                            : "");
        if (schemaVersion != "1.0") {
          importResult.diagnostics.push_back(
              {"unsupported_project_fixture_metadata_version",
               "Ignored project fixture metadata with unsupported schema "
               "version '" +
                   schemaVersion + "'."});
          continue;
        }
        for (tinyxml2::XMLElement *entry =
                 map->FirstChildElement("ProjectFixtureMetadata");
             entry;
             entry = entry->NextSiblingElement("ProjectFixtureMetadata")) {
          const std::string rawUuid =
              Trim(entry->Attribute("uuid") ? entry->Attribute("uuid") : "");
          const std::string uuid = CanonicalizeUuid(rawUuid);
          if (uuid.empty()) {
            importResult.diagnostics.push_back(
                {"malformed_project_fixture_metadata_uuid",
                 "Ignored project fixture metadata with malformed UUID '" +
                     rawUuid + "'."});
            continue;
          }
          ProjectFixtureIdentifiers identifiers;
          const bool hasIdentifiers =
              entry->QueryIntAttribute("fixtureId", &identifiers.fixtureId) ==
                  tinyxml2::XML_SUCCESS &&
              entry->QueryIntAttribute("fixtureIdNumeric",
                                       &identifiers.fixtureIdNumeric) ==
                  tinyxml2::XML_SUCCESS &&
              entry->QueryIntAttribute("unitNumber", &identifiers.unitNumber) ==
                  tinyxml2::XML_SUCCESS &&
              entry->Attribute("fixtureIdText") != nullptr;
          if (hasIdentifiers) {
            identifiers.fixtureIdText = entry->Attribute("fixtureIdText");
            projectFixtureIdentifiersByUuid.emplace(uuid,
                                                    std::move(identifiers));
          }
          const bool hasColorMarker =
              entry->Attribute("hasVisualColorHex") != nullptr;
          const std::string hasColor =
              ToLowerCopy(Trim(entry->Attribute("hasVisualColorHex")
                                   ? entry->Attribute("hasVisualColorHex")
                                   : ""));
          const std::string color =
              Trim(entry->Attribute("visualColorHex")
                       ? entry->Attribute("visualColorHex")
                       : "");
          if (!hasColorMarker && color.empty())
            continue;
          projectFixtureColorMetadataUuids.insert(uuid);
          const bool explicitClear = hasColorMarker && hasColor == "false";
          const bool present =
              (!hasColorMarker || hasColor == "true") && isHexRgb(color);
          if (!explicitClear && !present) {
            importResult.diagnostics.push_back(
                {"invalid_project_fixture_visual_color",
                 "Ignored invalid project fixture visualColorHex for UUID '" +
                     uuid + "'."});
            continue;
          }
          const std::string storedColor = explicitClear ? std::string{} : color;
          if (!projectFixtureColorsByUuid.emplace(uuid, storedColor).second) {
            importResult.diagnostics.push_back(
                {"duplicate_project_fixture_metadata_uuid",
                 "Ignored duplicate project fixture metadata for UUID '" +
                     uuid + "'; the first entry takes precedence."});
          }
        }
      }
    }
  };
  parseProjectFixtureMetadata(root->FirstChildElement("UserData"));

  std::unordered_map<std::string, tinyxml2::XMLElement *> rootTrussInfoByUuid;
  std::unordered_set<std::string> consumedRootTrussInfoUuids;
  // Collects root-level Perastage truss metadata by canonical exported UUID.
  auto parseRootTrussInfoMap = [&](tinyxml2::XMLElement *userDataNode) {
    if (!userDataNode)
      return;
    for (tinyxml2::XMLElement *data = userDataNode->FirstChildElement("Data");
         data; data = data->NextSiblingElement("Data")) {
      const std::string provider = ToLowerCopy(
          Trim(data->Attribute("provider") ? data->Attribute("provider") : ""));
      if (provider != "perastage")
        continue;
      if (!isSupportedPerastageMetadata(data))
        continue;
      for (tinyxml2::XMLElement *map = data->FirstChildElement("TrussInfoMap");
           map; map = map->NextSiblingElement("TrussInfoMap")) {
        for (tinyxml2::XMLElement *info = map->FirstChildElement("TrussInfo");
             info; info = info->NextSiblingElement("TrussInfo")) {
          const std::string uuid = CanonicalizeUuid(
              Trim(info->Attribute("uuid") ? info->Attribute("uuid") : ""));
          if (uuid.empty()) {
            importResult.diagnostics.push_back(
                {"invalid_truss_info_uuid",
                 "Ignored TrussInfo with a malformed UUID."});
          } else if (!rootTrussInfoByUuid.emplace(uuid, info).second) {
            importResult.diagnostics.push_back(
                {"duplicate_truss_info",
                 "Ignored duplicate TrussInfo for UUID '" + uuid + "'."});
          }
        }
      }
    }
  };
  parseRootTrussInfoMap(root->FirstChildElement("UserData"));

  std::unordered_map<std::string, std::vector<std::string>>
      rootPrimitiveModelRefsBySceneObjectAndFile;
  // Collects root-level Perastage primitive geometry metadata by SceneObject
  // UUID and archive file name.
  auto parseRootPrimitiveGeometryMap = [&](tinyxml2::XMLElement *userDataNode) {
    if (!userDataNode)
      return;
    for (tinyxml2::XMLElement *data = userDataNode->FirstChildElement("Data");
         data; data = data->NextSiblingElement("Data")) {
      const std::string provider = ToLowerCopy(
          Trim(data->Attribute("provider") ? data->Attribute("provider") : ""));
      if (provider != "perastage")
        continue;
      for (tinyxml2::XMLElement *map =
               data->FirstChildElement("PrimitiveGeometryMap");
           map; map = map->NextSiblingElement("PrimitiveGeometryMap")) {
        for (tinyxml2::XMLElement *entry = map->FirstChildElement("Entry");
             entry; entry = entry->NextSiblingElement("Entry")) {
          const char *fileName = entry->Attribute("fileName");
          const char *modelRef = entry->Attribute("perastageModelRef");
          if (!fileName || !modelRef)
            continue;
          const std::string rawSceneObjectUuid =
              Trim(entry->Attribute("sceneObjectUuid")
                       ? entry->Attribute("sceneObjectUuid")
                       : "");
          const std::string canonicalSceneObjectUuid =
              CanonicalizeUuid(rawSceneObjectUuid);
          if (rawSceneObjectUuid.empty() && canonicalSceneObjectUuid.empty())
            continue;
          const std::string normalizedFileName = ToLowerCopy(Trim(fileName));
          if (!canonicalSceneObjectUuid.empty()) {
            const std::string key =
                canonicalSceneObjectUuid + "|" + normalizedFileName;
            rootPrimitiveModelRefsBySceneObjectAndFile[key].push_back(
                Trim(modelRef));
          }
          if (!rawSceneObjectUuid.empty() &&
              rawSceneObjectUuid != canonicalSceneObjectUuid) {
            const std::string key =
                rawSceneObjectUuid + "|" + normalizedFileName;
            rootPrimitiveModelRefsBySceneObjectAndFile[key].push_back(
                Trim(modelRef));
          }
        }
      }
    }
  };
  parseRootPrimitiveGeometryMap(root->FirstChildElement("UserData"));

  std::unordered_map<std::string, tinyxml2::XMLElement *> rootHoistInfoByUuid;
  std::unordered_set<std::string> consumedRootHoistInfoUuids;
  // Collects root-level Perastage hoist metadata by exported Support UUID.
  auto parseRootHoistInfoMap = [&](tinyxml2::XMLElement *userDataNode) {
    if (!userDataNode)
      return;
    for (tinyxml2::XMLElement *data = userDataNode->FirstChildElement("Data");
         data; data = data->NextSiblingElement("Data")) {
      const std::string provider = ToLowerCopy(
          Trim(data->Attribute("provider") ? data->Attribute("provider") : ""));
      if (provider != "perastage")
        continue;
      if (!isSupportedPerastageMetadata(data))
        continue;
      for (tinyxml2::XMLElement *map = data->FirstChildElement("HoistInfoMap");
           map; map = map->NextSiblingElement("HoistInfoMap")) {
        for (tinyxml2::XMLElement *info = map->FirstChildElement("HoistInfo");
             info; info = info->NextSiblingElement("HoistInfo")) {
          const std::string rawUuid =
              Trim(info->Attribute("uuid") ? info->Attribute("uuid") : "");
          const std::string canonicalUuid = CanonicalizeUuid(rawUuid);
          if (canonicalUuid.empty()) {
            importResult.diagnostics.push_back(
                {"invalid_hoist_info_uuid",
                 "Ignored HoistInfo with a malformed UUID."});
            continue;
          }
          if (!rootHoistInfoByUuid.emplace(canonicalUuid, info).second) {
            importResult.diagnostics.push_back(
                {"duplicate_hoist_info",
                 "Ignored duplicate HoistInfo for UUID '" + canonicalUuid +
                     "'."});
          }
        }
      }
    }
  };
  parseRootHoistInfoMap(root->FirstChildElement("UserData"));

  // ---- Parse AUXData for Symdefs and Positions ----
  if (tinyxml2::XMLElement *auxNode = sceneNode->FirstChildElement("AUXData")) {
    for (tinyxml2::XMLElement *pos = auxNode->FirstChildElement("Position");
         pos; pos = pos->NextSiblingElement("Position")) {
      const std::string rawUid =
          Trim(pos->Attribute("uuid") ? pos->Attribute("uuid") : "");
      const char *name = pos->Attribute("name");
      referenceResolver.ImportPosition(
          rawUid, name ? std::optional<std::string>{name} : std::nullopt,
          scene);
    }
    std::function<void(tinyxml2::XMLElement *, const Matrix &,
                       std::vector<SymdefGeometry> &)>
        parseSymdefChildList;
    parseSymdefChildList = [&](tinyxml2::XMLElement *childList,
                               const Matrix &parent,
                               std::vector<SymdefGeometry> &geometries) {
      for (tinyxml2::XMLElement *child =
               childList ? childList->FirstChildElement() : nullptr;
           child; child = child->NextSiblingElement()) {
        const char *name = child->Name();
        if (!name)
          continue;

        Matrix local = MatrixUtils::Identity();
        if (tinyxml2::XMLElement *matrix = child->FirstChildElement("Matrix")) {
          if (const char *txt = matrix->GetText()) {
            std::string raw = txt;
            if (!MatrixUtils::ParseMatrix(raw, local))
              local = MatrixUtils::Identity();
          }
        }
        Matrix composed = MatrixUtils::Multiply(parent, local);

        if (std::string(name) == "Geometry3D") {
          SymdefGeometry g;
          if (const char *fname = child->Attribute("fileName"))
            g.file = RemapPackagePath(package, fname);
          if (const char *type = child->Attribute("geometryType"))
            g.geometryType = Trim(type);
          g.transform = composed;
          if (!g.file.empty())
            geometries.push_back(std::move(g));
        }

        if (tinyxml2::XMLElement *inner = child->FirstChildElement("ChildList"))
          parseSymdefChildList(inner, composed, geometries);
      }
    };

    for (tinyxml2::XMLElement *sym = auxNode->FirstChildElement("Symdef"); sym;
         sym = sym->NextSiblingElement("Symdef")) {
      const char *uid = sym->Attribute("uuid");
      if (!uid)
        continue;

      if (const char *type = sym->Attribute("geometryType"))
        scene.symdefTypes[uid] = Trim(type);

      std::vector<SymdefGeometry> geometries;
      if (tinyxml2::XMLElement *childList = sym->FirstChildElement("ChildList"))
        parseSymdefChildList(childList, MatrixUtils::Identity(), geometries);

      if (!geometries.empty()) {
        scene.symdefGeometries[uid] = geometries;
        scene.symdefFiles[uid] = geometries.front().file;
        scene.symdefMatrices[uid] = geometries.front().transform;
        if (!geometries.front().geometryType.empty())
          scene.symdefTypes[uid] = geometries.front().geometryType;
      }
    }
  }

  constexpr float kTinyScaleMaxNorm = 0.01f;
  constexpr float kUniformScaleRelativeTolerance = 0.05f;
  constexpr float kMinOutlierNorm = 0.1f;
  constexpr float kMaxOutlierNorm = 10.0f;
  constexpr size_t kMaxSuspiciousExamples = 10;

  struct MatrixScaleAggregation {
    size_t acceptedTinyUniformScaleCount = 0;
    size_t suspiciousMatrixCount = 0;
    std::unordered_map<std::string, size_t> acceptedByContext;
    std::unordered_map<std::string, size_t> suspiciousByContext;
    std::vector<std::string> suspiciousExamples;
  } matrixScaleAggregation;

  auto parseMatrixOrIdentity = [&](tinyxml2::XMLElement *parent,
                                   const char *elementName,
                                   const std::string &contextTag, Matrix &out,
                                   bool inspectScale = false) {
    out = MatrixUtils::Identity();
    if (!parent)
      return;
    if (tinyxml2::XMLElement *matrix = parent->FirstChildElement(elementName)) {
      if (const char *txt = matrix->GetText()) {
        std::string raw = txt;
        if (!MatrixUtils::ParseMatrix(raw, out)) {
          LogMessage("Failed to parse matrix in " + contextTag + ": " + raw);
          out = MatrixUtils::Identity();
          return;
        }

        if (!inspectScale)
          return;

        auto norm = [](const std::array<float, 3> &v) {
          return std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
        };

        const float nu = norm(out.u);
        const float nv = norm(out.v);
        const float nw = norm(out.w);
        const float minNorm = std::min({nu, nv, nw});
        const float maxNorm = std::max({nu, nv, nw});

        const bool finiteNorms =
            std::isfinite(nu) && std::isfinite(nv) && std::isfinite(nw);
        const bool strictlyPositiveNorms = minNorm > 0.0f;
        const bool isUniformScale =
            IsNearlyEqualRelative(nu, nv, kUniformScaleRelativeTolerance) &&
            IsNearlyEqualRelative(nu, nw, kUniformScaleRelativeTolerance);
        const bool isTinyUniformGeometryScale =
            finiteNorms && strictlyPositiveNorms &&
            maxNorm <= kTinyScaleMaxNorm && isUniformScale &&
            IsGeometryMatrixContext(contextTag);
        if (isTinyUniformGeometryScale) {
          ++matrixScaleAggregation.acceptedTinyUniformScaleCount;
          ++matrixScaleAggregation.acceptedByContext[contextTag];
          return;
        }

        const bool hasInvalidNorm = !finiteNorms || !strictlyPositiveNorms;
        const bool hasOutlierNorm =
            minNorm < kMinOutlierNorm || maxNorm > kMaxOutlierNorm;
        if (!hasInvalidNorm && !hasOutlierNorm)
          return;

        ++matrixScaleAggregation.suspiciousMatrixCount;
        ++matrixScaleAggregation.suspiciousByContext[contextTag];
        if (matrixScaleAggregation.suspiciousExamples.size() <
            kMaxSuspiciousExamples) {
          std::ostringstream oss;
          oss << contextTag << " (|u|=" << nu << ", |v|=" << nv
              << ", |w|=" << nw << ")";
          matrixScaleAggregation.suspiciousExamples.push_back(oss.str());
        }
      }
    }
  };

  auto normalizeGeometryFileName = [](std::string fileName) {
    fileName = Trim(fileName);
    if (fileName.empty())
      return fileName;
    return ToString(PathUtils::PathFromUtf8(fileName).u8string());
  };

  std::unordered_map<std::string, GdtfFixtureCategory::InferenceResult>
      categoryInferenceByResolvedPath;
  auto normalizeAndResolveGeometryFileName = [&](const std::string &path) {
    return normalizeGeometryFileName(RemapPackagePath(package, path));
  };

  auto appendGeometryInstance =
      [&](std::vector<GeometryInstance> &instances, const std::string &fileName,
          const Matrix &localTransform, const std::string &instanceKey,
          const std::string &sourceSymbolUuid = {},
          const std::string &sourceSymdefUuid = {}) {
        std::string normalized = normalizeAndResolveGeometryFileName(fileName);
        if (normalized.empty())
          return;
        GeometryInstance instance;
        instance.modelFile = normalized;
        instance.instanceKey = instanceKey;
        instance.sourceSymbolUuid = sourceSymbolUuid;
        instance.sourceSymdefUuid = sourceSymdefUuid;
        instance.localTransform = localTransform;
        instances.push_back(std::move(instance));
      };

  auto resolveSymdefReference = [&](tinyxml2::XMLElement *symbol,
                                    std::vector<SymdefGeometry> &outGeometries,
                                    std::string &outGeometryType,
                                    Matrix &outSymbolMatrix) {
    outGeometries.clear();
    outGeometryType.clear();
    outSymbolMatrix = MatrixUtils::Identity();

    if (!symbol)
      return;

    parseMatrixOrIdentity(symbol, "Matrix", "Symbol", outSymbolMatrix);

    const char *symdef = symbol->Attribute("symdef");
    if (!symdef)
      return;

    auto geosIt = scene.symdefGeometries.find(symdef);
    if (geosIt != scene.symdefGeometries.end() && !geosIt->second.empty()) {
      outGeometries = geosIt->second;
      for (auto &geo : outGeometries)
        geo.file = normalizeGeometryFileName(geo.file);
      for (const auto &geo : outGeometries) {
        if (!geo.geometryType.empty()) {
          outGeometryType = geo.geometryType;
          break;
        }
      }
      return;
    }

    auto it = scene.symdefFiles.find(symdef);
    if (it != scene.symdefFiles.end()) {
      SymdefGeometry fallback;
      fallback.file = normalizeGeometryFileName(it->second);
      auto mit = scene.symdefMatrices.find(symdef);
      if (mit != scene.symdefMatrices.end())
        fallback.transform = mit->second;
      auto tit = scene.symdefTypes.find(symdef);
      if (tit != scene.symdefTypes.end())
        fallback.geometryType = tit->second;
      if (!fallback.file.empty())
        outGeometries.push_back(std::move(fallback));
    }

    auto tit = scene.symdefTypes.find(symdef);
    if (tit != scene.symdefTypes.end())
      outGeometryType = tit->second;
  };

  // ---- Helper lambdas for object parsing ----
  int preservedGroupObjectCount = 0;
  std::function<void(tinyxml2::XMLElement *, const std::string &,
                     const Matrix &, const std::string &)>
      parseChildList;

  auto ensurePositionEntry = [&](const std::string &positionId) {
    return referenceResolver.EnsurePosition(positionId, scene);
  };

  using CachedCategory = mvr::SceneReadCachedCategory;
  std::unordered_map<std::string, CachedCategory> categoryByTypeKey;
  auto resolveStableUuid = [&](const char *kind, tinyxml2::XMLElement *node,
                               const std::string &layerName,
                               const Matrix &nodeTransform,
                               const std::string &legacyStableId = {}) {
    const char *uuidAttr = node->Attribute("uuid");
    const char *nameAttr = node->Attribute("name");
    return referenceResolver.ResolveStableUuid(
        {kind, layerName, nameAttr ? Trim(nameAttr) : "",
         MatrixUtils::FormatMatrix(nodeTransform),
         uuidAttr ? Trim(uuidAttr) : "", Trim(legacyStableId)});
  };

  auto referenceUuidForNode = [&](const char *kind, tinyxml2::XMLElement *node,
                                  const std::string &layerName,
                                  const Matrix &nodeTransform) {
    const char *uuidAttr = node->Attribute("uuid");
    const char *nameAttr = node->Attribute("name");
    return referenceResolver.ReferenceUuid(
        {kind,
         layerName,
         nameAttr ? Trim(nameAttr) : "",
         MatrixUtils::FormatMatrix(nodeTransform),
         uuidAttr ? Trim(uuidAttr) : "",
         {}});
  };
  std::unordered_map<std::string, SceneReadGdtfConflict>
      pendingGdtfConflictByType;
  mvr::MvrSceneReadServices sceneReadServices{
      {[&](const std::string &path) { return RemapPackagePath(package, path); },
       [&](const std::string &path) {
         return NormalizeImportArchivePath(path);
       },
       [&](const std::string &path) {
         return NormalizeImportArchivePath(path);
       },
       [&](const std::string &path) {
         const fs::path candidate =
             package.rootPath /
             PathUtils::PathFromUtf8(NormalizeImportArchivePath(path));
         std::error_code error;
         return fs::is_regular_file(candidate, error) && !error
                    ? PathUtils::PathToUtf8(candidate)
                    : std::string{};
       },
       [](const std::string &) { return ImportGdtfMetadata{}; },
       [](const std::string &, const std::string &mode, std::optional<int>) {
         return mode;
       },
       [](const std::string &, const std::string &) { return -1; },
       [](const std::string &) {
         return std::optional<GdtfDictionary::Entry>{};
       },
       [](const std::string &, Truss &) { return false; },
       [&](const std::string &path) {
         return package.rootPath /
                PathUtils::PathFromUtf8(NormalizeImportArchivePath(path));
       },
       normalizeAndResolveGeometryFileName},
      {[](const std::filesystem::path &, std::string *) {
         return std::optional<GeometryBounds>{};
       },
       [](Truss &, bool) {}, [](MvrScene &) {},
       [](const std::string &) { return std::optional<std::string>{}; }},
      textOf,
      intOf,
      fixtureIdOf,
      parseMatrixOrIdentity,
      buildFixtureTypeInfoKey,
      resolveStableUuid,
      referenceUuidForNode,
      [&](const std::string &rawUuid, const std::string &resolvedUuid) {
        referenceResolver.RecordFixtureUuid(rawUuid, resolvedUuid);
      },
      ensurePositionEntry,
      resolveSymdefReference,
      appendGeometryInstance,
      reportProgress,
      [](const std::string &message) {
        LogMessage(ReadLogLevel::Debug, message);
      },
      [](const std::string &message) {
        LogMessage(ReadLogLevel::Info, message);
      },
      [](const std::string &message) {
        LogMessage(ReadLogLevel::Warn, message);
      },
      [](const std::string &message) {
        LogMessage(ReadLogLevel::Error, message);
      }};
  mvr::MvrSceneReadMetadata sceneReadMetadata{
      {rootFixtureTypeInfoByKey, projectFixtureIdentifiersByUuid,
       projectFixtureColorsByUuid, projectFixtureColorMetadataUuids},
      {rootTrussInfoByUuid, rootHoistInfoByUuid, perastageTypeToGdtfPath,
       perastageInstanceToTypeKey},
      {rootPrimitiveModelRefsBySceneObjectAndFile,
       referenceResolver.LegacyPositionRemap()},
      {layerColorByUuid, layerColorByName}};
  mvr::MvrSceneReadState sceneReadState{
      {pendingGdtfConflictByType, categoryByTypeKey,
       categoryInferenceByResolvedPath, consumedProjectFixtureColorUuids},
      {consumedRootTrussInfoUuids, consumedRootHoistInfoUuids}};
  mvr::MvrSceneReadMetrics sceneReadMetrics;
  mvr::ReadMvrSceneNodes(sceneNode, scene, importResult, options,
                         sceneReadServices, sceneReadMetadata, sceneReadState,
                         sceneReadMetrics);

  auto metadataUuids = [](const auto &entries) {
    std::unordered_set<std::string> uuids;
    for (const auto &[uuid, value] : entries) {
      (void)value;
      uuids.insert(uuid);
    }
    return uuids;
  };
  const auto trussInfoUuids = metadataUuids(rootTrussInfoByUuid);
  const auto hoistInfoUuids = metadataUuids(rootHoistInfoByUuid);
  const auto projectFixtureMetadataUuids =
      metadataUuids(projectFixtureColorsByUuid);
  referenceResolver.Reconcile(
      importResult, {trussInfoUuids, consumedRootTrussInfoUuids, hoistInfoUuids,
                     consumedRootHoistInfoUuids, projectFixtureMetadataUuids,
                     consumedProjectFixtureColorUuids});
  return true;
}

} // namespace mvr
