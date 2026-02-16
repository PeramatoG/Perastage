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
#include "trussloader.h"

#include "truss_gdtf_builder.h"

#include <tinyxml2.h>
#include <wx/filename.h>
#include <wx/wfstream.h>
#include <wx/zipstrm.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <functional>
#include <fstream>
#include <memory>

namespace fs = std::filesystem;

namespace {

static std::string LowerExt(fs::path path) {
  std::string ext = path.extension().string();
  std::transform(ext.begin(), ext.end(), ext.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return ext;
}

static bool ParseFloatAttr(tinyxml2::XMLElement *node, const char *name, float &out) {
  if (!node)
    return false;
  float parsed = 0.0f;
  if (node->QueryFloatAttribute(name, &parsed) == tinyxml2::XML_SUCCESS) {
    out = parsed;
    return true;
  }
  return false;
}

static bool ExtractArchive(const fs::path &archivePath, const fs::path &destination) {
  wxFileInputStream input(archivePath.string());
  if (!input.IsOk())
    return false;

  wxZipInputStream zip(input);
  std::unique_ptr<wxZipEntry> entry;
  while ((entry.reset(zip.GetNextEntry())), entry) {
    fs::path entryPath = fs::path(entry->GetName().ToStdString()).lexically_normal();
    if (entryPath.empty())
      continue;
    fs::path target = destination / entryPath;
    if (entry->IsDir()) {
      wxFileName::Mkdir(target.string(), wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL);
      continue;
    }
    wxFileName::Mkdir(target.parent_path().string(), wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL);
    std::ofstream out(target, std::ios::binary);
    if (!out.is_open())
      return false;
    char buf[4096];
    while (true) {
      zip.Read(buf, sizeof(buf));
      size_t bytes = zip.LastRead();
      if (bytes == 0)
        break;
      out.write(buf, bytes);
    }
  }
  return true;
}


static fs::path BuildGdtfExtractionCacheDir(const fs::path &gdtfPath) {
  std::error_code ec;
  fs::path normalized = fs::weakly_canonical(gdtfPath, ec);
  if (ec)
    normalized = fs::absolute(gdtfPath, ec);
  if (ec)
    normalized = gdtfPath;

  std::string key = normalized.generic_string();
  std::error_code timeEc;
  const auto writeTime = fs::last_write_time(gdtfPath, timeEc);
  if (!timeEc)
    key += "#" + std::to_string(writeTime.time_since_epoch().count());

  const std::size_t hashValue = std::hash<std::string>{}(key);
  fs::path cacheRoot = fs::temp_directory_path() / "perastage-truss-gdtf-cache";
  return cacheRoot / std::to_string(hashValue);
}
static fs::path FindFirstExisting(const fs::path &base,
                                  std::initializer_list<fs::path> candidates) {
  for (const auto &candidate : candidates) {
    fs::path fullPath = base / candidate;
    if (fs::exists(fullPath))
      return fullPath;
  }
  return {};
}

} // namespace

bool LoadTrussGdtf(const std::string &gdtfPath, Truss &outTruss) {
  fs::path inputPath = fs::u8path(gdtfPath);
  if (!fs::exists(inputPath))
    return false;

  outTruss = Truss{};
  outTruss.modelFile = inputPath.string();
  outTruss.gdtfSpec = inputPath.string();

  fs::path baseDir = BuildGdtfExtractionCacheDir(inputPath);
  std::error_code ec;
  fs::create_directories(baseDir, ec);
  fs::path descPath = baseDir / "description.xml";
  if (!fs::exists(descPath)) {
    if (!ExtractArchive(inputPath, baseDir))
      return false;
  }

  if (!fs::exists(descPath))
    return false;

  tinyxml2::XMLDocument doc;
  if (doc.LoadFile(descPath.string().c_str()) != tinyxml2::XML_SUCCESS)
    return false;

  tinyxml2::XMLElement *fixtureType = doc.FirstChildElement("GDTF");
  fixtureType = fixtureType ? fixtureType->FirstChildElement("FixtureType")
                            : doc.FirstChildElement("FixtureType");
  if (!fixtureType)
    return false;

  if (const char *manufacturer = fixtureType->Attribute("Manufacturer"))
    outTruss.manufacturer = manufacturer;
  if (const char *name = fixtureType->Attribute("Name")) {
    outTruss.model = name;
    outTruss.name = name;
  }

  tinyxml2::XMLElement *models = fixtureType->FirstChildElement("Models");
  if (tinyxml2::XMLElement *model = models ? models->FirstChildElement("Model") : nullptr) {
    float lengthM = 0.0f;
    float widthM = 0.0f;
    float heightM = 0.0f;
    if (ParseFloatAttr(model, "Length", lengthM))
      outTruss.lengthMm = lengthM * 1000.0f;
    if (ParseFloatAttr(model, "Width", widthM))
      outTruss.widthMm = widthM * 1000.0f;
    if (ParseFloatAttr(model, "Height", heightM))
      outTruss.heightMm = heightM * 1000.0f;

    std::string fileBase = "main";
    if (const char *fileAttr = model->Attribute("File"); fileAttr && *fileAttr)
      fileBase = fileAttr;
    fs::path modelPath = FindFirstExisting(
        baseDir,
        {fs::path("models/gltf") / (fileBase + ".glb"),
         fs::path("models/3ds") / (fileBase + ".3ds")});
    if (!modelPath.empty())
      outTruss.symbolFile = modelPath.string();

    fs::path symbolPath = FindFirstExisting(baseDir, {fs::path("models/svg") / (fileBase + ".svg")});
    if (!symbolPath.empty() && outTruss.symbolFile.empty())
      outTruss.symbolFile = symbolPath.string();
  }

  tinyxml2::XMLElement *phys = fixtureType->FirstChildElement("PhysicalDescriptions");
  tinyxml2::XMLElement *props = phys ? phys->FirstChildElement("Properties") : nullptr;
  tinyxml2::XMLElement *weight = props ? props->FirstChildElement("Weight") : nullptr;
  if (weight)
    ParseFloatAttr(weight, "Value", outTruss.weightKg);

  tinyxml2::XMLElement *dmxModes = fixtureType->FirstChildElement("DMXModes");
  if (tinyxml2::XMLElement *mode = dmxModes ? dmxModes->FirstChildElement("DMXMode") : nullptr) {
    if (const char *modeName = mode->Attribute("Name"))
      outTruss.gdtfMode = modeName;
  }
  if (outTruss.gdtfMode.empty())
    outTruss.gdtfMode = "Default";

  return !outTruss.symbolFile.empty();
}

bool LoadTrussArchive(const std::string &archivePath, Truss &outTruss) {
  return LoadTrussDefinition(archivePath, outTruss);
}

bool LoadTrussDefinition(const std::string &path, Truss &outTruss) {
  fs::path inputPath = fs::u8path(path);
  std::string ext = LowerExt(inputPath);
  if (ext == ".gdtf")
    return LoadTrussGdtf(path, outTruss);

  if (ext == ".gtruss") {
    fs::path migratedPath = inputPath;
    migratedPath.replace_extension(".gdtf");
    std::string error;
    if (!fs::exists(migratedPath) &&
        !ConvertLegacyGtrussToGdtf(inputPath, migratedPath, &error)) {
      return false;
    }
    return LoadTrussGdtf(migratedPath.string(), outTruss);
  }

  outTruss = Truss{};
  outTruss.symbolFile = path;
  outTruss.modelFile = path;
  return true;
}
