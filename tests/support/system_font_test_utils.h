#pragma once

#include "typography/font_resources.h"
#include <cassert>
#include <chrono>
#include <fstream>

namespace system_font_test {
inline constexpr auto sans = "Perastage Test Sans";
inline constexpr auto adobeStyle = "Perastage Test Cff";

struct Files {
  std::filesystem::path root = std::filesystem::temp_directory_path() /
      ("perastage-install-font-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  Files() { std::filesystem::create_directories(root); }
  ~Files() { std::error_code error; std::filesystem::remove_all(root, error); }

  std::filesystem::path ChangeEmbedding(std::string_view name, unsigned flags) const {
    std::string bytes;
    assert(typography::ReadFileToString(std::filesystem::path(PERASTAGE_SYSTEM_FONT_FIXTURES) /
        "PerastageTestSans.ttf", bytes));
    uint32_t offset = 0, length = 0;
    assert(typography::FindTable(bytes, 0x4f532f32, offset, length) && length >= 10);
    bytes[offset + 8] = static_cast<char>(flags >> 8);
    bytes[offset + 9] = static_cast<char>(flags & 0xff);
    const auto path = root / std::filesystem::path(std::u8string_view(reinterpret_cast<const char8_t *>(name.data()), name.size()));
    std::ofstream file(path, std::ios::binary);
    file.write(bytes.data(), bytes.size());
    assert(file.good());
    return path;
  }
};

inline typography::SystemFontRecord Sans() {
  return {std::filesystem::path(PERASTAGE_SYSTEM_FONT_FIXTURES) / "PerastageTestSans.ttf",
          0, sans, "Regular", "PerastageTestSans-Regular"};
}
inline typography::SystemFontRecord Cff() {
  return {std::filesystem::path(PERASTAGE_SYSTEM_FONT_FIXTURES) / "PerastageTestCff.otf",
          0, adobeStyle, "Regular", "PerastageTestCff-Regular"};
}
inline typography::SystemFontRecord BoldCollection() {
  return {std::filesystem::path(PERASTAGE_SYSTEM_FONT_FIXTURES) / "PerastageTestCollection.ttc",
          1, sans, "Bold", "PerastageTestSans-Bold", 700};
}
}
