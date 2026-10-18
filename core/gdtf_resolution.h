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

#include "../mvr/gdtf_catalog_matcher.h"
#include <optional>
#include <string>
#include <vector>

namespace gdtf_resolution {

using CandidateRanking = mvr::gdtf_catalog_matcher::CandidateRanking;
enum class Source { Unresolved, OriginalMvr, Application, Local, Catalog };
enum class Status {
  Pending, Downloading, Downloaded, Reused, OriginalMvr, Application,
  Local, DownloadFailed, Unresolved, Canceled
};
enum class SelectionOrigin { Automatic, User };

struct Choice {
  Source source = Source::Unresolved;
  std::string revisionId;
  std::string path;
  std::string mode;
  bool operator==(const Choice &) const = default;
};

// Proposed and applied identity are separate until validation succeeds.
struct Resolution {
  CandidateRanking ranking;
  std::vector<Choice> availableResources;
  Choice fallback;
  Choice selected;
  Choice applied;
  SelectionOrigin origin = SelectionOrigin::Automatic;
  Status status = Status::Unresolved;
  std::vector<std::string> diagnostics;
};

Resolution Analyze(const mvr::gdtf_catalog_matcher::GdtfDownloadRequest &request,
                   const std::vector<mvr::gdtf_catalog_matcher::GdtfCatalogEntry> &entries,
                   std::vector<Choice> resources, const Choice &fallback,
                   std::optional<Choice> explicitChoice = {},
                   std::function<bool()> canceled = {});
bool Select(Resolution &resolution, const Choice &choice,
            SelectionOrigin origin = SelectionOrigin::User);
bool Complete(Resolution &resolution, const Choice &attempt,
              const std::string &validatedPath, const std::string &validatedMode,
              bool reused = false);
void Fail(Resolution &resolution, const Choice &attempt, const std::string &reason);
void Cancel(Resolution &resolution);
std::string ReplacementKey(const Choice &choice);

} // namespace gdtf_resolution
