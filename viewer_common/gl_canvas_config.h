#pragma once

#include <wx/glcanvas.h>

namespace gl_lifecycle {

// Returns the standard Perastage OpenGL canvas attributes.
const int *GetStandardCanvasAttributes();

// Returns OpenGL canvas attributes with the best supported MSAA level up to the request.
const int *GetMsaaCanvasAttributes(int requestedSamples);

} // namespace gl_lifecycle
