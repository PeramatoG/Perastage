#include "gdtfloader.h"
#include "loader3ds.h"
#include "matrixutils.h"
#include "consolepanel.h"

#include <tinyxml2.h>
#include <wx/wx.h>
#include <wx/wfstream.h>
#include <wx/zipstrm.h>
#include <wx/filename.h>

#include <filesystem>
#include <unordered_map>
#include <chrono>
#include <random>
#include <iostream>
#include <fstream>

namespace fs = std::filesystem;

struct ModelInfo {
    std::string file;
};

static std::string Find3dsFile(const std::string& baseDir,
                               const std::string& fileName)
{
    fs::path modelsDir = fs::path(baseDir) / "models";
    if (!fs::exists(modelsDir))
        return {};

    for (auto& p : fs::recursive_directory_iterator(modelsDir)) {
        if (!p.is_regular_file())
            continue;
        if (p.path().stem() == fileName && p.path().extension() == ".3ds")
            return p.path().string();
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
    wxFileInputStream input(zipPath);
    if (!input.IsOk()) {
        if (ConsolePanel::Instance())
            ConsolePanel::Instance()->AppendMessage("GDTF: cannot open " + wxString::FromUTF8(zipPath));
        return false;
    }
    wxZipInputStream zipStream(input);
    std::unique_ptr<wxZipEntry> entry;
    while ((entry.reset(zipStream.GetNextEntry())), entry) {
        std::string filename = entry->GetName().ToStdString();
        std::string fullPath = destDir + "/" + filename;
        if (ConsolePanel::Instance())
            ConsolePanel::Instance()->AppendMessage("GDTF: extracting " +
                wxString::FromUTF8(filename));
        if (entry->IsDir()) {
            wxFileName::Mkdir(fullPath, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL);
            continue;
        }
        wxFileName::Mkdir(wxFileName(fullPath).GetPath(), wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL);
        std::ofstream output(fullPath, std::ios::binary);
        if (!output.is_open()) {
            if (ConsolePanel::Instance())
                ConsolePanel::Instance()->AppendMessage("GDTF: cannot create " + wxString::FromUTF8(fullPath));
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
                          const std::unordered_map<std::string, ModelInfo>& models,
                          const std::string& baseDir,
                          std::unordered_map<std::string, Mesh>& meshCache,
                          std::vector<GdtfObject>& outObjects)
{
    Matrix local = MatrixUtils::Identity();
    if (const char* pos = node->Attribute("Position"))
        MatrixUtils::ParseMatrix(pos, local);

    Matrix transform = MatrixUtils::Multiply(parent, local);

    if (const char* modelName = node->Attribute("Model")) {
        auto it = models.find(modelName);
        if (it != models.end()) {
            std::string path = Find3dsFile(baseDir, it->second.file);
            if (!path.empty()) {
                auto mit = meshCache.find(path);
                if (mit == meshCache.end()) {
                    Mesh mesh;
                    if (Load3DS(path, mesh)) {
                        if (ConsolePanel::Instance()) {
                            wxString msg = wxString::Format(
                                "GDTF: loaded 3DS %s (v=%zu i=%zu)",
                                wxString::FromUTF8(path),
                                mesh.vertices.size() / 3,
                                mesh.indices.size() / 3);
                            ConsolePanel::Instance()->AppendMessage(msg);
                        }
                        mit = meshCache.emplace(path, std::move(mesh)).first;
                    } else if (ConsolePanel::Instance()) {
                        ConsolePanel::Instance()->AppendMessage("GDTF: failed to load 3DS " + wxString::FromUTF8(path));
                    }
                }
                if (mit != meshCache.end()) {
                    const char* geomName = node->Attribute("Name");
                    if (ConsolePanel::Instance() && geomName)
                        ConsolePanel::Instance()->AppendMessage("GDTF: using geometry " + wxString::FromUTF8(geomName));
                    outObjects.push_back({mit->second, transform});
                }
            } else if (ConsolePanel::Instance()) {
                ConsolePanel::Instance()->AppendMessage(
                    "GDTF: missing model file " + wxString::FromUTF8(it->second.file) +
                    " in " + wxString::FromUTF8(baseDir));
            }
        }
    }

    for (tinyxml2::XMLElement* child = node->FirstChildElement(); child; child = child->NextSiblingElement()) {
        std::string n = child->Name();
        if (n == "Geometry" || n == "Axis" || n.rfind("Filter",0)==0 || n=="Beam" ||
            n=="MediaServerLayer" || n=="MediaServerCamera" || n=="MediaServerMaster" ||
            n=="Display" || n=="GeometryReference" || n=="Laser" || n=="WiringObject" ||
            n=="Inventory" || n=="Structure" || n=="Support" || n=="Magnet") {
            ParseGeometry(child, transform, models, baseDir, meshCache, outObjects);
        }
    }
}

bool LoadGdtf(const std::string& gdtfPath, std::vector<GdtfObject>& outObjects)
{
    if (ConsolePanel::Instance())
        ConsolePanel::Instance()->AppendMessage("Loading GDTF " + wxString::FromUTF8(gdtfPath));

    outObjects.clear();
    std::string tempDir = CreateTempDir();
    if (!ExtractZip(gdtfPath, tempDir)) {
        if (ConsolePanel::Instance())
            ConsolePanel::Instance()->AppendMessage("GDTF: failed to extract " + wxString::FromUTF8(gdtfPath));
        return false;
    } else if (ConsolePanel::Instance()) {
        ConsolePanel::Instance()->AppendMessage("GDTF: extracted to " + wxString::FromUTF8(tempDir));
    }

    std::string descPath = tempDir + "/description.xml";
    tinyxml2::XMLDocument doc;
    if (doc.LoadFile(descPath.c_str()) != tinyxml2::XML_SUCCESS) {
        if (ConsolePanel::Instance())
            ConsolePanel::Instance()->AppendMessage("GDTF: cannot read description.xml in " + wxString::FromUTF8(gdtfPath));
        return false;
    }

    tinyxml2::XMLElement* ft = doc.FirstChildElement("GDTF");
    if (ft) ft = ft->FirstChildElement("FixtureType");
    else ft = doc.FirstChildElement("FixtureType");
    if (!ft) {
        if (ConsolePanel::Instance())
            ConsolePanel::Instance()->AppendMessage("GDTF: invalid fixture type in " + wxString::FromUTF8(gdtfPath));
        return false;
    }

    std::unordered_map<std::string, ModelInfo> models;
    if (tinyxml2::XMLElement* modelList = ft->FirstChildElement("Models")) {
        for (tinyxml2::XMLElement* m = modelList->FirstChildElement("Model"); m; m = m->NextSiblingElement("Model")) {
            const char* name = m->Attribute("Name");
            const char* file = m->Attribute("File");
            if (name && file) {
                models[name] = { file };
                if (ConsolePanel::Instance()) {
                    ConsolePanel::Instance()->AppendMessage(
                        "GDTF: model " + wxString::FromUTF8(name) +
                        " -> " + wxString::FromUTF8(file));
                }
            }
        }
    }

    std::unordered_map<std::string, Mesh> meshCache;
    if (tinyxml2::XMLElement* geoms = ft->FirstChildElement("Geometries")) {
        for (tinyxml2::XMLElement* g = geoms->FirstChildElement(); g; g = g->NextSiblingElement()) {
            ParseGeometry(g, MatrixUtils::Identity(), models, tempDir, meshCache, outObjects);
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

