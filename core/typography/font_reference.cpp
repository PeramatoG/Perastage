#include "font_reference.h"
#include <charconv>

namespace typography {
namespace {
constexpr std::string_view kPrefix = "perastage-installed-v1:";
}

FontReference DecodeFontReference(std::string_view value) {
  FontReference plain{value};
  if (!value.starts_with(kPrefix)) return plain;
  auto encoded = value.substr(kPrefix.size());
  bool valid = true;
  const auto read = [&]() -> std::string_view {
    size_t length = 0;
    const auto colon = encoded.find(':');
    if (colon == encoded.npos || colon > 5) { valid = false; return {}; }
    const auto parsed = std::from_chars(encoded.data(), encoded.data() + colon, length);
    if (parsed.ec != std::errc{} || parsed.ptr != encoded.data() + colon ||
        length > encoded.size() - colon - 1) { valid = false; return {}; }
    const auto field = encoded.substr(colon + 1, length);
    encoded.remove_prefix(colon + 1 + length);
    return field;
  };
  FontReference reference{read()};
  if (reference.family.empty()) return plain;
  for (size_t i = 0; valid && i < reference.postScriptNames.size(); ++i) {
    reference.postScriptNames[i] = read();
    reference.fingerprints[i] = read();
  }
  return valid && encoded.empty() ? reference : plain;
}

std::string EncodeFontReference(const FontReference &reference) {
  std::string result(kPrefix);
  const auto append = [&](std::string_view field) {
    result += std::to_string(field.size()) + ":";
    result.append(field);
  };
  append(reference.family);
  for (size_t i = 0; i < reference.postScriptNames.size(); ++i) {
    append(reference.postScriptNames[i]);
    append(reference.fingerprints[i]);
  }
  return result;
}
}
