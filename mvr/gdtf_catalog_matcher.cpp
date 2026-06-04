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
#include "gdtf_catalog_matcher.h"

#include <algorithm>
#include <array>
#include <cctype>

namespace mvr {
namespace gdtf_catalog_matcher {

// Trims whitespace from an MVR fixture identity candidate.
std::string TrimFixtureIdentity(const std::string &value) {
  const char *whitespace = " \t\r\n";
  const size_t start = value.find_first_not_of(whitespace);
  if (start == std::string::npos)
    return {};
  const size_t end = value.find_last_not_of(whitespace);
  return value.substr(start, end - start + 1);
}

// Lowercases ASCII text for fixture and manufacturer comparisons.
std::string ToLowerAscii(std::string text) {
  std::transform(text.begin(), text.end(), text.begin(), [](unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });
  return text;
}

// Extracts ordered digits to reject ambiguous partial matches.
std::string ExtractDigitSignature(const std::string &text) {
  std::string digits;
  for (unsigned char ch : text) {
    if (std::isdigit(ch))
      digits.push_back(static_cast<char>(ch));
  }
  return digits;
}

// Normalizes a GDTF catalog or request value into a compact comparison key.
std::string NormalizeForGdtfMatch(const std::string &text) {
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
std::string StripParenthesizedSections(const std::string &text) {
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
bool IsLikelyVersionToken(const std::string &token) {
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
std::string BuildCoreFixtureNameKey(const std::string &text) {
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

// Classifies fixture-name confidence before tie-breakers.
FixtureNameMatchTier ComputeFixtureNameMatchTier(
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

// Scores how well a requested GDTF mode aligns with catalog modes and footprint evidence.
GdtfModeMatchScore ComputeGdtfModeMatchScore(
    const std::string &requestedMode,
    const std::vector<GdtfCatalogModeCandidate> &catalogModes,
    int requestedFootprint) {
  GdtfModeMatchScore bestScore;
  const std::string requestedNormalized = NormalizeForGdtfMatch(requestedMode);
  const std::string requestedDigits = ExtractDigitSignature(requestedNormalized);
  std::string footprintModeName;
  bool selectedModeHasFootprint = false;

  for (const auto &mode : catalogModes) {
    const bool footprintMatch = requestedFootprint > 0 && mode.footprint == requestedFootprint;
    if (footprintMatch) {
      bestScore.footprintMatch = true;
      if (footprintModeName.empty())
        footprintModeName = mode.name;
    }

    const std::string modeNormalized = NormalizeForGdtfMatch(mode.name);
    GdtfModeMatchTier candidateTier = GdtfModeMatchTier::None;
    if (!requestedNormalized.empty() && !modeNormalized.empty() &&
        requestedNormalized == modeNormalized) {
      candidateTier = GdtfModeMatchTier::ExactNormalized;
    } else {
      const std::string modeDigits = ExtractDigitSignature(modeNormalized);
      if (!requestedDigits.empty() && !modeDigits.empty() && requestedDigits == modeDigits)
        candidateTier = GdtfModeMatchTier::DigitSignature;
    }

    if (candidateTier == GdtfModeMatchTier::None)
      continue;
    if (static_cast<int>(candidateTier) > static_cast<int>(bestScore.tier) ||
        (candidateTier == bestScore.tier && footprintMatch && !selectedModeHasFootprint)) {
      bestScore.tier = candidateTier;
      bestScore.modeName = mode.name;
      selectedModeHasFootprint = footprintMatch;
    }
  }

  if (bestScore.tier == GdtfModeMatchTier::None && bestScore.footprintMatch) {
    bestScore.tier = GdtfModeMatchTier::Footprint;
    bestScore.modeName = footprintModeName;
  }
  return bestScore;
}

// Compares download candidates by tier and tie-breakers.
bool IsBetterDownloadCandidate(const DownloadCandidateRank &candidate,
                               const DownloadCandidateRank &currentBest) {
  if (candidate.nameTier != currentBest.nameTier)
    return static_cast<int>(candidate.nameTier) > static_cast<int>(currentBest.nameTier);
  if (candidate.modeTier != currentBest.modeTier)
    return static_cast<int>(candidate.modeTier) > static_cast<int>(currentBest.modeTier);
  if (candidate.footprintMatch != currentBest.footprintMatch)
    return candidate.footprintMatch;
  if (candidate.manufacturerMatch != currentBest.manufacturerMatch)
    return candidate.manufacturerMatch;
  if (candidate.recency != currentBest.recency)
    return candidate.recency > currentBest.recency;
  return candidate.rating > currentBest.rating;
}

// Selects the fixture name searched in the GDTF catalog, falling back to type.
std::string SelectDownloadSearchFixtureName(const std::string &requestedFixtureName,
                                            const std::string &fixtureTypeName) {
  const std::string requested = TrimFixtureIdentity(requestedFixtureName);
  if (!requested.empty())
    return requested;
  return TrimFixtureIdentity(fixtureTypeName);
}

// Builds the human-readable reason for the currently selected catalog match.
static std::string BuildSelectionReason(const GdtfModeMatchScore &modeScore,
                                        bool footprintMatch,
                                        bool manufacturerMatch,
                                        bool hadPreviousBest,
                                        const GdtfCatalogEntry &entry,
                                        const DownloadCandidateRank &bestRank) {
  std::string selectionReason = "name";
  if (modeScore.tier == GdtfModeMatchTier::ExactNormalized) {
    selectionReason = "name+mode";
  } else if (modeScore.tier == GdtfModeMatchTier::DigitSignature) {
    selectionReason = "name+mode-digits";
  } else if (footprintMatch) {
    selectionReason = "name+footprint";
  } else if (manufacturerMatch) {
    selectionReason = "name+manufacturer";
  } else if (hadPreviousBest && entry.lastModifiedUnix > bestRank.recency) {
    selectionReason = "name+recency";
  } else if (hadPreviousBest && entry.rating > bestRank.rating) {
    selectionReason = "name+rating";
  }
  return selectionReason;
}

// Selects the best downloadable catalog entry for an MVR fixture request.
GdtfDownloadMatch SelectBestDownloadMatch(
    const std::string &requestedFixtureName,
    const std::string &fixtureTypeName,
    const std::string &requestedMode,
    const std::string &manufacturer,
    int requestedFootprint,
    const std::vector<GdtfCatalogEntry> &catalogEntries) {
  GdtfDownloadMatch bestMatch;
  DownloadCandidateRank bestRank;
  const std::string searchFixtureName =
      SelectDownloadSearchFixtureName(requestedFixtureName, fixtureTypeName);

  for (const auto &entry : catalogEntries) {
    const auto nameTier = ComputeFixtureNameMatchTier(entry.fixtureName, searchFixtureName);
    if (nameTier == FixtureNameMatchTier::None)
      continue;

    const auto modeScore =
        ComputeGdtfModeMatchScore(requestedMode, entry.modes, requestedFootprint);
    const bool footprintMatch = modeScore.footprintMatch;
    const bool manufacturerMatch =
        !manufacturer.empty() && !entry.manufacturer.empty() &&
        NormalizeForGdtfMatch(manufacturer) == NormalizeForGdtfMatch(entry.manufacturer);
    DownloadCandidateRank candidateRank{nameTier, footprintMatch, manufacturerMatch,
                                        entry.lastModifiedUnix, entry.rating};
    candidateRank.modeTier = modeScore.tier;
    const bool hadPreviousBest = bestMatch.found;
    if (!IsBetterDownloadCandidate(candidateRank, bestRank))
      continue;

    const std::string selectionReason = BuildSelectionReason(
        modeScore, footprintMatch, manufacturerMatch, hadPreviousBest, entry, bestRank);
    bestRank = candidateRank;
    bestMatch = {true, entry.rid, modeScore.modeName, selectionReason};
  }

  return bestMatch;
}

} // namespace gdtf_catalog_matcher
} // namespace mvr
