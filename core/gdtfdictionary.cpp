#include "gdtfdictionary.h"
#include "projectutils.h"
#include "../external/json.hpp"
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace GdtfDictionary {

static fs::path GetDictFile()
{
    fs::path dir = fs::u8path(ProjectUtils::GetDefaultLibraryPath("fixtures"));
    if (dir.empty())
        return {};
    fs::create_directories(dir);
    fs::path file = dir / "gdtf_dictionary.json";
    if (!fs::exists(file)) {
        std::ofstream create(file);
        if (create.is_open())
            create << "{}";
    }
    return file;
}

std::optional<std::unordered_map<std::string, std::string>> Load()
{
    std::unordered_map<std::string, std::string> dict;
    fs::path file = GetDictFile();
    if (file.empty())
        return std::nullopt;
    std::ifstream in(file);
    if (!in.is_open())
        return std::nullopt;
    nlohmann::json j;
    try {
        in >> j;
    } catch (...) {
        return std::nullopt;
    }
    if (!j.is_object())
        return std::nullopt;
    for (auto it = j.begin(); it != j.end(); ++it) {
        if (it.value().is_string())
            dict[it.key()] = it.value().get<std::string>();
    }
    return dict;
}

void Save(const std::unordered_map<std::string, std::string>& dict)
{
    fs::path file = GetDictFile();
    if (file.empty())
        return;
    nlohmann::json j;
    for (const auto& [type, path] : dict)
        j[type] = path;
    std::ofstream out(file);
    if (!out.is_open())
        return;
    out << j.dump(4);
}

std::optional<std::string> Get(const std::string& type)
{
    auto dictOpt = Load();
    if (!dictOpt)
        return std::nullopt;
    auto& dict = *dictOpt;
    auto it = dict.find(type);
    if (it == dict.end())
        return std::nullopt;
    if (!fs::exists(it->second)) {
        dict.erase(it);
        Save(dict);
        return std::nullopt;
    }
    return it->second;
}

void Update(const std::string& type, const std::string& gdtfPath)
{
    if (type.empty() || gdtfPath.empty())
        return;
    fs::path src = fs::u8path(gdtfPath);
    if (!fs::exists(src))
        return;
    fs::path file = GetDictFile();
    if (file.empty())
        return;
    fs::path dir = file.parent_path();
    fs::path dest = dir / src.filename();
    try {
        fs::copy_file(src, dest, fs::copy_options::overwrite_existing);
    } catch (...) {
        // ignore copy errors
    }
    auto dictOpt = Load();
    if (!dictOpt)
        return; // avoid overwriting existing dictionary on load failure
    auto& dict = *dictOpt;
    dict[type] = dest.string();
    Save(dict);
}

} // namespace GdtfDictionary

