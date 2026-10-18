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
#include "gdtf_resolution.h"
#include "../mvr/gdtf_import_matching.h"
#include <algorithm>

namespace gdtf_resolution {
namespace {
Status ResourceStatus(Source source) {
  switch (source) {
  case Source::OriginalMvr: return Status::OriginalMvr;
  case Source::Application: return Status::Application;
  case Source::Local: return Status::Local;
  default: return Status::Unresolved;
  }
}
} // namespace

Resolution Analyze(const mvr::gdtf_catalog_matcher::GdtfDownloadRequest &request,
                   const std::vector<mvr::gdtf_catalog_matcher::GdtfCatalogEntry> &entries,
                   std::vector<Choice> resources, const Choice &fallback,
                   std::optional<Choice> explicitChoice, std::function<bool()> canceled, CatalogIntent intent) {
  Resolution result;
  result.ranking = mvr::gdtf_catalog_matcher::RankDownloadCandidates(request, entries, 8, std::move(canceled));
  result.availableResources = std::move(resources);
  if (std::find(result.availableResources.begin(), result.availableResources.end(),
                fallback) != result.availableResources.end())
    result.fallback = fallback;
  result.applied = result.fallback;
  result.selected = result.fallback;
  result.status = ResourceStatus(result.fallback.source);
  if (explicitChoice) {
    Select(result, *explicitChoice);
  } else if (intent == CatalogIntent::DownloadRequested && result.ranking.downloadRid) {
    const auto candidate = std::find_if(result.ranking.candidates.begin(),
        result.ranking.candidates.end(), [&](const auto &item) {
          return item.rid == *result.ranking.downloadRid;
        });
    Select(result, {Source::Catalog, candidate->rid, {},
                    candidate->automaticSafe ? candidate->selectedMode : std::string{}},
           SelectionOrigin::DownloadRequested);
  } else if (intent == CatalogIntent::SafeAutomatic && result.ranking.recommendedRid) {
    const auto &candidate = result.ranking.candidates.front();
    Select(result, {Source::Catalog, candidate.rid, {}, candidate.selectedMode},
           SelectionOrigin::Automatic);
  }
  return result;
}

bool Select(Resolution &resolution, const Choice &choice, SelectionOrigin origin) {
  if (choice.source == Source::Catalog) {
    if (choice.revisionId.empty() ||
        (choice.mode.empty() && origin == SelectionOrigin::Automatic))
      return false;
    if (origin == SelectionOrigin::DownloadRequested &&
        (!resolution.ranking.downloadRid || *resolution.ranking.downloadRid != choice.revisionId))
      return false;
    if (origin == SelectionOrigin::Automatic &&
        (!resolution.ranking.recommendedRid ||
         *resolution.ranking.recommendedRid != choice.revisionId ||
         std::none_of(resolution.ranking.candidates.begin(),
                      resolution.ranking.candidates.end(), [&](const auto &candidate) {
           return candidate.rid == choice.revisionId && candidate.automaticSafe &&
               candidate.selectedMode == choice.mode;
         })))
      return false;
  } else if (choice.source == Source::Unresolved ||
             std::find(resolution.availableResources.begin(),
                       resolution.availableResources.end(), choice) ==
                 resolution.availableResources.end()) {
    return false;
  }
  resolution.selected = choice;
  resolution.origin = origin;
  resolution.diagnostics.clear();
  if (choice.source == Source::Catalog) {
    resolution.status = Status::Pending;
  } else {
    resolution.applied = choice;
    resolution.status = ResourceStatus(choice.source);
    if (origin == SelectionOrigin::User)
      resolution.fallback = choice;
  }
  return true;
}

bool Complete(Resolution &resolution, const Choice &attempt,
              const std::string &validatedPath, const std::string &validatedMode,
              bool reused) {
  if (resolution.selected != attempt || attempt.source != Source::Catalog ||
      validatedPath.empty() || validatedMode.empty() || validatedMode != attempt.mode)
    return false;
  resolution.applied = attempt;
  resolution.applied.path = validatedPath;
  resolution.status = reused ? Status::Reused : Status::Downloaded;
  return true;
}

void Fail(Resolution &resolution, const Choice &attempt, const std::string &reason) {
  if (resolution.selected != attempt)
    return;
  resolution.applied = resolution.fallback;
  resolution.status = Status::DownloadFailed;
  resolution.diagnostics = {reason};
}

void Cancel(Resolution &resolution) {
  if (resolution.status == Status::Pending || resolution.status == Status::Downloading)
    resolution.status = Status::Canceled;
}

std::string ReplacementKey(const Choice &choice) {
  return mvr::gdtf_import_matching::BuildSelectedReplacementIdentity(
      choice.revisionId, choice.mode);
}
} // namespace gdtf_resolution
