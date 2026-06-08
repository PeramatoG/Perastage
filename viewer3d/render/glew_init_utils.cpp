#include <GL/glew.h>
#include "glew_init_utils.h"
#include "diagnostics/DiagnosticLogger.h"
#include "diagnostics/DiagnosticReport.h"

#include <cstdlib>
#include <string>

#include <wx/log.h>

namespace {
// Logs Linux backend environment details once per process for diagnostics.
void LogLinuxBackendDiagnosticsOnce(const char *panelName) {
#ifdef __linux__
  static bool logged = false;
  if (logged) {
    return;
  }
  logged = true;
  const char *gdkBackend = std::getenv("GDK_BACKEND");
  const char *waylandDisplay = std::getenv("WAYLAND_DISPLAY");
  const char *display = std::getenv("DISPLAY");
  const char *sessionType = std::getenv("XDG_SESSION_TYPE");
  const char *qtQpaPlatform = std::getenv("QT_QPA_PLATFORM");
  wxLogDebug(
      "%s backend diagnostics: GDK_BACKEND=%s WAYLAND_DISPLAY=%s DISPLAY=%s "
      "XDG_SESSION_TYPE=%s QT_QPA_PLATFORM=%s",
      panelName, gdkBackend ? gdkBackend : "<unset>",
      waylandDisplay ? waylandDisplay : "<unset>",
      display ? display : "<unset>", sessionType ? sessionType : "<unset>",
      qtQpaPlatform ? qtQpaPlatform : "<unset>");
#else
  (void)panelName;
#endif
}
} // namespace

// Initializes GLEW and handles Wayland GLX probing only when OpenGL is usable.
GLEWInitResult InitializeGlewForCurrentContext(wxGLCanvas &canvas,
                                               wxGLContext &context,
                                               const char *panelName) {
  GLEWInitResult result{};
  if (!canvas.SetCurrent(context)) {
    result.message = std::string(panelName) + ": SetCurrent failed";
    return result;
  }

  LogLinuxBackendDiagnosticsOnce(panelName);

  glewExperimental = GL_TRUE;
  const GLenum glewResult = glewInit();
  const GLubyte *glVersionRaw = glGetString(GL_VERSION);
  const GLubyte *glVendorRaw = glGetString(GL_VENDOR);
  const GLubyte *glRendererRaw = glGetString(GL_RENDERER);
  const bool hasGlVersion = glVersionRaw != nullptr;
  wxLogDebug("%s GL context status: GL_VERSION %s", panelName,
             hasGlVersion ? "available" : "missing");
  if (hasGlVersion || glVendorRaw || glRendererRaw) {
    diagnostics::OpenGLInfo openGlInfo;
    openGlInfo.vendor = glVendorRaw
                            ? reinterpret_cast<const char *>(glVendorRaw)
                            : "unknown";
    openGlInfo.renderer = glRendererRaw
                              ? reinterpret_cast<const char *>(glRendererRaw)
                              : "unknown";
    openGlInfo.version = glVersionRaw
                             ? reinterpret_cast<const char *>(glVersionRaw)
                             : "unknown";
    diagnostics::DiagnosticReport::SetOpenGLInfo(openGlInfo);
    diagnostics::DiagnosticLogger::Info(
        std::string(panelName) + " OpenGL context: vendor=" +
        openGlInfo.vendor + " renderer=" + openGlInfo.renderer +
        " version=" + openGlInfo.version);
  }

  if (glewResult == GLEW_OK) {
    result.success = true;
    result.message = std::string(panelName) + ": GLEW initialized";
    diagnostics::DiagnosticLogger::Info(result.message);
  } else if (glewResult == GLEW_ERROR_NO_GLX_DISPLAY && hasGlVersion) {
    result.success = true;
    result.isWarningOnly = true;
    result.message = std::string(panelName) +
                     ": GLEW reported NO_GLX_DISPLAY but GL context is valid";
    diagnostics::DiagnosticLogger::Warning(result.message);
  } else {
    result.message = std::string(panelName) + ": GLEW init failed: " +
                     reinterpret_cast<const char *>(glewGetErrorString(glewResult));
    diagnostics::DiagnosticLogger::Error(result.message);
  }

  glGetError();
  return result;
}
