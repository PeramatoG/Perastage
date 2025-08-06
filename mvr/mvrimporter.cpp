#include "mvrimporter.h"
#include "configmanager.h"
#include "matrixutils.h"
#include "sceneobject.h"
#include "gdtfloader.h"

#include <filesystem>
#include <iostream>
#include <random>
#include <chrono>
#include <fstream> // Required for std::ofstream
#include <cstdlib>
#include <functional>
#include <algorithm>
#include <cctype>
#include "consolepanel.h"

// TinyXML2
#include <tinyxml2.h>

// wxWidgets zip support
#include <wx/wx.h>
#include <wx/wfstream.h>
class wxZipStreamLink;
#include <wx/zipstrm.h>
#include <wx/filename.h>

namespace fs = std::filesystem;

// Helper to convert between std::u8string and std::string without
// losing the underlying UTF-8 byte sequence.
static std::string ToString(const std::u8string& s)
{
    return std::string(s.begin(), s.end());
}

static std::string Trim(const std::string& s)
{
    const char* ws = " \t\r\n";
    size_t start = s.find_first_not_of(ws);
    if (start == std::string::npos)
        return {};
    size_t end = s.find_last_not_of(ws);
    return s.substr(start, end - start + 1);
}

bool MvrImporter::ImportFromFile(const std::string& filePath)
{
    // Use UTF-8 aware filesystem paths to support non-ASCII file names
    fs::path path = fs::u8path(filePath);
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    if (!fs::exists(path) || ext != ".mvr") {
        std::cerr << "Invalid MVR file: " << filePath << "\n";
        return false;
    }

    if (ConsolePanel::Instance()) {
        std::string pathUtf8 = ToString(path.u8string());
        wxString msg = wxString::Format("Importing MVR %s",
                                        wxString::FromUTF8(pathUtf8.c_str()));
        ConsolePanel::Instance()->AppendMessage(msg);
    }

    std::string tempDir = CreateTemporaryDirectory();
    std::string mvrPath = ToString(path.u8string());
    if (!ExtractMvrZip(mvrPath, tempDir)) {
        std::cerr << "Failed to extract MVR file.\n";
        return false;
    }

    fs::path sceneFile = fs::u8path(tempDir) / "GeneralSceneDescription.xml";
    if (!fs::exists(sceneFile)) {
        std::cerr << "Missing GeneralSceneDescription.xml in MVR.\n";
        return false;
    }

    std::string scenePath = ToString(sceneFile.u8string());
    return ParseSceneXml(scenePath);
}

std::string MvrImporter::CreateTemporaryDirectory()
{
    auto now = std::chrono::system_clock::now().time_since_epoch().count();
    std::string folderName = "Perastage_" + std::to_string(now);

    fs::path tempBase = fs::temp_directory_path();
    fs::path fullPath = tempBase / folderName;

    fs::create_directory(fullPath);
    return fullPath.string();
}

bool MvrImporter::ExtractMvrZip(const std::string& mvrPath, const std::string& destDir)
{
    wxFileInputStream input(wxString::FromUTF8(mvrPath.c_str()));
    if (!input.IsOk()) {
        std::cerr << "Failed to open MVR file.\n";
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
            wxFileName::Mkdir(wxString::FromUTF8(dirUtf8.c_str()),
                              wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL);
            continue;
        }

        std::string parentUtf8 = ToString(fullPath.parent_path().u8string());
        wxFileName::Mkdir(wxString::FromUTF8(parentUtf8.c_str()),
                          wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL);

        std::ofstream output(fullPath, std::ios::binary);
        if (!output.is_open()) {
            std::cerr << "Cannot create file: " << ToString(fullPath.u8string()) << "\n";
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

// Parses GeneralSceneDescription.xml and populates fixtures and trusses into the scene model
bool MvrImporter::ParseSceneXml(const std::string& sceneXmlPath)
{
    tinyxml2::XMLDocument doc;
    tinyxml2::XMLError result = doc.LoadFile(sceneXmlPath.c_str());
    if (result != tinyxml2::XML_SUCCESS) {
        std::cerr << "Failed to load XML: " << sceneXmlPath << "\n";
        return false;
    }

    if (ConsolePanel::Instance()) {
        wxString msg = wxString::Format("Parsing scene XML %s", wxString::FromUTF8(sceneXmlPath.c_str()));
        ConsolePanel::Instance()->AppendMessage(msg);
    }

    tinyxml2::XMLElement* root = doc.FirstChildElement("GeneralSceneDescription");
    if (!root) {
        std::cerr << "Missing GeneralSceneDescription node\n";
        return false;
    }

    ConfigManager::Get().Reset();
    auto& scene = ConfigManager::Get().GetScene();
    scene.basePath = ToString(fs::u8path(sceneXmlPath).parent_path().u8string());

    root->QueryIntAttribute("verMajor", &scene.versionMajor);
    root->QueryIntAttribute("verMinor", &scene.versionMinor);

    const char* provider = root->Attribute("provider");
    const char* version = root->Attribute("providerVersion");

    if (provider) scene.provider = provider;
    if (version) scene.providerVersion = version;

    tinyxml2::XMLElement* sceneNode = root->FirstChildElement("Scene");
    if (!sceneNode) return true;

    auto textOf = [](tinyxml2::XMLElement* parent, const char* name) -> std::string {
        tinyxml2::XMLElement* n = parent->FirstChildElement(name);
        if (n && n->GetText())
            return Trim(n->GetText());
        return {};
    };

    auto intOf = [](tinyxml2::XMLElement* parent, const char* name, int& out) {
        tinyxml2::XMLElement* n = parent->FirstChildElement(name);
        if (n && n->GetText()) out = std::atoi(n->GetText());
    };

    // ---- Parse AUXData for Symdefs and Positions ----
    if (tinyxml2::XMLElement* auxNode = sceneNode->FirstChildElement("AUXData")) {
        for (tinyxml2::XMLElement* pos = auxNode->FirstChildElement("Position");
             pos; pos = pos->NextSiblingElement("Position")) {
            const char* uid = pos->Attribute("uuid");
            const char* name = pos->Attribute("name");
            if (uid) scene.positions[uid] = name ? name : "";
        }

        for (tinyxml2::XMLElement* sym = auxNode->FirstChildElement("Symdef");
             sym; sym = sym->NextSiblingElement("Symdef")) {
            const char* uid = sym->Attribute("uuid");
            if (!uid) continue;
            std::string file;
            if (tinyxml2::XMLElement* childList = sym->FirstChildElement("ChildList")) {
                if (tinyxml2::XMLElement* geo = childList->FirstChildElement("Geometry3D")) {
                    const char* fname = geo->Attribute("fileName");
                    if (fname) file = fname;
                }
            }
            if (!file.empty()) scene.symdefFiles[uid] = file;
        }
    }

    // ---- Helper lambdas for object parsing ----
    std::function<void(tinyxml2::XMLElement*, const std::string&)> parseChildList;

    std::function<void(tinyxml2::XMLElement*, const std::string&)> parseFixture =
        [&](tinyxml2::XMLElement* node, const std::string& layerName) {
            const char* uuidAttr = node->Attribute("uuid");
            if (!uuidAttr) return;

            Fixture fixture;
            fixture.uuid = uuidAttr;
            fixture.layer = layerName;

            if (const char* nameAttr = node->Attribute("name"))
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
            if (!fixture.gdtfSpec.empty()) {
                fs::path p = scene.basePath.empty()
                               ? fs::u8path(fixture.gdtfSpec)
                               : fs::u8path(scene.basePath) / fs::u8path(fixture.gdtfSpec);
                std::string gdtfPath = ToString(p.u8string());
                fixture.typeName = GetGdtfFixtureName(gdtfPath);
            }
            auto posIt = scene.positions.find(fixture.position);
            if (posIt != scene.positions.end()) fixture.positionName = posIt->second;

            auto boolOf = [&](const char* name, bool& out) {
                tinyxml2::XMLElement* n = node->FirstChildElement(name);
                if (n && n->GetText()) {
                    std::string v = n->GetText();
                    out = (v == "true" || v == "1");
                }
            };

            boolOf("DMXInvertPan", fixture.dmxInvertPan);
            boolOf("DMXInvertTilt", fixture.dmxInvertTilt);

            if (tinyxml2::XMLElement* addresses = node->FirstChildElement("Addresses")) {
                tinyxml2::XMLElement* addr = addresses->FirstChildElement("Address");
                if (addr) {
                    const char* breakAttr = addr->Attribute("break");
                    int breakNum = breakAttr ? std::atoi(breakAttr) : 0;
                    const char* txt = addr->GetText();
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
                            normalized = std::to_string(universe) + "." + std::to_string(value);
                        } else {
                            normalized = t;
                        }
                        fixture.address = normalized;
                    }
                }
            }

            if (tinyxml2::XMLElement* matrix = node->FirstChildElement("Matrix")) {
                if (const char* txt = matrix->GetText()) {
                    fixture.matrixRaw = txt;
                    MatrixUtils::ParseMatrix(fixture.matrixRaw, fixture.transform);
                }
            }

            scene.fixtures[fixture.uuid] = fixture;
        };

    std::function<void(tinyxml2::XMLElement*, const std::string&)> parseTruss =
        [&](tinyxml2::XMLElement* node, const std::string& layerName) {
            const char* uuidAttr = node->Attribute("uuid");
            if (!uuidAttr) return;

            Truss truss;
            truss.uuid = uuidAttr;
            truss.layer = layerName;
            if (const char* nameAttr = node->Attribute("name")) truss.name = nameAttr;

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

            if (tinyxml2::XMLElement* geos = node->FirstChildElement("Geometries")) {
                if (tinyxml2::XMLElement* sym = geos->FirstChildElement("Symbol")) {
                    const char* symdef = sym->Attribute("symdef");
                    if (symdef) {
                        auto it = scene.symdefFiles.find(symdef);
                        if (it != scene.symdefFiles.end()) truss.symbolFile = it->second;
                    }
                }
            }

            if (tinyxml2::XMLElement* matrix = node->FirstChildElement("Matrix")) {
                if (const char* txt = matrix->GetText()) {
                    std::string raw = txt;
                    MatrixUtils::ParseMatrix(raw, truss.transform);
                }
            }

            if (tinyxml2::XMLElement* ud = node->FirstChildElement("UserData")) {
                for (tinyxml2::XMLElement* data = ud->FirstChildElement("Data"); data; data = data->NextSiblingElement("Data")) {
                    if (tinyxml2::XMLElement* info = data->FirstChildElement("TrussInfo")) {
                        if (tinyxml2::XMLElement* m = info->FirstChildElement("Manufacturer"))
                            if (m->GetText()) truss.manufacturer = Trim(m->GetText());
                        if (tinyxml2::XMLElement* mo = info->FirstChildElement("Model"))
                            if (mo->GetText()) truss.model = Trim(mo->GetText());
                        if (tinyxml2::XMLElement* len = info->FirstChildElement("Length"))
                            if (len->GetText()) truss.lengthMm = std::stof(len->GetText());
                        if (tinyxml2::XMLElement* wid = info->FirstChildElement("Width"))
                            if (wid->GetText()) truss.widthMm = std::stof(wid->GetText());
                        if (tinyxml2::XMLElement* hei = info->FirstChildElement("Height"))
                            if (hei->GetText()) truss.heightMm = std::stof(hei->GetText());
                        if (tinyxml2::XMLElement* wei = info->FirstChildElement("Weight"))
                            if (wei->GetText()) truss.weightKg = std::stof(wei->GetText());
                        if (tinyxml2::XMLElement* cs = info->FirstChildElement("CrossSection"))
                            if (cs->GetText()) truss.crossSection = Trim(cs->GetText());
                        break;
                    }
                }
            }

            scene.trusses[truss.uuid] = truss;
        };

    std::function<void(tinyxml2::XMLElement*, const std::string&)> parseSceneObj =
        [&](tinyxml2::XMLElement* node, const std::string& layerName) {
            const char* uuidAttr = node->Attribute("uuid");
            if (!uuidAttr) return;

            SceneObject obj;
            obj.uuid = uuidAttr;
            obj.layer = layerName;
            if (const char* nameAttr = node->Attribute("name")) obj.name = nameAttr;

            if (tinyxml2::XMLElement* geos = node->FirstChildElement("Geometries")) {
                if (tinyxml2::XMLElement* g3d = geos->FirstChildElement("Geometry3D")) {
                    const char* file = g3d->Attribute("fileName");
                    if (file) obj.modelFile = file;
                } else if (tinyxml2::XMLElement* sym = geos->FirstChildElement("Symbol")) {
                    const char* symdef = sym->Attribute("symdef");
                    if (symdef) {
                        auto it = scene.symdefFiles.find(symdef);
                        if (it != scene.symdefFiles.end()) obj.modelFile = it->second;
                    }
                }
            }

            if (tinyxml2::XMLElement* matrix = node->FirstChildElement("Matrix")) {
                if (const char* txt = matrix->GetText()) {
                    std::string raw = txt;
                    MatrixUtils::ParseMatrix(raw, obj.transform);
                }
            }

            scene.sceneObjects[obj.uuid] = obj;
        };

    parseChildList = [&](tinyxml2::XMLElement* cl, const std::string& layerName) {
        for (tinyxml2::XMLElement* child = cl->FirstChildElement(); child; child = child->NextSiblingElement()) {
            const char* name = child->Name();
            if (!name) continue;
            std::string nodeName = name;
            if (nodeName == "Fixture") {
                parseFixture(child, layerName);
            } else if (nodeName == "Truss") {
                parseTruss(child, layerName);
            } else if (nodeName == "SceneObject") {
                parseSceneObj(child, layerName);
            } else {
                if (tinyxml2::XMLElement* inner = child->FirstChildElement("ChildList"))
                    parseChildList(inner, layerName);
            }
        }
    };

    tinyxml2::XMLElement* layersNode = sceneNode->FirstChildElement("Layers");
    if (!layersNode) return true;

    for (tinyxml2::XMLElement* cl = layersNode->FirstChildElement("ChildList");
         cl; cl = cl->NextSiblingElement("ChildList")) {
        parseChildList(cl, DEFAULT_LAYER_NAME);
    }

    for (tinyxml2::XMLElement* layer = layersNode->FirstChildElement("Layer");
         layer; layer = layer->NextSiblingElement("Layer")) {
        const char* layerName = layer->Attribute("name");
        std::string layerStr = layerName ? layerName : "";

        tinyxml2::XMLElement* childList = layer->FirstChildElement("ChildList");
        if (childList) parseChildList(childList, layerStr);

        Layer l;
        const char* uuidAttr = layer->Attribute("uuid");
        if (uuidAttr) l.uuid = uuidAttr;
        l.name = layerStr;
        scene.layers[l.uuid] = l;
    }

    bool hasDefaultLayer = false;
    for (const auto& [uid, layer] : scene.layers) {
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

    if (ConsolePanel::Instance()) {
        wxString msg = wxString::Format(
            "Parsed scene: %zu fixtures, %zu trusses, %zu objects",
            scene.fixtures.size(), scene.trusses.size(), scene.sceneObjects.size());
        ConsolePanel::Instance()->AppendMessage(msg);
    }

    return true;
}

bool MvrImporter::ImportAndRegister(const std::string& filePath)
{
    MvrImporter importer;
    return importer.ImportFromFile(filePath);
}

