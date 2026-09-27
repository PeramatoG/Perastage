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

} // namespace gui::inspection
