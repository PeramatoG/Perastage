#pragma once

#include <GL/glew.h>
#ifdef __APPLE__
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

namespace viewer3d::render {

class OpenGLStateGuard {
public:
  OpenGLStateGuard();
  ~OpenGLStateGuard();

  OpenGLStateGuard(const OpenGLStateGuard &) = delete;
  OpenGLStateGuard &operator=(const OpenGLStateGuard &) = delete;

private:
  GLint m_currentProgram = 0;
  GLint m_vertexArrayBinding = 0;
  GLint m_arrayBufferBinding = 0;
  GLint m_elementArrayBufferBinding = 0;
  GLint m_framebufferBinding = 0;
  GLint m_readFramebufferBinding = 0;
  GLint m_drawFramebufferBinding = 0;
  GLint m_readBuffer = 0;
  GLint m_viewport[4] = {0, 0, 0, 0};
  GLboolean m_depthTest = GL_FALSE;
  GLint m_depthFunc = GL_LESS;
  GLboolean m_depthWriteMask = GL_TRUE;
  GLboolean m_cullFace = GL_FALSE;
  GLint m_cullFaceMode = GL_BACK;
  GLint m_frontFace = GL_CCW;
  GLboolean m_blend = GL_FALSE;
  GLboolean m_stencilTest = GL_FALSE;
  GLboolean m_colorWriteMask[4] = {GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE};
  GLboolean m_polygonOffsetFill = GL_FALSE;
  GLint m_activeTexture = GL_TEXTURE0;
  GLint m_texture2dBinding = 0;
  bool m_hasReadFramebufferBinding = false;
  bool m_hasDrawFramebufferBinding = false;
  bool m_hasVertexArrayBinding = false;
  bool m_hasProgramBinding = false;
};

bool HasCurrentOpenGLContext();
const char *SafeGlString(GLenum name);
void ClearOpenGLErrors();
GLenum DrainOpenGLErrors();

} // namespace viewer3d::render
