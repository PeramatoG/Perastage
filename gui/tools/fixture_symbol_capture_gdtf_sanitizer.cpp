#include "tools/fixture_symbol_capture_gdtf_sanitizer.h"

#include <algorithm>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include <tinyxml2.h>
#include <wx/filename.h>
#include <wx/log.h>
#include <wx/wfstream.h>
#include <wx/zipstrm.h>

namespace fs = std::filesystem;

namespace tools {
namespace {

// Normalize zip entry paths for deterministic filtering.
std::string NormalizeArchivePath(std::string value) {
  std::replace(value.begin(), value.end(), '\\', '/');
  while (value.rfind("./", 0) == 0)
    value.erase(0, 2);
  while (!value.empty() && value.front() == '/')
    value.erase(value.begin());
  return value;
}

// Read the full current zip entry payload into a string buffer.
bool ReadEntryBytes(wxZipInputStream &zip, std::string &out) {
  out.clear();
  char buffer[4096];
  while (true) {
    zip.Read(buffer, sizeof(buffer));
    const size_t count = zip.LastRead();
    if (count == 0)
      break;
    out.append(buffer, count);
  }
  return true;
}

// Return true when an archive path points to generated Perastage symbol SVG entries.
bool IsGeneratedSymbolSvgEntry(const std::string &normalizedPath) {
  const bool isSvg = normalizedPath.size() >= 4 &&
                     normalizedPath.substr(normalizedPath.size() - 4) == ".svg";
  if (!isSvg)
    return false;
  if (normalizedPath.rfind("models/svg/", 0) == 0)
    return true;
  if (normalizedPath.rfind("models/svg_side/", 0) == 0)
    return true;
  if (normalizedPath.rfind("models/svg_front/", 0) == 0)
    return true;
  if (normalizedPath.rfind("models/svg_bottom/", 0) == 0)
    return true;
  return false;
}

// Remove generated SVG offset attributes from every Model node in description.xml.
int RemoveSvgOffsetAttributes(tinyxml2::XMLDocument &doc) {
  static const char *kAttrs[] = {"SVGOffsetX", "SVGOffsetY", "SVGSideOffsetX",
                                 "SVGSideOffsetY", "SVGFrontOffsetX",
                                 "SVGFrontOffsetY"};
  int removed = 0;
  tinyxml2::XMLElement *fixtureType = doc.FirstChildElement("GDTF");
  if (fixtureType)
    fixtureType = fixtureType->FirstChildElement("FixtureType");
  if (!fixtureType)
    fixtureType = doc.FirstChildElement("FixtureType");
  if (!fixtureType)
    return removed;
  tinyxml2::XMLElement *models = fixtureType->FirstChildElement("Models");
  if (!models)
    return removed;
  for (tinyxml2::XMLElement *model = models->FirstChildElement("Model"); model;
       model = model->NextSiblingElement("Model")) {
    for (const char *attr : kAttrs) {
      if (model->Attribute(attr)) {
        model->DeleteAttribute(attr);
        ++removed;
      }
    }
  }
  return removed;
}

} // namespace

// Build a temporary sanitized GDTF archive that strips generated SVG symbol artifacts.
FixtureSymbolCaptureSanitizeResult BuildSanitizedFixtureCaptureGdtf(
    const std::string &sourceGdtfPath) {
  FixtureSymbolCaptureSanitizeResult result;
  wxFileInputStream input(sourceGdtfPath);
  if (!input.IsOk()) {
    result.error = "Could not open source GDTF for symbol-capture sanitization.";
    return result;
  }

  const wxFileName sourceName(wxString::FromUTF8(sourceGdtfPath));
  const wxString tempFile = wxFileName::CreateTempFileName("perastage_symbol_capture_");
  if (tempFile.empty()) {
    result.error = "Could not create temporary file for symbol-capture sanitization.";
    return result;
  }
  result.sanitizedGdtfPath = tempFile.ToStdString();

  wxFileOutputStream output(tempFile);
  if (!output.IsOk()) {
    result.error = "Could not open temporary GDTF output file for symbol capture.";
    return result;
  }

  wxZipInputStream zipIn(input);
  wxZipOutputStream zipOut(output);
  std::unique_ptr<wxZipEntry> entry;
  bool foundDescription = false;

  while ((entry.reset(zipIn.GetNextEntry())), entry) {
    if (entry->IsDir())
      continue;
    const std::string normalizedPath = NormalizeArchivePath(entry->GetName().ToStdString());
    std::string payload;
    if (!ReadEntryBytes(zipIn, payload)) {
      result.error = "Could not read a GDTF zip entry during capture sanitization.";
      return result;
    }

    if (IsGeneratedSymbolSvgEntry(normalizedPath)) {
      ++result.removedSvgEntries;
      continue;
    }

    if (normalizedPath == "description.xml") {
      foundDescription = true;
      tinyxml2::XMLDocument doc;
      if (doc.Parse(payload.c_str(), payload.size()) != tinyxml2::XML_SUCCESS) {
        result.error = "Could not parse description.xml while sanitizing GDTF for capture.";
        return result;
      }
      result.removedSvgOffsetAttributes += RemoveSvgOffsetAttributes(doc);
      tinyxml2::XMLPrinter printer;
      doc.Print(&printer);
      payload.assign(printer.CStr(), static_cast<size_t>(printer.CStrSize() - 1));
    }

    auto outEntry = std::make_unique<wxZipEntry>(wxString::FromUTF8(normalizedPath));
    if (!zipOut.PutNextEntry(outEntry.release())) {
      result.error = "Could not create output zip entry while sanitizing GDTF.";
      return result;
    }
    zipOut.Write(payload.data(), payload.size());
  }

  if (!foundDescription) {
    result.error = "Could not find description.xml while sanitizing GDTF for capture.";
    return result;
  }

  if (!zipOut.Close()) {
    result.error = "Could not finalize sanitized GDTF archive for capture.";
    return result;
  }

  result.ok = true;
  wxLogTrace("fixture_symbol_capture",
             "sanitizedGdtf source=%s temp=%s removedSvgEntries=%d removedSvgOffsets=%d",
             sourceGdtfPath, result.sanitizedGdtfPath, result.removedSvgEntries,
             result.removedSvgOffsetAttributes);
  return result;
}

} // namespace tools
