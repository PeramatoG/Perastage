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
#include "gdtf_metadata_summary.h"

#include "gdtf_archive_reader.h"
#include "gdtf_description_reader.h"
#include "filesystem_path_utils.h"

#include <wx/datetime.h>

namespace {

// Formats a GDTF timestamp for compact display.
std::string FormatMetadataTimestamp(const std::string &value) {
  if (value.empty())
    return {};

  wxDateTime parsed;
  if (parsed.ParseISOCombined(value.c_str())) {
    if (parsed.IsValid())
      return parsed.FormatISOCombined(' ').ToStdString();
  }

  wxDateTime parsedUtc;
  if (parsedUtc.ParseFormat(value.c_str(), "%Y-%m-%dT%H:%M:%SZ")) {
    if (parsedUtc.IsValid())
      return parsedUtc.FormatISOCombined(' ').ToStdString();
  }

  return value;
}

// Copies the latest ordered revision fields into the presentation summary.
void ApplyLatestRevision(const gdtf::GdtfDescriptionSnapshot &snapshot,
                         GdtfMetadataSummary &summary) {
  if (snapshot.revisions.empty())
    return;

  const gdtf::GdtfRevisionInfo &firstRevision = snapshot.revisions.front();
  const gdtf::GdtfRevisionInfo &latestRevision = snapshot.revisions.back();
  if (summary.revision.empty())
    summary.revision = latestRevision.text;
  summary.lastModified = latestRevision.date;
  summary.modifiedBy = latestRevision.modifiedBy;
  summary.userId = latestRevision.userId.empty() ? "0" : latestRevision.userId;
  if (summary.creationDate.empty())
    summary.creationDate = firstRevision.date;
}
} // namespace

// Loads a compact metadata summary from a GDTF archive.
bool LoadGdtfMetadataSummary(const std::string &gdtfPath,
                             GdtfMetadataSummary &outSummary) {
  return LoadGdtfMetadataSummary(PathUtils::PathFromUtf8(gdtfPath), outSummary);
}

// Loads a compact metadata summary from a native filesystem path.
bool LoadGdtfMetadataSummary(const std::filesystem::path &gdtfPath,
                             GdtfMetadataSummary &outSummary) {
  outSummary = {};
  outSummary.userId = "0";
  if (gdtfPath.empty())
    return false;

  gdtf::ArchiveReadResult archive = gdtf::ReadGdtfArchive(gdtfPath);
  if (!archive.Success())
    return false;

  std::vector<std::string> entryPaths;
  entryPaths.reserve(archive.entries.size());
  for (const gdtf::ArchiveEntry &entry : archive.entries)
    entryPaths.push_back(entry.path);

  gdtf::GdtfDescriptionSnapshot snapshot =
      gdtf::ReadGdtfDescription(archive.descriptionXml, entryPaths);
  if (!snapshot.Success())
    return false;

  outSummary.manufacturer = snapshot.manufacturer;
  outSummary.description = snapshot.description;
  outSummary.creationDate = snapshot.createDate;
  outSummary.revision = snapshot.revision;
  outSummary.version = snapshot.dataVersion;

  ApplyLatestRevision(snapshot, outSummary);

  if (outSummary.version.empty())
    outSummary.version = outSummary.revision;
  outSummary.creationDate = FormatMetadataTimestamp(outSummary.creationDate);
  outSummary.lastModified = FormatMetadataTimestamp(outSummary.lastModified);

  return true;
}
