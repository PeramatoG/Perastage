#include "gdtfdictionary.h"
#include "projectutils.h"
#include "../external/json.hpp"
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <cctype>

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

std::unordered_map<std::string, std::string> Load()
{
    std::unordered_map<std::string, std::string> dict;
    fs::path file = GetDictFile();
    if (file.empty())
        return dict;
    std::ifstream in(file);
    if (!in.is_open())
        return dict;
    nlohmann::json j;
    try {
        in >> j;
    } catch (...) {
        return dict;
    }
    if (!j.is_object())
        return dict;
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

static std::string Trim(const std::string& s)
{
    const char* ws = " \t\r\n";
    size_t start = s.find_first_not_of(ws);
    if (start == std::string::npos)
        return {};
    size_t end = s.find_last_not_of(ws);
    return s.substr(start, end - start + 1);
}

static std::string ToLower(const std::string& s)
{
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return out;
}

std::optional<std::string> Get(const std::string& type)
{
    auto dict = Load();
    std::string target = ToLower(Trim(type));
    for (auto it = dict.begin(); it != dict.end(); ++it) {
        std::string key = ToLower(Trim(it->first));
        if (key != target)
            continue;
        if (!fs::exists(it->second)) {
            dict.erase(it);
            Save(dict);
            return std::nullopt;
        }
        return it->second;
    }
    return std::nullopt;
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
    auto dict = Load();
    dict[type] = dest.string();
    Save(dict);
}

} // namespace GdtfDictionary

