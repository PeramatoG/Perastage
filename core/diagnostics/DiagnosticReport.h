#pragma once

#include <filesystem>
#include <string>

namespace diagnostics {

// Stores OpenGL driver information captured after a context is initialized.
struct OpenGLInfo {
  std::string vendor;
  std::string renderer;
  std::string version;
};

// Builds and exports local diagnostic reports without sending data anywhere.
class DiagnosticReport {
public:
  static void SetOpenGLInfo(OpenGLInfo info);
  static OpenGLInfo GetOpenGLInfo();
  static std::string BuildTextReport(const std::string &reason,
                                     const std::string &eventDetails = {},
                                     const std::string &stackTrace = {});
  static bool ExportToFile(std::filesystem::path *outPath,
                           std::string *error = nullptr);
};

} // namespace diagnostics
