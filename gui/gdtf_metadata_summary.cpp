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

#include <initializer_list>
#include <memory>
#include <tinyxml2.h>
#include <wx/datetime.h>
#include <wx/string.h>
#include <wx/wfstream.h>
#include <wx/zipstrm.h>

namespace {

// Returns the first non-empty attribute value from a metadata element.
std::string FirstNonEmptyAttribute(tinyxml2::XMLElement *element,
                                   std::initializer_list<const char *> names) {
  if (!element)
    return "";
  for (const char *name : names) {
    if (!name)
      continue;
    if (const char *value = element->Attribute(name); value && *value)
      return value;
  }
  return "";
}

// Extracts the ModifiedBy attribute from a GDTF revision element.
std::string ExtractRevisionModifiedBy(tinyxml2::XMLElement *revision) {
  if (!revision)
    return {};
  if (const char *modifiedBy = revision->Attribute("ModifiedBy");
      modifiedBy && *modifiedBy) {
    return modifiedBy;
  }
  return {};
}

// Extracts the UserID attribute from a GDTF revision element.
std::string ExtractRevisionUserId(tinyxml2::XMLElement *revision) {
  if (!revision)
    return "0";
  if (const char *userId = revision->Attribute("UserID"); userId && *userId)
    return userId;
  return "0";
}

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
} // namespace

// Loads a compact metadata summary from a GDTF archive.
bool LoadGdtfMetadataSummary(const std::string &gdtfPath,
                             GdtfMetadataSummary &outSummary) {
  outSummary = {};
  outSummary.userId = "0";
  if (gdtfPath.empty())
    return false;

  wxFileInputStream input(wxString::FromUTF8(gdtfPath));
  if (!input.IsOk())
    return false;

  wxZipInputStream zipInput(input);
  std::unique_ptr<wxZipEntry> entry;
  std::string descriptionXml;
  while ((entry.reset(zipInput.GetNextEntry())), entry) {
    if (entry->GetName().CmpNoCase("description.xml") != 0)
      continue;
    char buffer[4096];
    while (true) {
      zipInput.Read(buffer, sizeof(buffer));
      const size_t count = zipInput.LastRead();
      if (count == 0)
        break;
      descriptionXml.append(buffer, buffer + count);
    }
    break;
  }
  if (descriptionXml.empty())
    return false;

  tinyxml2::XMLDocument doc;
  if (doc.Parse(descriptionXml.c_str(), descriptionXml.size()) !=
      tinyxml2::XML_SUCCESS) {
    return false;
  }

  tinyxml2::XMLElement *root = doc.FirstChildElement("GDTF");
  tinyxml2::XMLElement *fixtureType =
      root ? root->FirstChildElement("FixtureType")
           : doc.FirstChildElement("FixtureType");
  if (!fixtureType)
    return false;

  outSummary.manufacturer =
      FirstNonEmptyAttribute(fixtureType, {"Manufacturer"});
  outSummary.description = FirstNonEmptyAttribute(fixtureType, {"Description"});
  outSummary.creationDate = FirstNonEmptyAttribute(
      fixtureType, {"CreateDate", "CreationDate", "DateCreated"});
  outSummary.revision = FirstNonEmptyAttribute(
      fixtureType, {"Revision", "DataVersion", "Version"});

  if (root) {
    if (outSummary.version.empty())
      outSummary.version = FirstNonEmptyAttribute(
          root, {"DataVersion", "Version", "CreatedWith"});
    if (outSummary.creationDate.empty())
      outSummary.creationDate = FirstNonEmptyAttribute(
          root, {"CreateDate", "CreationDate", "DateCreated"});
  }

  tinyxml2::XMLElement *revisions = fixtureType->FirstChildElement("Revisions");
  if (revisions) {
    tinyxml2::XMLElement *firstRevision =
        revisions->FirstChildElement("Revision");
    tinyxml2::XMLElement *latestRevision = nullptr;
    for (tinyxml2::XMLElement *rev = revisions->FirstChildElement("Revision");
         rev; rev = rev->NextSiblingElement("Revision")) {
      latestRevision = rev;
    }
    if (latestRevision) {
      if (outSummary.revision.empty()) {
        outSummary.revision = FirstNonEmptyAttribute(
            latestRevision, {"Text", "Comment", "Version"});
      }
      outSummary.lastModified =
          FirstNonEmptyAttribute(latestRevision, {"Date", "TimeStamp"});
      outSummary.modifiedBy = ExtractRevisionModifiedBy(latestRevision);
      outSummary.userId = ExtractRevisionUserId(latestRevision);
      if (outSummary.creationDate.empty()) {
        outSummary.creationDate =
            FirstNonEmptyAttribute(firstRevision, {"Date", "TimeStamp"});
      }
    }
  }

  if (outSummary.version.empty())
    outSummary.version = outSummary.revision;
  outSummary.creationDate = FormatMetadataTimestamp(outSummary.creationDate);
  outSummary.lastModified = FormatMetadataTimestamp(outSummary.lastModified);

  return true;
}
