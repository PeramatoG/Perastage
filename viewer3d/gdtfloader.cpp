#include "gdtfloader.h"
#include "loader3ds.h"
#include "loaderglb.h"
#include "matrixutils.h"
#include "consolepanel.h"

#include <tinyxml2.h>
#include <wx/wx.h>
#include <wx/wfstream.h>
class wxZipStreamLink;
#include <wx/zipstrm.h>
#include <wx/filename.h>

#include <filesystem>
#include <unordered_map>
#include <chrono>
#include <random>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <cfloat>
#include <sstream>

namespace fs = std::filesystem;

static std::string ToLower(const std::string& s)
{
    std::string t = s;
    std::transform(t.begin(), t.end(), t.begin(), [](unsigned char c){ return std::tolower(c); });
    return t;
}

static bool HasExtension(const fs::path& p, const std::string& ext)
{
    return ToLower(p.extension().string()) == ToLower(ext);
}

static std::string FindModelFile(const std::string& baseDir,
                                 const std::string& fileName)
{
    fs::path modelsDir = fs::path(baseDir) / "models";
    if (!fs::exists(modelsDir))
        return {};

    fs::path namePath = fileName;
    std::string stem = namePath.stem().string();
    std::string ext = ToLower(namePath.extension().string());

    auto tryExt = [&](const std::string& e) -> std::string {
        fs::path d = modelsDir / e.substr(1) / (stem + e);
        if(fs::exists(d))
            return d.string();
        return {};
    };

    if(!ext.empty()) {
        std::string res = tryExt(ext);
        if(!res.empty()) return res;
    } else {
        std::string res = tryExt(".3ds");
        if(!res.empty()) return res;
        res = tryExt(".glb");
        if(!res.empty()) return res;
    }

    for (auto& p : fs::recursive_directory_iterator(modelsDir)) {
        if (!p.is_regular_file())
            continue;
        if (p.path().stem() != stem)
            continue;
        if(ext.empty()) {
            if(HasExtension(p.path(), ".3ds") || HasExtension(p.path(), ".glb"))
                return p.path().string();
        } else if(HasExtension(p.path(), ext)) {
            return p.path().string();
        }
    }
    return {};
}

static std::string CreateTempDir()
{
    auto now = std::chrono::system_clock::now().time_since_epoch().count();
    std::string folderName = "GDTF_" + std::to_string(now);
    fs::path base = fs::temp_directory_path();
    fs::path full = base / folderName;
    fs::create_directory(full);
    return full.string();
}

static bool ExtractZip(const std::string& zipPath, const std::string& destDir)
{
    if (!fs::exists(zipPath)) {
        if (ConsolePanel::Instance()) {
            wxString msg = wxString::Format("GDTF: cannot open %s", wxString::FromUTF8(zipPath));
            ConsolePanel::Instance()->AppendMessage(msg);
        }
        return false;
    }
    wxLogNull logNo;
    wxFileInputStream input(zipPath);
    if (!input.IsOk()) {
        if (ConsolePanel::Instance()) {
            wxString msg = wxString::Format("GDTF: cannot open %s", wxString::FromUTF8(zipPath));
            ConsolePanel::Instance()->AppendMessage(msg);
        }
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
            if (ConsolePanel::Instance()) {
                wxString msg = wxString::Format("GDTF: cannot create %s", wxString::FromUTF8(fullPath));
                ConsolePanel::Instance()->AppendMessage(msg);
            }
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

static void ParseGeometry(tinyxml2::XMLElement* node,
                          const Matrix& parent,
                          const std::unordered_map<std::string, GdtfModelInfo>& models,
                          const std::string& baseDir,
                          const std::unordered_map<std::string, tinyxml2::XMLElement*>& geomMap,
                          std::unordered_map<std::string, Mesh>& meshCache,
                          std::vector<GdtfObject>& outObjects,
                          const char* overrideModel = nullptr)
{
    Matrix local = MatrixUtils::Identity();
    if (const char* pos = node->Attribute("Position"))
        MatrixUtils::ParseMatrix(pos, local);

    Matrix transform = MatrixUtils::Multiply(parent, local);

    std::string nodeType = node->Name();
    if (nodeType == "GeometryReference") {
        const char* refName = node->Attribute("Geometry");
        if (refName) {
            auto it = geomMap.find(refName);
            if (it != geomMap.end()) {
                const char* m = node->Attribute("Model");
                ParseGeometry(it->second, transform, models, baseDir, geomMap, meshCache, outObjects, m ? m : overrideModel);
            }
        }
        return;
    }

    const char* modelName = overrideModel ? overrideModel : node->Attribute("Model");
    if (modelName) {
        auto it = models.find(modelName);
        if (it != models.end()) {
            std::string path = FindModelFile(baseDir, it->second.file);
            if (!path.empty()) {
                auto mit = meshCache.find(path);
                if (mit == meshCache.end()) {
                    Mesh mesh;
                    bool loaded = false;
                    if (HasExtension(path, ".3ds"))
                        loaded = Load3DS(path, mesh);
                    else if (HasExtension(path, ".glb"))
                        loaded = LoadGLB(path, mesh);

                    if (loaded) {
                        
                        // Apply model dimension scaling if provided
                        float minX = FLT_MAX, minY = FLT_MAX, minZ = FLT_MAX;
                        float maxX = -FLT_MAX, maxY = -FLT_MAX, maxZ = -FLT_MAX;
                        for (size_t vi = 0; vi + 2 < mesh.vertices.size(); vi += 3) {
                            float x = mesh.vertices[vi];
                            float y = mesh.vertices[vi + 1];
                            float z = mesh.vertices[vi + 2];
                            minX = std::min(minX, x); maxX = std::max(maxX, x);
                            minY = std::min(minY, y); maxY = std::max(maxY, y);
                            minZ = std::min(minZ, z); maxZ = std::max(maxZ, z);
                        }
                        float sizeX = maxX - minX;
                        float sizeY = maxY - minY;
                        float sizeZ = maxZ - minZ;
                        float targetX = it->second.length * 1000.0f; // meters -> mm
                        float targetY = it->second.width  * 1000.0f;
                        float targetZ = it->second.height * 1000.0f;
                        float sx = (targetX > 0.0f && sizeX > 0.0f) ? targetX / sizeX : 1.0f;
                        float sy = (targetY > 0.0f && sizeY > 0.0f) ? targetY / sizeY : 1.0f;
                        float sz = (targetZ > 0.0f && sizeZ > 0.0f) ? targetZ / sizeZ : 1.0f;
                        if (sx != 1.0f || sy != 1.0f || sz != 1.0f) {
                            for (size_t vi = 0; vi + 2 < mesh.vertices.size(); vi += 3) {
                                mesh.vertices[vi]     *= sx;
                                mesh.vertices[vi + 1] *= sy;
                                mesh.vertices[vi + 2] *= sz;
                            }
                        }
                        mit = meshCache.emplace(path, std::move(mesh)).first;
                    } else if (ConsolePanel::Instance()) {
                        wxString msg = wxString::Format("GDTF: failed to load model %s", wxString::FromUTF8(path));
                        ConsolePanel::Instance()->AppendMessage(msg);
                    }
                }
                if (mit != meshCache.end()) {
                    outObjects.push_back({mit->second, transform});
                }
            } else if (ConsolePanel::Instance()) {
                wxString msg = wxString::Format(
                    "GDTF: missing model file %s in %s",
                    wxString::FromUTF8(it->second.file),
                    wxString::FromUTF8(baseDir));
                ConsolePanel::Instance()->AppendMessage(msg);
            }
        }
    }

    for (tinyxml2::XMLElement* child = node->FirstChildElement(); child; child = child->NextSiblingElement()) {
        std::string n = child->Name();
        if (n == "Geometry" || n == "Axis" || n.rfind("Filter",0)==0 || n=="Beam" ||
            n=="MediaServerLayer" || n=="MediaServerCamera" || n=="MediaServerMaster" ||
            n=="Display" || n=="GeometryReference" || n=="Laser" || n=="WiringObject" ||
            n=="Inventory" || n=="Structure" || n=="Support" || n=="Magnet") {
            ParseGeometry(child, transform, models, baseDir, geomMap, meshCache, outObjects);
        }
    }
}

bool LoadGdtf(const std::string& gdtfPath, std::vector<GdtfObject>& outObjects)
{
    if (ConsolePanel::Instance()) {
        wxString msg = wxString::Format("Loading GDTF %s", wxString::FromUTF8(gdtfPath));
        ConsolePanel::Instance()->AppendMessage(msg);
    }

    outObjects.clear();
    std::string tempDir = CreateTempDir();
    if (!ExtractZip(gdtfPath, tempDir)) {
        if (ConsolePanel::Instance()) {
            wxString msg = wxString::Format("GDTF: failed to extract %s", wxString::FromUTF8(gdtfPath));
            ConsolePanel::Instance()->AppendMessage(msg);
        }
        return false;
    } else if (ConsolePanel::Instance()) {
        wxString msg = wxString::Format("GDTF: extracted to %s", wxString::FromUTF8(tempDir));
        ConsolePanel::Instance()->AppendMessage(msg);
    }

    std::string descPath = tempDir + "/description.xml";
    tinyxml2::XMLDocument doc;
    if (doc.LoadFile(descPath.c_str()) != tinyxml2::XML_SUCCESS) {
        if (ConsolePanel::Instance()) {
            wxString msg = wxString::Format("GDTF: cannot read description.xml in %s", wxString::FromUTF8(gdtfPath));
            ConsolePanel::Instance()->AppendMessage(msg);
        }
        return false;
    }

    tinyxml2::XMLElement* ft = doc.FirstChildElement("GDTF");
    if (ft) ft = ft->FirstChildElement("FixtureType");
    else ft = doc.FirstChildElement("FixtureType");
    if (!ft) {
        if (ConsolePanel::Instance()) {
            wxString msg = wxString::Format("GDTF: invalid fixture type in %s", wxString::FromUTF8(gdtfPath));
            ConsolePanel::Instance()->AppendMessage(msg);
        }
        return false;
    }

    std::unordered_map<std::string, GdtfModelInfo> models;
    if (tinyxml2::XMLElement* modelList = ft->FirstChildElement("Models")) {
        for (tinyxml2::XMLElement* m = modelList->FirstChildElement("Model"); m; m = m->NextSiblingElement("Model")) {
            const char* name = m->Attribute("Name");
            const char* file = m->Attribute("File");
            if (name && file) {
                GdtfModelInfo info;
                info.file = file;
                m->QueryFloatAttribute("Length", &info.length);
                m->QueryFloatAttribute("Width", &info.width);
                m->QueryFloatAttribute("Height", &info.height);
                models[name] = info;
            }
        }
    }

    std::unordered_map<std::string, Mesh> meshCache;
    if (tinyxml2::XMLElement* geoms = ft->FirstChildElement("Geometries")) {
        std::unordered_map<std::string, tinyxml2::XMLElement*> geomMap;
        for (tinyxml2::XMLElement* g = geoms->FirstChildElement(); g; g = g->NextSiblingElement()) {
            if (const char* n = g->Attribute("Name"))
                geomMap[n] = g;
        }
        for (tinyxml2::XMLElement* g = geoms->FirstChildElement(); g; g = g->NextSiblingElement()) {
            ParseGeometry(g, MatrixUtils::Identity(), models, tempDir, geomMap, meshCache, outObjects);
        }
    }

    if (ConsolePanel::Instance()) {
        wxString msg = wxString::Format("GDTF: loaded %zu objects from %s",
                                       outObjects.size(),
                                       wxString::FromUTF8(gdtfPath));
        ConsolePanel::Instance()->AppendMessage(msg);
    }

    return !outObjects.empty();
}

int GetGdtfModeChannelCount(const std::string& gdtfPath,
                            const std::string& modeName)
{
    if (gdtfPath.empty() || modeName.empty())
        return -1;

    std::string tempDir = CreateTempDir();
    if (!ExtractZip(gdtfPath, tempDir))
        return -1;

    std::string descPath = tempDir + "/description.xml";
    tinyxml2::XMLDocument doc;
    if (doc.LoadFile(descPath.c_str()) != tinyxml2::XML_SUCCESS)
        return -1;

    tinyxml2::XMLElement* ft = doc.FirstChildElement("GDTF");
    if (ft)
        ft = ft->FirstChildElement("FixtureType");
    else
        ft = doc.FirstChildElement("FixtureType");
    if (!ft)
        return -1;

    tinyxml2::XMLElement* modes = ft->FirstChildElement("DMXModes");
    if (!modes)
        return -1;

    for (tinyxml2::XMLElement* m = modes->FirstChildElement("DMXMode");
         m; m = m->NextSiblingElement("DMXMode"))
    {
        const char* name = m->Attribute("Name");
        if (name && modeName == name)
        {
            tinyxml2::XMLElement* channels = m->FirstChildElement("DMXChannels");
            int count = 0;
            if (channels)
            {
                for (tinyxml2::XMLElement* c = channels->FirstChildElement("DMXChannel");
                     c; c = c->NextSiblingElement("DMXChannel"))
                {
                    const char* offset = c->Attribute("Offset");
                    if (!offset || !*offset || std::string(offset) == "None")
                        continue;
                    std::string offStr = offset;
                    std::stringstream ss(offStr);
                    std::string token;
                    while (std::getline(ss, token, ','))
                    {
                        token.erase(0, token.find_first_not_of(" \t\r\n"));
                        token.erase(token.find_last_not_of(" \t\r\n") + 1);
                        if (token.empty())
                            continue;
                        try { (void)std::stoi(token); ++count; }
                        catch (...) { /* ignore invalid */ }
                    }
                }
            }
            return count;
        }
    }

    return -1;
}

std::vector<std::string> GetGdtfModes(const std::string& gdtfPath)
{
    std::vector<std::string> result;
    if (gdtfPath.empty())
        return result;

    std::string tempDir = CreateTempDir();
    if (!ExtractZip(gdtfPath, tempDir))
        return result;

    std::string descPath = tempDir + "/description.xml";
    tinyxml2::XMLDocument doc;
    if (doc.LoadFile(descPath.c_str()) != tinyxml2::XML_SUCCESS)
        return result;

    tinyxml2::XMLElement* ft = doc.FirstChildElement("GDTF");
    if (ft)
        ft = ft->FirstChildElement("FixtureType");
    else
        ft = doc.FirstChildElement("FixtureType");
    if (!ft)
        return result;

    tinyxml2::XMLElement* modes = ft->FirstChildElement("DMXModes");
    if (!modes)
        return result;

    for (tinyxml2::XMLElement* m = modes->FirstChildElement("DMXMode");
         m; m = m->NextSiblingElement("DMXMode"))
    {
        const char* name = m->Attribute("Name");
        if (name)
            result.push_back(name);
    }

    return result;
}

std::vector<GdtfChannelInfo> GetGdtfModeChannels(
    const std::string& gdtfPath,
    const std::string& modeName)
{
    std::vector<GdtfChannelInfo> result;
    if (gdtfPath.empty() || modeName.empty())
        return result;

    std::string tempDir = CreateTempDir();
    if (!ExtractZip(gdtfPath, tempDir))
        return result;

    std::string descPath = tempDir + "/description.xml";
    tinyxml2::XMLDocument doc;
    if (doc.LoadFile(descPath.c_str()) != tinyxml2::XML_SUCCESS)
        return result;

    tinyxml2::XMLElement* ft = doc.FirstChildElement("GDTF");
    if (ft)
        ft = ft->FirstChildElement("FixtureType");
    else
        ft = doc.FirstChildElement("FixtureType");
    if (!ft)
        return result;

    tinyxml2::XMLElement* modes = ft->FirstChildElement("DMXModes");
    if (!modes)
        return result;

    for (tinyxml2::XMLElement* m = modes->FirstChildElement("DMXMode");
         m; m = m->NextSiblingElement("DMXMode"))
    {
        const char* name = m->Attribute("Name");
        if (!name || modeName != name)
            continue;

        tinyxml2::XMLElement* channels = m->FirstChildElement("DMXChannels");
        if (!channels)
            break;

        for (tinyxml2::XMLElement* c = channels->FirstChildElement("DMXChannel");
             c; c = c->NextSiblingElement("DMXChannel"))
        {
            GdtfChannelInfo info;

            if (const char* offset = c->Attribute("Offset"))
            {
                std::string offStr = offset;
                size_t comma = offStr.find(',');
                std::string first = offStr.substr(0, comma);
                try { info.channel = std::stoi(first); }
                catch (...) { info.channel = 0; }
            }
            if (info.channel == 0)
                info.channel = static_cast<int>(result.size()) + 1;

            if (tinyxml2::XMLElement* lc = c->FirstChildElement("LogicalChannel"))
            {
                if (const char* attr = lc->Attribute("Attribute"))
                    info.function = attr;
            }

            result.push_back(info);
        }
        break;
    }

    return result;
}

std::string GetGdtfFixtureName(const std::string& gdtfPath)
{
    if (gdtfPath.empty())
        return {};

    std::string tempDir = CreateTempDir();
    if (!ExtractZip(gdtfPath, tempDir))
        return {};

    std::string descPath = tempDir + "/description.xml";
    tinyxml2::XMLDocument doc;
    if (doc.LoadFile(descPath.c_str()) != tinyxml2::XML_SUCCESS)
        return {};

    tinyxml2::XMLElement* ft = doc.FirstChildElement("GDTF");
    if (ft)
        ft = ft->FirstChildElement("FixtureType");
    else
        ft = doc.FirstChildElement("FixtureType");
    if (!ft)
        return {};

    const char* nameAttr = ft->Attribute("Name");
    if (nameAttr)
        return nameAttr;
    const char* shortName = ft->Attribute("ShortName");
    if (shortName)
        return shortName;
    const char* longName = ft->Attribute("LongName");
    if (longName)
        return longName;
    return {};
}

bool GetGdtfProperties(const std::string& gdtfPath,
                       float& outWeightKg,
                       float& outPowerW)
{
    outWeightKg = 0.0f;
    outPowerW = 0.0f;
    if (gdtfPath.empty())
        return false;

    std::string tempDir = CreateTempDir();
    if (!ExtractZip(gdtfPath, tempDir))
        return false;

    std::string descPath = tempDir + "/description.xml";
    tinyxml2::XMLDocument doc;
    if (doc.LoadFile(descPath.c_str()) != tinyxml2::XML_SUCCESS)
        return false;

    tinyxml2::XMLElement* ft = doc.FirstChildElement("GDTF");
    if (ft)
        ft = ft->FirstChildElement("FixtureType");
    else
        ft = doc.FirstChildElement("FixtureType");
    if (!ft)
        return false;

    tinyxml2::XMLElement* phys = ft->FirstChildElement("PhysicalDescriptions");
    if (!phys)
        return true;
    tinyxml2::XMLElement* props = phys->FirstChildElement("Properties");
    if (!props)
        return true;

    if (tinyxml2::XMLElement* w = props->FirstChildElement("Weight")) {
        if (const char* v = w->Attribute("Value"))
            outWeightKg = std::stof(v);
    }

    if (tinyxml2::XMLElement* pc = props->FirstChildElement("PowerConsumption")) {
        if (const char* v = pc->Attribute("Value"))
            outPowerW = std::stof(v);
    }

    return true;
}

