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

#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <string>
#include <vector>

namespace mvr {
namespace gdtf_import_matching {

// Trims whitespace from an MVR fixture identity candidate.
inline std::string TrimFixtureIdentity(const std::string &value) {
  const char *whitespace = " \t\r\n";
  const size_t start = value.find_first_not_of(whitespace);
  if (start == std::string::npos)
    return {};
  const size_t end = value.find_last_not_of(whitespace);
  return value.substr(start, end - start + 1);
}

// Lowercases ASCII text for fixture and manufacturer comparisons.
inline std::string ToLowerAscii(std::string text) {
  std::transform(text.begin(), text.end(), text.begin(), [](unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });
  return text;
}

// Extracts ordered digits to reject ambiguous partial matches.
inline std::string ExtractDigitSignature(const std::string &text) {
  std::string digits;
  for (unsigned char ch : text) {
    if (std::isdigit(ch))
      digits.push_back(static_cast<char>(ch));
  }
  return digits;
}

// Normalizes a GDTF catalog or request value into a compact comparison key.
inline std::string NormalizeForGdtfMatch(const std::string &text) {
  static const std::array<std::string, 16> kSuffixes = {
      " lighting", " light", " gmbh",   " ltd",         " inc", " corp",
      " co",       " llc",   " electronics", " ag",     " sa",  " sl",
      " bv",       " nv",    " s.a.",   " s.l."};
  std::string normalized = ToLowerAscii(TrimFixtureIdentity(text));
  bool removed = false;
  do {
    removed = false;
    for (const auto &suffix : kSuffixes) {
      if (normalized.size() >= suffix.size() &&
          normalized.rfind(suffix) == normalized.size() - suffix.size()) {
        normalized = TrimFixtureIdentity(normalized.substr(0, normalized.size() - suffix.size()));
        removed = true;
      }
    }
  } while (removed);

  std::string compact;
  compact.reserve(normalized.size());
  for (unsigned char ch : normalized) {
    if (std::isalnum(ch))
      compact.push_back(static_cast<char>(ch));
  }
  return compact;
}

// Removes parenthesized variant suffixes from fixture names.
inline std::string StripParenthesizedSections(const std::string &text) {
  std::string stripped;
  stripped.reserve(text.size());
  int depth = 0;
  for (char ch : text) {
    if (ch == '(') {
      ++depth;
      continue;
    }
    if (ch == ')') {
      if (depth > 0)
        --depth;
      continue;
    }
    if (depth == 0)
      stripped.push_back(ch);
  }
  return TrimFixtureIdentity(stripped);
}

// Detects version-like fixture-name tokens.
inline bool IsLikelyVersionToken(const std::string &token) {
  if (token.empty())
    return false;
  const bool allDigits = std::all_of(token.begin(), token.end(),
                                     [](unsigned char ch) { return std::isdigit(ch); });
  if (allDigits)
    return true;

  if (token.size() <= 6) {
    const std::string roman = ToLowerAscii(token);
    const bool allRoman = std::all_of(roman.begin(), roman.end(), [](unsigned char ch) {
      return ch == 'i' || ch == 'v' || ch == 'x' || ch == 'l' || ch == 'c' ||
             ch == 'd' || ch == 'm';
    });
    if (allRoman)
      return true;
  }
  return false;
}

// Builds a core fixture-name key without variant tokens.
inline std::string BuildCoreFixtureNameKey(const std::string &text) {
  const std::string stripped = StripParenthesizedSections(text);
  const std::string lower = ToLowerAscii(stripped);
  std::vector<std::string> tokens;
  std::string current;
  for (unsigned char ch : lower) {
    if (std::isalnum(ch)) {
      current.push_back(static_cast<char>(ch));
    } else if (!current.empty()) {
      tokens.push_back(current);
      current.clear();
    }
  }
  if (!current.empty())
    tokens.push_back(current);

  std::string compact;
  for (const auto &token : tokens) {
    if (IsLikelyVersionToken(token))
      continue;
    compact += token;
  }
  return compact;
}

enum class FixtureNameMatchTier {
  None = 0,
  Partial = 1,
  CoreName = 2,
  ParenthesisStripped = 3,
  ExactNormalized = 4
};

// Classifies fixture-name confidence before tie-breakers.
inline FixtureNameMatchTier ComputeFixtureNameMatchTier(
    const std::string &catalogFixtureName,
    const std::string &requestedFixtureName) {
  const std::string catalogNormalized = NormalizeForGdtfMatch(catalogFixtureName);
  const std::string requestedNormalized = NormalizeForGdtfMatch(requestedFixtureName);

  if (catalogNormalized.empty() || requestedNormalized.empty())
    return FixtureNameMatchTier::None;
  if (catalogNormalized == requestedNormalized)
    return FixtureNameMatchTier::ExactNormalized;

  const std::string catalogNoParentheses =
      NormalizeForGdtfMatch(StripParenthesizedSections(catalogFixtureName));
  const std::string requestedNoParentheses =
      NormalizeForGdtfMatch(StripParenthesizedSections(requestedFixtureName));
  if (!catalogNoParentheses.empty() && !requestedNoParentheses.empty() &&
      catalogNoParentheses == requestedNoParentheses) {
    return FixtureNameMatchTier::ParenthesisStripped;
  }

  const std::string catalogCoreName = BuildCoreFixtureNameKey(catalogFixtureName);
  const std::string requestedCoreName = BuildCoreFixtureNameKey(requestedFixtureName);
  if (!catalogCoreName.empty() && !requestedCoreName.empty() &&
      catalogCoreName == requestedCoreName) {
    return FixtureNameMatchTier::CoreName;
  }

  const auto hasContainsMatch = [](const std::string &lhs,
                                   const std::string &rhs) -> bool {
    if (lhs.empty() || rhs.empty())
      return false;
    return (lhs.size() >= 4 && rhs.find(lhs) != std::string::npos) ||
           (rhs.size() >= 4 && lhs.find(rhs) != std::string::npos);
  };

  const bool containsMatch =
      (catalogNormalized.size() >= 5 &&
       requestedNormalized.find(catalogNormalized) != std::string::npos) ||
      (requestedNormalized.size() >= 5 &&
       catalogNormalized.find(requestedNormalized) != std::string::npos) ||
      hasContainsMatch(catalogCoreName, requestedCoreName) ||
      hasContainsMatch(catalogNoParentheses, requestedNoParentheses) ||
      hasContainsMatch(catalogNormalized, requestedNoParentheses) ||
      hasContainsMatch(catalogNoParentheses, requestedNormalized);
  if (!containsMatch)
    return FixtureNameMatchTier::None;

  const std::string catalogDigits = ExtractDigitSignature(catalogNormalized);
  const std::string requestedDigits = ExtractDigitSignature(requestedNormalized);
  if (!catalogDigits.empty() && !requestedDigits.empty() &&
      catalogDigits != requestedDigits) {
    return FixtureNameMatchTier::None;
  }

  return FixtureNameMatchTier::Partial;
}

struct DownloadCandidateRank {
  FixtureNameMatchTier nameTier = FixtureNameMatchTier::None;
  bool footprintMatch = false;
  bool manufacturerMatch = false;
  long long recency = 0;
  float rating = 0.0f;
};

// Compares download candidates by tier and tie-breakers.
inline bool IsBetterDownloadCandidate(const DownloadCandidateRank &candidate,
                                      const DownloadCandidateRank &currentBest) {
  if (candidate.nameTier != currentBest.nameTier)
    return static_cast<int>(candidate.nameTier) > static_cast<int>(currentBest.nameTier);
  if (candidate.footprintMatch != currentBest.footprintMatch)
    return candidate.footprintMatch;
  if (candidate.manufacturerMatch != currentBest.manufacturerMatch)
    return candidate.manufacturerMatch;
  if (candidate.recency != currentBest.recency)
    return candidate.recency > currentBest.recency;
  return candidate.rating > currentBest.rating;
}

// Normalizes path separators before deriving a fixture identity from GDTFSpec.
inline std::string NormalizeGdtfSpecSeparators(std::string gdtfSpec) {
  std::replace(gdtfSpec.begin(), gdtfSpec.end(), '\\', '/');
  return gdtfSpec;
}

// Extracts the most useful fixture name candidate from an MVR GDTFSpec value.
inline std::string ExtractFixtureNameFromGdtfSpec(const std::string &gdtfSpec) {
  const std::string normalized = TrimFixtureIdentity(NormalizeGdtfSpecSeparators(gdtfSpec));
  if (normalized.empty())
    return {};
  const std::filesystem::path specPath(normalized);
  std::string stem = specPath.filename().stem().string();
  return TrimFixtureIdentity(stem);
}

// Selects the original MVR fixture identity to use for automatic GDTF matching.
inline std::string SelectRequestedFixtureName(const std::string &rawFixtureNodeName,
                                              const std::string &rawGdtfSpec) {
  const std::string fixtureNodeName = TrimFixtureIdentity(rawFixtureNodeName);
  if (!fixtureNodeName.empty())
    return fixtureNodeName;
  return ExtractFixtureNameFromGdtfSpec(rawGdtfSpec);
}

// Selects the fixture name searched in the GDTF catalog, falling back to type.
inline std::string SelectDownloadSearchFixtureName(const std::string &requestedFixtureName,
                                                   const std::string &fixtureTypeName) {
  const std::string requested = TrimFixtureIdentity(requestedFixtureName);
  if (!requested.empty())
    return requested;
  return TrimFixtureIdentity(fixtureTypeName);
}

} // namespace gdtf_import_matching
} // namespace mvr
