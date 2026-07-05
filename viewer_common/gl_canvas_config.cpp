#include "gl_canvas_config.h"

namespace gl_lifecycle {
namespace {
static const int kStandardAttributes[] = {
    WX_GL_RGBA,
    WX_GL_DOUBLEBUFFER,
    WX_GL_DEPTH_SIZE, 24,
    0
};

static const int kMsaa2xAttributes[] = {
    WX_GL_RGBA,
    WX_GL_DOUBLEBUFFER,
    WX_GL_DEPTH_SIZE, 24,
    WX_GL_SAMPLE_BUFFERS, 1,
    WX_GL_SAMPLES, 2,
    0
};

static const int kMsaa4xAttributes[] = {
    WX_GL_RGBA,
    WX_GL_DOUBLEBUFFER,
    WX_GL_DEPTH_SIZE, 24,
    WX_GL_SAMPLE_BUFFERS, 1,
    WX_GL_SAMPLES, 4,
    0
};
} // namespace

// Returns the standard Perastage OpenGL canvas attributes.
const int *GetStandardCanvasAttributes() { return kStandardAttributes; }

// Returns OpenGL canvas attributes with the best supported MSAA level up to the request.
const int *GetMsaaCanvasAttributes(int requestedSamples) {
  if (requestedSamples >= 4 && wxGLCanvas::IsDisplaySupported(kMsaa4xAttributes))
    return kMsaa4xAttributes;
  if (requestedSamples >= 2 && wxGLCanvas::IsDisplaySupported(kMsaa2xAttributes))
    return kMsaa2xAttributes;
  if (requestedSamples >= 4 && wxGLCanvas::IsDisplaySupported(kMsaa2xAttributes))
    return kMsaa2xAttributes;
  return kStandardAttributes;
}

} // namespace gl_lifecycle
