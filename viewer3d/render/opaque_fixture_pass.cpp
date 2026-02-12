#include "render/opaque_fixture_pass.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#undef DrawText
#endif

#include <GL/glew.h>
#ifdef __APPLE__
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

#include "matrixutils.h"
#include "render/opaque_pass_utils.h"
#include "scenedatamanager.h"
#include "viewer3dcontroller.h"

void OpaqueFixturePass::Render(
    Viewer3DController &controller, const RenderFrameContext &context,
    const Viewer3DVisibleSet &visibleSet,
    const std::function<std::array<float, 3>(const std::string &, const std::string &)> &getTypeColor,
    const std::function<std::array<float, 3>(const std::string &)> &getLayerColor,
    const std::function<SymbolViewKind(Viewer2DView)> &resolveSymbolView) {
  const bool wireframe = context.wireframe;
  const Viewer2DRenderMode mode = context.mode;
  const bool skipCapture = context.skipCapture;
  const bool is2DViewer = context.is2DViewer;

  const auto &fixtures = SceneDataManager::Instance().GetFixtures();

  glShadeModel(GL_FLAT);
  const bool forceFixturesOnTop = wireframe;
  GLboolean depthEnabled = GL_FALSE;
  if (forceFixturesOnTop) {
    depthEnabled = glIsEnabled(GL_DEPTH_TEST);
    if (depthEnabled)
      glDisable(GL_DEPTH_TEST);
  }
  for (const auto &uuid : visibleSet.fixtureUuids) {
    auto fixtureIt = fixtures.find(uuid);
    if (fixtureIt == fixtures.end())
      continue;
    const auto &f = fixtureIt->second;
    glPushMatrix();

    std::string fixtureCaptureKey;
    if (controller.m_captureCanvas && !skipCapture) {
      fixtureCaptureKey = !f.typeName.empty()
                              ? f.typeName
                              : (!f.gdtfSpec.empty() ? f.gdtfSpec : "unknown");
      controller.m_captureCanvas->SetSourceKey(fixtureCaptureKey);
    }

    bool highlight = (!controller.m_highlightUuid.empty() &&
                      uuid == controller.m_highlightUuid);
    bool selected = (controller.m_selectedUuids.find(uuid) !=
                     controller.m_selectedUuids.end());

    float matrix[16];
    MatrixToArray(f.transform, matrix);
    controller.ApplyTransform(matrix, true);

    float cx = 0.0f, cy = 0.0f, cz = 0.0f;
    auto fbit = controller.m_fixtureBounds.find(uuid);
    if (fbit != controller.m_fixtureBounds.end()) {
      cx = (fbit->second.min[0] + fbit->second.max[0]) * 0.5f;
      cy = (fbit->second.min[1] + fbit->second.max[1]) * 0.5f;
      cz = (fbit->second.min[2] + fbit->second.max[2]) * 0.5f;
      cx -= f.transform.o[0] * RENDER_SCALE;
      cy -= f.transform.o[1] * RENDER_SCALE;
      cz -= f.transform.o[2] * RENDER_SCALE;
    }

    float r = 1.0f, g = 1.0f, b = 1.0f;
    if (wireframe) {
      if (mode == Viewer2DRenderMode::ByFixtureType) {
        auto c = getTypeColor(f.gdtfSpec, f.color);
        r = c[0];
        g = c[1];
        b = c[2];
      } else if (mode == Viewer2DRenderMode::ByLayer) {
        auto c = getLayerColor(f.layer);
        r = c[0];
        g = c[1];
        b = c[2];
      }
    }

    Matrix fixtureTransform = f.transform;
    fixtureTransform.o[0] *= RENDER_SCALE;
    fixtureTransform.o[1] *= RENDER_SCALE;
    fixtureTransform.o[2] *= RENDER_SCALE;

    auto applyFixtureCapture = [fixtureTransform](const std::array<float, 3> &p) {
      return TransformPoint(fixtureTransform, p);
    };

    std::string gdtfPath;
    auto gdtfPathIt = controller.m_resourceSyncState.resolvedGdtfSpecs.find(
        ResolveCacheKey(f.gdtfSpec));
    if (gdtfPathIt != controller.m_resourceSyncState.resolvedGdtfSpecs.end() &&
        gdtfPathIt->second.attempted)
      gdtfPath = gdtfPathIt->second.resolvedPath;
    auto itg = controller.m_resourceSyncState.loadedGdtf.find(gdtfPath);

    const bool useSymbolInstancing =
        (controller.m_captureUseSymbols &&
         (controller.m_captureView == Viewer2DView::Bottom ||
          controller.m_captureView == Viewer2DView::Top ||
          controller.m_captureView == Viewer2DView::Front ||
          controller.m_captureView == Viewer2DView::Side) &&
         !highlight && !selected);
    bool placedInstance = false;
    if (useSymbolInstancing && controller.m_captureCanvas && !skipCapture) {
      std::string modelKey = NormalizeModelKey(gdtfPath);
      if (modelKey.empty() && !f.gdtfSpec.empty())
        modelKey = NormalizeModelKey(f.gdtfSpec);
      if (modelKey.empty() && !f.typeName.empty())
        modelKey = f.typeName;
      if (modelKey.empty())
        modelKey = "unknown";

      if (!modelKey.empty()) {
        SymbolKey symbolKey;
        symbolKey.modelKey = modelKey;
        symbolKey.viewKind = resolveSymbolView(controller.m_captureView);
        symbolKey.styleVersion = 1;

        const auto &symbol =
            controller.m_bottomSymbolCache.GetOrCreate(symbolKey, [&](const SymbolKey &,
                                                         uint32_t symbolId) {
              SymbolDefinition definition{};
              definition.symbolId = symbolId;
              auto localCanvas =
                  CreateRecordingCanvas(definition.localCommands, false);
              CanvasTransform transform{};
              localCanvas->BeginFrame();
              localCanvas->SetTransform(transform);

              ICanvas2D *prevCanvas = controller.m_captureCanvas;
              Viewer2DView prevView = controller.m_captureView;
              bool prevCaptureOnly = controller.m_captureOnly;
              bool prevIncludeGrid = controller.m_captureIncludeGrid;
              controller.m_captureCanvas = localCanvas.get();
              controller.m_captureView = prevView;
              controller.m_captureOnly = true;
              controller.m_captureIncludeGrid = false;

              if (itg != controller.m_resourceSyncState.loadedGdtf.end()) {
                size_t partIndex = 0;
                for (const auto &obj : itg->second) {
                  controller.m_captureCanvas->SetSourceKey(
                      fixtureCaptureKey + "_part" + std::to_string(partIndex));
                  auto applyCapture = [objTransform = obj.transform](
                                          const std::array<float, 3> &p) {
                    return TransformPoint(objTransform, p);
                  };
                  float partR = r;
                  float partG = g;
                  float partB = b;
                  if (!is2DViewer && obj.isLens) {
                    partR = 1.0f;
                    partG = 0.78f;
                    partB = 0.35f;
                  }
                  controller.DrawMeshWithOutline(
                      obj.mesh, partR, partG, partB, RENDER_SCALE, false, false,
                      0.0f, 0.0f, 0.0f, wireframe, mode, applyCapture, false);
                  ++partIndex;
                }
              } else {
                controller.m_captureCanvas->SetSourceKey(fixtureCaptureKey);
                controller.DrawCubeWithOutline(0.2f, r, g, b, false, false, 0.0f,
                                               0.0f, 0.0f, wireframe, mode,
                                               [](const std::array<float, 3> &p) {
                                                 return p;
                                               });
              }

              localCanvas->EndFrame();
              definition.bounds = ComputeSymbolBounds(definition.localCommands);

              controller.m_captureCanvas = prevCanvas;
              controller.m_captureView = prevView;
              controller.m_captureOnly = prevCaptureOnly;
              controller.m_captureIncludeGrid = prevIncludeGrid;
              return definition;
            });

        Transform2D instanceTransform =
            BuildInstanceTransform2D(fixtureTransform, controller.m_captureView);
        controller.m_captureCanvas->PlaceSymbolInstance(symbol.symbolId,
                                                        instanceTransform);
        placedInstance = true;
      }
    }

    auto drawFixtureGeometry = [&]() {
      if (itg != controller.m_resourceSyncState.loadedGdtf.end()) {
        size_t partIndex = 0;
        for (const auto &obj : itg->second) {
          glPushMatrix();
          if (controller.m_captureCanvas && !skipCapture) {
            controller.m_captureCanvas->SetSourceKey(
                fixtureCaptureKey + "_part" + std::to_string(partIndex));
          }
          float m2[16];
          MatrixToArray(obj.transform, m2);
          controller.ApplyTransform(m2, false);
          auto applyCapture =
              [fixtureTransform, objTransform = obj.transform](
                  const std::array<float, 3> &p) {
                auto local = TransformPoint(objTransform, p);
                return TransformPoint(fixtureTransform, local);
              };
          float partR = r;
          float partG = g;
          float partB = b;
          if (!is2DViewer && obj.isLens) {
            partR = 1.0f;
            partG = 0.78f;
            partB = 0.35f;
          }
          const bool drawUnlit = !is2DViewer && obj.isLens;
          controller.DrawMeshWithOutline(obj.mesh, partR, partG, partB,
                                         RENDER_SCALE, highlight, selected, cx,
                                         cy, cz, wireframe, mode, applyCapture,
                                         drawUnlit);
          glPopMatrix();
          ++partIndex;
        }
      } else {
        controller.DrawCubeWithOutline(0.2f, r, g, b, highlight, selected, cx,
                                       cy, cz, wireframe, mode,
                                       applyFixtureCapture);
      }
    };

    if (placedInstance) {
      ICanvas2D *prevCanvas = controller.m_captureCanvas;
      bool prevCaptureOnly = controller.m_captureOnly;
      controller.m_captureCanvas = nullptr;
      controller.m_captureOnly = false;
      drawFixtureGeometry();
      controller.m_captureCanvas = prevCanvas;
      controller.m_captureOnly = prevCaptureOnly;
    } else {
      drawFixtureGeometry();
    }

    glPopMatrix();

    if (controller.m_captureCanvas && !skipCapture)
      controller.m_captureCanvas->SetSourceKey("unknown");
  }
  if (forceFixturesOnTop && depthEnabled)
    glEnable(GL_DEPTH_TEST);
}
