#include "mvrimporter.h"
#include "configmanager.h"
#include "gdtfdictionary.h"
#include "gdtfloader.h"
#include "matrixutils.h"
#include "sceneobject.h"

#include "consolepanel.h"
#include "logger.h"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream> // Required for std::ofstream
#include <functional>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// TinyXML2
#include <tinyxml2.h>

// wxWidgets zip support
#include <wx/wfstream.h>
#include <wx/wx.h>
class wxZipStreamLink;
#include <wx/filename.h>
#include <wx/zipstrm.h>

namespace fs = std::filesystem;

// Helper to convert between std::u8string and std::string without
// losing the underlying UTF-8 byte sequence.
static std::string ToString(const std::u8string &s) {
  return std::string(s.begin(), s.end());
}

static std::string Trim(const std::string &s) {
  const char *ws = " \t\r\n";
  size_t start = s.find_first_not_of(ws);
  if (start == std::string::npos)
    return {};
  size_t end = s.find_last_not_of(ws);
  return s.substr(start, end - start + 1);
}

static std::string CieToHex(const std::string &cie) {
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
    return c <= 0.0031308 ? 12.92 * c
                          : 1.055 * std::pow(c, 1.0 / 2.4) - 0.055;
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
  os << '#' << std::uppercase << std::hex << std::setfill('0')
     << std::setw(2) << R << std::setw(2) << G << std::setw(2) << B;
  return os.str();
}

// Helper to log errors both to stderr and the application's console panel.
// Log a message to both the log file and the application's console panel.
// Console updates are queued to the GUI thread to avoid blocking.
static void LogMessage(const std::string &msg) {
  Logger::Instance().Log(msg);
  if (ConsolePanel::Instance() && wxTheApp) {
    wxString wmsg = wxString::FromUTF8(msg.c_str());
    wxTheApp->CallAfter([wmsg]() {
      if (ConsolePanel::Instance())
        ConsolePanel::Instance()->AppendMessage(wmsg);
    });
  }
}

struct GdtfConflict {
  std::string type;
  std::string mvrPath;
  std::string appPath;
};

static std::unordered_map<std::string, std::string>
PromptGdtfConflicts(const std::vector<GdtfConflict> &conflicts) {
  std::unordered_map<std::string, std::string> chosen;
  if (conflicts.empty())
    return chosen;

  wxDialog dlg(nullptr, wxID_ANY, "GDTF conflicts");
  wxBoxSizer *topSizer = new wxBoxSizer(wxVERTICAL);
  wxFlexGridSizer *grid = new wxFlexGridSizer(3, 5, 5);
  grid->Add(new wxStaticText(&dlg, wxID_ANY, "Type"));
  grid->Add(new wxStaticText(&dlg, wxID_ANY, "MVR"));
  grid->Add(new wxStaticText(&dlg, wxID_ANY, "App"));

  std::vector<wxRadioButton *> mvrBtns;
  std::vector<wxRadioButton *> appBtns;
  for (const auto &c : conflicts) {
    grid->Add(
        new wxStaticText(&dlg, wxID_ANY, wxString::FromUTF8(c.type.c_str())));
    wxRadioButton *mvr = new wxRadioButton(
        &dlg, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
    wxRadioButton *app = new wxRadioButton(&dlg, wxID_ANY, "");
    mvr->SetValue(true);
    grid->Add(mvr, 0, wxALIGN_CENTER);
    grid->Add(app, 0, wxALIGN_CENTER);
    mvrBtns.push_back(mvr);
    appBtns.push_back(app);
  }

  topSizer->Add(grid, 1, wxALL, 10);
  topSizer->Add(dlg.CreateSeparatedButtonSizer(wxOK | wxCANCEL), 0,
                wxEXPAND | wxALL, 10);
  dlg.SetSizerAndFit(topSizer);

  if (dlg.ShowModal() != wxID_OK)
    return chosen;

  for (size_t i = 0; i < conflicts.size(); ++i) {
    const auto &c = conflicts[i];
    chosen[c.type] = mvrBtns[i]->GetValue() ? c.mvrPath : c.appPath;
  }
  return chosen;
}

bool MvrImporter::ImportFromFile(const std::string &filePath,
                                 bool promptConflicts,
                                 bool applyDictionary) {
  // Treat the incoming path as UTF-8 to preserve any non-ASCII characters
  fs::path path = fs::u8path(filePath);

  std::string ext = path.extension().string();
  std::transform(ext.begin(), ext.end(), ext.begin(),
                 [](unsigned char c) { return std::tolower(c); });

  if (!fs::exists(path)) {
    LogMessage("MVR file does not exist: " + filePath);
    return false;
  }
  if (ext != ".mvr") {
    LogMessage("MVR file has invalid extension: " + ext);
    return false;
  }

  std::string tempDir = CreateTemporaryDirectory();
  std::string mvrPath = ToString(path.u8string());
  fs::path tempPath(tempDir);
  if (!ExtractMvrZip(mvrPath, tempDir)) {
    LogMessage("Failed to extract MVR file.");
    return false;
  }

  fs::path sceneFile = tempPath / "GeneralSceneDescription.xml";
  if (!fs::exists(sceneFile)) {
    // Some MVR packages may store the file with a different case.
    std::string target = "generalscenedescription.xml";
    for (const auto &entry : fs::directory_iterator(tempPath)) {
      if (entry.is_regular_file()) {
        std::string name = entry.path().filename().string();
        std::string lower = name;
        std::transform(lower.begin(), lower.end(), lower.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        if (lower == target) {
          sceneFile = entry.path();
          break;
        }
      }
    }
  }
  if (!fs::exists(sceneFile)) {
    LogMessage("Missing GeneralSceneDescription.xml in MVR.");
    return false;
  }

  std::string scenePath = ToString(sceneFile.u8string());
  return ParseSceneXml(scenePath, promptConflicts, applyDictionary);
}

std::string MvrImporter::CreateTemporaryDirectory() {
  auto now = std::chrono::system_clock::now().time_since_epoch().count();
  std::string folderName = "Perastage_" + std::to_string(now);

  fs::path tempBase = fs::temp_directory_path();
  fs::path fullPath = tempBase / folderName;

  fs::create_directory(fullPath);
  // Return the path encoded as UTF-8 so it can safely be converted back
  // using fs::u8path or passed to wxWidgets APIs expecting UTF-8 strings.
  return ToString(fullPath.u8string());
}

bool MvrImporter::ExtractMvrZip(const std::string &mvrPath,
                                const std::string &destDir) {
  wxFileInputStream input(wxString::FromUTF8(mvrPath.c_str()));
  if (!input.IsOk()) {
    LogMessage("Failed to open MVR file.");
    return false;
  }

  wxZipInputStream zipStream(input);
  std::unique_ptr<wxZipEntry> entry;

  while ((entry.reset(zipStream.GetNextEntry())), entry) {
    // Extract entry names using UTF-8 to preserve special characters
    std::string filename = entry->GetName().ToUTF8().data();
    fs::path fullPath = fs::u8path(destDir) / fs::u8path(filename);

    if (entry->IsDir()) {
      std::string dirUtf8 = ToString(fullPath.u8string());
      wxFileName::Mkdir(wxString::FromUTF8(dirUtf8.c_str()), wxS_DIR_DEFAULT,
                        wxPATH_MKDIR_FULL);
      continue;
    }

    std::string parentUtf8 = ToString(fullPath.parent_path().u8string());
    wxFileName::Mkdir(wxString::FromUTF8(parentUtf8.c_str()), wxS_DIR_DEFAULT,
                      wxPATH_MKDIR_FULL);

    std::ofstream output(fullPath, std::ios::binary);
    if (!output.is_open()) {
      LogMessage("Cannot create file: " + ToString(fullPath.u8string()));
      return false;
    }

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

// Parses GeneralSceneDescription.xml and populates fixtures and trusses into
// the scene model
bool MvrImporter::ParseSceneXml(const std::string &sceneXmlPath,
                                bool promptConflicts,
                                bool applyDictionary) {
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

  ConfigManager::Get().Reset();
  auto &scene = ConfigManager::Get().GetScene();
  scene.basePath = ToString(fs::u8path(sceneXmlPath).parent_path().u8string());

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

  // ---- Parse AUXData for Symdefs and Positions ----
  if (tinyxml2::XMLElement *auxNode = sceneNode->FirstChildElement("AUXData")) {
    for (tinyxml2::XMLElement *pos = auxNode->FirstChildElement("Position");
         pos; pos = pos->NextSiblingElement("Position")) {
      const char *uid = pos->Attribute("uuid");
      const char *name = pos->Attribute("name");
      if (uid)
        scene.positions[uid] = name ? name : "";
    }

    for (tinyxml2::XMLElement *sym = auxNode->FirstChildElement("Symdef"); sym;
         sym = sym->NextSiblingElement("Symdef")) {
      const char *uid = sym->Attribute("uuid");
      if (!uid)
        continue;
      std::string file;
      if (tinyxml2::XMLElement *childList =
              sym->FirstChildElement("ChildList")) {
        if (tinyxml2::XMLElement *geo =
                childList->FirstChildElement("Geometry3D")) {
          const char *fname = geo->Attribute("fileName");
          if (fname)
            file = fname;
        }
      }
      if (!file.empty())
        scene.symdefFiles[uid] = file;
    }
  }

  // ---- Helper lambdas for object parsing ----
  std::function<void(tinyxml2::XMLElement *, const std::string &)>
      parseChildList;

  std::function<void(tinyxml2::XMLElement *, const std::string &)>
      parseFixture = [&](tinyxml2::XMLElement *node,
                         const std::string &layerName) {
        const char *uuidAttr = node->Attribute("uuid");
        if (!uuidAttr)
          return;

        Fixture fixture;
        fixture.uuid = uuidAttr;
        fixture.layer = layerName;

        if (const char *nameAttr = node->Attribute("name"))
          fixture.instanceName = nameAttr;

        intOf(node, "FixtureID", fixture.fixtureId);
        intOf(node, "FixtureIDNumeric", fixture.fixtureIdNumeric);
        intOf(node, "UnitNumber", fixture.unitNumber);
        intOf(node, "CustomId", fixture.customId);
        intOf(node, "CustomIdType", fixture.customIdType);

        fixture.gdtfSpec = textOf(node, "GDTFSpec");
        fixture.gdtfMode = textOf(node, "GDTFMode");
        fixture.focus = textOf(node, "Focus");
        fixture.function = textOf(node, "Function");
        fixture.position = textOf(node, "Position");
        if (tinyxml2::XMLElement *colorNode =
                node->FirstChildElement("Color")) {
          if (const char *txt = colorNode->GetText())
            fixture.color = CieToHex(txt);
        }
        if (tinyxml2::XMLElement *pcNode =
                node->FirstChildElement("PowerConsumption")) {
          if (const char *txt = pcNode->GetText())
            fixture.powerConsumptionW = std::stof(txt);
        }
        if (tinyxml2::XMLElement *wNode =
                node->FirstChildElement("Weight")) {
          if (const char *txt = wNode->GetText())
            fixture.weightKg = std::stof(txt);
        }
        if (!fixture.gdtfSpec.empty()) {
          fs::path p =
              scene.basePath.empty()
                  ? fs::u8path(fixture.gdtfSpec)
                  : fs::u8path(scene.basePath) / fs::u8path(fixture.gdtfSpec);
          std::string gdtfPath = ToString(p.u8string());
          fixture.typeName = Trim(GetGdtfFixtureName(gdtfPath));
        if (!fixture.typeName.empty())
          fixture.gdtfSpec = gdtfPath;
        if (fixture.color.empty())
          fixture.color = GetGdtfModelColor(gdtfPath);
        float gw = 0.0f, gp = 0.0f;
        if (GetGdtfProperties(gdtfPath, gw, gp)) {
          if (fixture.weightKg == 0.0f)
            fixture.weightKg = gw;
          if (fixture.powerConsumptionW == 0.0f)
            fixture.powerConsumptionW = gp;
        }
      }
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
          if (const char *txt = matrix->GetText()) {
            fixture.matrixRaw = txt;
            MatrixUtils::ParseMatrix(fixture.matrixRaw, fixture.transform);
          }
        }

        scene.fixtures[fixture.uuid] = fixture;
      };

  std::function<void(tinyxml2::XMLElement *, const std::string &)> parseTruss =
      [&](tinyxml2::XMLElement *node, const std::string &layerName) {
        const char *uuidAttr = node->Attribute("uuid");
        if (!uuidAttr)
          return;

        Truss truss;
        truss.uuid = uuidAttr;
        truss.layer = layerName;
        if (const char *nameAttr = node->Attribute("name"))
          truss.name = nameAttr;

        intOf(node, "UnitNumber", truss.unitNumber);
        intOf(node, "CustomId", truss.customId);
        intOf(node, "CustomIdType", truss.customIdType);

        truss.gdtfSpec = textOf(node, "GDTFSpec");
        truss.gdtfMode = textOf(node, "GDTFMode");
        truss.function = textOf(node, "Function");
        truss.position = textOf(node, "Position");
        auto posItT = scene.positions.find(truss.position);
        if (posItT != scene.positions.end())
          truss.positionName = posItT->second;

        if (tinyxml2::XMLElement *geos =
                node->FirstChildElement("Geometries")) {
          if (tinyxml2::XMLElement *g3d =
                  geos->FirstChildElement("Geometry3D")) {
            const char *file = g3d->Attribute("fileName");
            if (file)
              truss.symbolFile = file;
          } else if (tinyxml2::XMLElement *sym =
                         geos->FirstChildElement("Symbol")) {
            const char *symdef = sym->Attribute("symdef");
            if (symdef) {
              auto it = scene.symdefFiles.find(symdef);
              if (it != scene.symdefFiles.end())
                truss.symbolFile = it->second;
            }
          }
        }

        if (tinyxml2::XMLElement *matrix = node->FirstChildElement("Matrix")) {
          if (const char *txt = matrix->GetText()) {
            std::string raw = txt;
            MatrixUtils::ParseMatrix(raw, truss.transform);
          }
        }

        if (tinyxml2::XMLElement *ud = node->FirstChildElement("UserData")) {
          for (tinyxml2::XMLElement *data = ud->FirstChildElement("Data"); data;
               data = data->NextSiblingElement("Data")) {
            if (tinyxml2::XMLElement *info =
                    data->FirstChildElement("TrussInfo")) {
              if (tinyxml2::XMLElement *m =
                      info->FirstChildElement("Manufacturer"))
                if (m->GetText())
                  truss.manufacturer = Trim(m->GetText());
              if (tinyxml2::XMLElement *mo = info->FirstChildElement("Model"))
                if (mo->GetText())
                  truss.model = Trim(mo->GetText());
              if (tinyxml2::XMLElement *len = info->FirstChildElement("Length"))
                if (len->GetText())
                  truss.lengthMm = std::stof(len->GetText());
              if (tinyxml2::XMLElement *wid = info->FirstChildElement("Width"))
                if (wid->GetText())
                  truss.widthMm = std::stof(wid->GetText());
              if (tinyxml2::XMLElement *hei = info->FirstChildElement("Height"))
                if (hei->GetText())
                  truss.heightMm = std::stof(hei->GetText());
              if (tinyxml2::XMLElement *wei = info->FirstChildElement("Weight"))
                if (wei->GetText())
                  truss.weightKg = std::stof(wei->GetText());
              if (tinyxml2::XMLElement *cs =
                      info->FirstChildElement("CrossSection"))
                if (cs->GetText())
                  truss.crossSection = Trim(cs->GetText());
              if (tinyxml2::XMLElement *mf =
                      info->FirstChildElement("ModelFile"))
                if (mf->GetText())
                  truss.modelFile = Trim(mf->GetText());
              if (tinyxml2::XMLElement *hp = info->FirstChildElement("HangPos"))
                if (hp->GetText())
                  truss.positionName = Trim(hp->GetText());
              break;
            }
          }
        }

        scene.trusses[truss.uuid] = truss;
      };

  std::function<void(tinyxml2::XMLElement *, const std::string &)>
      parseSceneObj = [&](tinyxml2::XMLElement *node,
                          const std::string &layerName) {
        const char *uuidAttr = node->Attribute("uuid");
        if (!uuidAttr)
          return;

        SceneObject obj;
        obj.uuid = uuidAttr;
        obj.layer = layerName;
        if (const char *nameAttr = node->Attribute("name"))
          obj.name = nameAttr;

        if (tinyxml2::XMLElement *geos =
                node->FirstChildElement("Geometries")) {
          if (tinyxml2::XMLElement *g3d =
                  geos->FirstChildElement("Geometry3D")) {
            const char *file = g3d->Attribute("fileName");
            if (file)
              obj.modelFile = file;
          } else if (tinyxml2::XMLElement *sym =
                         geos->FirstChildElement("Symbol")) {
            const char *symdef = sym->Attribute("symdef");
            if (symdef) {
              auto it = scene.symdefFiles.find(symdef);
              if (it != scene.symdefFiles.end())
                obj.modelFile = it->second;
            }
          }
        }

        if (tinyxml2::XMLElement *matrix = node->FirstChildElement("Matrix")) {
          if (const char *txt = matrix->GetText()) {
            std::string raw = txt;
            MatrixUtils::ParseMatrix(raw, obj.transform);
          }
        }

        scene.sceneObjects[obj.uuid] = obj;
      };

  parseChildList = [&](tinyxml2::XMLElement *cl, const std::string &layerName) {
    for (tinyxml2::XMLElement *child = cl->FirstChildElement(); child;
         child = child->NextSiblingElement()) {
      const char *name = child->Name();
      if (!name)
        continue;
      std::string nodeName = name;
      if (nodeName == "Fixture") {
        parseFixture(child, layerName);
      } else if (nodeName == "Truss") {
        parseTruss(child, layerName);
      } else if (nodeName == "SceneObject") {
        parseSceneObj(child, layerName);
      } else {
        if (tinyxml2::XMLElement *inner = child->FirstChildElement("ChildList"))
          parseChildList(inner, layerName);
      }
    }
  };

  tinyxml2::XMLElement *layersNode = sceneNode->FirstChildElement("Layers");
  if (!layersNode)
    return true;

  for (tinyxml2::XMLElement *cl = layersNode->FirstChildElement("ChildList");
       cl; cl = cl->NextSiblingElement("ChildList")) {
    parseChildList(cl, DEFAULT_LAYER_NAME);
  }

  for (tinyxml2::XMLElement *layer = layersNode->FirstChildElement("Layer");
       layer; layer = layer->NextSiblingElement("Layer")) {
    const char *layerName = layer->Attribute("name");
    std::string layerStr = layerName ? layerName : "";
    bool isDefaultLayer = layerStr.empty();

    tinyxml2::XMLElement *childList = layer->FirstChildElement("ChildList");
    if (childList)
      parseChildList(childList, isDefaultLayer ? DEFAULT_LAYER_NAME : layerStr);

    if (!isDefaultLayer) {
      Layer l;
      const char *uuidAttr = layer->Attribute("uuid");
      if (uuidAttr)
        l.uuid = uuidAttr;
      l.name = layerStr;
      if (tinyxml2::XMLElement *colorNode =
              layer->FirstChildElement("Color")) {
        if (const char *txt = colorNode->GetText())
          l.color = CieToHex(txt);
      }
      scene.layers[l.uuid] = l;
    }
  }

  // After parsing the entire scene, resolve any GDTF conflicts using the
  // dictionary only if requested. This occurs before rendering so user choices
  // are applied to the final scene data.
  if (applyDictionary) {
    std::vector<GdtfConflict> gdtfConflicts;
    std::unordered_set<std::string> conflictTypes;
    for (const auto &[uid, f] : scene.fixtures) {
      if (auto dictEntry = GdtfDictionary::Get(f.typeName)) {
        if (conflictTypes.insert(f.typeName).second) {
          gdtfConflicts.push_back({f.typeName, f.gdtfSpec, dictEntry->path});
        }
      }
    }
    if (!gdtfConflicts.empty()) {
      if (promptConflicts) {
        auto choices = PromptGdtfConflicts(gdtfConflicts);
        for (auto &[uid, f] : scene.fixtures) {
          auto typeKey = f.typeName;
          auto it = choices.find(typeKey);
          if (it != choices.end()) {
            f.gdtfSpec = it->second;
            std::string parsed = Trim(GetGdtfFixtureName(f.gdtfSpec));
            if (!parsed.empty())
              f.typeName = parsed;
            if (auto dictEntry = GdtfDictionary::Get(typeKey)) {
              if (f.gdtfMode.empty())
                f.gdtfMode = dictEntry->mode;
            }
          }
        }
      } else {
        for (auto &[uid, f] : scene.fixtures) {
          if (auto dictEntry = GdtfDictionary::Get(f.typeName)) {
            f.gdtfSpec = dictEntry->path;
            if (f.gdtfMode.empty())
              f.gdtfMode = dictEntry->mode;
            std::string parsed = Trim(GetGdtfFixtureName(f.gdtfSpec));
            if (!parsed.empty())
              f.typeName = parsed;
          }
        }
      }
    }
  }

  bool hasDefaultLayer = false;
  for (const auto &[uid, layer] : scene.layers) {
    if (layer.name == DEFAULT_LAYER_NAME) {
      hasDefaultLayer = true;
      break;
    }
  }
  if (!hasDefaultLayer) {
    Layer l;
    l.uuid = "layer_default";
    l.name = DEFAULT_LAYER_NAME;
    scene.layers[l.uuid] = l;
  }

  std::string summary =
      "Parsed scene: " + std::to_string(scene.fixtures.size()) + " fixtures, " +
      std::to_string(scene.trusses.size()) + " trusses, " +
      std::to_string(scene.sceneObjects.size()) + " objects";
  LogMessage(summary);
  return true;
}

bool MvrImporter::ImportAndRegister(const std::string &filePath,
                                    bool promptConflicts,
                                    bool applyDictionary) {
  MvrImporter importer;
  return importer.ImportFromFile(filePath, promptConflicts, applyDictionary);
}
