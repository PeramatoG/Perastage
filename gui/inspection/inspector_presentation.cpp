#include "inspection/inspector_presentation.h"

#include "filesystem_path_utils.h"

#include <sstream>

namespace gui::inspection {

// Formats every structured diagnostic location component deterministically.
std::string
DiagnosticLocationText(const perastage::inspection::Diagnostic &diagnostic) {
  if (!diagnostic.location)
    return {};
  const auto &location = *diagnostic.location;
  std::ostringstream text;
  bool separator = false;
  const auto append = [&text, &separator](const char *name,
                                          const std::string &value) {
    if (separator)
      text << "; ";
    text << name << '=' << value;
    separator = true;
  };
  if (location.sourcePath)
    append("source_path", PathUtils::PathToUtf8(*location.sourcePath));
  if (location.packageEntry)
    append("package_entry", *location.packageEntry);
  if (location.xmlPath)
    append("xml_path", *location.xmlPath);
  if (location.line)
    append("line", std::to_string(*location.line));
  if (location.column)
    append("column", std::to_string(*location.column));
  return text.str();
}

// Maps the existing Core GDTF read status to a localizable source label.
const char *GdtfReadStatusLabel(perastage::inspection::GdtfReadStatus status) {
  using perastage::inspection::GdtfReadStatus;
  switch (status) {
  case GdtfReadStatus::Canonical:
    return "Canonical";
  case GdtfReadStatus::CompatibilityAccepted:
    return "Compatibility accepted";
  case GdtfReadStatus::Unusable:
    return "Unusable";
  }
  return "Unusable";
}

// Returns a technical resource-kind label only for package files.
const char *
ResourceKindLabel(const perastage::inspection::ResourceDescriptor &resource) {
  using perastage::inspection::PackageEntryType;
  using perastage::inspection::ResourceKind;
  if (resource.entryType == PackageEntryType::Directory)
    return "";
  switch (resource.kind) {
  case ResourceKind::XmlText:
    return "XML text";
  case ResourceKind::Text:
    return "text";
  case ResourceKind::Image:
    return "image";
  case ResourceKind::Model:
    return "model";
  case ResourceKind::NestedGdtf:
    return "nested GDTF";
  case ResourceKind::Binary:
    return "binary";
  }
  return "binary";
}

// Returns the stable preference token for one semantic details page.
const char *InspectorDetailsPageToken(InspectorDetailsPage page) {
  switch (page) {
  case InspectorDetailsPage::Summary: return "summary";
  case InspectorDetailsPage::GdtfDetails: return "gdtf-details";
  case InspectorDetailsPage::Issues: return "issues";
  case InspectorDetailsPage::Diagnostics: return "diagnostics";
  }
  return "summary";
}

// Parses current tokens and permissively migrates legacy numeric indices.
InspectorDetailsPage ParseInspectorDetailsPageToken(const std::string &value) {
  if (value == "gdtf-details") return InspectorDetailsPage::GdtfDetails;
  if (value == "issues" || value == "1") return InspectorDetailsPage::Issues;
  if (value == "diagnostics" || value == "2" || value == "3")
    return InspectorDetailsPage::Diagnostics;
  return InspectorDetailsPage::Summary;
}

// Formats an issue row with deterministic ASCII punctuation around UTF-8 data.
std::string FormatIssueGroupLine(std::size_t count, const std::string &code,
                                 const std::string &classification,
                                 const std::string &severity) {
  return std::to_string(count) + " x " + code + "  [" + classification +
         ", " + severity + "]\n";
}

// Returns the decimal width needed for a positive displayed XML line count.
std::size_t XmlLineNumberDigits(int lineCount) {
  return std::to_string(lineCount > 0 ? lineCount : 1).size();
}

} // namespace gui::inspection
