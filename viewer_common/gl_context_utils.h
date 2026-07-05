#pragma once

#include "glew_init_utils.h"

#include <wx/glcanvas.h>

namespace gl_lifecycle {

// Binds an OpenGL context and logs a throttled diagnostic when binding fails.
bool TrySetCurrent(wxGLCanvas &canvas, wxGLContext *context,
                   const char *componentName, const char *operationName,
                   bool includeShownOnScreen = true);

// Initializes GLEW for a validated current context using the shared diagnostic path.
GLEWInitResult InitializeGlew(wxGLCanvas &canvas, wxGLContext &context,
                              const char *componentName);

} // namespace gl_lifecycle
