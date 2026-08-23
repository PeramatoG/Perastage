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

#include <string>
#include <utility>
#include <vector>

namespace mvr {
namespace gdtf_catalog_matcher {

struct GdtfCatalogModeCandidate {
  std::string name;
  int footprint = 0;
};

struct GdtfCatalogEntry {
  // Creates an empty catalog entry for parser population.
  GdtfCatalogEntry() = default;
  // Creates a catalog entry while preserving the established test call shape.
  GdtfCatalogEntry(std::string revisionId, std::string manufacturerName,
                   std::string modelName,
                   std::vector<GdtfCatalogModeCandidate> catalogModes,
                   long long modified, float catalogRating)
      : rid(std::move(revisionId)), manufacturer(std::move(manufacturerName)),
        fixtureName(std::move(modelName)), modes(std::move(catalogModes)),
        lastModifiedUnix(modified), rating(catalogRating), downloadable(true) {}

  std::string rid;
  std::string manufacturer;
  std::string fixtureName;
  std::vector<GdtfCatalogModeCandidate> modes;
  long long lastModifiedUnix = 0;
  float rating = 0.0f;
  std::string uuid;
  std::string uploader;
  std::string url;
  std::string creator;
  std::string creationDate;
  std::string revision;
  std::string version;
  std::string ratingText;
  bool downloadable = false;
  std::string downloadabilityReason;
};

struct GdtfDownloadMatch {
  bool found = false;
  std::string rid;
  std::string modeName;
  std::string selectionReason;
};

struct GdtfDownloadRequest {
  std::vector<std::string> authoritativeFixtureNames;
  std::vector<std::string> secondaryAliases;
  std::string requestedMode;
  std::string manufacturer;
  int requestedFootprint = 0;
  bool authoritativeIdentityIsPlaceholder = false;
  std::string fixtureTypeId;
  std::string catalogSnapshotSource;
  std::string catalogUpdatedAt;
  std::string catalogPayloadFingerprint;
  std::size_t catalogPayloadBytes = 0;
  std::size_t catalogParsedEntryCount = 0;
};

enum class FixtureIdentitySource {
  Catalog,
  AuthoritativeGdtf,
  MvrAlias
};

struct CanonicalFixtureModel {
  std::string rawText;
  std::string canonicalText;
  std::vector<std::string> modelTerms;
  std::vector<std::string> numericModelTokens;
  FixtureIdentitySource source = FixtureIdentitySource::Catalog;
};

enum class FixtureNameMatchTier {
  None = 0,
  Partial = 1,
  CoreName = 2,
  ParenthesisStripped = 3,
  ExactNormalized = 4
};

enum class GdtfModeMatchTier {
  None = 0,
  Footprint = 1,
  DigitSignature = 2,
  ExactNormalized = 3
};

enum class NumericTokenCompatibility {
  Different = -1,
  Missing = 0,
  Exact = 1
};

struct GdtfModeMatchScore {
  GdtfModeMatchTier tier = GdtfModeMatchTier::None;
  bool footprintMatch = false;
  std::string modeName;
};

struct DownloadCandidateRank {
  bool authoritativeName = false;
  FixtureNameMatchTier nameTier = FixtureNameMatchTier::None;
  bool footprintMatch = false;
  bool manufacturerMatch = false;
  bool uploaderAuthority = false;
  long long recency = 0;
  float rating = 0.0f;
  GdtfModeMatchTier modeTier = GdtfModeMatchTier::None;
  NumericTokenCompatibility numericCompatibility =
      NumericTokenCompatibility::Missing;
  std::string stableKey;
};

std::string TrimFixtureIdentity(const std::string &value);
std::string ToLowerAscii(std::string text);
std::string ExtractDigitSignature(const std::string &text);
std::string NormalizeForGdtfMatch(const std::string &text);
std::string StripParenthesizedSections(const std::string &text);
bool IsLikelyVersionToken(const std::string &token);
bool IsGenericFixtureIdentity(const std::string &identity);
std::string BuildCoreFixtureNameKey(const std::string &text);
CanonicalFixtureModel BuildCanonicalFixtureModel(
    const std::string &text, const std::string &structuredManufacturer = {},
    FixtureIdentitySource source = FixtureIdentitySource::Catalog);
NumericTokenCompatibility ComputeNumericTokenCompatibility(
    const CanonicalFixtureModel &catalog,
    const CanonicalFixtureModel &requested);
FixtureNameMatchTier ComputeFixtureNameMatchTier(
    const std::string &catalogFixtureName,
    const std::string &requestedFixtureName);
GdtfModeMatchScore ComputeGdtfModeMatchScore(
    const std::string &requestedMode,
    const std::vector<GdtfCatalogModeCandidate> &catalogModes,
    int requestedFootprint = 0);
bool IsBetterDownloadCandidate(const DownloadCandidateRank &candidate,
                               const DownloadCandidateRank &currentBest);
std::string SelectDownloadSearchFixtureName(const std::string &requestedFixtureName,
                                            const std::string &fixtureTypeName);
GdtfDownloadMatch SelectBestDownloadMatch(
    const GdtfDownloadRequest &request,
    const std::vector<GdtfCatalogEntry> &catalogEntries);
GdtfDownloadMatch SelectBestDownloadMatch(
    const std::string &requestedFixtureName,
    const std::string &fixtureTypeName,
    const std::string &requestedMode,
    const std::string &manufacturer,
    int requestedFootprint,
    const std::vector<GdtfCatalogEntry> &catalogEntries);

} // namespace gdtf_catalog_matcher
} // namespace mvr
