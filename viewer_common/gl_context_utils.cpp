#include "gl_context_utils.h"

#include "diagnostics/DiagnosticLogger.h"

#include <set>
#include <sstream>
#include <string>

#include <wx/log.h>

namespace gl_lifecycle {
namespace {
// Logs a context-bind failure once for each component and operation pair.
void LogBindFailureOnce(wxGLCanvas &canvas, const char *componentName,
                        const char *operationName, const char *reason,
                        bool includeShownOnScreen) {
  static std::set<std::string> loggedFailures;
  const std::string component = componentName ? componentName : "OpenGL component";
  const std::string operation = operationName ? operationName : "SetCurrent";
  const std::string key = component + "|" + operation + "|" + reason;
  if (!loggedFailures.insert(key).second)
    return;

  std::ostringstream message;
  message << component << ": " << operation << " failed: " << reason;
  if (includeShownOnScreen)
    message << "; shownOnScreen=" << (canvas.IsShownOnScreen() ? "true" : "false");
  diagnostics::DiagnosticLogger::Warning(message.str());
  wxLogWarning("%s", message.str().c_str());
}
} // namespace

// Binds an OpenGL context and logs a throttled diagnostic when binding fails.
bool TrySetCurrent(wxGLCanvas &canvas, wxGLContext *context,
                   const char *componentName, const char *operationName,
                   bool includeShownOnScreen) {
  if (!context) {
    LogBindFailureOnce(canvas, componentName, operationName, "missing context",
                       includeShownOnScreen);
    return false;
  }
  if (!canvas.SetCurrent(*context)) {
    LogBindFailureOnce(canvas, componentName, operationName, "SetCurrent returned false",
                       includeShownOnScreen);
    return false;
  }
  return true;
}

// Initializes GLEW for a validated current context using the shared diagnostic path.
GLEWInitResult InitializeGlew(wxGLCanvas &canvas, wxGLContext &context,
                              const char *componentName) {
  return InitializeGlewForCurrentContext(canvas, context, componentName);
}

} // namespace gl_lifecycle
