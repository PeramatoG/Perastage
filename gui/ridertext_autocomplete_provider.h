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
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

class RiderTextAutocompleteProvider {
public:
  enum class SuggestionKind { Keyword, Dictionary, ColorName, ColorHex, ColorRgb };

  struct Suggestion {
    std::string displayText;
    std::string insertText;
    SuggestionKind kind = SuggestionKind::Keyword;
    int score = 0;
    std::string colorHex;
  };

  struct RankingWeights {
    int exactMatchBoost = 300;
    int prefixBoost = 220;
    int containsBoost = 130;
    int colorContextBoost = 90;
    int positionContextBoost = 80;
    int fixtureContextBoost = 60;
    int recentUseMultiplier = 25;
  };

  RiderTextAutocompleteProvider();

  void RefreshDynamicTerms();
  void RecordSuggestionAccepted(const std::string &insertText);
  void SetRankingWeights(const RankingWeights &weights);
  RankingWeights GetRankingWeights() const;
  std::vector<Suggestion> Query(const std::string &fullText,
                                size_t cursorByteOffset,
                                size_t maxItems = 10) const;

private:
  struct Entry {
    std::string displayText;
    std::string insertText;
    std::string normalizedToken;
    SuggestionKind kind = SuggestionKind::Keyword;
    int baseScore = 0;
  };

  void BuildStaticEntries();
  static std::string ToLower(std::string value);
  static bool IsTokenDelimiter(char c);
  static std::string ExtractCurrentToken(const std::string &text, size_t cursorByteOffset);
  static std::optional<std::string> ParseColorHexFromToken(const Entry &entry);
  static std::optional<std::string> DetectContextToken(const std::string &fullText,
                                                       size_t cursorByteOffset);

  std::vector<Entry> staticEntries;
  std::vector<Entry> dynamicEntries;
  std::unordered_map<std::string, int> usageCountByToken;
  RankingWeights rankingWeights;
};
