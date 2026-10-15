#pragma once

#include <cmath>
#include <locale>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>

namespace perastage::numeric {

// Parses the finite decimal prefix accepted by floating from_chars (general
// format). No leading whitespace/plus or hex syntax; callers own separators,
// optional plus signs, and whole-token validation. Never changes global locale.
template <typename Float>
bool ParseDecimalPrefix(std::string_view text, Float &value, size_t &consumed) {
  static_assert(std::is_floating_point_v<Float>);
  consumed = 0;
  size_t pos = 0;
  if (pos < text.size() && text[pos] == '-')
    ++pos;
  const auto isDigit = [](char ch) { return ch >= '0' && ch <= '9'; };
  bool hasDigit = false;
  bool nonzero = false;
  auto readDigits = [&] {
    while (pos < text.size() && isDigit(text[pos])) {
      hasDigit = true;
      nonzero = nonzero || text[pos] != '0';
      ++pos;
    }
  };
  readDigits();
  if (pos < text.size() && text[pos] == '.') {
    ++pos;
    readDigits();
  }
  if (!hasDigit)
    return false;
  const size_t significandEnd = pos;
  if (pos < text.size() && (text[pos] == 'e' || text[pos] == 'E')) {
    ++pos;
    if (pos < text.size() && (text[pos] == '+' || text[pos] == '-'))
      ++pos;
    const size_t exponentBegin = pos;
    while (pos < text.size() && isDigit(text[pos]))
      ++pos;
    // An incomplete exponent is trailing text, as with general from_chars.
    if (pos == exponentBegin)
      pos = significandEnd;
  }

  // Classic-locale extraction is available in older libc++ versions lacking
  // floating from_chars. Extract directly into Float to check its range.
  std::istringstream stream{std::string(text.substr(0, pos))};
  stream.imbue(std::locale::classic());
  Float parsed = 0;
  stream >> std::noskipws >> parsed;
  if (stream.fail() || !stream.eof() || !std::isfinite(parsed) ||
      (parsed == 0 && nonzero))
    return false; // Includes overflow and underflow to zero.
  value = parsed;
  consumed = pos;
  return true;
}

} // namespace perastage::numeric
