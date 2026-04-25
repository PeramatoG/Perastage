#include "selection_overlay_pass.h"

#include <GL/glew.h>
#ifdef __APPLE__
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

#include <algorithm>
#include <array>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

#include "configmanager.h"
#include "logger.h"
#include "opaque_fixture_pass.h"
#include "opaque_object_pass.h"
#include "opaque_truss_pass.h"
#include "scenedatamanager.h"
#include "viewer3dcontroller.h"

namespace {
std::array<float, 3> MakeDeterministicColor(std::string_view key) {
  const uint64_t hash = std::hash<std::string_view>{}(key);
  const uint8_t r = static_cast<uint8_t>(64u + (hash & 0x7Fu));
  const uint8_t g = static_cast<uint8_t>(64u + ((hash >> 8) & 0x7Fu));
  const uint8_t b = static_cast<uint8_t>(64u + ((hash >> 16) & 0x7Fu));
  return {static_cast<float>(r) / 255.0f, static_cast<float>(g) / 255.0f,
          static_cast<float>(b) / 255.0f};
}

bool HexToRGB(const std::string &hex, float &r, float &g, float &b) {
  auto hexNibble = [](char ch) -> int {
    if (ch >= '0' && ch <= '9')
      return ch - '0';
    if (ch >= 'a' && ch <= 'f')
      return 10 + (ch - 'a');
    if (ch >= 'A' && ch <= 'F')
      return 10 + (ch - 'A');
    return -1;
  };
  auto hexPairToInt = [&](char hi, char lo) -> int {
    const int hiNibble = hexNibble(hi);
    const int loNibble = hexNibble(lo);
    if (hiNibble < 0 || loNibble < 0)
      return -1;
    return (hiNibble << 4) | loNibble;
  };
  if (hex.size() != 7 || hex[0] != '#')
    return false;
  const int ri = hexPairToInt(hex[1], hex[2]);
  const int gi = hexPairToInt(hex[3], hex[4]);
  const int bi = hexPairToInt(hex[5], hex[6]);
  if (ri < 0 || gi < 0 || bi < 0)
    return false;
  r = static_cast<float>(ri) / 255.0f;
  g = static_cast<float>(gi) / 255.0f;
  b = static_cast<float>(bi) / 255.0f;
  return true;
}

bool ContainsUuid(const std::vector<std::string> &uuids,
                  const std::string &uuid) {
  return std::find(uuids.begin(), uuids.end(), uuid) != uuids.end();
}

void AppendUuidIfRenderable(const std::string &uuid,
                            const std::unordered_map<std::string, Fixture> &fixtures,
                            const std::unordered_map<std::string, Truss> &trusses,
                            const std::unordered_map<std::string, SceneObject> &objects,
                            Viewer3DVisibleSet &overlaySet) {
  if (uuid.empty())
    return;

  if (fixtures.find(uuid) != fixtures.end() &&
      !ContainsUuid(overlaySet.fixtureUuids, uuid)) {
    overlaySet.fixtureUuids.push_back(uuid);
    return;
  }
  if (trusses.find(uuid) != trusses.end() &&
      !ContainsUuid(overlaySet.trussUuids, uuid)) {
    overlaySet.trussUuids.push_back(uuid);
    return;
  }
  if (objects.find(uuid) != objects.end() &&
      !ContainsUuid(overlaySet.objectUuids, uuid)) {
    overlaySet.objectUuids.push_back(uuid);
    return;
  }
}

struct ScreenSpaceOutlineResources {
  GLuint fbo = 0;
  GLuint colorTexture = 0;
  GLuint depthBuffer = 0;
  GLuint program = 0;
  GLint texelSizeUniform = -1;
  GLint outlineColorUniform = -1;
  GLint fillAlphaUniform = -1;
  GLint outlineAlphaUniform = -1;
  int width = 0;
  int height = 0;

  ~ScreenSpaceOutlineResources() {
    if (program != 0)
      glDeleteProgram(program);
    if (depthBuffer != 0)
      glDeleteRenderbuffers(1, &depthBuffer);
    if (colorTexture != 0)
      glDeleteTextures(1, &colorTexture);
    if (fbo != 0)
      glDeleteFramebuffers(1, &fbo);
  }

  bool EnsureFramebuffer(int targetWidth, int targetHeight) {
    if (targetWidth <= 0 || targetHeight <= 0)
      return false;

    if (fbo == 0)
      glGenFramebuffers(1, &fbo);
    if (colorTexture == 0)
      glGenTextures(1, &colorTexture);
    if (depthBuffer == 0)
      glGenRenderbuffers(1, &depthBuffer);

    if (width == targetWidth && height == targetHeight)
      return true;

    width = targetWidth;
    height = targetHeight;

    glBindTexture(GL_TEXTURE_2D, colorTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindRenderbuffer(GL_RENDERBUFFER, depthBuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);

    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           colorTexture, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                              GL_RENDERBUFFER, depthBuffer);

    const bool complete =
        glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    return complete;
  }

  static GLuint CompileShader(GLenum shaderType, const char *source) {
    const GLuint shader = glCreateShader(shaderType);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint compileOk = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compileOk);
    if (compileOk == GL_TRUE)
      return shader;

    GLint infoLogLength = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLogLength);
    if (infoLogLength > 1) {
      std::string infoLog(static_cast<size_t>(infoLogLength), '\0');
      glGetShaderInfoLog(shader, infoLogLength, nullptr, infoLog.data());
      Logger::Instance().Log(
          Logger::Level::Warn,
          std::string("Selection outline shader compilation failed: ") + infoLog);
    }

    glDeleteShader(shader);
    return 0;
  }

  bool EnsureProgram() {
    if (program != 0)
      return true;

    static const char *kVertexShader =
        "#version 120\n"
        "varying vec2 vTexCoord;\n"
        "void main() {\n"
        "  vTexCoord = gl_MultiTexCoord0.xy;\n"
        "  gl_Position = gl_Vertex;\n"
        "}\n";

    static const char *kFragmentShader =
        "#version 120\n"
        "uniform sampler2D uMask;\n"
        "uniform vec2 uTexelSize;\n"
        "uniform vec3 uOutlineColor;\n"
        "uniform float uFillAlpha;\n"
        "uniform float uOutlineAlpha;\n"
        "varying vec2 vTexCoord;\n"
        "float sampleMask(vec2 uv) {\n"
        "  return texture2D(uMask, uv).r;\n"
        "}\n"
        "void main() {\n"
        "  float center = sampleMask(vTexCoord);\n"
        "  float edge = 0.0;\n"
        "  edge = max(edge, sampleMask(vTexCoord + vec2(uTexelSize.x, 0.0)));\n"
        "  edge = max(edge, sampleMask(vTexCoord - vec2(uTexelSize.x, 0.0)));\n"
        "  edge = max(edge, sampleMask(vTexCoord + vec2(0.0, uTexelSize.y)));\n"
        "  edge = max(edge, sampleMask(vTexCoord - vec2(0.0, uTexelSize.y)));\n"
        "  edge = max(edge, sampleMask(vTexCoord + uTexelSize));\n"
        "  edge = max(edge, sampleMask(vTexCoord - uTexelSize));\n"
        "  edge = max(edge, sampleMask(vTexCoord + vec2(uTexelSize.x, -uTexelSize.y)));\n"
        "  edge = max(edge, sampleMask(vTexCoord + vec2(-uTexelSize.x, uTexelSize.y)));\n"
        "  float outline = max(0.0, edge - center);\n"
        "  float alpha = max(center * uFillAlpha, outline * uOutlineAlpha);\n"
        "  if (alpha <= 0.001)\n"
        "    discard;\n"
        "  gl_FragColor = vec4(uOutlineColor, alpha);\n"
        "}\n";

    const GLuint vertex = CompileShader(GL_VERTEX_SHADER, kVertexShader);
    const GLuint fragment = CompileShader(GL_FRAGMENT_SHADER, kFragmentShader);
    if (vertex == 0 || fragment == 0) {
      if (vertex != 0)
        glDeleteShader(vertex);
      if (fragment != 0)
        glDeleteShader(fragment);
      return false;
    }

    program = glCreateProgram();
    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    glLinkProgram(program);

    glDeleteShader(vertex);
    glDeleteShader(fragment);

    GLint linkOk = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &linkOk);
    if (linkOk != GL_TRUE) {
      GLint infoLogLength = 0;
      glGetProgramiv(program, GL_INFO_LOG_LENGTH, &infoLogLength);
      if (infoLogLength > 1) {
        std::string infoLog(static_cast<size_t>(infoLogLength), '\0');
        glGetProgramInfoLog(program, infoLogLength, nullptr, infoLog.data());
        Logger::Instance().Log(
            Logger::Level::Warn,
            std::string("Selection outline shader link failed: ") + infoLog);
      }
      glDeleteProgram(program);
      program = 0;
      return false;
    }

    texelSizeUniform = glGetUniformLocation(program, "uTexelSize");
    outlineColorUniform = glGetUniformLocation(program, "uOutlineColor");
    fillAlphaUniform = glGetUniformLocation(program, "uFillAlpha");
    outlineAlphaUniform = glGetUniformLocation(program, "uOutlineAlpha");
    return texelSizeUniform >= 0 && outlineColorUniform >= 0 &&
           fillAlphaUniform >= 0 && outlineAlphaUniform >= 0;
  }
};

ScreenSpaceOutlineResources &OutlineResources() {
  static ScreenSpaceOutlineResources resources;
  return resources;
}

struct OverlaySelectionSets {
  Viewer3DVisibleSet highlightSet;
  Viewer3DVisibleSet selectedSet;

  bool Empty() const {
    return highlightSet.Empty() && selectedSet.Empty();
  }
};

OverlaySelectionSets BuildOverlaySets(Viewer3DController &controller,
                                      const Viewer3DVisibleSet &visibleSet) {
  const auto &fixtures = SceneDataManager::Instance().GetFixtures();
  const auto &trusses = SceneDataManager::Instance().GetTrusses();
  const auto &objects = SceneDataManager::Instance().GetSceneObjects();

  const std::string &highlightUuid = controller.GetOverlayHighlightUuid();
  const auto &selectedUuids = controller.GetOverlaySelectedUuids();

  OverlaySelectionSets sets;
  const auto appendFromVisibility =
      [&](const std::vector<std::string> &sourceUuids,
          std::vector<std::string> &highlightTarget,
          std::vector<std::string> &selectedTarget) {
        for (const auto &uuid : sourceUuids) {
          if (uuid == highlightUuid)
            highlightTarget.push_back(uuid);
          else if (selectedUuids.find(uuid) != selectedUuids.end())
            selectedTarget.push_back(uuid);
        }
      };

  appendFromVisibility(visibleSet.fixtureUuids, sets.highlightSet.fixtureUuids,
                       sets.selectedSet.fixtureUuids);
  appendFromVisibility(visibleSet.trussUuids, sets.highlightSet.trussUuids,
                       sets.selectedSet.trussUuids);
  appendFromVisibility(visibleSet.objectUuids, sets.highlightSet.objectUuids,
                       sets.selectedSet.objectUuids);

  AppendUuidIfRenderable(highlightUuid, fixtures, trusses, objects,
                         sets.highlightSet);
  for (const auto &uuid : selectedUuids) {
    if (uuid != highlightUuid)
      AppendUuidIfRenderable(uuid, fixtures, trusses, objects, sets.selectedSet);
  }

  return sets;
}

void RenderSelectedGeometry(Viewer3DController &controller,
                            const RenderFrameContext &context,
                            const Viewer3DVisibleSet &overlaySet) {
  auto getTypeColor = [&](const std::string &key, const std::string &hex) {
    std::array<float, 3> c;
    if (!hex.empty() && HexToRGB(hex, c[0], c[1], c[2]))
      return c;
    return MakeDeterministicColor("type:" + key);
  };
  auto getLayerColor = [&](const std::string &key) {
    std::array<float, 3> c;
    auto opt = ConfigManager::Get().GetLayerColor(key);
    if (opt && HexToRGB(*opt, c[0], c[1], c[2]))
      return c;
    return MakeDeterministicColor("layer:" + key);
  };
  auto resolveSymbolView = [](Viewer2DView viewKind) {
    switch (viewKind) {
    case Viewer2DView::Top:
      return SymbolViewKind::Top;
    case Viewer2DView::Front:
      return SymbolViewKind::Front;
    case Viewer2DView::Side:
      return SymbolViewKind::Left;
    case Viewer2DView::Bottom:
    default:
      return SymbolViewKind::Bottom;
    }
  };
  auto getPickColor = [](const std::string &) {
    return std::array<float, 3>{1.0f, 1.0f, 1.0f};
  };

  OpaqueObjectPass::Render(controller, context, overlaySet, getLayerColor,
                           resolveSymbolView, getPickColor);
  OpaqueTrussPass::Render(controller, context, overlaySet, getLayerColor,
                          resolveSymbolView, getPickColor);
  OpaqueFixturePass::Render(controller, context, overlaySet, getTypeColor,
                            getLayerColor, resolveSymbolView, getPickColor);
}

void RenderLegacyOverlay(Viewer3DController &controller,
                         const RenderFrameContext &context,
                         const Viewer3DVisibleSet &overlaySet) {
  if (overlaySet.Empty())
    return;
  RenderFrameContext overlayContext = context;
  overlayContext.skipCapture = true;
  overlayContext.selectionOverlayPass = true;
  GLint previousDepthFunc = GL_LESS;
  glGetIntegerv(GL_DEPTH_FUNC, &previousDepthFunc);
  glDepthFunc(GL_LEQUAL);
  RenderSelectedGeometry(controller, overlayContext, overlaySet);
  glDepthFunc(static_cast<GLenum>(previousDepthFunc));
}

bool UseScreenSpaceOverlay(const RenderFrameContext &context) {
  if (context.is2DViewer)
    return false;
  return ConfigManager::Get().GetFloat("viewer3d_selection_outline_screenspace") >=
         0.5f;
}

void RenderMask(Viewer3DController &controller, const RenderFrameContext &context,
                const Viewer3DVisibleSet &overlaySet, int width, int height) {
  if (overlaySet.Empty())
    return;

  glViewport(0, 0, width, height);
  glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
  glClear(GL_COLOR_BUFFER_BIT);

  GLint previousDepthFunc = GL_LESS;
  glGetIntegerv(GL_DEPTH_FUNC, &previousDepthFunc);

  RenderFrameContext maskContext = context;
  maskContext.skipCapture = true;
  maskContext.selectionOverlayPass = false;
  maskContext.selectionMaskPass = true;
  maskContext.wireframe = false;

  glDepthFunc(GL_LEQUAL);
  RenderSelectedGeometry(controller, maskContext, overlaySet);
  glDepthFunc(static_cast<GLenum>(previousDepthFunc));
}

void CompositeMaskToCurrentFramebuffer(const ScreenSpaceOutlineResources &resources,
                                       int width, int height, float colorR,
                                       float colorG, float colorB,
                                       float fillAlpha, float outlineAlpha) {
  glUseProgram(resources.program);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, resources.colorTexture);
  glUniform1i(glGetUniformLocation(resources.program, "uMask"), 0);
  glUniform2f(resources.texelSizeUniform, 1.0f / static_cast<float>(width),
              1.0f / static_cast<float>(height));
  glUniform3f(resources.outlineColorUniform, colorR, colorG, colorB);
  glUniform1f(resources.fillAlphaUniform, fillAlpha);
  glUniform1f(resources.outlineAlphaUniform, outlineAlpha);

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

void RenderScreenSpaceOverlay(Viewer3DController &controller,
                              const RenderFrameContext &context,
                              const OverlaySelectionSets &overlaySets) {
  static bool loggedFramebufferFallback = false;
  static bool loggedShaderFallback = false;

  GLint viewport[4] = {0, 0, 0, 0};
  glGetIntegerv(GL_VIEWPORT, viewport);
  const int width = viewport[2];
  const int height = viewport[3];
  if (width <= 0 || height <= 0) {
    RenderLegacyOverlay(controller, context, overlaySets.selectedSet);
    RenderLegacyOverlay(controller, context, overlaySets.highlightSet);
    return;
  }

  auto &resources = OutlineResources();
  if (!resources.EnsureFramebuffer(width, height)) {
    if (!loggedFramebufferFallback) {
      Logger::Instance().Log(
          Logger::Level::Warn,
          "Selection outline screen-space disabled: failed to initialize mask framebuffer; using legacy fallback");
      loggedFramebufferFallback = true;
    }
    RenderLegacyOverlay(controller, context, overlaySets.selectedSet);
    RenderLegacyOverlay(controller, context, overlaySets.highlightSet);
    return;
  }
  if (!resources.EnsureProgram()) {
    if (!loggedShaderFallback) {
      Logger::Instance().Log(
          Logger::Level::Warn,
          "Selection outline screen-space disabled: failed to initialize shader program; using legacy fallback");
      loggedShaderFallback = true;
    }
    RenderLegacyOverlay(controller, context, overlaySets.selectedSet);
    RenderLegacyOverlay(controller, context, overlaySets.highlightSet);
    return;
  }

  GLint drawFramebuffer = 0;
  glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFramebuffer);

  glBindFramebuffer(GL_READ_FRAMEBUFFER, drawFramebuffer);
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, resources.fbo);
  glBlitFramebuffer(0, 0, width, height, 0, 0, width, height,
                    GL_DEPTH_BUFFER_BIT, GL_NEAREST);

  glBindFramebuffer(GL_FRAMEBUFFER, drawFramebuffer);
  glViewport(viewport[0], viewport[1], width, height);

  const GLboolean depthWasEnabled = glIsEnabled(GL_DEPTH_TEST);
  const GLboolean blendWasEnabled = glIsEnabled(GL_BLEND);
  const GLboolean cullWasEnabled = glIsEnabled(GL_CULL_FACE);
  const GLboolean lightingWasEnabled = glIsEnabled(GL_LIGHTING);
  const GLboolean textureWasEnabled = glIsEnabled(GL_TEXTURE_2D);
  GLint activeProgram = 0;
  glGetIntegerv(GL_CURRENT_PROGRAM, &activeProgram);

  glDisable(GL_DEPTH_TEST);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDisable(GL_LIGHTING);
  glDisable(GL_CULL_FACE);
  if (!textureWasEnabled)
    glEnable(GL_TEXTURE_2D);

  glBindFramebuffer(GL_FRAMEBUFFER, resources.fbo);
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  RenderMask(controller, context, overlaySets.selectedSet, width, height);

  glBindFramebuffer(GL_FRAMEBUFFER, drawFramebuffer);
  CompositeMaskToCurrentFramebuffer(resources, width, height, 0.0f, 1.0f, 1.0f,
                                    0.20f, 1.0f);

  glBindFramebuffer(GL_FRAMEBUFFER, resources.fbo);
  RenderMask(controller, context, overlaySets.highlightSet, width, height);

  glBindFramebuffer(GL_FRAMEBUFFER, drawFramebuffer);
  CompositeMaskToCurrentFramebuffer(resources, width, height, 0.0f, 1.0f, 0.0f,
                                    0.32f, 1.0f);

  glUseProgram(activeProgram);
  glBindTexture(GL_TEXTURE_2D, 0);

  if (!textureWasEnabled)
    glDisable(GL_TEXTURE_2D);
  if (depthWasEnabled)
    glEnable(GL_DEPTH_TEST);
  else
    glDisable(GL_DEPTH_TEST);
  if (blendWasEnabled)
    glEnable(GL_BLEND);
  else
    glDisable(GL_BLEND);
  if (cullWasEnabled)
    glEnable(GL_CULL_FACE);
  else
    glDisable(GL_CULL_FACE);
  if (lightingWasEnabled)
    glEnable(GL_LIGHTING);
  else
    glDisable(GL_LIGHTING);
}
} // namespace

void SelectionOverlayPass::Render(Viewer3DController &controller,
                                  const RenderFrameContext &context,
                                  const Viewer3DVisibleSet &visibleSet) {
  OverlaySelectionSets overlaySets = BuildOverlaySets(controller, visibleSet);
  if (overlaySets.Empty())
    return;

  if (UseScreenSpaceOverlay(context)) {
    RenderScreenSpaceOverlay(controller, context, overlaySets);
    return;
  }

  RenderLegacyOverlay(controller, context, overlaySets.selectedSet);
  RenderLegacyOverlay(controller, context, overlaySets.highlightSet);
}
