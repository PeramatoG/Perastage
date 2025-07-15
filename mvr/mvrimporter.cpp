#include "mvrimporter.h"
#include "configmanager.h"

#include <filesystem>
#include <iostream>
#include <random>
#include <chrono>
#include <fstream> // Required for std::ofstream
#include <cstdlib>

// TinyXML2
#include <tinyxml2.h>

// wxWidgets zip support
#include <wx/wx.h>
#include <wx/wfstream.h>
#include <wx/zipstrm.h>
#include <wx/filename.h>

namespace fs = std::filesystem;

bool MvrImporter::ImportFromFile(const std::string& filePath)
{
    if (!fs::exists(filePath) || fs::path(filePath).extension() != ".mvr") {
        std::cerr << "Invalid MVR file: " << filePath << "\n";
        return false;
    }

    std::string tempDir = CreateTemporaryDirectory();
    if (!ExtractMvrZip(filePath, tempDir)) {
        std::cerr << "Failed to extract MVR file.\n";
        return false;
    }

    std::string sceneFile = tempDir + "/GeneralSceneDescription.xml";
    if (!fs::exists(sceneFile)) {
        std::cerr << "Missing GeneralSceneDescription.xml in MVR.\n";
        return false;
    }

    return ParseSceneXml(sceneFile);
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
    wxFileInputStream input(mvrPath);
    if (!input.IsOk()) {
        std::cerr << "Failed to open MVR file.\n";
        return false;
    }

    wxZipInputStream zipStream(input);
    std::unique_ptr<wxZipEntry> entry;

    while ((entry.reset(zipStream.GetNextEntry())), entry) {
        std::string filename = entry->GetName().ToStdString();
        std::string fullPath = destDir + "/" + filename;

        if (entry->IsDir()) {
            wxFileName::Mkdir(fullPath, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL);
            continue;
        }

        wxFileName::Mkdir(wxFileName(fullPath).GetPath(), wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL);

        std::ofstream output(fullPath, std::ios::binary);
        if (!output.is_open()) {
            std::cerr << "Cannot create file: " << fullPath << "\n";
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

bool MvrImporter::ParseSceneXml(const std::string& sceneXmlPath)
{
    tinyxml2::XMLDocument doc;
    tinyxml2::XMLError result = doc.LoadFile(sceneXmlPath.c_str());
    if (result != tinyxml2::XML_SUCCESS) {
        std::cerr << "Failed to load XML: " << sceneXmlPath << "\n";
        return false;
    }

    tinyxml2::XMLElement* root = doc.FirstChildElement("GeneralSceneDescription");
    if (!root) {
        std::cerr << "Missing GeneralSceneDescription node\n";
        return false;
    }

    ConfigManager::Get().Reset();
    auto& scene = ConfigManager::Get().GetScene();

    root->QueryIntAttribute("verMajor", &scene.versionMajor);
    root->QueryIntAttribute("verMinor", &scene.versionMinor);

    const char* provider = root->Attribute("provider");
    const char* version = root->Attribute("providerVersion");

    if (provider) scene.provider = provider;
    if (version) scene.providerVersion = version;

    tinyxml2::XMLElement* sceneNode = root->FirstChildElement("Scene");
    if (!sceneNode) return true;

    tinyxml2::XMLElement* layersNode = sceneNode->FirstChildElement("Layers");
    if (!layersNode) return true;

    for (tinyxml2::XMLElement* layer = layersNode->FirstChildElement("Layer");
         layer; layer = layer->NextSiblingElement("Layer"))
    {
        const char* layerName = layer->Attribute("name");
        std::string layerStr = layerName ? layerName : "";

        tinyxml2::XMLElement* childList = layer->FirstChildElement("ChildList");
        if (!childList) continue;

        for (tinyxml2::XMLElement* fixtureNode = childList->FirstChildElement("Fixture");
             fixtureNode; fixtureNode = fixtureNode->NextSiblingElement("Fixture"))
        {
            const char* uuidAttr = fixtureNode->Attribute("uuid");
            if (!uuidAttr) continue;

            Fixture fixture;
            fixture.uuid = uuidAttr;
            fixture.layer = layerStr;

            auto textOf = [](tinyxml2::XMLElement* parent, const char* name) -> std::string {
                tinyxml2::XMLElement* n = parent->FirstChildElement(name);
                return (n && n->GetText()) ? n->GetText() : std::string();
            };

            auto intOf = [](tinyxml2::XMLElement* parent, const char* name, int& out) {
                tinyxml2::XMLElement* n = parent->FirstChildElement(name);
                if (n && n->GetText()) out = std::atoi(n->GetText());
            };

            auto boolOf = [](tinyxml2::XMLElement* parent, const char* name, bool& out) {
                tinyxml2::XMLElement* n = parent->FirstChildElement(name);
                if (n && n->GetText()) {
                    std::string v = n->GetText();
                    out = (v == "true" || v == "1");
                }
            };

            const char* nameAttr = fixtureNode->Attribute("name");
            if (nameAttr) fixture.name = nameAttr;

            intOf(fixtureNode, "FixtureID", fixture.fixtureId);
            intOf(fixtureNode, "FixtureIDNumeric", fixture.fixtureIdNumeric);
            intOf(fixtureNode, "UnitNumber", fixture.unitNumber);
            intOf(fixtureNode, "CustomId", fixture.customId);
            intOf(fixtureNode, "CustomIdType", fixture.customIdType);

            fixture.gdtfSpec = textOf(fixtureNode, "GDTFSpec");
            fixture.gdtfMode = textOf(fixtureNode, "GDTFMode");
            fixture.focus = textOf(fixtureNode, "Focus");
            fixture.function = textOf(fixtureNode, "Function");
            fixture.position = textOf(fixtureNode, "Position");

            boolOf(fixtureNode, "DMXInvertPan", fixture.dmxInvertPan);
            boolOf(fixtureNode, "DMXInvertTilt", fixture.dmxInvertTilt);

            if (tinyxml2::XMLElement* addresses = fixtureNode->FirstChildElement("Addresses")) {
                tinyxml2::XMLElement* addr = addresses->FirstChildElement("Address");
                if (addr && addr->GetText())
                    fixture.address = addr->GetText();
            }

            if (tinyxml2::XMLElement* matrix = fixtureNode->FirstChildElement("Matrix")) {
                if (matrix->GetText())
                    fixture.matrixRaw = matrix->GetText();
            }

            scene.fixtures[fixture.uuid] = fixture;
        }
    }

    return true;
}




bool MvrImporter::ImportAndRegister(const std::string& filePath)
{
    MvrImporter importer;
    return importer.ImportFromFile(filePath);
}

