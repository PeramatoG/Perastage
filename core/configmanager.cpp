#include "configmanager.h"
#include "../external/json.hpp"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <chrono>
#include "mvrexporter.h"
#include "mvrimporter.h"
#include <wx/wfstream.h>
#include <wx/zipstrm.h>

ConfigManager& ConfigManager::Get()
{
    static ConfigManager instance;
    return instance;
}

// -- Config key-value access --

void ConfigManager::SetValue(const std::string& key, const std::string& value)
{
    configData[key] = value;
}

std::optional<std::string> ConfigManager::GetValue(const std::string& key) const
{
    auto it = configData.find(key);
    if (it != configData.end())
        return it->second;
    return std::nullopt;
}

bool ConfigManager::HasKey(const std::string& key) const
{
    return configData.find(key) != configData.end();
}

void ConfigManager::RemoveKey(const std::string& key)
{
    configData.erase(key);
}

void ConfigManager::ClearValues()
{
    configData.clear();
}

// -- Scene access --

MvrScene& ConfigManager::GetScene()
{
    return scene;
}

const MvrScene& ConfigManager::GetScene() const
{
    return scene;
}

// -- Persistence --

bool ConfigManager::LoadFromFile(const std::string& path)
{
    std::ifstream file(path);
    if (!file.is_open())
        return false;

    nlohmann::json j;
    try
    {
        file >> j;
    }
    catch (...)
    {
        return false;
    }

    configData = j.get<std::unordered_map<std::string, std::string>>();
    return true;
}

bool ConfigManager::SaveToFile(const std::string& path) const
{
    std::ofstream file(path);
    if (!file.is_open())
        return false;

    nlohmann::json j(configData);
    file << j.dump(4);
    return true;
}

bool ConfigManager::SaveProject(const std::string& path)
{
    namespace fs = std::filesystem;
    fs::path tempDir = fs::temp_directory_path() /
        ("PerastageProj_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()));
    fs::create_directory(tempDir);

    fs::path configPath = tempDir / "config.json";
    fs::path scenePath = tempDir / "scene.mvr";

    if (!SaveToFile(configPath.string()))
    {
        fs::remove_all(tempDir);
        return false;
    }

    MvrExporter exporter;
    if (!exporter.ExportToFile(scenePath.string()))
    {
        fs::remove_all(tempDir);
        return false;
    }

    wxFileOutputStream out(path);
    if (!out.IsOk())
    {
        fs::remove_all(tempDir);
        return false;
    }

    wxZipOutputStream zip(out);

    auto addFile = [&](const fs::path& p, const std::string& name)
    {
        auto* entry = new wxZipEntry(name);
        entry->SetMethod(wxZIP_METHOD_DEFLATE);
        zip.PutNextEntry(entry);
        std::ifstream in(p, std::ios::binary);
        char buf[4096];
        while (in.good())
        {
            in.read(buf, sizeof(buf));
            std::streamsize s = in.gcount();
            if (s > 0)
                zip.Write(buf, s);
        }
        zip.CloseEntry();
    };

    addFile(configPath, "config.json");
    addFile(scenePath, "scene.mvr");

    zip.Close();
    fs::remove_all(tempDir);
    return true;
}

bool ConfigManager::LoadProject(const std::string& path)
{
    namespace fs = std::filesystem;

    wxFileInputStream in(path);
    if (!in.IsOk())
        return false;

    wxZipInputStream zip(in);
    std::unique_ptr<wxZipEntry> entry;

    fs::path tempDir = fs::temp_directory_path() /
        ("PerastageProj_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()));
    fs::create_directory(tempDir);

    fs::path configPath;
    fs::path scenePath;

    while ((entry.reset(zip.GetNextEntry())), entry)
    {
        std::string name = entry->GetName().ToStdString();
        fs::path outPath;
        if (name == "config.json")
            outPath = tempDir / "config.json";
        else if (name == "scene.mvr")
            outPath = tempDir / "scene.mvr";
        else
            continue;

        std::ofstream out(outPath, std::ios::binary);
        char buf[4096];
        while (true)
        {
            zip.Read(buf, sizeof(buf));
            size_t bytes = zip.LastRead();
            if (bytes == 0)
                break;
            out.write(buf, bytes);
        }
        out.close();

        if (name == "config.json")
            configPath = outPath;
        else if (name == "scene.mvr")
            scenePath = outPath;
    }

    bool ok = true;
    if (!configPath.empty())
        ok &= LoadFromFile(configPath.string());
    if (!scenePath.empty())
        ok &= MvrImporter::ImportAndRegister(scenePath.string());

    fs::remove_all(tempDir);
    return ok;
}

void ConfigManager::Reset()
{
    configData.clear();
    scene.Clear();
}
