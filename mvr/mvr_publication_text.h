#pragma once
#include <string>
#include <string_view>
namespace mvr::publication {
// Retain valid Unicode codepoints; repair only malformed UTF-8 or XML 1.0
// forbidden characters. Resource filesystem identities are never passed here.
inline std::string XmlText(std::string_view input) {
  std::string out;
  for (size_t at = 0; at < input.size();) {
    const unsigned char first = input[at];
    size_t width = first < 0x80                     ? 1
                   : first >= 0xc2 && first <= 0xdf ? 2
                   : first >= 0xe0 && first <= 0xef ? 3
                   : first >= 0xf0 && first <= 0xf4 ? 4
                                                    : 0;
    unsigned value = width == 1 ? first
                     : width    ? first & ((1u << (7 - width)) - 1)
                                : 0;
    bool valid = width && at + width <= input.size();
    for (size_t i = 1; valid && i < width; ++i) {
      unsigned char c = input[at + i];
      valid = (c & 0xc0) == 0x80;
      value = (value << 6) | (c & 0x3f);
    }
    valid = valid && !(width == 2 && value < 0x80) &&
            !(width == 3 && value < 0x800) &&
            !(width == 4 && value < 0x10000) &&
            (value == 9 || value == 10 || value == 13 ||
             (value >= 0x20 && value <= 0xd7ff) ||
             (value >= 0xe000 && value <= 0xfffd) ||
             (value >= 0x10000 && value <= 0x10ffff));
    if (valid) {
      out.append(input.substr(at, width));
      at += width;
    } else {
      out.append("\xef\xbf\xbd");
      ++at;
    }
  }
  return out;
}
inline std::string DiagnosticText(std::string_view input) {
  if (XmlText(input) == input)
    return std::string(input);
  static constexpr char hex[] = "0123456789ABCDEF";
  std::string out = "original bytes (hex): ";
  out.reserve(out.size() + 2 * input.size());
  for (unsigned char byte : input) {
    out += hex[byte >> 4];
    out += hex[byte & 15];
  }
  return out;
}
} // namespace mvr::publication
