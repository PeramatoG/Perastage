#pragma once

#include "inspection/gdtf_inspection.h"
#include "inspection/resource_inspection.h"

#include <string>

namespace gui::inspection {

std::string
DiagnosticLocationText(const perastage::inspection::Diagnostic &diagnostic);
const char *GdtfReadStatusLabel(perastage::inspection::GdtfReadStatus status);
const char *
ResourceKindLabel(const perastage::inspection::ResourceDescriptor &resource);

} // namespace gui::inspection
