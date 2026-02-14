#include "asset_cache.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <sstream>

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

std::string ZipAssetCache::ensure_extracted(const std::string &archive_relative_path) {
    if (archive_relative_path.empty()) {
        return {};
    }

    const std::string normalized = normalize_archive_path(archive_relative_path);
    if (normalized.empty()) {
        return {};
    }

    const std::filesystem::path out_path = cache_dir_ / std::filesystem::u8path(normalized);
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

} // namespace peraviz
