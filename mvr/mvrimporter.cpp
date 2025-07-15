#include "mvrimporter.h"
#include "configmanager.h"

#include <filesystem>
#include <iostream>
#include <random>
#include <chrono>
#include <fstream> // Required for std::ofstream

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

// Parses the GeneralSceneDescription.xml file and stores fixtures in the scene data structure
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

    for (tinyxml2::XMLElement* layer = layersNode->FirstChildElement("Layer"); layer; layer = layer->NextSiblingElement("Layer"))
    {
        const char* layerName = layer->Attribute("name");
        std::string layerStr = layerName ? layerName : "";

        tinyxml2::XMLElement* childList = layer->FirstChildElement("ChildList");
        if (!childList) continue;

        for (tinyxml2::XMLElement* child = childList->FirstChildElement("Child"); child; child = child->NextSiblingElement("Child"))
        {
            const char* type = child->Attribute("type");
            const char* uuid = child->Attribute("uuid");
            if (!type || !uuid) continue;

            std::string typeStr = type;
            std::string uuidStr = uuid;

            if (typeStr == "Fixture")
            {
                Fixture fixture;
                fixture.uuid = uuidStr;
                fixture.layer = layerStr;

                tinyxml2::XMLElement* fixtureNode = child->FirstChildElement("Fixture");
                if (!fixtureNode) continue;

                // Required attributes
                const char* fixtureID = fixtureNode->Attribute("fixtureID");
                if (fixtureID) fixture.fixtureId = std::atoi(fixtureID);

                const char* name = fixtureNode->Attribute("name");
                if (name) fixture.name = name;

                const char* gdtfSpec = fixtureNode->Attribute("GDTFSpec");
                if (gdtfSpec) fixture.gdtfSpec = gdtfSpec;

                const char* gdtfMode = fixtureNode->Attribute("GDTFMode");
                if (gdtfMode) fixture.gdtfMode = gdtfMode;

                const char* focus = fixtureNode->Attribute("focus");
                if (focus) fixture.focus = focus;

                const char* function = fixtureNode->Attribute("function");
                if (function) fixture.function = function;

                // Optional attributes
                fixtureNode->QueryIntAttribute("fixtureIdNumeric", &fixture.fixtureIdNumeric);
                fixtureNode->QueryIntAttribute("unitNumber", &fixture.unitNumber);
                fixtureNode->QueryIntAttribute("customId", &fixture.customId);
                fixtureNode->QueryIntAttribute("customIdType", &fixture.customIdType);
                fixtureNode->QueryBoolAttribute("dmxInvertPan", &fixture.dmxInvertPan);
                fixtureNode->QueryBoolAttribute("dmxInvertTilt", &fixture.dmxInvertTilt);

                // Position element
                tinyxml2::XMLElement* position = fixtureNode->FirstChildElement("Position");
                if (position && position->GetText())
                    fixture.position = position->GetText();

                // Addresses element
                tinyxml2::XMLElement* addresses = fixtureNode->FirstChildElement("Addresses");
                if (addresses) {
                    tinyxml2::XMLElement* addr = addresses->FirstChildElement("Address");
                    if (addr && addr->GetText())
                        fixture.address = addr->GetText();
                }

                // Transform matrix element
                tinyxml2::XMLElement* transform = fixtureNode->FirstChildElement("Transform");
                if (transform) {
                    const char* matrix = transform->Attribute("matrix");
                    if (matrix)
                        fixture.matrixRaw = matrix;
                }

                // Store fixture in scene using its UUID
                scene.fixtures[fixture.uuid] = fixture;
            }
        }
    }

    return true;
}





bool MvrImporter::ImportAndRegister(const std::string& filePath)
{
    MvrImporter importer;
    return importer.ImportFromFile(filePath);
}

