#include "utf8_utils.h"

#include <array>
#include <cstdint>
#include <iomanip>
#include <sstream>

namespace {

// Appends one Unicode code point as UTF-8.
void AppendCodePoint(std::string &out, uint32_t cp) {
  if (cp <= 0x7F) {
    out.push_back(static_cast<char>(cp));
  } else if (cp <= 0x7FF) {
    out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
    out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
  } else if (cp <= 0xFFFF) {
    out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
    out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
  } else {
    out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
    out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
  }
}

// Returns the Unicode scalar value for one Windows-1252 byte.
std::optional<uint32_t> Windows1252CodePoint(unsigned char byte) {
  static constexpr std::array<uint16_t, 32> kTable = {
      0x20AC, 0,      0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021,
      0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0,      0x017D, 0,
      0,      0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
      0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0,      0x017E, 0x0178};
  if (byte < 0x80)
    return byte;
  if (byte >= 0xA0)
    return byte;
  const uint16_t cp = kTable[byte - 0x80];
  if (cp == 0)
    return std::nullopt;
  return cp;
}

} // namespace

// Validates that text is well-formed UTF-8 and reports the first bad byte.
Utf8ValidationResult ValidateUtf8(std::string_view text) {
  for (size_t i = 0; i < text.size();) {
    const unsigned char c = static_cast<unsigned char>(text[i]);
    size_t needed = 0;
    uint32_t cp = 0;
    if (c <= 0x7F) {
      ++i;
      continue;
    } else if (c >= 0xC2 && c <= 0xDF) {
      needed = 1;
      cp = c & 0x1F;
    } else if (c >= 0xE0 && c <= 0xEF) {
      needed = 2;
      cp = c & 0x0F;
    } else if (c >= 0xF0 && c <= 0xF4) {
      needed = 3;
      cp = c & 0x07;
    } else {
      return {false, i};
    }
    if (i + needed >= text.size())
      return {false, i};
    for (size_t j = 1; j <= needed; ++j) {
      const unsigned char cc = static_cast<unsigned char>(text[i + j]);
      if ((cc & 0xC0) != 0x80)
        return {false, i + j};
      cp = (cp << 6) | (cc & 0x3F);
    }
    if ((needed == 2 && cp < 0x800) || (needed == 3 && cp < 0x10000) ||
        cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF))
      return {false, i};
    i += needed + 1;
  }
  return {true, 0};
}

// Returns true when text is well-formed UTF-8.
bool IsValidUtf8(std::string_view text) { return ValidateUtf8(text).valid; }

// Repairs invalid Windows-1252 bytes while leaving valid UTF-8 sequences intact.
std::optional<std::string> RepairWindows1252AsUtf8(std::string_view text) {
  if (IsValidUtf8(text))
    return std::string(text);
  std::string out;
  out.reserve(text.size());
  for (size_t i = 0; i < text.size();) {
    const auto valid = ValidateUtf8(text.substr(i));
    if (valid.valid) {
      out.append(text.substr(i));
      break;
    }
    if (valid.errorOffset > 0) {
      out.append(text.substr(i, valid.errorOffset));
      i += valid.errorOffset;
      continue;
    }
    const unsigned char byte = static_cast<unsigned char>(text[i++]);
    const auto cp = Windows1252CodePoint(byte);
    if (!cp)
      return std::nullopt;
    AppendCodePoint(out, *cp);
  }
  return IsValidUtf8(out) ? std::optional<std::string>(out) : std::nullopt;
}

// Escapes bytes for concise diagnostics without exposing full raw content.
std::string EscapeTextForDiagnostics(std::string_view text) {
  std::ostringstream out;
  size_t count = 0;
  for (unsigned char c : text) {
    if (count++ >= 64) {
      out << "...";
      break;
    }
    if (c >= 0x20 && c <= 0x7E && c != '\\')
      out << static_cast<char>(c);
    else
      out << "\\x" << std::uppercase << std::hex << std::setw(2)
          << std::setfill('0') << static_cast<int>(c) << std::dec;
  }
  return out.str();
}
