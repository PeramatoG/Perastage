#include "glew_init_utils.h"

// Converts the non-fatal Wayland GLX probe error into success so OpenGL setup can continue.
GLenum NormalizeGlewInitResultForLinux(GLenum glewResult) {
#if defined(__linux__)
  if (glewResult == GLEW_ERROR_NO_GLX_DISPLAY)
    return GLEW_OK;
#endif
  return glewResult;
}
