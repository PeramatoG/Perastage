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

namespace fs = std::filesystem;

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

bool MvrExporter::ExportToFile(const std::string &filePath) {
  const auto &scene = ConfigManager::Get().GetScene();

  wxFileOutputStream output(filePath);
  if (!output.IsOk())
    return false;

  wxZipOutputStream zip(output);

  std::set<std::string> resourceFiles;

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
  for (const auto &[uuid, name] : scene.positions) {
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

    addInt("FixtureID", f.fixtureId);
    addInt("FixtureIDNumeric", f.fixtureIdNumeric);
    addInt("UnitNumber", f.unitNumber);
    addInt("CustomId", f.customId);
    addInt("CustomIdType", f.customIdType);
    addStr("GDTFSpec", f.gdtfSpec);
    if (!f.gdtfSpec.empty())
      resourceFiles.insert(f.gdtfSpec);
    addStr("GDTFMode", f.gdtfMode);
    addStr("Focus", f.focus);
    addStr("Function", f.function);
    addStr("Position", f.position);

    if (!f.color.empty() && f.color.size() == 7 && f.color[0] == '#') {
      unsigned int rgb = 0;
      std::istringstream iss(f.color.substr(1));
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
      tinyxml2::XMLElement *col = doc.NewElement("Color");
      col->SetText(colStr.str().c_str());
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
      for (const auto &[puuid, pname] : scene.positions) {
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
