#pragma once

#include <filesystem>
#include <string>
#include <cstdint>

namespace diagnostics {

// Stores OpenGL driver information captured after a context is initialized.
struct OpenGLInfo {
  std::string vendor;
  std::string renderer;
  std::string version;
};

// Identifies the most recent Viewer2D RGBA capture backend.
enum class Viewer2DCaptureBackend {
  NotUsed,
  Fbo,
  BackBufferFallback,
  Failed,
};

// Stores local Viewer2D RGBA capture diagnostics for manual reports.
struct Viewer2DCaptureInfo {
  Viewer2DCaptureBackend backend = Viewer2DCaptureBackend::NotUsed;
  std::uint64_t fboSuccessCount = 0;
  std::uint64_t fallbackCount = 0;
  std::uint64_t failureCount = 0;
  int lastWidth = 0;
  int lastHeight = 0;
  std::string lastDiagnostic;
  bool fallbackEverUsed = false;
};

// Builds and exports local diagnostic reports without sending data anywhere.
class DiagnosticReport {
public:
  static void SetOpenGLInfo(OpenGLInfo info);
  static OpenGLInfo GetOpenGLInfo();
  static void RecordViewer2DFboCapture(int width, int height);
  static void RecordViewer2DBackBufferFallback(int width, int height,
                                               const std::string &reason);
  static void RecordViewer2DCaptureFailure(int width, int height,
                                           const std::string &reason);
  static Viewer2DCaptureInfo GetViewer2DCaptureInfo();
  static std::string Viewer2DCaptureBackendName(Viewer2DCaptureBackend backend);
  static std::string BuildTextReport(const std::string &reason,
                                     const std::string &eventDetails = {},
                                     const std::string &stackTrace = {});
  static bool ExportToFile(std::filesystem::path *outPath,
                           std::string *error = nullptr);
};

} // namespace diagnostics
