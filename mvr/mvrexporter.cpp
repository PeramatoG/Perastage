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
#include "mvrexporter.h"
#include "configmanager.h"
#include "matrixutils.h"

#include <wx/wfstream.h>
#include <wx/wx.h>
class wxZipStreamLink;
#include <wx/filename.h>
#include <wx/zipstrm.h>

#include <tinyxml2.h>

#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <unordered_map>
#include <chrono>

namespace fs = std::filesystem;

struct GdtfOverrides {
  std::string color;
  float weightKg = 0.0f;
  float powerW = 0.0f;
};

static std::pair<int, int> ParseAddress(const std::string &addr) {
  size_t dot = addr.find('.');
  if (dot == std::string::npos)
    return {0, 0};
  int u = 0, c = 0;
  try {
    u = std::stoi(addr.substr(0, dot));
  } catch (...) {
  }
  try {
    c = std::stoi(addr.substr(dot + 1));
  } catch (...) {
  }
  return {u, c};
}

static std::string HexToCie(const std::string &hex) {
  if (hex.size() != 7 || hex[0] != '#')
    return {};
  unsigned int rgb = 0;
  std::istringstream iss(hex.substr(1));
  iss >> std::hex >> rgb;
  unsigned int R = (rgb >> 16) & 0xFF;
  unsigned int G = (rgb >> 8) & 0xFF;
  unsigned int B = rgb & 0xFF;
  auto invGamma = [](double c) {
    return c <= 0.04045 ? c / 12.92
                        : std::pow((c + 0.055) / 1.055, 2.4);
  };
  double r = invGamma(R / 255.0);
  double g = invGamma(G / 255.0);
  double b = invGamma(B / 255.0);
  double X = 0.4124 * r + 0.3576 * g + 0.1805 * b;
  double Y = 0.2126 * r + 0.7152 * g + 0.0722 * b;
  double Z = 0.0193 * r + 0.1192 * g + 0.9505 * b;
  double sum = X + Y + Z;
  double x = 0.0, y = 0.0;
  if (sum > 0.0) {
    x = X / sum;
    y = Y / sum;
  }
  std::ostringstream colStr;
  colStr << std::fixed << std::setprecision(6) << x << "," << y << "," << Y;
  return colStr.str();
}

static std::string CreateTempDir() {
  auto now = std::chrono::system_clock::now().time_since_epoch().count();
  fs::path base = fs::temp_directory_path();
  fs::path full = base / ("GDTF_" + std::to_string(now));
  fs::create_directory(full);
  return full.string();
}

static bool ExtractZip(const std::string &zipPath, const std::string &destDir) {
  if (!fs::exists(zipPath))
    return false;
  wxLogNull logNo;
  wxFileInputStream input(zipPath);
  if (!input.IsOk())
    return false;
  wxZipInputStream zipStream(input);
  std::unique_ptr<wxZipEntry> entry;
  while ((entry.reset(zipStream.GetNextEntry())), entry) {
    std::string filename = entry->GetName().ToStdString();
    std::string fullPath = destDir + "/" + filename;
    if (entry->IsDir()) {
      wxFileName::Mkdir(fullPath, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL);
      continue;
    }
    wxFileName::Mkdir(wxFileName(fullPath).GetPath(), wxS_DIR_DEFAULT,
                      wxPATH_MKDIR_FULL);
    std::ofstream output(fullPath, std::ios::binary);
    if (!output.is_open())
      return false;
    char buffer[4096];
    while (true) {
      zipStream.Read(buffer, sizeof(buffer));
      size_t bytes = zipStream.LastRead();
      if (bytes == 0)
        break;
      output.write(buffer, bytes);
    }
    output.close();
  }
  return true;
}

static bool ZipDir(const std::string &srcDir, const std::string &dstZip) {
  wxFileOutputStream output(dstZip);
  if (!output.IsOk())
    return false;
  wxZipOutputStream zip(output);
  for (auto &p : fs::recursive_directory_iterator(srcDir)) {
    if (!p.is_regular_file())
      continue;
    fs::path rel = fs::relative(p.path(), srcDir);
    auto *e = new wxZipEntry(rel.generic_string());
    e->SetMethod(wxZIP_METHOD_DEFLATE);
    zip.PutNextEntry(e);
    std::ifstream in(p.path(), std::ios::binary);
    char buf[4096];
    while (in.good()) {
      in.read(buf, sizeof(buf));
      std::streamsize s = in.gcount();
      if (s > 0)
        zip.Write(buf, s);
    }
    zip.CloseEntry();
  }
  zip.Close();
  return true;
}

static std::string CreatePatchedGdtf(const std::string &gdtfPath,
                                     const GdtfOverrides &ov) {
  std::string tempDir = CreateTempDir();
  if (!ExtractZip(gdtfPath, tempDir))
    return {};
  std::string descPath = tempDir + "/description.xml";
  tinyxml2::XMLDocument doc;
  if (doc.LoadFile(descPath.c_str()) != tinyxml2::XML_SUCCESS)
    return {};
  tinyxml2::XMLElement *ft = doc.FirstChildElement("GDTF");
  if (ft)
    ft = ft->FirstChildElement("FixtureType");
  else
    ft = doc.FirstChildElement("FixtureType");
  if (!ft)
    return {};
  if (!ov.color.empty()) {
    tinyxml2::XMLElement *models = ft->FirstChildElement("Models");
    if (models) {
      std::string cie = HexToCie(ov.color);
      for (tinyxml2::XMLElement *m = models->FirstChildElement("Model"); m;
           m = m->NextSiblingElement("Model"))
        m->SetAttribute("Color", cie.c_str());
    }
  }
  if (ov.weightKg != 0.0f || ov.powerW != 0.0f) {
    tinyxml2::XMLElement *phys = ft->FirstChildElement("PhysicalDescriptions");
    if (!phys)
      phys = ft->InsertNewChildElement("PhysicalDescriptions");
    tinyxml2::XMLElement *props = phys->FirstChildElement("Properties");
    if (!props)
      props = phys->InsertNewChildElement("Properties");
    if (ov.weightKg != 0.0f) {
      tinyxml2::XMLElement *w = props->FirstChildElement("Weight");
      if (!w)
        w = props->InsertNewChildElement("Weight");
      w->SetAttribute("Value", ov.weightKg);
    }
    if (ov.powerW != 0.0f) {
      tinyxml2::XMLElement *p = props->FirstChildElement("PowerConsumption");
      if (!p)
        p = props->InsertNewChildElement("PowerConsumption");
      p->SetAttribute("Value", ov.powerW);
    }
  }
  doc.SaveFile(descPath.c_str());
  std::string outPath = tempDir + ".gdtf";
  if (!ZipDir(tempDir, outPath))
    return {};
  return outPath;
}

bool MvrExporter::ExportToFile(const std::string &filePath) {
  const auto &scene = ConfigManager::Get().GetScene();
  auto positions = scene.positions;

  auto ensurePositionEntry = [&](const std::string &positionId,
                                 const std::string &nameHint) {
    if (positionId.empty())
      return;

    auto it = positions.find(positionId);
    if (it == positions.end()) {
      positions[positionId] = nameHint;
    } else if (!nameHint.empty() && it->second != nameHint) {
      // Refresh the stored name so Hang Position edits are preserved on export.
      it->second = nameHint;
    }
  };

  for (const auto &[uid, fixture] : scene.fixtures)
    ensurePositionEntry(fixture.position, fixture.positionName);
  for (const auto &[uid, truss] : scene.trusses)
    ensurePositionEntry(truss.position, truss.positionName);

  wxFileOutputStream output(filePath);
  if (!output.IsOk())
    return false;

  wxZipOutputStream zip(output);

  std::set<std::string> resourceFiles;
  std::unordered_map<std::string, GdtfOverrides> gdtfOverrides;

  tinyxml2::XMLDocument doc;
  doc.InsertEndChild(doc.NewDeclaration());

  tinyxml2::XMLElement *root = doc.NewElement("GeneralSceneDescription");
  root->SetAttribute("verMajor", scene.versionMajor);
  root->SetAttribute("verMinor", scene.versionMinor);
  if (!scene.provider.empty())
    root->SetAttribute("provider", scene.provider.c_str());
  if (!scene.providerVersion.empty())
    root->SetAttribute("providerVersion", scene.providerVersion.c_str());
  doc.InsertEndChild(root);

  tinyxml2::XMLElement *sceneNode = doc.NewElement("Scene");
  root->InsertEndChild(sceneNode);

  // ---- AUXData ----
  tinyxml2::XMLElement *aux = doc.NewElement("AUXData");
  for (const auto &[uuid, name] : positions) {
    tinyxml2::XMLElement *pos = doc.NewElement("Position");
    pos->SetAttribute("uuid", uuid.c_str());
    if (!name.empty())
      pos->SetAttribute("name", name.c_str());
    aux->InsertEndChild(pos);
  }
  for (const auto &[uuid, file] : scene.symdefFiles) {
    tinyxml2::XMLElement *sym = doc.NewElement("Symdef");
    sym->SetAttribute("uuid", uuid.c_str());
    if (!file.empty()) {
      tinyxml2::XMLElement *cl = doc.NewElement("ChildList");
      tinyxml2::XMLElement *g3d = doc.NewElement("Geometry3D");
      std::string fname = fs::path(file).filename().generic_string();
      g3d->SetAttribute("fileName", fname.c_str());
      cl->InsertEndChild(g3d);
      sym->InsertEndChild(cl);
      resourceFiles.insert(file);
    }
    aux->InsertEndChild(sym);
  }
  if (aux->FirstChild())
    sceneNode->InsertEndChild(aux);

  // ---- Layers ----
  tinyxml2::XMLElement *layersNode = doc.NewElement("Layers");

  auto exportFixture = [&](tinyxml2::XMLElement *parent, const Fixture &f) {
    tinyxml2::XMLElement *fe = doc.NewElement("Fixture");
    fe->SetAttribute("uuid", f.uuid.c_str());
    if (!f.instanceName.empty())
      fe->SetAttribute("name", f.instanceName.c_str());

    auto addInt = [&](const char *n, int v) {
      if (v != 0) {
        tinyxml2::XMLElement *e = doc.NewElement(n);
        e->SetText(std::to_string(v).c_str());
        fe->InsertEndChild(e);
      }
    };
    auto addStr = [&](const char *n, const std::string &s) {
      if (!s.empty()) {
        tinyxml2::XMLElement *e = doc.NewElement(n);
        e->SetText(s.c_str());
        fe->InsertEndChild(e);
      }
    };
    auto addNum = [&](const char *n, float v, const char *unit) {
      if (v != 0.0f) {
        tinyxml2::XMLElement *e = doc.NewElement(n);
        e->SetAttribute("unit", unit);
        e->SetText(v);
        fe->InsertEndChild(e);
      }
    };

    addInt("FixtureID", f.fixtureId);
    addInt("FixtureIDNumeric", f.fixtureIdNumeric);
    addInt("UnitNumber", f.unitNumber);
    addInt("CustomId", f.customId);
    addInt("CustomIdType", f.customIdType);
    addStr("GDTFSpec", f.gdtfSpec);
    if (!f.gdtfSpec.empty())
      resourceFiles.insert(f.gdtfSpec);
    if (!f.gdtfSpec.empty() &&
        (!f.color.empty() || f.weightKg != 0.0f ||
         f.powerConsumptionW != 0.0f)) {
      auto &ov = gdtfOverrides[f.gdtfSpec];
      if (!f.color.empty())
        ov.color = f.color;
      if (f.weightKg != 0.0f)
        ov.weightKg = f.weightKg;
      if (f.powerConsumptionW != 0.0f)
        ov.powerW = f.powerConsumptionW;
    }
    addStr("GDTFMode", f.gdtfMode);
    addStr("Focus", f.focus);
    addStr("Function", f.function);
    if (!f.position.empty()) {
      addStr("Position", f.position);
    } else if (!f.positionName.empty()) {
      for (const auto &[puuid, pname] : positions) {
        if (pname == f.positionName) {
          addStr("Position", puuid);
          break;
        }
      }
    }

    addNum("PowerConsumption", f.powerConsumptionW, "W");
    addNum("Weight", f.weightKg, "kg");

    if (!f.color.empty() && f.color.size() == 7 && f.color[0] == '#') {
      std::string cie = HexToCie(f.color);
      tinyxml2::XMLElement *col = doc.NewElement("Color");
      col->SetText(cie.c_str());
      fe->InsertEndChild(col);
    }

    if (f.dmxInvertPan) {
      tinyxml2::XMLElement *e = doc.NewElement("DMXInvertPan");
      e->SetText("true");
      fe->InsertEndChild(e);
    }
    if (f.dmxInvertTilt) {
      tinyxml2::XMLElement *e = doc.NewElement("DMXInvertTilt");
      e->SetText("true");
      fe->InsertEndChild(e);
    }

    if (!f.address.empty()) {
      auto [u, c] = ParseAddress(f.address);
      int brk = (u > 0) ? u - 1 : 0;
      while (c > 512) {
        c -= 512;
        ++brk;
      }
      tinyxml2::XMLElement *addresses = doc.NewElement("Addresses");
      tinyxml2::XMLElement *addr = doc.NewElement("Address");
      addr->SetAttribute("break", brk);
      addr->SetText(std::to_string(c).c_str());
      addresses->InsertEndChild(addr);
      fe->InsertEndChild(addresses);
    }

    std::string mstr = MatrixUtils::FormatMatrix(f.transform);
    tinyxml2::XMLElement *mat = doc.NewElement("Matrix");
    mat->SetText(mstr.c_str());
    fe->InsertEndChild(mat);

    parent->InsertEndChild(fe);
  };

  auto exportTruss = [&](tinyxml2::XMLElement *parent, const Truss &t) {
    tinyxml2::XMLElement *te = doc.NewElement("Truss");
    te->SetAttribute("uuid", t.uuid.c_str());
    if (!t.name.empty())
      te->SetAttribute("name", t.name.c_str());

    auto addInt = [&](const char *n, int v) {
      if (v != 0) {
        tinyxml2::XMLElement *e = doc.NewElement(n);
        e->SetText(std::to_string(v).c_str());
        te->InsertEndChild(e);
      }
    };
    addInt("UnitNumber", t.unitNumber);
    addInt("CustomId", t.customId);
    addInt("CustomIdType", t.customIdType);

    if (!t.gdtfSpec.empty()) {
      tinyxml2::XMLElement *e = doc.NewElement("GDTFSpec");
      e->SetText(t.gdtfSpec.c_str());
      te->InsertEndChild(e);
      resourceFiles.insert(t.gdtfSpec);
    }
    if (!t.gdtfMode.empty()) {
      tinyxml2::XMLElement *e = doc.NewElement("GDTFMode");
      e->SetText(t.gdtfMode.c_str());
      te->InsertEndChild(e);
    }
    if (!t.function.empty()) {
      tinyxml2::XMLElement *e = doc.NewElement("Function");
      e->SetText(t.function.c_str());
      te->InsertEndChild(e);
    }
    if (!t.position.empty()) {
      tinyxml2::XMLElement *e = doc.NewElement("Position");
      e->SetText(t.position.c_str());
      te->InsertEndChild(e);
    } else if (!t.positionName.empty()) {
      for (const auto &[puuid, pname] : positions) {
        if (pname == t.positionName) {
          tinyxml2::XMLElement *e = doc.NewElement("Position");
          e->SetText(puuid.c_str());
          te->InsertEndChild(e);
          break;
        }
      }
    }

    if (!t.symbolFile.empty()) {
      tinyxml2::XMLElement *geos = doc.NewElement("Geometries");
      bool usedSym = false;
      for (const auto &[sUuid, file] : scene.symdefFiles) {
        if (file == t.symbolFile) {
          tinyxml2::XMLElement *sym = doc.NewElement("Symbol");
          sym->SetAttribute("symdef", sUuid.c_str());
          geos->InsertEndChild(sym);
          usedSym = true;
          break;
        }
      }
      if (!usedSym) {
        tinyxml2::XMLElement *g3d = doc.NewElement("Geometry3D");
        std::string fname = fs::path(t.symbolFile).filename().generic_string();
        g3d->SetAttribute("fileName", fname.c_str());
        geos->InsertEndChild(g3d);
      }
      te->InsertEndChild(geos);
      resourceFiles.insert(t.symbolFile);
    }
    if (!t.modelFile.empty()) {
      resourceFiles.insert(t.modelFile);
    }

    std::string mstr = MatrixUtils::FormatMatrix(t.transform);
    tinyxml2::XMLElement *mat = doc.NewElement("Matrix");
    mat->SetText(mstr.c_str());
    te->InsertEndChild(mat);

    bool hasMeta = !t.manufacturer.empty() || !t.model.empty() ||
                   t.lengthMm != 0.0f || t.widthMm != 0.0f ||
                   t.heightMm != 0.0f || t.weightKg != 0.0f ||
                   !t.crossSection.empty() || !t.modelFile.empty() ||
                   !t.positionName.empty();
    if (hasMeta) {
      tinyxml2::XMLElement *ud = doc.NewElement("UserData");
      tinyxml2::XMLElement *data = doc.NewElement("Data");
      data->SetAttribute("provider", "Perastage");
      data->SetAttribute("ver", "1.0");
      tinyxml2::XMLElement *info = doc.NewElement("TrussInfo");
      info->SetAttribute("uuid", t.uuid.c_str());
      auto addTxt = [&](const char *n, const std::string &v) {
        if (!v.empty()) {
          tinyxml2::XMLElement *e = doc.NewElement(n);
          e->SetText(v.c_str());
          info->InsertEndChild(e);
        }
      };
      auto addNum = [&](const char *n, float v, const char *unit) {
        if (v != 0.0f) {
          tinyxml2::XMLElement *e = doc.NewElement(n);
          e->SetAttribute("unit", unit);
          e->SetText(std::to_string(v).c_str());
          info->InsertEndChild(e);
        }
      };
      addTxt("Manufacturer", t.manufacturer);
      addTxt("Model", t.model);
      addNum("Length", t.lengthMm, "mm");
      addNum("Width", t.widthMm, "mm");
      addNum("Height", t.heightMm, "mm");
      addNum("Weight", t.weightKg, "kg");
      addTxt("CrossSection", t.crossSection);
      addTxt("ModelFile", t.modelFile);
      addTxt("HangPos", t.positionName);
      data->InsertEndChild(info);
      ud->InsertEndChild(data);
      te->InsertEndChild(ud);
    }

    parent->InsertEndChild(te);
  };

  auto exportSceneObject = [&](tinyxml2::XMLElement *parent,
                               const SceneObject &obj) {
    tinyxml2::XMLElement *oe = doc.NewElement("SceneObject");
    oe->SetAttribute("uuid", obj.uuid.c_str());
    if (!obj.name.empty())
      oe->SetAttribute("name", obj.name.c_str());

    if (!obj.modelFile.empty()) {
      tinyxml2::XMLElement *geos = doc.NewElement("Geometries");
      bool usedSym = false;
      for (const auto &[sUuid, file] : scene.symdefFiles) {
        if (file == obj.modelFile) {
          tinyxml2::XMLElement *sym = doc.NewElement("Symbol");
          sym->SetAttribute("symdef", sUuid.c_str());
          geos->InsertEndChild(sym);
          usedSym = true;
          break;
        }
      }
      if (!usedSym) {
        tinyxml2::XMLElement *g3d = doc.NewElement("Geometry3D");
        g3d->SetAttribute("fileName", obj.modelFile.c_str());
        geos->InsertEndChild(g3d);
      }
      oe->InsertEndChild(geos);
      resourceFiles.insert(obj.modelFile);
    }

    std::string mstr = MatrixUtils::FormatMatrix(obj.transform);
    tinyxml2::XMLElement *mat = doc.NewElement("Matrix");
    mat->SetText(mstr.c_str());
    oe->InsertEndChild(mat);

    parent->InsertEndChild(oe);
  };

  for (const auto &[layerUuid, layer] : scene.layers) {
    if (layer.name == DEFAULT_LAYER_NAME)
      continue;
    tinyxml2::XMLElement *layerElem = doc.NewElement("Layer");
    if (!layerUuid.empty())
      layerElem->SetAttribute("uuid", layerUuid.c_str());
    if (!layer.name.empty())
      layerElem->SetAttribute("name", layer.name.c_str());

    if (!layer.color.empty() && layer.color.size() == 7 &&
        layer.color[0] == '#') {
      std::string cie = HexToCie(layer.color);
      tinyxml2::XMLElement *col = doc.NewElement("Color");
      col->SetText(cie.c_str());
      layerElem->InsertEndChild(col);
    }

    tinyxml2::XMLElement *childList = doc.NewElement("ChildList");

    for (const auto &[uid, f] : scene.fixtures) {
      if (f.layer != layer.name)
        continue;
      exportFixture(childList, f);
    }

    for (const auto &[uid, t] : scene.trusses) {
      if (t.layer != layer.name)
        continue;
      exportTruss(childList, t);
    }

    for (const auto &[uid, obj] : scene.sceneObjects) {
      if (obj.layer != layer.name)
        continue;
      exportSceneObject(childList, obj);
    }

    if (childList->FirstChild())
      layerElem->InsertEndChild(childList);

    layersNode->InsertEndChild(layerElem);
  }

  // Objects with no layer
  tinyxml2::XMLElement *rootChildList = doc.NewElement("ChildList");
  for (const auto &[uid, f] : scene.fixtures) {
    if (f.layer == DEFAULT_LAYER_NAME || f.layer.empty())
      exportFixture(rootChildList, f);
  }
  for (const auto &[uid, t] : scene.trusses) {
    if (t.layer == DEFAULT_LAYER_NAME || t.layer.empty())
      exportTruss(rootChildList, t);
  }
  for (const auto &[uid, obj] : scene.sceneObjects) {
    if (obj.layer == DEFAULT_LAYER_NAME || obj.layer.empty())
      exportSceneObject(rootChildList, obj);
  }
  if (rootChildList->FirstChild())
    layersNode->InsertEndChild(rootChildList);

  sceneNode->InsertEndChild(layersNode);

  // Serialize XML
  tinyxml2::XMLPrinter printer;
  doc.Print(&printer);
  std::string xmlData = printer.CStr();

  // Store XML file inside zip
  {
    auto *entry = new wxZipEntry("GeneralSceneDescription.xml");
    entry->SetMethod(wxZIP_METHOD_DEFLATE);
    zip.PutNextEntry(entry);
    zip.Write(xmlData.c_str(), xmlData.size());
    zip.CloseEntry();
  }

  // Add referenced resource files
  for (const std::string &rel : resourceFiles) {
    fs::path src = rel;
    if (src.is_relative() && !scene.basePath.empty())
      src = fs::path(scene.basePath) / rel;
    if (!fs::exists(src))
      continue;

    auto cit = gdtfOverrides.find(rel);
    if (cit != gdtfOverrides.end()) {
      std::string tmp = CreatePatchedGdtf(src.string(), cit->second);
      if (!tmp.empty())
        src = tmp;
    }

    fs::path entryName =
        fs::path(rel).is_absolute() ? fs::path(rel).filename() : fs::path(rel);

    auto *e = new wxZipEntry(entryName.generic_string());
    e->SetMethod(wxZIP_METHOD_DEFLATE);
    zip.PutNextEntry(e);
    std::ifstream in(src, std::ios::binary);
    char buf[4096];
    while (in.good()) {
      in.read(buf, sizeof(buf));
      std::streamsize s = in.gcount();
      if (s > 0)
        zip.Write(buf, s);
    }
    zip.CloseEntry();
  }

  zip.Close();
  return true;
}
