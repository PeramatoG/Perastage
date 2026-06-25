#include "opengl_state_guard.h"

namespace viewer3d::render {

// Captures OpenGL state that picking and highlight passes commonly modify.
OpenGLStateGuard::OpenGLStateGuard() {
  m_hasProgramBinding = GLEW_VERSION_2_0;
  m_hasVertexArrayBinding = GLEW_VERSION_3_0 || GLEW_ARB_vertex_array_object;
  m_hasReadFramebufferBinding = GLEW_VERSION_3_0 || GLEW_EXT_framebuffer_blit;
  m_hasDrawFramebufferBinding = GLEW_VERSION_3_0 || GLEW_EXT_framebuffer_blit;

  if (m_hasProgramBinding)
    glGetIntegerv(GL_CURRENT_PROGRAM, &m_currentProgram);
  if (m_hasVertexArrayBinding)
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &m_vertexArrayBinding);
  glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &m_arrayBufferBinding);
  glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &m_elementArrayBufferBinding);
  glGetIntegerv(GL_FRAMEBUFFER_BINDING, &m_framebufferBinding);
  if (m_hasReadFramebufferBinding)
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &m_readFramebufferBinding);
  if (m_hasDrawFramebufferBinding)
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &m_drawFramebufferBinding);
  glGetIntegerv(GL_READ_BUFFER, &m_readBuffer);
  glGetIntegerv(GL_VIEWPORT, m_viewport);
  m_depthTest = glIsEnabled(GL_DEPTH_TEST);
  glGetIntegerv(GL_DEPTH_FUNC, &m_depthFunc);
  glGetBooleanv(GL_DEPTH_WRITEMASK, &m_depthWriteMask);
  m_cullFace = glIsEnabled(GL_CULL_FACE);
  glGetIntegerv(GL_CULL_FACE_MODE, &m_cullFaceMode);
  glGetIntegerv(GL_FRONT_FACE, &m_frontFace);
  m_blend = glIsEnabled(GL_BLEND);
  m_stencilTest = glIsEnabled(GL_STENCIL_TEST);
  glGetBooleanv(GL_COLOR_WRITEMASK, m_colorWriteMask);
  m_polygonOffsetFill = glIsEnabled(GL_POLYGON_OFFSET_FILL);
  glGetIntegerv(GL_ACTIVE_TEXTURE, &m_activeTexture);
  glGetIntegerv(GL_TEXTURE_BINDING_2D, &m_texture2dBinding);
}

// Restores OpenGL state captured before the guarded picking or highlight work.
OpenGLStateGuard::~OpenGLStateGuard() {
  if (m_hasProgramBinding)
    glUseProgram(static_cast<GLuint>(m_currentProgram));
  if (m_hasVertexArrayBinding)
    glBindVertexArray(static_cast<GLuint>(m_vertexArrayBinding));
  glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(m_arrayBufferBinding));
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,
               static_cast<GLuint>(m_elementArrayBufferBinding));
  if (m_hasReadFramebufferBinding || m_hasDrawFramebufferBinding) {
    if (m_hasReadFramebufferBinding)
      glBindFramebuffer(GL_READ_FRAMEBUFFER,
                        static_cast<GLuint>(m_readFramebufferBinding));
    if (m_hasDrawFramebufferBinding)
      glBindFramebuffer(GL_DRAW_FRAMEBUFFER,
                        static_cast<GLuint>(m_drawFramebufferBinding));
  } else {
    glBindFramebuffer(GL_FRAMEBUFFER,
                      static_cast<GLuint>(m_framebufferBinding));
  }
  glReadBuffer(static_cast<GLenum>(m_readBuffer));
  glViewport(m_viewport[0], m_viewport[1], m_viewport[2], m_viewport[3]);
  m_depthTest ? glEnable(GL_DEPTH_TEST) : glDisable(GL_DEPTH_TEST);
  glDepthFunc(static_cast<GLenum>(m_depthFunc));
  glDepthMask(m_depthWriteMask);
  m_cullFace ? glEnable(GL_CULL_FACE) : glDisable(GL_CULL_FACE);
  glCullFace(static_cast<GLenum>(m_cullFaceMode));
  glFrontFace(static_cast<GLenum>(m_frontFace));
  m_blend ? glEnable(GL_BLEND) : glDisable(GL_BLEND);
  m_stencilTest ? glEnable(GL_STENCIL_TEST) : glDisable(GL_STENCIL_TEST);
  glColorMask(m_colorWriteMask[0], m_colorWriteMask[1], m_colorWriteMask[2],
              m_colorWriteMask[3]);
  m_polygonOffsetFill ? glEnable(GL_POLYGON_OFFSET_FILL)
                      : glDisable(GL_POLYGON_OFFSET_FILL);
  glActiveTexture(static_cast<GLenum>(m_activeTexture));
  glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(m_texture2dBinding));
}

// Returns whether basic OpenGL queries can see a current context.
bool HasCurrentOpenGLContext() { return glGetString(GL_VERSION) != nullptr; }

// Returns an OpenGL string or a stable placeholder when no context reports it.
const char *SafeGlString(GLenum name) {
  const GLubyte *value = glGetString(name);
  return value ? reinterpret_cast<const char *>(value) : "unknown";
}

// Clears queued OpenGL errors before an operation that needs clean diagnostics.
void ClearOpenGLErrors() {
  while (glGetError() != GL_NO_ERROR) {
  }
}

// Drains OpenGL errors and returns the first error seen, if any.
GLenum DrainOpenGLErrors() {
  GLenum first = GL_NO_ERROR;
  for (GLenum err = glGetError(); err != GL_NO_ERROR; err = glGetError()) {
    if (first == GL_NO_ERROR)
      first = err;
  }
  return first;
}

} // namespace viewer3d::render
