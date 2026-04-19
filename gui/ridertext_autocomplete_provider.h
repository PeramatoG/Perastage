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
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

class RiderTextAutocompleteProvider {
public:
  enum class SuggestionKind { Keyword, Dictionary };

  struct RankingWeights {
    int exactMatch = 300;
    int prefixMatch = 220;
    int substringMatch = 130;
    int fuzzyScale = 1;
    int recentUseBoost = 36;
    int contextPositionBoost = 95;
    int contextTypeBoost = 70;
  };

  struct Suggestion {
    std::string displayText;
    std::string insertText;
    SuggestionKind kind = SuggestionKind::Keyword;
    int score = 0;
  };

  RiderTextAutocompleteProvider();

  void RefreshDynamicTerms();
  void RegisterAcceptedSuggestion(const std::string &insertText);
  void SetRankingWeights(const RankingWeights &weights);
  const RankingWeights &GetRankingWeights() const;
  std::vector<Suggestion> Query(const std::string &fullText,
                                size_t cursorByteOffset,
                                size_t maxItems = 10) const;

private:
  enum class EntrySemantic { Generic, Position, Type };
  enum class QueryContext { Generic, ExpectPosition, ExpectType };

  struct Entry {
    std::string displayText;
    std::string insertText;
    std::string normalizedToken;
    SuggestionKind kind = SuggestionKind::Keyword;
    int baseScore = 0;
    EntrySemantic semantic = EntrySemantic::Generic;
  };

  void BuildStaticEntries();
  static QueryContext DetectContext(const std::string &fullText, size_t cursorByteOffset);
  static std::string ExtractPreviousToken(const std::string &text, size_t cursorByteOffset);
  static bool IsNumericToken(const std::string &token);
  static std::string ToLower(std::string value);
  static bool IsTokenDelimiter(char c);
  static std::string ExtractCurrentToken(const std::string &text, size_t cursorByteOffset);

  std::vector<Entry> staticEntries;
  std::vector<Entry> dynamicEntries;
  RankingWeights weights;
  std::unordered_map<std::string, uint64_t> recentUseTicksByToken;
  uint64_t recentUseTickCounter = 0;
};
