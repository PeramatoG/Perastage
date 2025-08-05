#pragma once

#include <cctype>
#include <cstdlib>
#include <string>
#include <utility>

namespace StringUtils {

inline bool NaturalLess(const std::string &a, const std::string &b) {
  auto extract = [](const std::string &s) -> std::pair<std::string, long> {
    size_t i = s.size();
    while (i > 0 && std::isdigit(static_cast<unsigned char>(s[i - 1]))) {
      --i;
    }
    long num = 0;
    if (i < s.size()) {
      num = std::strtol(s.c_str() + i, nullptr, 10);
    }
    return {s.substr(0, i), num};
  };

  auto [prefixA, numA] = extract(a);
  auto [prefixB, numB] = extract(b);

  bool aHasNum = prefixA.size() != a.size();
  bool bHasNum = prefixB.size() != b.size();
  if (prefixA == prefixB && (aHasNum || bHasNum)) {
    if (numA != numB)
      return numA < numB;
    return a.size() < b.size();
  }
  return a < b;
}

} // namespace StringUtils

