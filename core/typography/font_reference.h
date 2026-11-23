#pragma once

#include <array>
#include <string>
#include <string_view>

namespace typography {
struct FontReference {
  std::string_view family;
  std::array<std::string_view, 4> postScriptNames{}, fingerprints{};
  static constexpr size_t Variant(bool bold, bool italic) {
    return (italic ? 2 : 0) + (bold ? 1 : 0);
  }
  std::string_view PostScriptName(bool bold, bool italic) const {
    return postScriptNames[Variant(bold, italic)];
  }
};

FontReference DecodeFontReference(std::string_view value);
std::string EncodeFontReference(const FontReference &reference);
}
