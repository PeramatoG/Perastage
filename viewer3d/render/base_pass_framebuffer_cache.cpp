#include "base_pass_framebuffer_cache.h"

#include <GL/glew.h>
#ifdef __APPLE__
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

BasePassFramebufferCache::~BasePassFramebufferCache() {
  if (m_depthRenderbuffer != 0)
    glDeleteRenderbuffers(1, &m_depthRenderbuffer);
  if (m_colorTexture != 0)
    glDeleteTextures(1, &m_colorTexture);
  if (m_fbo != 0)
    glDeleteFramebuffers(1, &m_fbo);
}

void BasePassFramebufferCache::Invalidate() { m_hasSnapshot = false; }

void BasePassFramebufferCache::AbandonResources() {
  m_fbo = 0;
  m_colorTexture = 0;
  m_depthRenderbuffer = 0;
  m_width = 0;
  m_height = 0;
  m_hasSnapshot = false;
}

void BasePassFramebufferCache::EnsureFramebufferSize(int width, int height) {
  if (width <= 0 || height <= 0)
    return;
  if (m_fbo != 0 && m_width == width && m_height == height)
    return;

  if (m_depthRenderbuffer != 0)
    glDeleteRenderbuffers(1, &m_depthRenderbuffer);
  if (m_colorTexture != 0)
    glDeleteTextures(1, &m_colorTexture);
  if (m_fbo == 0)
    glGenFramebuffers(1, &m_fbo);

  glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);

  glGenTextures(1, &m_colorTexture);
  glBindTexture(GL_TEXTURE_2D, m_colorTexture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, nullptr);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                         m_colorTexture, 0);

  glGenRenderbuffers(1, &m_depthRenderbuffer);
  glBindRenderbuffer(GL_RENDERBUFFER, m_depthRenderbuffer);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                            GL_RENDERBUFFER, m_depthRenderbuffer);

  m_width = width;
  m_height = height;
  m_hasSnapshot = false;
}

bool BasePassFramebufferCache::RestoreToDefaultFramebuffer(
    int width, int height, size_t cameraFingerprint,
    const std::unordered_set<std::string> &hiddenLayers,
    size_t sceneVersion) const {
  if (!m_hasSnapshot || m_fbo == 0 || width <= 0 || height <= 0)
    return false;

  const bool viewportChanged = m_width != width || m_height != height;
  const bool cameraChanged = m_lastCameraFingerprint != cameraFingerprint;
  const bool layersChanged = m_lastHiddenLayers != hiddenLayers;
  const bool sceneChanged = m_lastSceneVersion != sceneVersion;
  if (viewportChanged || cameraChanged || layersChanged || sceneChanged)
    return false;

  glBindFramebuffer(GL_READ_FRAMEBUFFER, m_fbo);
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
  glBlitFramebuffer(0, 0, width, height, 0, 0, width, height,
                    GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT, GL_NEAREST);
  glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
  return true;
}

void BasePassFramebufferCache::CaptureFromDefaultFramebuffer(
    int width, int height, size_t cameraFingerprint,
    const std::unordered_set<std::string> &hiddenLayers,
    size_t sceneVersion) {
  EnsureFramebufferSize(width, height);
  if (m_fbo == 0 || width <= 0 || height <= 0)
    return;

  glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_fbo);
  glBlitFramebuffer(0, 0, width, height, 0, 0, width, height,
                    GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT, GL_NEAREST);
  glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

  m_lastCameraFingerprint = cameraFingerprint;
  m_lastHiddenLayers = hiddenLayers;
  m_lastSceneVersion = sceneVersion;
  m_hasSnapshot = true;
}
