#include "id_pick_pass.h"

#include "picking_coordinate_utils.h"
#include "render/opengl_state_guard.h"

#include <GL/glew.h>
#ifdef __APPLE__
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

#include "opaque_fixture_pass.h"
#include "opaque_object_pass.h"
#include "opaque_truss_pass.h"
#include "scenedatamanager.h"
#include "viewer3dcontroller.h"

#include <algorithm>
#include <wx/log.h>

namespace {
bool MatricesEqual(const std::array<double, 16> &a,
                   const std::array<double, 16> &b) {
  return std::equal(a.begin(), a.end(), b.begin());
}
} // namespace

IdPickPass::IdPickPass(Viewer3DController &controller) : m_controller(controller) {}

IdPickPass::~IdPickPass() {
  if (m_depthRenderbuffer != 0)
    glDeleteRenderbuffers(1, &m_depthRenderbuffer);
  if (m_colorTexture != 0)
    glDeleteTextures(1, &m_colorTexture);
  if (m_fbo != 0)
    glDeleteFramebuffers(1, &m_fbo);
}

uint32_t IdPickPass::GetOrCreatePickId(const std::string &uuid) {
  auto it = m_uuidToPickId.find(uuid);
  if (it != m_uuidToPickId.end())
    return it->second;
  const uint32_t pickId = m_nextPickId++;
  m_uuidToPickId[uuid] = pickId;
  m_pickIdToUuid[pickId] = uuid;
  return pickId;
}

std::array<float, 3> IdPickPass::GetPickColor(const std::string &uuid) {
  const uint32_t pickId = GetOrCreatePickId(uuid);
  const float r = static_cast<float>((pickId >> 16) & 0xFF) / 255.0f;
  const float g = static_cast<float>((pickId >> 8) & 0xFF) / 255.0f;
  const float b = static_cast<float>(pickId & 0xFF) / 255.0f;
  return {r, g, b};
}

void IdPickPass::EnsureFramebufferSize(int width, int height) {
  if (width <= 0 || height <= 0) {
    m_framebufferUsable = false;
    return;
  }
  if (m_fbo != 0 && m_width == width && m_height == height)
    return;

  m_framebufferUsable = false;

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
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, width, height, 0, GL_RGB,
               GL_UNSIGNED_BYTE, nullptr);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                         m_colorTexture, 0);

  glGenRenderbuffers(1, &m_depthRenderbuffer);
  glBindRenderbuffer(GL_RENDERBUFFER, m_depthRenderbuffer);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                            GL_RENDERBUFFER, m_depthRenderbuffer);

  const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
  if (status != GL_FRAMEBUFFER_COMPLETE) {
    if (!m_loggedFramebufferFailure) {
      wxLogDebug(
          "Viewer3D ID picking framebuffer is incomplete (status=0x%04x). Falling back to ray picking for this session.",
          static_cast<unsigned int>(status));
      m_loggedFramebufferFailure = true;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return;
  }

  m_width = width;
  m_height = height;
  m_framebufferUsable = true;
  m_dirty = true;
}

void IdPickPass::RebuildIfNeeded(
    int width, int height,
    const std::unordered_set<std::string> &hiddenLayers) {
  viewer3d::render::OpenGLStateGuard stateGuard;
  EnsureFramebufferSize(width, height);
  if (m_fbo == 0 || !m_framebufferUsable || width <= 0 || height <= 0)
    return;

  int viewport[4] = {0, 0, 0, 0};
  double model[16] = {0.0};
  double projection[16] = {0.0};
  glGetIntegerv(GL_VIEWPORT, viewport);
  glGetDoublev(GL_MODELVIEW_MATRIX, model);
  glGetDoublev(GL_PROJECTION_MATRIX, projection);

  std::array<int, 4> currentViewport = {viewport[0], viewport[1], viewport[2],
                                        viewport[3]};
  std::array<double, 16> currentModel = {};
  std::array<double, 16> currentProjection = {};
  std::copy(std::begin(model), std::end(model), currentModel.begin());
  std::copy(std::begin(projection), std::end(projection),
            currentProjection.begin());

  const bool cameraChanged = currentViewport != m_lastViewport ||
                             !MatricesEqual(currentModel, m_lastModel) ||
                             !MatricesEqual(currentProjection, m_lastProjection);
  const bool layersChanged = hiddenLayers != m_lastHiddenLayers;
  const bool sceneChanged = m_lastSceneVersion != m_controller.GetSceneVersion();
  if (!m_dirty && !cameraChanged && !layersChanged && !sceneChanged)
    return;

  glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
  glViewport(0, 0, width, height);
  glDisable(GL_BLEND);
  glEnable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
  glDisable(GL_LIGHTING);
  glDisable(GL_TEXTURE_2D);
  glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  glLoadMatrixd(projection);
  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glLoadMatrixd(model);

  Viewer3DViewFrustumSnapshot frustum{};
  std::copy(std::begin(viewport), std::end(viewport), std::begin(frustum.viewport));
  std::copy(std::begin(model), std::end(model), std::begin(frustum.model));
  std::copy(std::begin(projection), std::end(projection),
            std::begin(frustum.projection));

  RenderFrameContext context;
  context.hiddenLayers = hiddenLayers;
  context.useFrustumCulling = true;
  context.idOnlyPass = true;
  context.skipCapture = true;
  const auto &visibleSet =
      m_controller.GetVisibleSet(frustum, hiddenLayers, true, 0.0f);

  auto getLayerColor = [](const std::string &) {
    return std::array<float, 3>{1.0f, 1.0f, 1.0f};
  };
  auto getTypeColor = [](const std::string &, const std::string &) {
    return std::array<float, 3>{1.0f, 1.0f, 1.0f};
  };
  auto resolveSymbolView = [](Viewer2DView) { return SymbolViewKind::Top; };
  auto getPickColor = [this](const std::string &uuid) {
    return GetPickColor(uuid);
  };

  OpaqueObjectPass::Render(m_controller, context, visibleSet, getLayerColor,
                           resolveSymbolView, getPickColor);
  OpaqueTrussPass::Render(m_controller, context, visibleSet, getLayerColor,
                          resolveSymbolView, getPickColor);
  OpaqueFixturePass::Render(m_controller, context, visibleSet, getTypeColor,
                            getLayerColor, resolveSymbolView, getPickColor);

  glPopMatrix();
  glMatrixMode(GL_PROJECTION);
  glPopMatrix();
  glMatrixMode(GL_MODELVIEW);

  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);

  m_lastViewport = currentViewport;
  m_lastModel = currentModel;
  m_lastProjection = currentProjection;
  m_lastHiddenLayers = hiddenLayers;
  m_lastSceneVersion = m_controller.GetSceneVersion();
  m_dirty = false;
}

// Reads the UUID encoded in the ID-picking framebuffer at the requested pixel.
IdPickPass::ReadResult IdPickPass::ReadUuidAtDetailed(
    int mouseX, int mouseY, int width, int height,
    const std::unordered_set<std::string> &hiddenLayers, std::string &outUuid) {
  outUuid.clear();
  RebuildIfNeeded(width, height, hiddenLayers);
  if (m_fbo == 0 || !m_framebufferUsable)
    return ReadResult::Unavailable;

  int framebufferX = 0;
  int framebufferY = 0;
  if (!TryConvertMouseToFramebufferPoint(mouseX, mouseY, width, height,
                                         framebufferX, framebufferY)) {
    if (!m_loggedInvalidReadCoordinates) {
      wxLogDebug(
          "Viewer3D ID picking skipped an out-of-range read at mouse=(%d,%d), framebuffer=(%d,%d).",
          mouseX, mouseY, width, height);
      m_loggedInvalidReadCoordinates = true;
    }
    return ReadResult::Miss;
  }

  viewer3d::render::OpenGLStateGuard stateGuard;
  unsigned char pixel[3] = {0, 0, 0};
  viewer3d::render::ClearOpenGLErrors();
  glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
  glReadBuffer(GL_COLOR_ATTACHMENT0);
  glReadPixels(framebufferX, framebufferY, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, pixel);
  const GLenum readError = viewer3d::render::DrainOpenGLErrors();
  if (readError != GL_NO_ERROR) {
    wxLogDebug(
        "Picking: glReadPixels ID read failed error=0x%04x mouse=(%d,%d) framebuffer=(%d,%d) size=(%d,%d) vendor=%s renderer=%s.",
        static_cast<unsigned int>(readError), mouseX, mouseY, framebufferX,
        framebufferY, width, height, viewer3d::render::SafeGlString(GL_VENDOR),
        viewer3d::render::SafeGlString(GL_RENDERER));
    return ReadResult::Unavailable;
  }

  const uint32_t pickId = (static_cast<uint32_t>(pixel[0]) << 16) |
                          (static_cast<uint32_t>(pixel[1]) << 8) |
                          static_cast<uint32_t>(pixel[2]);
  if (pickId == 0)
    return ReadResult::Miss;

  auto it = m_pickIdToUuid.find(pickId);
  if (it == m_pickIdToUuid.end())
    return ReadResult::Miss;

  outUuid = it->second;
  return ReadResult::Hit;
}

// Reads whether the ID-picking framebuffer contains a UUID at the requested pixel.
bool IdPickPass::ReadUuidAt(int mouseX, int mouseY, int width, int height,
                            const std::unordered_set<std::string> &hiddenLayers,
                            std::string &outUuid) {
  return ReadUuidAtDetailed(mouseX, mouseY, width, height, hiddenLayers,
                            outUuid) == ReadResult::Hit;
}
