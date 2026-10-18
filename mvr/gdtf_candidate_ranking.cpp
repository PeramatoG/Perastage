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
#include <sstream>

namespace mvr::gdtf_catalog_matcher {

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

// Selects the best downloadable catalog entry from structured identity evidence.
static GdtfDownloadMatch CollectCandidates(
    const GdtfDownloadRequest &request,
    const std::vector<GdtfCatalogEntry> &catalogEntries,
    CandidateRanking *ranking) {
  GdtfDownloadMatch bestMatch;
  DownloadCandidateRank bestRank;
  std::string bestCanonical;
  std::vector<std::string> evaluated;
  std::vector<std::string> rejectedEvaluated;
  std::vector<std::pair<DownloadCandidateRank, std::string>> eligibleRanks;
  bool hasCanonicalCandidate = false;
  std::size_t canonicalCandidateCount = 0;
  std::size_t downloadableCanonicalCandidateCount = 0;

  for (const auto &entry : catalogEntries) {
    FixtureNameMatchTier nameTier = FixtureNameMatchTier::None;
    bool authoritativeName = false;
    bool matchedFromAlias = false;
    CanonicalFixtureModel matchedModel;
    const auto catalogModel = BuildCanonicalFixtureModel(
        entry.fixtureName, entry.manufacturer, FixtureIdentitySource::Catalog);
    bool canonicalNameCandidate = false;
    for (const auto &identity : request.authoritativeFixtureNames) {
      const auto requestedModel = BuildCanonicalFixtureModel(
          identity, IsUnknownManufacturer(request.manufacturer) ? entry.manufacturer : request.manufacturer,
          FixtureIdentitySource::AuthoritativeGdtf);
      if (!catalogModel.canonicalText.empty() &&
          catalogModel.canonicalText == requestedModel.canonicalText) {
        canonicalNameCandidate = true;
        break;
      }
    }
    if (canonicalNameCandidate) {
      ++canonicalCandidateCount;
      if (entry.downloadable)
        ++downloadableCanonicalCandidateCount;
    }
    if (!entry.downloadable || entry.rid.empty()) {
      std::ostringstream unavailable;
      unavailable << "candidate=" << (entry.rid.empty() ? "<missing>" : entry.rid)
                  << "; manufacturer='" << entry.manufacturer << "'; model='"
                  << entry.fixtureName << "'; canonical='"
                  << catalogModel.canonicalText << "'; uuid='" << entry.uuid
                  << "'; identity="
                  << (canonicalNameCandidate ? "canonical" : "unrelated")
                  << "; eligible=no; reason=not-downloadable:"
                  << entry.downloadabilityReason;
      if (rejectedEvaluated.size() < 12)
        rejectedEvaluated.push_back(unavailable.str());
      continue;
    }
    for (const auto &identity : request.authoritativeFixtureNames) {
      const auto requestedModel = BuildCanonicalFixtureModel(
          identity, IsUnknownManufacturer(request.manufacturer) ? entry.manufacturer : request.manufacturer,
          FixtureIdentitySource::AuthoritativeGdtf);
      auto tier = std::max(
          ComputeFixtureNameMatchTier(entry.fixtureName, identity),
          ComputeFixtureNameMatchTier(entry.manufacturer + " " +
                                          entry.fixtureName,
                                      identity));
      if (tier < FixtureNameMatchTier::CoreName && !catalogModel.canonicalText.empty() &&
          catalogModel.canonicalText == requestedModel.canonicalText)
        tier = FixtureNameMatchTier::CoreName;
      if (tier > nameTier) {
        nameTier = tier;
        matchedModel = requestedModel;
        authoritativeName = !request.authoritativeIdentityIsPlaceholder;
        matchedFromAlias = false;
      }
    }
    const bool aliasesMayEstablishIdentity =
        request.authoritativeFixtureNames.empty() ||
        request.authoritativeIdentityIsPlaceholder;
    for (const auto &alias : request.secondaryAliases) {
      if (!aliasesMayEstablishIdentity)
        break;
      const auto aliasModel = BuildCanonicalFixtureModel(
          alias, request.manufacturer, FixtureIdentitySource::MvrAlias);
      auto tier = ComputeFixtureNameMatchTier(entry.fixtureName, alias);
      if (tier < FixtureNameMatchTier::CoreName && !catalogModel.canonicalText.empty() &&
          catalogModel.canonicalText == aliasModel.canonicalText)
        tier = FixtureNameMatchTier::CoreName;
      if (tier > nameTier) {
        nameTier = tier;
        matchedModel = aliasModel;
        authoritativeName = false;
        matchedFromAlias = true;
      }
    }
    const auto numeric =
        ComputeNumericTokenCompatibility(catalogModel, matchedModel);
    if (nameTier == FixtureNameMatchTier::None) {
      std::ostringstream rejected;
      rejected << "candidate=" << entry.rid << "; model='" << entry.fixtureName
               << "'; manufacturer='" << entry.manufacturer << "'; uuid='"
               << entry.uuid
               << "'; canonical='" << catalogModel.canonicalText
               << "'; eligible=no; reason=insufficient-model-identity"
               << "; numeric=" << static_cast<int>(numeric);
      if (rejectedEvaluated.size() < 12)
        rejectedEvaluated.push_back(rejected.str());
      continue;
    }
    hasCanonicalCandidate = hasCanonicalCandidate ||
        nameTier >= FixtureNameMatchTier::CoreName;

    const auto modeScore =
        ComputeGdtfModeMatchScore(request.requestedMode, entry.modes,
                                  request.requestedFootprint);
    const bool footprintMatch = modeScore.footprintMatch;
    const bool manufacturerMatch =
        !IsUnknownManufacturer(request.manufacturer) && !entry.manufacturer.empty() &&
        NormalizeForGdtfMatch(request.manufacturer) ==
            NormalizeForGdtfMatch(entry.manufacturer);
    DownloadCandidateRank candidateRank;
    candidateRank.authoritativeName = authoritativeName;
    candidateRank.nameTier = nameTier;
    candidateRank.footprintMatch = footprintMatch;
    candidateRank.manufacturerMatch = manufacturerMatch;
    candidateRank.uploaderAuthority = !entry.uploader.empty() &&
        !entry.manufacturer.empty() &&
        NormalizeForGdtfMatch(entry.uploader) ==
            NormalizeForGdtfMatch(entry.manufacturer);
    candidateRank.recency = entry.lastModifiedUnix;
    candidateRank.rating = entry.rating;
    candidateRank.modeTier = modeScore.tier;
    candidateRank.numericCompatibility = numeric;
    candidateRank.stableKey = entry.rid;
    eligibleRanks.emplace_back(candidateRank, catalogModel.canonicalText);
    if (ranking) {
      RankedCandidate candidate;
      candidate.rid = entry.rid;
      candidate.manufacturer = entry.manufacturer;
      candidate.fixtureName = entry.fixtureName;
      candidate.revision = entry.revision;
      candidate.modes = entry.modes;
      candidate.selectedMode = modeScore.modeName;
      if (candidate.selectedMode.empty() && request.requestedMode.empty() &&
          entry.modes.size() == 1)
        candidate.selectedMode = entry.modes.front().name;
      for (const auto &mode : entry.modes)
        if (mode.name == candidate.selectedMode)
          candidate.footprint = mode.footprint;
      candidate.rank = candidateRank;
      candidate.canonicalModel = catalogModel.canonicalText;
      const bool exactIdentity = !matchedModel.canonicalText.empty() &&
          matchedModel.canonicalText == catalogModel.canonicalText;
      const bool manufacturerConflict = !IsUnknownManufacturer(request.manufacturer) &&
          !IsUnknownManufacturer(entry.manufacturer) && !manufacturerMatch;
      const bool modeCompatible = !candidate.selectedMode.empty() &&
          (request.requestedMode.empty() ||
           modeScore.tier == GdtfModeMatchTier::ExactNormalized) &&
          (request.requestedFootprint <= 0 ||
           candidate.footprint == request.requestedFootprint);
      candidate.automaticSafe = exactIdentity && !manufacturerConflict &&
          numeric != NumericTokenCompatibility::Different && modeCompatible &&
          request.importedModes.size() <= 1 &&
          !IsGenericFixtureIdentity(matchedModel.rawText);
      if (request.importedModes.size() > 1)
        candidate.warnings.push_back("multiple-imported-modes");
      candidate.reasons.push_back(exactIdentity ? "canonical-model" : "partial-model");
      if (manufacturerMatch)
        candidate.reasons.push_back("manufacturer");
      if (IsUnknownManufacturer(request.manufacturer)) {
        candidate.reasons.push_back("manufacturer-unknown");
        for (const auto &identity : request.authoritativeFixtureNames)
          if (BuildCanonicalFixtureModel(identity).canonicalText !=
              BuildCanonicalFixtureModel(identity, entry.manufacturer).canonicalText)
            candidate.reasons.push_back("manufacturer-inferred-from-prefix");
      }
      if (matchedFromAlias)
        candidate.reasons.push_back("secondary-alias");
      if (manufacturerConflict)
        candidate.warnings.push_back("manufacturer-mismatch");
      if (!exactIdentity)
        candidate.warnings.push_back("model-variant-requires-review");
      if (numeric == NumericTokenCompatibility::Different)
        candidate.warnings.push_back("numeric-model-mismatch");
      if (!modeCompatible)
        candidate.warnings.push_back("mode-requires-review");
      if (footprintMatch)
        candidate.warnings.push_back("footprint-does-not-prove-channel-semantics");
      if (request.requestedFootprint > 0 && candidate.footprint > 0 &&
          candidate.footprint != request.requestedFootprint)
        candidate.warnings.push_back("footprint-mismatch");
      candidate.confidence = candidate.automaticSafe ? MatchConfidence::Safe
                                                    : MatchConfidence::Review;
      ranking->candidates.push_back(std::move(candidate));
    }

    const bool hadPreviousBest = bestMatch.found;
    std::ostringstream candidateDiagnostic;
    candidateDiagnostic << "candidate=" << entry.rid << "; model='"
                        << entry.fixtureName << "'; manufacturer-name='"
                        << entry.manufacturer << "'; canonical='"
                        << catalogModel.canonicalText << "'; model-identity='"
                        << catalogModel.modelIdentityText
                        << "'; eligible=yes; evidence-source="
                        << (matchedFromAlias ? "secondary-alias" : "authoritative")
                        << "; name-tier="
                        << static_cast<int>(nameTier) << "; numeric="
                        << static_cast<int>(numeric) << "; manufacturer="
                        << (manufacturerMatch ? "match" : "none") << "; mode-tier="
                        << static_cast<int>(modeScore.tier) << "; footprint="
                        << (footprintMatch ? "match" : "none") << "; uuid='"
                        << entry.uuid << "'; uploader-authority="
                        << (candidateRank.uploaderAuthority ? "yes" : "no");
    evaluated.push_back(candidateDiagnostic.str());
    if (!IsBetterDownloadCandidate(candidateRank, bestRank)) {
      continue;
    }

    const std::string selectionReason = BuildSelectionReason(
        modeScore, footprintMatch, manufacturerMatch, hadPreviousBest, entry, bestRank);
    bestRank = candidateRank;
    bestCanonical = catalogModel.canonicalText;
    std::ostringstream diagnostics;
    diagnostics << selectionReason
                << (authoritativeName ? "; authoritative" : "; secondary")
                << "; numeric="
                << static_cast<int>(candidateRank.numericCompatibility)
                << "; requested-mode=" << request.requestedMode
                << "; selected-mode=" << modeScore.modeName
                << "; manufacturer=" << (manufacturerMatch ? "match" : "none")
                << "; footprint=" << (footprintMatch ? "match" : "none");
    bestMatch = {true, entry.rid, modeScore.modeName, diagnostics.str()};
  }

  std::ostringstream fullDiagnostics;
  fullDiagnostics << "request-model='";
  if (!request.authoritativeFixtureNames.empty())
    fullDiagnostics << request.authoritativeFixtureNames.front();
  const auto requestCanonical = request.authoritativeFixtureNames.empty()
      ? CanonicalFixtureModel{}
      : BuildCanonicalFixtureModel(request.authoritativeFixtureNames.front(),
                                   request.manufacturer,
                                   FixtureIdentitySource::AuthoritativeGdtf);
  fullDiagnostics << "'; display-type-key='" << request.displayTypeKey
                  << "'; resolved-fixture-name='" << request.resolvedFixtureName
                  << "'; request-canonical='" << requestCanonical.canonicalText
                  << "'; request-model-identity='"
                  << requestCanonical.modelIdentityText
                  << "'; manufacturer='" << request.manufacturer
                  << "'; fixture-type-id='" << request.fixtureTypeId
                  << "'; requested-mode='" << request.requestedMode
                  << "'; footprint=" << request.requestedFootprint
                  << "; snapshot-source='" << request.catalogSnapshotSource
                  << "'; updated-at='" << request.catalogUpdatedAt
                  << "'; payload-bytes=" << request.catalogPayloadBytes
                  << "; payload-fingerprint='" << request.catalogPayloadFingerprint
                  << "'; parsed-entry-count=" << request.catalogParsedEntryCount
                  << "; canonical-candidates=" << canonicalCandidateCount
                  << "; downloadable-canonical-candidates="
                  << downloadableCanonicalCandidateCount;
  fullDiagnostics << "; placeholder-authority="
                  << (request.authoritativeIdentityIsPlaceholder ? "yes" : "no")
                  << "; secondary-aliases='";
  for (std::size_t index = 0; index < request.secondaryAliases.size(); ++index) {
    if (index > 0)
      fullDiagnostics << ", ";
    fullDiagnostics << request.secondaryAliases[index];
  }
  fullDiagnostics << "'; diagnostic-aliases='";
  for (std::size_t index = 0; index < request.diagnosticAliases.size(); ++index) {
    if (index > 0)
      fullDiagnostics << ", ";
    fullDiagnostics << request.diagnosticAliases[index];
  }
  fullDiagnostics << "'";
  if (!hasCanonicalCandidate)
    fullDiagnostics << "; no exact/canonical model candidate in catalog";
  for (const auto &diagnostic : evaluated)
    fullDiagnostics << " | " << diagnostic;
  for (const auto &diagnostic : rejectedEvaluated)
    fullDiagnostics << " | rejected: " << diagnostic;
  const bool ambiguous = std::any_of(
      eligibleRanks.begin(), eligibleRanks.end(),
      [&](const auto &evaluatedCandidate) {
        const auto &[rank, canonical] = evaluatedCandidate;
        return canonical != bestCanonical && rank.nameTier == bestRank.nameTier &&
               rank.numericCompatibility == bestRank.numericCompatibility &&
               rank.manufacturerMatch == bestRank.manufacturerMatch;
      });
  if (ambiguous) {
    bestMatch = {};
    bestMatch.selectionReason = fullDiagnostics.str() +
        "; decision=no-match; reason=ambiguous-model-candidates";
  } else if (bestMatch.found) {
    bestMatch.selectionReason += " | " + fullDiagnostics.str() +
        "; decision=winner:" + bestMatch.rid;
  } else {
    bestMatch.selectionReason = fullDiagnostics.str() + "; decision=no-match";
  }

  if (ranking) {
    ranking->diagnostics = bestMatch.selectionReason;
    // A literal manufacturer prefix plus exact model evidence can disambiguate
    // generic imported metadata without treating a catalog uploader as a maker.
    std::vector<std::string> inferredManufacturers;
    if (IsUnknownManufacturer(request.manufacturer)) {
      for (const auto &candidate : ranking->candidates) {
        const auto hasReason = [&](const std::string &reason) {
          return std::find(candidate.reasons.begin(), candidate.reasons.end(), reason) !=
              candidate.reasons.end();
        };
        if (hasReason("manufacturer-inferred-from-prefix") && hasReason("canonical-model")) {
          const auto key = NormalizeForGdtfMatch(candidate.manufacturer);
          if (std::find(inferredManufacturers.begin(), inferredManufacturers.end(), key) ==
              inferredManufacturers.end())
            inferredManufacturers.push_back(key);
        }
      }
    }
    for (auto &candidate : ranking->candidates) {
      if (inferredManufacturers.size() == 1 &&
          NormalizeForGdtfMatch(candidate.manufacturer) != inferredManufacturers.front()) {
        candidate.automaticSafe = false;
        candidate.confidence = MatchConfidence::Review;
        candidate.warnings.push_back("manufacturer-mismatch");
      } else if (inferredManufacturers.size() > 1) {
        candidate.automaticSafe = false;
        candidate.confidence = MatchConfidence::Ambiguous;
        candidate.warnings.push_back("ambiguous-model-candidates");
      }
    }
    std::stable_sort(ranking->candidates.begin(), ranking->candidates.end(),
        [](const auto &a, const auto &b) {
          if (a.automaticSafe != b.automaticSafe)
            return a.automaticSafe;
          const bool exactA = std::find(a.reasons.begin(), a.reasons.end(),
              "canonical-model") != a.reasons.end();
          const bool exactB = std::find(b.reasons.begin(), b.reasons.end(),
              "canonical-model") != b.reasons.end();
          if (exactA != exactB)
            return exactA;
          return IsBetterDownloadCandidate(a.rank, b.rank);
        });
    if (!ranking->candidates.empty()) {
      const auto &first = ranking->candidates.front();
      const bool ambiguousModels = std::any_of(
          ranking->candidates.begin() + 1, ranking->candidates.end(),
          [&](const auto &other) {
            return other.automaticSafe == first.automaticSafe &&
                (other.canonicalModel != first.canonicalModel ||
                 (IsUnknownManufacturer(request.manufacturer) &&
                  NormalizeForGdtfMatch(other.manufacturer) !=
                      NormalizeForGdtfMatch(first.manufacturer) &&
                  std::find(first.reasons.begin(), first.reasons.end(),
                            "manufacturer-inferred-from-prefix") == first.reasons.end())) &&
                other.rank.nameTier == first.rank.nameTier &&
                other.rank.numericCompatibility == first.rank.numericCompatibility &&
                other.rank.manufacturerMatch == first.rank.manufacturerMatch;
          });
      ranking->confidence = ambiguousModels ? MatchConfidence::Ambiguous
                                           : first.confidence;
      if (ambiguousModels) {
        for (auto &candidate : ranking->candidates) {
          candidate.automaticSafe = false;
          candidate.confidence = MatchConfidence::Ambiguous;
          candidate.warnings.push_back("ambiguous-model-candidates");
        }
      } else if (first.automaticSafe) {
        ranking->recommendedRid = first.rid;
      }
    }
  }
  return bestMatch;
}


bool IsUnknownManufacturer(const std::string &manufacturer) {
  const auto key = NormalizeForGdtfMatch(manufacturer);
  return key.empty() || key == "custom" || key == "generic" ||
         key == "unknown" || key == "nomanufacturer";
}

CandidateRanking RankDownloadCandidates(
    const GdtfDownloadRequest &request,
    const std::vector<GdtfCatalogEntry> &catalogEntries, std::size_t limit) {
  CandidateRanking ranking;
  CollectCandidates(request, catalogEntries, &ranking);
  if (ranking.candidates.size() > limit)
    ranking.candidates.resize(limit);
  return ranking;
}

// Retains the legacy best-candidate contract; automatic consumers use ranking.
GdtfDownloadMatch SelectBestDownloadMatch(
    const GdtfDownloadRequest &request,
    const std::vector<GdtfCatalogEntry> &catalogEntries) {
  return CollectCandidates(request, catalogEntries, nullptr);
}

} // namespace mvr::gdtf_catalog_matcher
