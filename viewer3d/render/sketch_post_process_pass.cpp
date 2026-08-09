#include "sketch_post_process_pass.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#include <GL/glew.h>
#ifdef __APPLE__
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

#include "opengl_state_guard.h"

namespace {

// Compiles one post-process shader stage and returns zero on failure.
GLuint CompileShader(GLenum type, const char *source) {
  const GLuint shader = glCreateShader(type);
  if (shader == 0)
    return 0;
  glShaderSource(shader, 1, &source, nullptr);
  glCompileShader(shader);
  GLint compiled = GL_FALSE;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
  if (compiled == GL_TRUE)
    return shader;
  glDeleteShader(shader);
  return 0;
}

// Creates the fullscreen three-tone program used after Standard rendering.
GLuint CreateProgram() {
  static constexpr const char *kVertexShader = R"glsl(
    #version 120
    varying vec2 vTexCoord;
    void main() {
      vTexCoord = gl_MultiTexCoord0.xy;
      gl_Position = gl_Vertex;
    }
  )glsl";
  static constexpr const char *kFragmentShader = R"glsl(
    #version 120
    uniform sampler2D uNeutralBase;
    uniform float uDarkLuminanceThreshold;
    uniform float uLightLuminanceThreshold;
    varying vec2 vTexCoord;
    void main() {
      vec4 base = texture2D(uNeutralBase, vTexCoord);
      float luminance = dot(base.rgb, vec3(0.2126, 0.7152, 0.0722));
      vec3 tone = vec3(1.0);
      if (luminance <= uDarkLuminanceThreshold)
        tone = vec3(0.62);
      else if (luminance <= uLightLuminanceThreshold)
        tone = vec3(0.84);
      float inkCoverage = 1.0 - base.a;
      gl_FragColor = vec4(mix(tone, vec3(0.0), inkCoverage), 1.0);
    }
  )glsl";

  const GLuint vertexShader = CompileShader(GL_VERTEX_SHADER, kVertexShader);
  const GLuint fragmentShader =
      CompileShader(GL_FRAGMENT_SHADER, kFragmentShader);
  if (vertexShader == 0 || fragmentShader == 0) {
    if (vertexShader != 0)
      glDeleteShader(vertexShader);
    if (fragmentShader != 0)
      glDeleteShader(fragmentShader);
    return 0;
  }

  const GLuint program = glCreateProgram();
  glAttachShader(program, vertexShader);
  glAttachShader(program, fragmentShader);
  glLinkProgram(program);
  glDeleteShader(vertexShader);
  glDeleteShader(fragmentShader);
  GLint linked = GL_FALSE;
  glGetProgramiv(program, GL_LINK_STATUS, &linked);
  if (linked == GL_TRUE)
    return program;
  glDeleteProgram(program);
  return 0;
}

} // namespace

struct SketchPostProcessPass::Impl {
  GLuint framebuffer = 0;
  GLuint colorTexture = 0;
  GLuint depthStencil = 0;
  GLuint program = 0;
  GLint destinationFramebuffer = 0;
  GLint viewport[4] = {0, 0, 0, 0};
  int width = 0;
  int height = 0;
  bool active = false;
  bool targetComplete = false;

  // Releases framebuffer and shader resources owned by the pass.
  void Release() {
    if (program != 0)
      glDeleteProgram(program);
    if (depthStencil != 0)
      glDeleteRenderbuffers(1, &depthStencil);
    if (colorTexture != 0)
      glDeleteTextures(1, &colorTexture);
    if (framebuffer != 0)
      glDeleteFramebuffers(1, &framebuffer);
    program = 0;
    depthStencil = 0;
    colorTexture = 0;
    framebuffer = 0;
    targetComplete = false;
  }

  // Allocates or resizes the neutral-base framebuffer for the current viewport.
  bool EnsureTarget(int requestedWidth, int requestedHeight) {
    if (requestedWidth <= 0 || requestedHeight <= 0)
      return false;
    if (framebuffer != 0 && width == requestedWidth &&
        height == requestedHeight)
      return targetComplete;

    if (depthStencil != 0)
      glDeleteRenderbuffers(1, &depthStencil);
    if (colorTexture != 0)
      glDeleteTextures(1, &colorTexture);
    if (framebuffer == 0)
      glGenFramebuffers(1, &framebuffer);
    targetComplete = false;

    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glGenTextures(1, &colorTexture);
    glBindTexture(GL_TEXTURE_2D, colorTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, requestedWidth, requestedHeight, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           colorTexture, 0);

    glGenRenderbuffers(1, &depthStencil);
    glBindRenderbuffer(GL_RENDERBUFFER, depthStencil);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, requestedWidth,
                          requestedHeight);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                              GL_RENDERBUFFER, depthStencil);
    width = requestedWidth;
    height = requestedHeight;
    targetComplete =
        glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    return targetComplete;
  }
};

// Initializes an empty reusable Sketch post-process pass.
SketchPostProcessPass::SketchPostProcessPass() : m_impl(std::make_unique<Impl>()) {}

// Releases the Sketch post-process OpenGL resources.
SketchPostProcessPass::~SketchPostProcessPass() { m_impl->Release(); }

// Redirects Standard opaque rendering into a neutral-base framebuffer.
bool SketchPostProcessPass::Begin() {
  m_impl->active = false;
  glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &m_impl->destinationFramebuffer);
  glGetIntegerv(GL_VIEWPORT, m_impl->viewport);
  GLint previousTexture = 0;
  GLint previousRenderbuffer = 0;
  glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture);
  glGetIntegerv(GL_RENDERBUFFER_BINDING, &previousRenderbuffer);
  if (!m_impl->EnsureTarget(m_impl->viewport[2], m_impl->viewport[3])) {
    glBindFramebuffer(GL_FRAMEBUFFER,
                      static_cast<GLuint>(m_impl->destinationFramebuffer));
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousTexture));
    glBindRenderbuffer(GL_RENDERBUFFER,
                       static_cast<GLuint>(previousRenderbuffer));
    return false;
  }

  GLfloat clearColor[4];
  glGetFloatv(GL_COLOR_CLEAR_VALUE, clearColor);
  glBindFramebuffer(GL_FRAMEBUFFER, m_impl->framebuffer);
  glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousTexture));
  glBindRenderbuffer(GL_RENDERBUFFER,
                     static_cast<GLuint>(previousRenderbuffer));
  glViewport(0, 0, m_impl->viewport[2], m_impl->viewport[3]);
  glClearColor(clearColor[0], clearColor[1], clearColor[2], 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
  m_impl->active = true;
  return true;
}

// Posterizes the neutral base into its original destination and restores depth.
void SketchPostProcessPass::Composite() {
  if (!m_impl->active)
    return;
  m_impl->active = false;
  if (m_impl->program == 0)
    m_impl->program = CreateProgram();
  if (m_impl->program == 0) {
    glBindFramebuffer(GL_READ_FRAMEBUFFER, m_impl->framebuffer);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER,
                      static_cast<GLuint>(m_impl->destinationFramebuffer));
    glBlitFramebuffer(0, 0, m_impl->width, m_impl->height,
                      m_impl->viewport[0], m_impl->viewport[1],
                      m_impl->viewport[0] + m_impl->viewport[2],
                      m_impl->viewport[1] + m_impl->viewport[3],
                      GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER,
                      static_cast<GLuint>(m_impl->destinationFramebuffer));
    glViewport(m_impl->viewport[0], m_impl->viewport[1], m_impl->viewport[2],
               m_impl->viewport[3]);
    return;
  }

  {
    viewer3d::render::OpenGLStateGuard stateGuard;
    glBindFramebuffer(GL_READ_FRAMEBUFFER, m_impl->framebuffer);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER,
                      static_cast<GLuint>(m_impl->destinationFramebuffer));
    glBlitFramebuffer(0, 0, m_impl->width, m_impl->height,
                      m_impl->viewport[0], m_impl->viewport[1],
                      m_impl->viewport[0] + m_impl->viewport[2],
                      m_impl->viewport[1] + m_impl->viewport[3],
                      GL_DEPTH_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER,
                      static_cast<GLuint>(m_impl->destinationFramebuffer));
    glViewport(m_impl->viewport[0], m_impl->viewport[1], m_impl->viewport[2],
               m_impl->viewport[3]);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_BLEND);
    glUseProgram(m_impl->program);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_impl->colorTexture);
    glUniform1i(glGetUniformLocation(m_impl->program, "uNeutralBase"), 0);
    glUniform1f(
        glGetUniformLocation(m_impl->program, "uDarkLuminanceThreshold"),
        kSketchDarkLuminanceThreshold);
    glUniform1f(
        glGetUniformLocation(m_impl->program, "uLightLuminanceThreshold"),
        kSketchLightLuminanceThreshold);
    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f);
    glVertex2f(-1.0f, -1.0f);
    glTexCoord2f(1.0f, 0.0f);
    glVertex2f(1.0f, -1.0f);
    glTexCoord2f(1.0f, 1.0f);
    glVertex2f(1.0f, 1.0f);
    glTexCoord2f(0.0f, 1.0f);
    glVertex2f(-1.0f, 1.0f);
    glEnd();
  }
  glBindFramebuffer(GL_FRAMEBUFFER,
                    static_cast<GLuint>(m_impl->destinationFramebuffer));
  glViewport(m_impl->viewport[0], m_impl->viewport[1], m_impl->viewport[2],
             m_impl->viewport[3]);
}
