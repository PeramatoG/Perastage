#pragma once

#include <filesystem>
#include <set>
#include <string>

namespace peraviz {

class ZipAssetCache {
public:
    explicit ZipAssetCache(std::string source_path);

    const std::filesystem::path &cache_dir() const;
    int extracted_assets() const;

    std::string ensure_extracted(const std::string &archive_relative_path);
    std::string ensure_gdtf_model_extracted(const std::string &model_reference);

private:
    std::filesystem::path source_path_;
    std::filesystem::path cache_dir_;
    std::set<std::string> extracted_;
};

} // namespace peraviz
