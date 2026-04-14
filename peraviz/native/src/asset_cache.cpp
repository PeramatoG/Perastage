#include "asset_cache.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <optional>
#include <sstream>
#include <vector>

#include <wx/filename.h>
#include <wx/wfstream.h>
#include <wx/zipstrm.h>

namespace {

std::string to_lower_ascii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

std::string normalize_archive_path(const std::string &raw) {
    std::string out = raw;
    std::replace(out.begin(), out.end(), '\\', '/');
    while (!out.empty() && (out.front() == '/' || out.front() == '.')) {
        out.erase(out.begin());
    }
    return out;
}

std::string sanitize_path_component_for_fs(std::string component) {
    for (char &ch : component) {
        const unsigned char c = static_cast<unsigned char>(ch);
        const bool is_control = c < 32;
        const bool is_forbidden =
            ch == '<' || ch == '>' || ch == ':' || ch == '"' || ch == '|' ||
            ch == '?' || ch == '*' || ch == '\\' || ch == '/';
        if (is_control || is_forbidden) {
            ch = '_';
        }
    }

    while (!component.empty() &&
           (component.back() == ' ' || component.back() == '.')) {
        component.pop_back();
    }

    if (component.empty()) {
        return "_";
    }
    return component;
}

std::filesystem::path cache_safe_relative_path(const std::string &normalized_archive_path) {
    std::filesystem::path normalized =
        std::filesystem::u8path(normalized_archive_path).lexically_normal();

    std::filesystem::path safe_path;
    for (const std::filesystem::path &part : normalized) {
        const std::string component = part.u8string();
        if (component.empty() || component == ".") {
            continue;
        }
        if (component == "..") {
            continue;
        }
        safe_path /= std::filesystem::u8path(sanitize_path_component_for_fs(component));
    }
    return safe_path;
}

std::string trim_ascii(std::string value) {
    const auto is_space = [](unsigned char c) {
        return std::isspace(c) != 0;
    };

    while (!value.empty() && is_space(static_cast<unsigned char>(value.front()))) {
        value.erase(value.begin());
    }
    while (!value.empty() && is_space(static_cast<unsigned char>(value.back()))) {
        value.pop_back();
    }
    return value;
}

std::string path_stem_lower(const std::string &normalized_path) {
    const std::filesystem::path path = std::filesystem::u8path(normalized_path);
    return to_lower_ascii(path.stem().u8string());
}

std::string path_extension_lower(const std::string &normalized_path) {
    const std::filesystem::path path = std::filesystem::u8path(normalized_path);
    return to_lower_ascii(path.extension().u8string());
}

bool is_supported_model_extension(const std::string &extension_lower) {
    return extension_lower == ".3ds" || extension_lower == ".glb" ||
           extension_lower == ".gltf" || extension_lower == ".obj" ||
           extension_lower == ".dae" || extension_lower == ".fbx" ||
           extension_lower == ".stl";
}

bool is_supported_texture_or_scene_dependency_extension(const std::string &extension_lower) {
    return extension_lower == ".png" || extension_lower == ".jpg" ||
           extension_lower == ".jpeg" || extension_lower == ".bmp" ||
           extension_lower == ".tga" || extension_lower == ".gif" ||
           extension_lower == ".webp" || extension_lower == ".hdr" ||
           extension_lower == ".dds" || extension_lower == ".ktx2" ||
           extension_lower == ".mtl" || extension_lower == ".bin";
}

std::string gdtf_lookup_key(const std::string &archive_path) {
    const std::string normalized = trim_ascii(normalize_archive_path(archive_path));
    if (normalized.empty()) {
        return {};
    }

    std::filesystem::path path = std::filesystem::u8path(normalized);
    std::string stem = trim_ascii(path.stem().u8string());
    std::string extension = to_lower_ascii(path.extension().u8string());
    if (extension.empty()) {
        extension = ".gdtf";
    }
    if (extension != ".gdtf") {
        return {};
    }

    return to_lower_ascii(stem + extension);
}

std::string hash_file_contents(const std::filesystem::path &path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return "missing";
    }

    std::uint64_t hash = 1469598103934665603ULL;
    constexpr std::uint64_t prime = 1099511628211ULL;
    char buffer[4096];
    while (input.read(buffer, sizeof(buffer)) || input.gcount() > 0) {
        for (std::streamsize i = 0; i < input.gcount(); ++i) {
            hash ^= static_cast<unsigned char>(buffer[i]);
            hash *= prime;
        }
    }

    std::ostringstream ss;
    ss << std::hex << hash;
    return ss.str();
}

std::string parent_archive_dir(const std::string &normalized_path) {
    const std::filesystem::path path = std::filesystem::u8path(normalized_path);
    const std::filesystem::path parent = path.parent_path();
    if (parent.empty()) {
        return {};
    }
    std::string value = normalize_archive_path(parent.u8string());
    if (!value.empty() && value.back() != '/') {
        value.push_back('/');
    }
    return value;
}

} // namespace

namespace peraviz {

ZipAssetCache::ZipAssetCache(std::string source_path)
    : source_path_(std::filesystem::u8path(source_path)) {
    const std::filesystem::path base = std::filesystem::temp_directory_path() / "peraviz_cache";
    const std::string source_name = source_path_.filename().u8string();
    const std::string cache_key = source_name + "_" + hash_file_contents(source_path_);
    cache_dir_ = base / cache_key;
    std::error_code ec;
    std::filesystem::create_directories(cache_dir_, ec);
}

const std::filesystem::path &ZipAssetCache::cache_dir() const {
    return cache_dir_;
}

int ZipAssetCache::extracted_assets() const {
    return static_cast<int>(extracted_.size());
}

static void extract_related_model_assets(ZipAssetCache &cache,
                                         const std::filesystem::path &source_path,
                                         const std::string &model_archive_path) {
    const std::string normalized_model_path = normalize_archive_path(model_archive_path);
    if (normalized_model_path.empty()) {
        return;
    }

    const std::string model_dir = parent_archive_dir(normalized_model_path);
    wxFileInputStream input(wxString::FromUTF8(source_path.u8string().c_str()));
    if (!input.IsOk()) {
        return;
    }

    wxZipInputStream zip(input);
    std::unique_ptr<wxZipEntry> entry;
    while ((entry.reset(zip.GetNextEntry())), entry) {
        const std::string entry_name = normalize_archive_path(entry->GetName().ToUTF8().data());
        if (entry_name.empty()) {
            continue;
        }
        if (!model_dir.empty()) {
            if (entry_name.rfind(model_dir, 0) != 0) {
                continue;
            }
        } else if (entry_name.find('/') != std::string::npos) {
            continue;
        }

        const std::string extension = path_extension_lower(entry_name);
        if (!is_supported_texture_or_scene_dependency_extension(extension)) {
            continue;
        }

        cache.ensure_extracted(entry_name);
    }
}

std::string ZipAssetCache::ensure_extracted(const std::string &archive_relative_path) {
    if (archive_relative_path.empty()) {
        return {};
    }

    const std::string normalized = normalize_archive_path(archive_relative_path);
    if (normalized.empty()) {
        return {};
    }

    const std::filesystem::path safe_relative = cache_safe_relative_path(normalized);
    if (safe_relative.empty()) {
        return {};
    }

    const std::filesystem::path out_path = cache_dir_ / safe_relative;
    std::error_code ec;
    std::filesystem::create_directories(out_path.parent_path(), ec);
    if (std::filesystem::exists(out_path)) {
        return out_path.u8string();
    }

    wxFileInputStream input(wxString::FromUTF8(source_path_.u8string().c_str()));
    if (!input.IsOk()) {
        return {};
    }

    const std::string target_lower = to_lower_ascii(normalized);
    wxZipInputStream zip(input);
    std::unique_ptr<wxZipEntry> entry;
    while ((entry.reset(zip.GetNextEntry())), entry) {
        const std::string entry_name = normalize_archive_path(entry->GetName().ToUTF8().data());
        if (to_lower_ascii(entry_name) != target_lower) {
            continue;
        }

        wxFileName file_name(wxString::FromUTF8(out_path.u8string().c_str()));
        file_name.Mkdir(wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL);

        wxFileOutputStream output(wxString::FromUTF8(out_path.u8string().c_str()));
        if (!output.IsOk()) {
            return {};
        }

        char buffer[8192];
        while (!zip.Eof()) {
            zip.Read(buffer, sizeof(buffer));
            const size_t bytes = zip.LastRead();
            if (bytes == 0) {
                break;
            }
            output.Write(buffer, bytes);
        }

        extracted_.insert(normalized);
        return out_path.u8string();
    }

    return {};
}

std::string ZipAssetCache::ensure_archive_file_extracted(const std::string &file_name) {
    const std::string normalized_file = to_lower_ascii(trim_ascii(normalize_archive_path(file_name)));
    if (normalized_file.empty()) {
        return {};
    }

    if (const std::string direct = ensure_extracted(normalized_file); !direct.empty()) {
        return direct;
    }

    wxFileInputStream input(wxString::FromUTF8(source_path_.u8string().c_str()));
    if (!input.IsOk()) {
        return {};
    }

    wxZipInputStream zip(input);
    std::unique_ptr<wxZipEntry> entry;
    while ((entry.reset(zip.GetNextEntry())), entry) {
        const std::string entry_name = normalize_archive_path(entry->GetName().ToUTF8().data());
        const std::string entry_name_lower = to_lower_ascii(entry_name);
        if (entry_name_lower != normalized_file &&
            entry_name_lower.rfind("/" + normalized_file) == std::string::npos) {
            continue;
        }

        return ensure_extracted(entry_name);
    }

    return {};
}

std::string ZipAssetCache::ensure_gdtf_spec_extracted(const std::string &gdtf_spec) {
    const std::string normalized = trim_ascii(normalize_archive_path(gdtf_spec));
    if (normalized.empty()) {
        return {};
    }

    const std::filesystem::path spec_path = std::filesystem::u8path(normalized);
    const std::string extension = to_lower_ascii(spec_path.extension().u8string());

    std::vector<std::string> candidates;
    candidates.push_back(normalized);
    if (extension.empty()) {
        candidates.push_back(normalized + ".gdtf");
    }

    for (const std::string &candidate : candidates) {
        if (const std::string extracted = ensure_extracted(candidate); !extracted.empty()) {
            return extracted;
        }
    }

    const std::string expected_key = gdtf_lookup_key(normalized);
    const std::string expected_filename =
        to_lower_ascii(trim_ascii(spec_path.filename().u8string()));

    wxFileInputStream input(wxString::FromUTF8(source_path_.u8string().c_str()));
    if (!input.IsOk()) {
        return {};
    }

    std::optional<std::string> key_match;
    std::optional<std::string> file_name_match;

    wxZipInputStream zip(input);
    std::unique_ptr<wxZipEntry> entry;
    while ((entry.reset(zip.GetNextEntry())), entry) {
        const std::string entry_name = normalize_archive_path(entry->GetName().ToUTF8().data());
        if (entry_name.empty()) {
            continue;
        }

        const std::filesystem::path entry_path = std::filesystem::u8path(entry_name);
        const std::string entry_extension = to_lower_ascii(entry_path.extension().u8string());
        if (entry_extension != ".gdtf") {
            continue;
        }

        if (!expected_key.empty() && gdtf_lookup_key(entry_name) == expected_key) {
            key_match = entry_name;
            break;
        }

        if (!expected_filename.empty()) {
            const std::string entry_file_name =
                to_lower_ascii(trim_ascii(entry_path.filename().u8string()));
            if (!file_name_match.has_value() && entry_file_name == expected_filename) {
                file_name_match = entry_name;
            }
        }
    }

    if (key_match.has_value()) {
        return ensure_extracted(*key_match);
    }
    if (file_name_match.has_value()) {
        return ensure_extracted(*file_name_match);
    }
    return {};
}

std::string ZipAssetCache::ensure_mvr_model_extracted(const std::string &model_reference) {
    const std::string normalized = normalize_archive_path(model_reference);
    if (normalized.empty()) {
        return {};
    }

    const std::filesystem::path model_path = std::filesystem::u8path(normalized);
    const std::string stem = model_path.stem().u8string();
    const std::string ext = path_extension_lower(normalized);

    std::vector<std::string> candidates;
    if (is_supported_model_extension(ext)) {
        candidates.push_back(normalized);
        candidates.push_back("models/" + ext.substr(1) + "/" + stem + ext);
    } else {
        candidates.push_back(normalized + ".3ds");
        candidates.push_back(normalized + ".glb");
        candidates.push_back(normalized + ".gltf");
        candidates.push_back(normalized + ".obj");
        candidates.push_back(normalized + ".dae");
        candidates.push_back(normalized + ".fbx");
        candidates.push_back(normalized + ".stl");
        candidates.push_back("models/3ds/" + stem + ".3ds");
        candidates.push_back("models/glb/" + stem + ".glb");
        candidates.push_back("models/gltf/" + stem + ".gltf");
        candidates.push_back("models/obj/" + stem + ".obj");
        candidates.push_back("models/dae/" + stem + ".dae");
        candidates.push_back("models/fbx/" + stem + ".fbx");
        candidates.push_back("models/stl/" + stem + ".stl");
    }

    for (const std::string &candidate : candidates) {
        if (const std::string extracted = ensure_extracted(candidate); !extracted.empty()) {
            extract_related_model_assets(*this, source_path_, candidate);
            return extracted;
        }
    }

    wxFileInputStream input(wxString::FromUTF8(source_path_.u8string().c_str()));
    if (!input.IsOk()) {
        return {};
    }

    const std::string stem_lower = to_lower_ascii(trim_ascii(stem));
    wxZipInputStream zip(input);
    std::unique_ptr<wxZipEntry> entry;
    std::optional<std::string> best_entry;
    while ((entry.reset(zip.GetNextEntry())), entry) {
        const std::string entry_name = normalize_archive_path(entry->GetName().ToUTF8().data());
        if (path_stem_lower(entry_name) != stem_lower) {
            continue;
        }

        const std::string entry_ext = path_extension_lower(entry_name);
        if (!is_supported_model_extension(entry_ext)) {
            continue;
        }

        if (!best_entry.has_value() || entry_ext == ".3ds") {
            best_entry = entry_name;
            if (entry_ext == ".3ds") {
                break;
            }
        }
    }

    if (!best_entry.has_value()) {
        return {};
    }
    extract_related_model_assets(*this, source_path_, *best_entry);
    return ensure_extracted(*best_entry);
}

std::string ZipAssetCache::ensure_gdtf_model_extracted(const std::string &model_reference) {
    if (model_reference.empty()) {
        return {};
    }

    const std::string normalized = normalize_archive_path(model_reference);
    if (normalized.empty()) {
        return {};
    }

    const std::filesystem::path model_path = std::filesystem::u8path(normalized);
    const std::string stem = model_path.stem().u8string();
    const std::string ext = path_extension_lower(normalized);

    std::vector<std::string> candidates;
    if (is_supported_model_extension(ext)) {
        candidates.push_back(normalized);
        candidates.push_back("models/" + ext.substr(1) + "/" + stem + ext);
    } else {
        candidates.push_back(normalized + ".3ds");
        candidates.push_back(normalized + ".glb");
        candidates.push_back(normalized + ".gltf");
        candidates.push_back(normalized + ".obj");
        candidates.push_back(normalized + ".dae");
        candidates.push_back(normalized + ".fbx");
        candidates.push_back(normalized + ".stl");
        candidates.push_back("models/3ds/" + stem + ".3ds");
        candidates.push_back("models/glb/" + stem + ".glb");
        candidates.push_back("models/gltf/" + stem + ".gltf");
        candidates.push_back("models/obj/" + stem + ".obj");
        candidates.push_back("models/dae/" + stem + ".dae");
        candidates.push_back("models/fbx/" + stem + ".fbx");
        candidates.push_back("models/stl/" + stem + ".stl");
    }

    for (const std::string &candidate : candidates) {
        const std::string extracted = ensure_extracted(candidate);
        if (!extracted.empty()) {
            extract_related_model_assets(*this, source_path_, candidate);
            return extracted;
        }
    }

    wxFileInputStream input(wxString::FromUTF8(source_path_.u8string().c_str()));
    if (!input.IsOk()) {
        return {};
    }

    const std::string stem_lower = to_lower_ascii(stem);
    wxZipInputStream zip(input);
    std::unique_ptr<wxZipEntry> entry;
    std::optional<std::string> best_entry;
    while ((entry.reset(zip.GetNextEntry())), entry) {
        const std::string entry_name = normalize_archive_path(entry->GetName().ToUTF8().data());
        if (path_stem_lower(entry_name) != stem_lower) {
            continue;
        }

        const std::string entry_ext = path_extension_lower(entry_name);
        const bool supported = is_supported_model_extension(entry_ext);
        if (!supported) {
            continue;
        }

        if (!best_entry.has_value() || entry_ext == ".3ds") {
            best_entry = entry_name;
            if (entry_ext == ".3ds") {
                break;
            }
        }
    }

    if (!best_entry.has_value()) {
        return {};
    }

    extract_related_model_assets(*this, source_path_, *best_entry);
    return ensure_extracted(*best_entry);
}

} // namespace peraviz
