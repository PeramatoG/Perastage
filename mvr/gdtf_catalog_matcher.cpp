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
#include <sstream>
#include <unordered_set>

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

// Extracts ordered digits for legacy mode-signature comparisons.
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
  static const std::array<std::string, 17> kSuffixes = {
      " lighting", " light", " gmbh",   " ltd",         " inc", " corp",
      " co",       " llc",   " electronics", " ag",     " sa",  " sl",
      " bv",       " nv",    " s.a.",   " s.l.", " professional"};
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
  if (token.size() < 2 || (token.front() != 'v' && token.front() != 'V'))
    return false;
  return std::all_of(token.begin() + 1, token.end(),
                     [](unsigned char ch) { return std::isdigit(ch); });
}

// Detects generic identities that may legitimately be rescued by an instance alias.
bool IsGenericFixtureIdentity(const std::string &identity) {
  const std::string normalized = NormalizeForGdtfMatch(identity);
  static const std::array<std::string, 5> markers = {
      "dummy", "placeholder", "generic", "standardmode", "defaultfixture"};
  return std::any_of(markers.begin(), markers.end(), [&](const std::string &marker) {
    return normalized == marker || normalized.starts_with(marker + "fixture") ||
           normalized.starts_with(marker + "mode") ||
           (marker == "standardmode" && normalized.find(marker) != std::string::npos);
  });
}

// Converts Roman numerals used as standalone revision numbers to decimal text.
static std::string NormalizeRomanNumber(const std::string &token) {
  static const std::array<std::pair<const char *, const char *>, 10> roman = {{
      {"i", "1"}, {"ii", "2"}, {"iii", "3"}, {"iv", "4"}, {"v", "5"},
      {"vi", "6"}, {"vii", "7"}, {"viii", "8"}, {"ix", "9"}, {"x", "10"}}};
  for (const auto &[value, decimal] : roman) {
    if (token == value)
      return decimal;
  }
  return token;
}

// Builds structured model evidence while excluding parenthesized annotations.
CanonicalFixtureModel BuildCanonicalFixtureModel(
    const std::string &text, const std::string &structuredManufacturer,
    FixtureIdentitySource source) {
  CanonicalFixtureModel model;
  model.rawText = text;
  model.source = source;
  model.normalizedFullText =
      NormalizeForGdtfMatch(StripParenthesizedSections(text));
  const std::string stripped = ToLowerAscii(StripParenthesizedSections(text));
  std::vector<std::string> tokens;
  std::string current;
  int previousClass = 0;
  auto flush = [&]() {
    if (!current.empty())
      tokens.push_back(NormalizeRomanNumber(current));
    current.clear();
    previousClass = 0;
  };
  for (unsigned char ch : stripped) {
    const int characterClass = std::isdigit(ch) ? 2 : (std::isalpha(ch) ? 1 : 0);
    if (characterClass == 0) {
      flush();
      continue;
    }
    if (previousClass != 0 && previousClass != characterClass)
      flush();
    current.push_back(static_cast<char>(ch));
    previousClass = characterClass;
  }
  flush();

  const std::string manufacturerKey = NormalizeForGdtfMatch(structuredManufacturer);
  if (!manufacturerKey.empty()) {
    std::string prefix;
    size_t removeCount = 0;
    for (const auto &token : tokens) {
      prefix += token;
      if (!manufacturerKey.starts_with(prefix))
        break;
      ++removeCount;
      if (prefix == manufacturerKey)
        break;
    }
    if (prefix == manufacturerKey && removeCount < tokens.size())
      tokens.erase(tokens.begin(), tokens.begin() + static_cast<long>(removeCount));
  }

  static const std::unordered_set<std::string> genericTerms = {
      "bulb", "lens", "led", "rgb", "rgbw", "rgbwa", "cmy", "profile",
      "spot", "wash", "beam", "par", "fixture", "light", "lighting", "ip"};
  for (const auto &token : tokens) {
    model.canonicalText += token;
    if (std::any_of(token.begin(), token.end(), [](unsigned char ch) {
          return std::isdigit(ch);
        })) {
      model.numericModelTokens.push_back(token);
      model.modelIdentityText += token;
    } else if (token.size() >= 3 && !genericTerms.contains(token)) {
      model.modelTerms.push_back(token);
      model.modelIdentityText += token;
    } else {
      model.descriptiveTerms.push_back(token);
    }
  }
  return model;
}

// Classifies agreement, absence, and contradiction in numeric model evidence.
NumericTokenCompatibility ComputeNumericTokenCompatibility(
    const CanonicalFixtureModel &catalog,
    const CanonicalFixtureModel &requested) {
  if (catalog.numericModelTokens.empty() || requested.numericModelTokens.empty())
    return NumericTokenCompatibility::Missing;
  for (const auto &token : catalog.numericModelTokens) {
    if (std::find(requested.numericModelTokens.begin(),
                  requested.numericModelTokens.end(), token) !=
        requested.numericModelTokens.end())
      return NumericTokenCompatibility::Exact;
  }
  return NumericTokenCompatibility::Different;
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

  const auto catalogModel = BuildCanonicalFixtureModel(catalogFixtureName);
  const auto requestedModel = BuildCanonicalFixtureModel(requestedFixtureName);
  const auto numeric =
      ComputeNumericTokenCompatibility(catalogModel, requestedModel);
  size_t sharedTerms = 0;
  for (const auto &term : catalogModel.modelTerms) {
    if (std::find(requestedModel.modelTerms.begin(), requestedModel.modelTerms.end(),
                  term) != requestedModel.modelTerms.end())
      ++sharedTerms;
  }
  const size_t maximumTerms =
      std::max(catalogModel.modelTerms.size(), requestedModel.modelTerms.size());
  const double coverage = maximumTerms == 0
                              ? 0.0
                              : static_cast<double>(sharedTerms) / maximumTerms;
  const bool compactContains = catalogModel.modelIdentityText.size() >= 5 &&
      requestedModel.modelIdentityText.size() >= 5 &&
      (catalogModel.modelIdentityText.find(requestedModel.modelIdentityText) !=
           std::string::npos ||
       requestedModel.modelIdentityText.find(catalogModel.modelIdentityText) !=
           std::string::npos);
  const bool strongOverlap =
      (sharedTerms >= 2 && coverage >= 0.5) ||
      (sharedTerms >= 1 && numeric == NumericTokenCompatibility::Exact) ||
      (compactContains && numeric != NumericTokenCompatibility::Different);
  if (!strongOverlap)
    return FixtureNameMatchTier::None;

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
  if (candidate.authoritativeName != currentBest.authoritativeName)
    return candidate.authoritativeName;
  if (candidate.nameTier != currentBest.nameTier)
    return static_cast<int>(candidate.nameTier) > static_cast<int>(currentBest.nameTier);
  if (candidate.numericCompatibility != currentBest.numericCompatibility)
    return static_cast<int>(candidate.numericCompatibility) >
           static_cast<int>(currentBest.numericCompatibility);
  if (candidate.manufacturerMatch != currentBest.manufacturerMatch)
    return candidate.manufacturerMatch;
  if (candidate.modeTier != currentBest.modeTier)
    return static_cast<int>(candidate.modeTier) > static_cast<int>(currentBest.modeTier);
  if (candidate.footprintMatch != currentBest.footprintMatch)
    return candidate.footprintMatch;
  if (candidate.uploaderAuthority != currentBest.uploaderAuthority)
    return candidate.uploaderAuthority;
  if (candidate.recency != currentBest.recency)
    return candidate.recency > currentBest.recency;
  if (candidate.rating != currentBest.rating)
    return candidate.rating > currentBest.rating;
  return candidate.stableKey < currentBest.stableKey;
}

// Selects the fixture name searched in the GDTF catalog, falling back to type.
std::string SelectDownloadSearchFixtureName(const std::string &requestedFixtureName,
                                            const std::string &fixtureTypeName) {
  const std::string requested = TrimFixtureIdentity(requestedFixtureName);
  if (!requested.empty())
    return requested;
  return TrimFixtureIdentity(fixtureTypeName);
}

// Adapts the legacy call shape into authoritative and secondary evidence.
GdtfDownloadMatch SelectBestDownloadMatch(
    const std::string &requestedFixtureName,
    const std::string &fixtureTypeName,
    const std::string &requestedMode,
    const std::string &manufacturer,
    int requestedFootprint,
    const std::vector<GdtfCatalogEntry> &catalogEntries) {
  GdtfDownloadRequest request;
  const std::string authoritative = TrimFixtureIdentity(fixtureTypeName);
  if (!authoritative.empty())
    request.authoritativeFixtureNames.push_back(authoritative);
  else if (!TrimFixtureIdentity(requestedFixtureName).empty())
    request.authoritativeFixtureNames.push_back(TrimFixtureIdentity(requestedFixtureName));
  if (!authoritative.empty() && !TrimFixtureIdentity(requestedFixtureName).empty())
    request.secondaryAliases.push_back(TrimFixtureIdentity(requestedFixtureName));
  request.authoritativeIdentityIsPlaceholder = IsGenericFixtureIdentity(authoritative);
  request.requestedMode = requestedMode;
  request.manufacturer = manufacturer;
  request.requestedFootprint = requestedFootprint;
  return SelectBestDownloadMatch(request, catalogEntries);
}

} // namespace gdtf_catalog_matcher
} // namespace mvr
