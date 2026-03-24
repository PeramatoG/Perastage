/*
 * This file is part of Perastage.
 * Copyright (C) 2025 Luisma Peramato
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
#include "truss_gdtf_builder.h"

#include "json.hpp"

#include <tinyxml2.h>
#include <wx/filename.h>
#include <wx/log.h>
#include <wx/wfstream.h>
#include <wx/zipstrm.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <memory>
#include <sstream>

namespace fs = std::filesystem;
using nlohmann::json;

namespace {

uint64_t Fnv1a64(const std::string &value, uint64_t seed) {
  uint64_t hash = 1469598103934665603ull ^ seed;
  for (unsigned char c : value) {
    hash ^= static_cast<uint64_t>(c);
    hash *= 1099511628211ull;
  }
  return hash;
}

std::string BytesToUuid(const std::array<uint8_t, 16> &bytes) {
  static constexpr char kHex[] = "0123456789abcdef";
  std::string out;
  out.reserve(36);
  for (size_t i = 0; i < bytes.size(); ++i) {
    if (i == 4 || i == 6 || i == 8 || i == 10)
      out.push_back('-');
    out.push_back(kHex[(bytes[i] >> 4) & 0x0F]);
    out.push_back(kHex[bytes[i] & 0x0F]);
  }
  return out;
}

std::string BuildDeterministicUuid(const std::string &seed) {
  const uint64_t a = Fnv1a64(seed, 0x9e3779b97f4a7c15ull);
  const uint64_t b = Fnv1a64(seed, 0xc2b2ae3d27d4eb4full);
  std::array<uint8_t, 16> bytes{};
  for (int i = 0; i < 8; ++i)
    bytes[i] = static_cast<uint8_t>((a >> (56 - i * 8)) & 0xFFu);
  for (int i = 0; i < 8; ++i)
    bytes[8 + i] = static_cast<uint8_t>((b >> (56 - i * 8)) & 0xFFu);
  bytes[6] = static_cast<uint8_t>((bytes[6] & 0x0Fu) | 0x50u);
  bytes[8] = static_cast<uint8_t>((bytes[8] & 0x3Fu) | 0x80u);
  return BytesToUuid(bytes);
}

struct TrussSourceData {
  std::string manufacturer;
  std::string model;
  float lengthMm = 0.0f;
  float widthMm = 0.0f;
  float heightMm = 0.0f;
  float weightKg = 0.0f;
  fs::path geometryPath;
  fs::path symbolPath;
  std::string typeKey;
};

static std::string Trim(std::string value) {
  auto isSpace = [](unsigned char c) { return std::isspace(c) != 0; };
  value.erase(value.begin(), std::find_if(value.begin(), value.end(),
                                          [&](unsigned char c) { return !isSpace(c); }));
  value.erase(std::find_if(value.rbegin(), value.rend(),
                           [&](unsigned char c) { return !isSpace(c); })
                  .base(),
              value.end());
  return value;
}

static std::string Slug(const std::string &input, const std::string &fallback) {
  std::string out;
  out.reserve(input.size());
  bool lastUnderscore = false;
  for (unsigned char c : input) {
    if (std::isalnum(c) != 0) {
      out.push_back(static_cast<char>(std::tolower(c)));
      lastUnderscore = false;
    } else if (!lastUnderscore) {
      out.push_back('_');
      lastUnderscore = true;
    }
  }
  while (!out.empty() && out.front() == '_')
    out.erase(out.begin());
  while (!out.empty() && out.back() == '_')
    out.pop_back();
  return out.empty() ? fallback : out;
}

static bool WriteZipFile(const fs::path &zipPath,
                         const std::vector<std::pair<std::string, fs::path>> &entries,
                         std::string *outError) {
  wxFileOutputStream output(zipPath.string());
  if (!output.IsOk()) {
    if (outError)
      *outError = "Failed to open destination file";
    return false;
  }

  wxZipOutputStream zip(output);
  for (const auto &[archiveName, sourcePath] : entries) {
    auto *entry = new wxZipEntry(archiveName);
    entry->SetMethod(wxZIP_METHOD_DEFLATE);
    zip.PutNextEntry(entry);
    std::ifstream in(sourcePath, std::ios::binary);
    if (!in.is_open()) {
      if (outError)
        *outError = "Failed to read source file: " + sourcePath.string();
      return false;
    }
    char buffer[4096];
    while (in.good()) {
      in.read(buffer, sizeof(buffer));
      std::streamsize read = in.gcount();
      if (read > 0)
        zip.Write(buffer, read);
    }
    zip.CloseEntry();
  }
  zip.Close();
  return true;
}

static bool ReadZipEntryToString(wxZipInputStream &zip, std::string &out) {
  out.clear();
  char buf[4096];
  while (true) {
    zip.Read(buf, sizeof(buf));
    size_t bytes = zip.LastRead();
    if (bytes == 0)
      break;
    out.append(buf, bytes);
  }
  return true;
}

static bool ReadLegacyGtruss(const fs::path &gtrussPath, TrussSourceData &out,
                             std::string *outError) {
  wxFileInputStream input(gtrussPath.string());
  if (!input.IsOk()) {
    if (outError)
      *outError = "Failed to open legacy truss archive";
    return false;
  }

  wxZipInputStream zip(input);
  std::unique_ptr<wxZipEntry> entry;
  std::string metadata;

  fs::path tempDir = fs::temp_directory_path() /
                     ("perastage-gtruss-" + Slug(gtrussPath.stem().string(), "truss"));
  std::error_code ec;
  fs::create_directories(tempDir, ec);

  std::string geometryName;
  std::string symbolName;

  while ((entry.reset(zip.GetNextEntry())), entry) {
    const std::string entryName = entry->GetName().ToStdString();
    if (entry->IsDir())
      continue;

    const fs::path entryPath = fs::path(entryName);
    const std::string ext = entryPath.extension().string();
    if (entryPath.filename() == "Truss.json") {
      ReadZipEntryToString(zip, metadata);
      continue;
    }

    if (ext == ".glb" || ext == ".3ds" || ext == ".svg") {
      fs::path target = tempDir / entryPath.filename();
      std::ofstream outFile(target, std::ios::binary);
      if (!outFile.is_open()) {
        if (outError)
          *outError = "Failed to extract embedded model";
        return false;
      }
      char buf[4096];
      while (true) {
        zip.Read(buf, sizeof(buf));
        size_t bytes = zip.LastRead();
        if (bytes == 0)
          break;
        outFile.write(buf, bytes);
      }
      if (ext == ".svg") {
        symbolName = target.string();
      } else {
        geometryName = target.string();
      }
    }
  }

  if (metadata.empty() || geometryName.empty()) {
    if (outError)
      *outError = "Legacy archive is missing Truss.json or geometry";
    return false;
  }

  json parsed = json::parse(metadata, nullptr, false);
  if (parsed.is_discarded()) {
    if (outError)
      *outError = "Invalid Truss.json metadata";
    return false;
  }

  out.manufacturer = parsed.value("Manufacturer", "");
  out.model = parsed.value("Model", parsed.value("Name", ""));
  out.lengthMm = parsed.value("Length_mm", 0.0f);
  out.widthMm = parsed.value("Width_mm", 0.0f);
  out.heightMm = parsed.value("Height_mm", 0.0f);
  out.weightKg = parsed.value("Weight_kg", 0.0f);
  out.geometryPath = geometryName;
  if (!symbolName.empty())
    out.symbolPath = symbolName;
  return true;
}

static std::string BuildStableFixtureTypeId(const TrussSourceData &data) {
  std::ostringstream seed;
  seed << "perastage-truss-type:v2|" << data.typeKey << '|'
       << Slug(data.manufacturer, "manufacturer") << '|'
       << Slug(data.model, "model") << '|'
       << Slug(data.geometryPath.generic_string(), "geometry") << '|'
       << Slug(data.symbolPath.generic_string(), "symbol") << '|'
       << std::fixed << std::setprecision(3) << data.lengthMm << '|'
       << std::fixed << std::setprecision(3) << data.widthMm << '|'
       << std::fixed << std::setprecision(3) << data.heightMm << '|'
       << std::fixed << std::setprecision(3) << data.weightKg;
  const std::string fixtureTypeId = BuildDeterministicUuid(seed.str());
  wxLogMessage("Truss GDTF FixtureTypeID generated typeKey='%s' fixtureTypeId='%s'",
               data.typeKey.c_str(), fixtureTypeId.c_str());
  return fixtureTypeId;
}

static std::string BuildDescriptionXml(const TrussSourceData &data) {
  tinyxml2::XMLDocument doc;
  auto *decl = doc.NewDeclaration("xml version=\"1.0\" encoding=\"UTF-8\"");
  doc.InsertEndChild(decl);

  auto *root = doc.NewElement("GDTF");
  root->SetAttribute("DataVersion", "1.2");
  doc.InsertEndChild(root);

  auto *fixtureType = doc.NewElement("FixtureType");
  const std::string fixtureName = data.model.empty() ? "Truss" : data.model;
  fixtureType->SetAttribute("Name", fixtureName.c_str());
  fixtureType->SetAttribute("ShortName", fixtureName.c_str());
  fixtureType->SetAttribute("Manufacturer", data.manufacturer.c_str());
  fixtureType->SetAttribute("FixtureTypeID", BuildStableFixtureTypeId(data).c_str());
  root->InsertEndChild(fixtureType);

  auto *attributes = doc.NewElement("AttributeDefinitions");
  auto *activationGroups = doc.NewElement("ActivationGroups");
  auto *featureGroups = doc.NewElement("FeatureGroups");
  auto *features = doc.NewElement("Features");
  auto *attributesNode = doc.NewElement("Attributes");
  attributes->InsertEndChild(activationGroups);
  attributes->InsertEndChild(featureGroups);
  attributes->InsertEndChild(features);
  attributes->InsertEndChild(attributesNode);
  fixtureType->InsertEndChild(attributes);

  auto *models = doc.NewElement("Models");
  auto *mainModel = doc.NewElement("Model");
  mainModel->SetAttribute("Name", "Main");
  mainModel->SetAttribute("Length", data.lengthMm / 1000.0f);
  mainModel->SetAttribute("Width", data.widthMm / 1000.0f);
  mainModel->SetAttribute("Height", data.heightMm / 1000.0f);
  mainModel->SetAttribute("PrimitiveType", "Base");
  mainModel->SetAttribute("File", "main");
  models->InsertEndChild(mainModel);
  fixtureType->InsertEndChild(models);

  auto *geometries = doc.NewElement("Geometries");
  auto *geometry = doc.NewElement("Geometry");
  geometry->SetAttribute("Name", "Root");
  geometry->SetAttribute("Model", "Main");
  geometries->InsertEndChild(geometry);
  fixtureType->InsertEndChild(geometries);

  auto *dmxModes = doc.NewElement("DMXModes");
  auto *mode = doc.NewElement("DMXMode");
  mode->SetAttribute("Name", "Default");
  mode->SetAttribute("Geometry", "Root");
  auto *dmxChannels = doc.NewElement("DMXChannels");
  mode->InsertEndChild(dmxChannels);
  auto *relations = doc.NewElement("Relations");
  mode->InsertEndChild(relations);
  auto *ftMacros = doc.NewElement("FTMacros");
  mode->InsertEndChild(ftMacros);
  dmxModes->InsertEndChild(mode);
  fixtureType->InsertEndChild(dmxModes);

  auto *physical = doc.NewElement("PhysicalDescriptions");
  auto *properties = doc.NewElement("Properties");
  auto *weight = doc.NewElement("Weight");
  weight->SetAttribute("Value", data.weightKg);
  properties->InsertEndChild(weight);
  physical->InsertEndChild(properties);
  fixtureType->InsertEndChild(physical);

  tinyxml2::XMLPrinter printer;
  doc.Print(&printer);
  return printer.CStr();
}

static bool BuildFromSourceData(const TrussSourceData &data,
                                const fs::path &outGdtfPath,
                                std::string *outError) {
  const std::string ext = data.geometryPath.extension().string();
  if (ext != ".glb" && ext != ".3ds") {
    if (outError)
      *outError = "Only .glb and .3ds truss geometry is supported";
    return false;
  }

  fs::path tmpDir = fs::temp_directory_path() /
                    ("perastage-truss-gdtf-" + Slug(outGdtfPath.stem().string(), "truss"));
  std::error_code ec;
  fs::remove_all(tmpDir, ec);
  fs::create_directories(tmpDir, ec);

  fs::path descPath = tmpDir / "description.xml";
  std::ofstream descOut(descPath);
  if (!descOut.is_open()) {
    if (outError)
      *outError = "Failed to write description.xml";
    return false;
  }
  descOut << BuildDescriptionXml(data);
  descOut.close();

  std::vector<std::pair<std::string, fs::path>> entries;
  entries.push_back({"description.xml", descPath});

  if (ext == ".glb") {
    entries.push_back({"models/gltf/main.glb", data.geometryPath});
  } else {
    entries.push_back({"models/3ds/main.3ds", data.geometryPath});
  }

  if (!data.symbolPath.empty() && data.symbolPath.extension() == ".svg")
    entries.push_back({"models/svg/main.svg", data.symbolPath});

  fs::create_directories(outGdtfPath.parent_path(), ec);
  return WriteZipFile(outGdtfPath, entries, outError);
}

} // namespace

bool ConvertLegacyGtrussToGdtf(const fs::path &gtrussPath,
                               const fs::path &outGdtfPath,
                               std::string *outError) {
  TrussSourceData source;
  if (!ReadLegacyGtruss(gtrussPath, source, outError))
    return false;
  return BuildFromSourceData(source, outGdtfPath, outError);
}

bool BuildTrussGdtfFromInstance(const Truss &truss, const fs::path &outGdtfPath,
                                std::string *outError) {
  TrussSourceData source;
  source.manufacturer = truss.manufacturer;
  source.model = truss.model.empty() ? truss.name : truss.model;
  source.lengthMm = truss.lengthMm;
  source.widthMm = truss.widthMm;
  source.heightMm = truss.heightMm;
  source.weightKg = truss.weightKg;
  source.typeKey = truss.perastageTypeKey;

  auto pickGeometry = [&](const std::string &path) {
    fs::path p = fs::u8path(path);
    std::string ext = p.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if ((ext == ".glb" || ext == ".3ds") && fs::exists(p)) {
      source.geometryPath = p;
      return true;
    }
    if (ext == ".svg" && fs::exists(p)) {
      source.symbolPath = p;
      return false;
    }
    return false;
  };

  if (!pickGeometry(truss.symbolFile))
    pickGeometry(truss.modelFile);

  if (source.geometryPath.empty()) {
    fs::path modelArchive = fs::u8path(truss.modelFile);
    std::string ext = modelArchive.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (ext == ".gtruss" && fs::exists(modelArchive))
      return ConvertLegacyGtrussToGdtf(modelArchive, outGdtfPath, outError);
  }

  if (source.geometryPath.empty()) {
    if (outError)
      *outError = "No truss geometry file found";
    return false;
  }

  return BuildFromSourceData(source, outGdtfPath, outError);
}
