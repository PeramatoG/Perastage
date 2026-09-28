#pragma once

#include "inspection/gdtf_inspection.h"
#include "inspection/resource_inspection.h"

#include <string>

namespace gui::inspection {

enum class InspectorDetailsPage { Summary, GdtfDetails, Issues, Diagnostics };

std::string
DiagnosticLocationText(const perastage::inspection::Diagnostic &diagnostic);
const char *GdtfReadStatusLabel(perastage::inspection::GdtfReadStatus status);
const char *
ResourceKindLabel(const perastage::inspection::ResourceDescriptor &resource);
const char *InspectorDetailsPageToken(InspectorDetailsPage page);
InspectorDetailsPage ParseInspectorDetailsPageToken(const std::string &value);
std::string FormatIssueGroupLine(std::size_t count, const std::string &code,
                                 const std::string &classification,
                                 const std::string &severity);
std::size_t XmlLineNumberDigits(int lineCount);

} // namespace gui::inspection
