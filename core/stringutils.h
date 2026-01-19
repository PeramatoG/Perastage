/*
 * This file is part of Perastage.
 * Copyright (C) 2025 Luisma Peramato
 *
 * Perastage is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Perastage is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Perastage. If not, see <https://www.gnu.org/licenses/>.
 */
#pragma once

#include <cctype>
#include <string>
#include <string_view>
#include <utility>

namespace StringUtils {

inline bool NaturalLess(std::string_view a, std::string_view b) {
  struct ParsedSuffix {
    std::string_view prefix;
    long number;
    bool hasNumber;
  };

  auto extract = [](std::string_view s) -> ParsedSuffix {
    size_t i = s.size();
    while (i > 0 && std::isdigit(static_cast<unsigned char>(s[i - 1]))) {
      --i;
    }
    long num = 0;
    if (i < s.size()) {
      for (size_t j = i; j < s.size(); ++j) {
        num = (num * 10) + (s[j] - '0');
      }
    }
    return {s.substr(0, i), num, i < s.size()};
  };

  ParsedSuffix aParsed = extract(a);
  ParsedSuffix bParsed = extract(b);

  if (aParsed.prefix == bParsed.prefix &&
      (aParsed.hasNumber || bParsed.hasNumber)) {
    if (aParsed.number != bParsed.number)
      return aParsed.number < bParsed.number;
    return a.size() < b.size();
  }
  return a < b;
}

} // namespace StringUtils
