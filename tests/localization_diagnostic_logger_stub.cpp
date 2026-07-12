#include "diagnostics/DiagnosticLogger.h"

namespace diagnostics {

// Ignores localization diagnostics in lightweight unit tests.
void DiagnosticLogger::Info(const std::string &) {}

// Ignores localization warnings in lightweight unit tests.
void DiagnosticLogger::Warning(const std::string &) {}

} // namespace diagnostics
