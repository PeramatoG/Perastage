/*
 * This file is part of Perastage.
 * Copyright (C) 2026 Luisma Peramato
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
#include "ridertext_autocomplete_provider.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <string_view>
#include <unordered_set>

#include "gdtfdictionary.h"
#include "trussdictionary.h"

namespace {
struct NamedColor {
  const char *name;
  const char *hex;
};

constexpr std::array<const char *, 21> kRiderKeywords = {
    "rigging", "lx1",   "lx2",    "lx3",      "lx4",    "sides",
    "floor",   "screen", "backdrop", "truss",   "pipe",   "motor",
    "for",     "kg",    "m",      "primitive:cube",
    "primitive:cylinder", "led screen", "apply filter", "hazer", "fan"};

constexpr std::array<NamedColor, 20> kNamedColors = {{
    {"red", "#FF0000"},
    {"green", "#00FF00"},
    {"blue", "#0000FF"},
    {"amber", "#FFBF00"},
    {"warm white", "#FFD7A3"},
    {"cool white", "#F3F8FF"},
    {"white", "#FFFFFF"},
    {"cyan", "#00FFFF"},
    {"magenta", "#FF00FF"},
    {"yellow", "#FFFF00"},
    {"orange", "#FF7F00"},
    {"pink", "#FF69B4"},
    {"purple", "#8F00FF"},
    {"lavender", "#C7A3C7"},
    {"lime", "#BFFF00"},
    {"indigo", "#4B0082"},
    {"teal", "#008080"},
    {"gold", "#FFD700"},
    {"ctb", "#B8D8FF"},
    {"cto", "#FFB347"},
}};

int ComputeSubsequenceGapScore(std::string_view text, std::string_view needle) {
  if (needle.empty())
    return 0;

  size_t position = 0;
  size_t gap = 0;
  for (const char ch : needle) {
    const size_t found = text.find(ch, position);
    if (found == std::string_view::npos)
      return -1;
    gap += found - position;
    position = found + 1;
  }

  const int penalty = static_cast<int>(std::min<size_t>(gap, 80));
  return std::max(0, 80 - penalty);
}

} // namespace

RiderTextAutocompleteProvider::RiderTextAutocompleteProvider() {
  BuildStaticEntries();
  RefreshDynamicTerms();
}

void RiderTextAutocompleteProvider::BuildStaticEntries() {
  staticEntries.clear();

  for (const char *keyword : kRiderKeywords) {
    Entry entry;
    entry.displayText = keyword;
    entry.insertText = keyword;
    entry.normalizedToken = ToLower(keyword);
    entry.kind = SuggestionKind::Keyword;
    entry.baseScore = 10;
    staticEntries.push_back(std::move(entry));
  }

  for (const NamedColor &namedColor : kNamedColors) {
    Entry byName;
    byName.displayText = std::string(namedColor.name) + " (" + namedColor.hex + ")";
    byName.insertText = namedColor.name;
    byName.normalizedToken = ToLower(namedColor.name);
    byName.kind = SuggestionKind::ColorName;
    byName.baseScore = 18;
    staticEntries.push_back(std::move(byName));

    Entry byHex;
    byHex.displayText = std::string(namedColor.name) + " " + namedColor.hex;
    byHex.insertText = namedColor.hex;
    byHex.normalizedToken = ToLower(namedColor.hex);
    byHex.kind = SuggestionKind::ColorHex;
    byHex.baseScore = 16;
    staticEntries.push_back(std::move(byHex));

    Entry byRgb;
    const unsigned int r = std::stoul(std::string(namedColor.hex + 1, 2), nullptr, 16);
    const unsigned int g = std::stoul(std::string(namedColor.hex + 3, 2), nullptr, 16);
    const unsigned int b = std::stoul(std::string(namedColor.hex + 5, 2), nullptr, 16);
    byRgb.displayText = std::string("rgb(") + std::to_string(r) + "," +
                        std::to_string(g) + "," + std::to_string(b) + ")";
    byRgb.insertText = byRgb.displayText;
    byRgb.normalizedToken = ToLower(byRgb.insertText);
    byRgb.kind = SuggestionKind::ColorRgb;
    byRgb.baseScore = 12;
    staticEntries.push_back(std::move(byRgb));
  }
}

void RiderTextAutocompleteProvider::RefreshDynamicTerms() {
  dynamicEntries.clear();
  std::unordered_set<std::string> seen;

  if (const auto gdtf = GdtfDictionary::Load()) {
    for (const auto &[typeName, entryData] : *gdtf) {
      (void)entryData;
      const std::string trimmed = typeName;
      const std::string normalized = ToLower(trimmed);
      if (trimmed.empty() || !seen.insert(normalized).second)
        continue;

      Entry entry;
      entry.displayText = trimmed;
      entry.insertText = trimmed;
      entry.normalizedToken = normalized;
      entry.kind = SuggestionKind::Dictionary;
      entry.baseScore = 25;
      dynamicEntries.push_back(std::move(entry));
    }
  }

  if (const auto truss = TrussDictionary::Load()) {
    for (const auto &[modelName, filePath] : *truss) {
      (void)filePath;
      const std::string trimmed = modelName;
      const std::string normalized = ToLower(trimmed);
      if (trimmed.empty() || !seen.insert(normalized).second)
        continue;

      Entry entry;
      entry.displayText = trimmed;
      entry.insertText = trimmed;
      entry.normalizedToken = normalized;
      entry.kind = SuggestionKind::Dictionary;
      entry.baseScore = 23;
      dynamicEntries.push_back(std::move(entry));
    }
  }
}

std::vector<RiderTextAutocompleteProvider::Suggestion>
RiderTextAutocompleteProvider::Query(const std::string &fullText,
                                     const size_t cursorByteOffset,
                                     const size_t maxItems) const {
  const std::string currentToken =
      ToLower(ExtractCurrentToken(fullText, cursorByteOffset));

  if (currentToken.empty())
    return {};

  const bool colorLikeToken =
      !currentToken.empty() &&
      (currentToken[0] == '#' || currentToken.rfind("rgb", 0) == 0 ||
       currentToken.rfind("col", 0) == 0 || currentToken.rfind("red", 0) == 0);

  std::vector<Suggestion> matches;
  matches.reserve(24);

  auto matchEntry = [&](const Entry &entry) {
    int score = entry.baseScore;
    if (entry.normalizedToken == currentToken) {
      score += 300;
    } else if (entry.normalizedToken.rfind(currentToken, 0) == 0) {
      score += 220;
    } else if (entry.normalizedToken.find(currentToken) != std::string::npos) {
      score += 130;
    } else {
      const int fuzzy = ComputeSubsequenceGapScore(entry.normalizedToken, currentToken);
      if (fuzzy <= 0)
        return;
      score += fuzzy;
    }

    if (colorLikeToken &&
        (entry.kind == SuggestionKind::ColorName ||
         entry.kind == SuggestionKind::ColorHex ||
         entry.kind == SuggestionKind::ColorRgb)) {
      score += 90;
    }

    matches.push_back({entry.displayText, entry.insertText, entry.kind, score});
  };

  for (const Entry &entry : dynamicEntries)
    matchEntry(entry);
  for (const Entry &entry : staticEntries)
    matchEntry(entry);

  std::sort(matches.begin(), matches.end(), [](const Suggestion &lhs, const Suggestion &rhs) {
    if (lhs.score != rhs.score)
      return lhs.score > rhs.score;
    return lhs.displayText < rhs.displayText;
  });

  std::vector<Suggestion> unique;
  unique.reserve(matches.size());
  std::unordered_set<std::string> emitted;
  for (const Suggestion &match : matches) {
    const std::string dedupeKey = ToLower(match.insertText);
    if (!emitted.insert(dedupeKey).second)
      continue;
    unique.push_back(match);
    if (unique.size() >= maxItems)
      break;
  }

  return unique;
}

std::string RiderTextAutocompleteProvider::ToLower(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(),
                 [](const unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return value;
}

bool RiderTextAutocompleteProvider::IsTokenDelimiter(const char c) {
  switch (c) {
  case ' ':
  case '\t':
  case '\r':
  case '\n':
  case ',':
  case ';':
  case '(':
  case ')':
  case '[':
  case ']':
  case '{':
  case '}':
  case ':':
  case '"':
    return true;
  default:
    return false;
  }
}

std::string RiderTextAutocompleteProvider::ExtractCurrentToken(
    const std::string &text, const size_t cursorByteOffset) {
  if (text.empty())
    return {};

  const size_t clampedCursor = std::min(cursorByteOffset, text.size());

  size_t start = clampedCursor;
  while (start > 0 && !IsTokenDelimiter(text[start - 1]))
    --start;

  size_t end = clampedCursor;
  while (end < text.size() && !IsTokenDelimiter(text[end]))
    ++end;

  if (end <= start)
    return {};

  return text.substr(start, end - start);
}
