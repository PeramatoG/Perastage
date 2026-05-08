#pragma once

#include <string>

#include <wx/glcanvas.h>

// Stores the result of a guarded GLEW initialization attempt.
struct GLEWInitResult {
  bool success = false;
  bool isWarningOnly = false;
  std::string message;
};

// Initializes GLEW for a current context and validates OpenGL availability.
GLEWInitResult InitializeGlewForCurrentContext(wxGLCanvas &canvas,
                                               wxGLContext &context,
                                               const char *panelName);
