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
constexpr std::array<const char *, 21> kRiderKeywords = {
    "rigging", "lx1",   "lx2",    "lx3",      "lx4",    "sides",
    "floor",   "screen", "backdrop", "truss",   "pipe",   "motor",
    "for",     "kg",    "m",      "primitive:cube",
    "primitive:cylinder", "led screen", "apply filter", "hazer", "fan"};

constexpr std::array<const char *, 7> kPositionKeywords = {
    "lx1", "lx2", "lx3", "lx4", "sides", "floor", "screen"};

constexpr std::array<const char *, 6> kTypeKeywords = {
    "truss", "pipe", "motor", "primitive:cube", "primitive:cylinder", "led screen"};

template <size_t N>
bool ArrayContains(const std::array<const char *, N> &items,
                   const std::string &needle) {
  return std::find(items.begin(), items.end(), needle) != items.end();
}

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
    const std::string normalized = ToLower(keyword);
    Entry entry;
    entry.displayText = keyword;
    entry.insertText = keyword;
    entry.normalizedToken = normalized;
    entry.kind = SuggestionKind::Keyword;
    entry.baseScore = 10;
    if (ArrayContains(kPositionKeywords, normalized)) {
      entry.semantic = EntrySemantic::Position;
    } else if (ArrayContains(kTypeKeywords, normalized)) {
      entry.semantic = EntrySemantic::Type;
    }
    staticEntries.push_back(std::move(entry));
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
      entry.semantic = EntrySemantic::Type;
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
      entry.semantic = EntrySemantic::Type;
      dynamicEntries.push_back(std::move(entry));
    }
  }
}

void RiderTextAutocompleteProvider::RegisterAcceptedSuggestion(
    const std::string &insertText) {
  const std::string normalized = ToLower(insertText);
  if (normalized.empty())
    return;
  ++recentUseTickCounter;
  recentUseTicksByToken[normalized] = recentUseTickCounter;
}

void RiderTextAutocompleteProvider::SetRankingWeights(const RankingWeights &newWeights) {
  weights = newWeights;
}

const RiderTextAutocompleteProvider::RankingWeights &
RiderTextAutocompleteProvider::GetRankingWeights() const {
  return weights;
}

std::vector<RiderTextAutocompleteProvider::Suggestion>
RiderTextAutocompleteProvider::Query(const std::string &fullText,
                                     const size_t cursorByteOffset,
                                     const size_t maxItems) const {
  const std::string currentToken =
      ToLower(ExtractCurrentToken(fullText, cursorByteOffset));
  const QueryContext context = DetectContext(fullText, cursorByteOffset);

  if (currentToken.empty())
    return {};

  std::vector<Suggestion> matches;
  matches.reserve(24);

  auto matchEntry = [&](const Entry &entry) {
    int score = entry.baseScore;
    if (entry.normalizedToken == currentToken) {
      score += weights.exactMatch;
    } else if (entry.normalizedToken.rfind(currentToken, 0) == 0) {
      score += weights.prefixMatch;
    } else if (entry.normalizedToken.find(currentToken) != std::string::npos) {
      score += weights.substringMatch;
    } else {
      const int fuzzy = ComputeSubsequenceGapScore(entry.normalizedToken, currentToken);
      if (fuzzy <= 0)
        return;
      score += (fuzzy * std::max(0, weights.fuzzyScale));
    }

    if (context == QueryContext::ExpectPosition &&
        entry.semantic == EntrySemantic::Position) {
      score += weights.contextPositionBoost;
    } else if (context == QueryContext::ExpectType &&
               entry.semantic == EntrySemantic::Type) {
      score += weights.contextTypeBoost;
    }

    const auto recentIt = recentUseTicksByToken.find(entry.normalizedToken);
    if (recentIt != recentUseTicksByToken.end()) {
      const uint64_t age = recentUseTickCounter - recentIt->second;
      if (age < 1024) {
        const int decay = static_cast<int>(age / 4);
        score += std::max(0, weights.recentUseBoost - decay);
      }
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

RiderTextAutocompleteProvider::QueryContext
RiderTextAutocompleteProvider::DetectContext(const std::string &fullText,
                                             const size_t cursorByteOffset) {
  const std::string previousToken = ToLower(ExtractPreviousToken(fullText, cursorByteOffset));
  if (previousToken == "for")
    return QueryContext::ExpectPosition;
  if (IsNumericToken(previousToken))
    return QueryContext::ExpectType;
  return QueryContext::Generic;
}

std::string RiderTextAutocompleteProvider::ExtractPreviousToken(
    const std::string &text, const size_t cursorByteOffset) {
  if (text.empty() || cursorByteOffset == 0)
    return {};

  const size_t clampedCursor = std::min(cursorByteOffset, text.size());
  size_t pos = clampedCursor;
  while (pos > 0 && IsTokenDelimiter(text[pos - 1]))
    --pos;
  if (pos == 0)
    return {};

  size_t tokenStart = pos;
  while (tokenStart > 0 && !IsTokenDelimiter(text[tokenStart - 1]))
    --tokenStart;

  const std::string currentToken = ExtractCurrentToken(text, clampedCursor);
  if (tokenStart < clampedCursor && !currentToken.empty()) {
    const std::string candidate = text.substr(tokenStart, pos - tokenStart);
    if (candidate == currentToken) {
      size_t previousEnd = tokenStart;
      while (previousEnd > 0 && IsTokenDelimiter(text[previousEnd - 1]))
        --previousEnd;
      if (previousEnd == 0)
        return {};
      size_t previousStart = previousEnd;
      while (previousStart > 0 && !IsTokenDelimiter(text[previousStart - 1]))
        --previousStart;
      return text.substr(previousStart, previousEnd - previousStart);
    }
  }

  return text.substr(tokenStart, pos - tokenStart);
}

bool RiderTextAutocompleteProvider::IsNumericToken(const std::string &token) {
  if (token.empty())
    return false;
  bool sawDigit = false;
  for (const unsigned char ch : token) {
    if (std::isdigit(ch) != 0) {
      sawDigit = true;
      continue;
    }
    if (ch == '.' || ch == ',')
      continue;
    return false;
  }
  return sawDigit;
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
