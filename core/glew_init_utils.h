#pragma once

#include <GL/glew.h>

// Normalizes GLEW initialization results for Linux platforms with Wayland/EGL contexts.
GLenum NormalizeGlewInitResultForLinux(GLenum glewResult);
