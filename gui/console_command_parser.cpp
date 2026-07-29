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
#include "console_command_parser.h"

#include <algorithm>
#include <cerrno>
#include <cctype>
#include <cstdlib>
#include <sstream>

namespace gui::console {
namespace {

// Trims ASCII whitespace from both sides of a console token string.
std::string Trim(const std::string &text) {
  const size_t start = text.find_first_not_of(" \t\n\r");
  const size_t end = text.find_last_not_of(" \t\n\r");
  if (start == std::string::npos)
    return std::string();
  return text.substr(start, end - start + 1);
}

// Converts a token to lowercase for case-insensitive command parsing.
std::string ToLower(std::string token) {
  std::transform(token.begin(), token.end(), token.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  return token;
}

// Returns true when the token is a range separator in transform value lists.
bool IsRangeSeparator(const std::string &token) {
  const std::string lower = ToLower(token);
  return lower == "t" || lower == "thru";
}


// Returns true when the token is a transform-space modifier.
bool IsLocalModifier(const std::string &token) {
  const std::string lower = ToLower(token);
  return lower == "--local" || lower == "-l";
}

// Returns true when the token is a group-rotation modifier.
bool IsGroupModifier(const std::string &token) {
  const std::string lower = ToLower(token);
  return lower == "--group" || lower == "--g";
}
// Parses a floating-point token and requires the full token to be consumed.
bool TryParseFloat(const std::string &token, float &value) {
  if (token.empty())
    return false;

  char *end = nullptr;
  errno = 0;
  const float parsed = std::strtof(token.c_str(), &end);
  if (errno != 0 || end != token.c_str() + token.size())
    return false;

  value = parsed;
  return true;
}

} // namespace

// Parses console transform values and modifiers from one command segment.
TransformCommandSegment ParseTransformCommandSegment(const std::string &text) {
  TransformCommandSegment result;
  std::string remaining = Trim(text);
  float sign = 1.0f;
  if (remaining.rfind("++", 0) == 0) {
    result.relative = true;
    remaining = Trim(remaining.substr(2));
  } else if (remaining.rfind("--", 0) == 0) {
    const std::string compactValue = remaining.substr(2);
    float parsedCompactValue = 0.0f;
    const size_t tokenEnd = compactValue.find_first_of(" \t\n\r");
    const std::string firstToken = compactValue.substr(0, tokenEnd);
    if (remaining.size() == 2 ||
        std::isspace(static_cast<unsigned char>(remaining[2])) ||
        TryParseFloat(firstToken, parsedCompactValue)) {
      result.relative = true;
      sign = -1.0f;
      remaining = Trim(remaining.substr(2));
    }
  }

  std::stringstream stream(remaining);
  std::string token;
  bool collectingRemainder = false;
  while (stream >> token) {
    if (!collectingRemainder && IsLocalModifier(token)) {
      result.space = transform_space::TransformSpace::Local;
      continue;
    }
    if (!collectingRemainder && IsGroupModifier(token)) {
      result.group = true;
      continue;
    }
    if (!collectingRemainder && IsRangeSeparator(token))
      continue;

    float value = 0.0f;
    if (!collectingRemainder && TryParseFloat(token, value)) {
      result.values.push_back(sign * value);
      continue;
    }

    if (!result.remainder.empty())
      result.remainder += ' ';
    result.remainder += token;
    collectingRemainder = true;
  }
  return result;
}

// Parses console position and rotation value lists, including optional range separators.
std::vector<float> ParseTransformValues(const std::string &text,
                                        bool &relative) {
  const auto segment = ParseTransformCommandSegment(text);
  relative = segment.relative;
  return segment.values;
}

} // namespace gui::console
