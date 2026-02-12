#include "render/opaque_truss_pass.h"

#ifdef __APPLE__
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

#include "matrixutils.h"
#include "projectutils.h"
#include "scenedatamanager.h"
#include "viewer3dcontroller.h"

#include <sstream>

void OpaqueTrussPass::Render(
    Viewer3DController &controller, const RenderFrameContext &context,
    const Viewer3DVisibleSet &visibleSet,
    const std::function<std::array<float, 3>(const std::string &)> &getLayerColor,
    const std::function<SymbolViewKind(Viewer2DView)> &resolveSymbolView) {
  const bool wireframe = context.wireframe;
  const Viewer2DRenderMode mode = context.mode;
  const bool skipCapture = context.skipCapture;

  const auto &trusses = SceneDataManager::Instance().GetTrusses();

  glShadeModel(GL_SMOOTH);
  for (const auto &uuid : visibleSet.trussUuids) {
    auto trussIt = trusses.find(uuid);
    if (trussIt == trusses.end())
      continue;
    const auto &t = trussIt->second;
    glPushMatrix();

    std::string trussCaptureKey;
    if (controller.m_captureCanvas && !skipCapture) {
      trussCaptureKey = t.model.empty() ? t.name : t.model;
      if (trussCaptureKey.empty())
        trussCaptureKey = "truss";
      controller.m_captureCanvas->SetSourceKey(trussCaptureKey);
    }

    bool highlight = (!controller.m_highlightUuid.empty() &&
                      uuid == controller.m_highlightUuid);
    bool selected = (controller.m_selectedUuids.find(uuid) !=
                     controller.m_selectedUuids.end());

    float matrix[16];
    MatrixToArray(t.transform, matrix);
    controller.ApplyTransform(matrix, true);

    float cx = 0.0f, cy = 0.0f, cz = 0.0f;
    auto tbit = controller.m_trussBounds.find(uuid);
    if (tbit != controller.m_trussBounds.end()) {
      cx = (tbit->second.min[0] + tbit->second.max[0]) * 0.5f;
      cy = (tbit->second.min[1] + tbit->second.max[1]) * 0.5f;
      cz = (tbit->second.min[2] + tbit->second.max[2]) * 0.5f;
      cx -= t.transform.o[0] * RENDER_SCALE;
      cy -= t.transform.o[1] * RENDER_SCALE;
      cz -= t.transform.o[2] * RENDER_SCALE;
    }

    float r = 1.0f, g = 1.0f, b = 1.0f;
    if (wireframe && mode == Viewer2DRenderMode::ByLayer) {
      auto c = getLayerColor(t.layer);
      r = c[0];
      g = c[1];
      b = c[2];
    }

    Matrix captureTransform = t.transform;
    captureTransform.o[0] *= RENDER_SCALE;
    captureTransform.o[1] *= RENDER_SCALE;
    captureTransform.o[2] *= RENDER_SCALE;
    auto applyCapture = [captureTransform](const std::array<float, 3> &p) {
      return TransformPoint(captureTransform, p);
    };

    const Mesh *trussMesh = nullptr;
    std::string trussPath;
    if (!t.symbolFile.empty()) {
      auto trussPathIt = controller.m_resourceSyncState.resolvedModelRefs.find(
          ResolveCacheKey(t.symbolFile));
      if (trussPathIt != controller.m_resourceSyncState.resolvedModelRefs.end() &&
          trussPathIt->second.attempted)
        trussPath = trussPathIt->second.resolvedPath;
      if (!trussPath.empty()) {
        auto it = controller.m_resourceSyncState.loadedMeshes.find(trussPath);
        if (it != controller.m_resourceSyncState.loadedMeshes.end())
          trussMesh = &it->second;
      }
    }

    float trussLen = t.lengthMm * RENDER_SCALE;
    float trussWid = (t.widthMm > 0 ? t.widthMm : 400.0f) * RENDER_SCALE;
    float trussHei = (t.heightMm > 0 ? t.heightMm : 400.0f) * RENDER_SCALE;
    float trussWidthMm = (t.widthMm > 0 ? t.widthMm : 400.0f);
    float trussHeightMm = (t.heightMm > 0 ? t.heightMm : 400.0f);

    auto drawTrussGeometry =
        [&](const std::function<std::array<float, 3>(
                const std::array<float, 3> &)> &captureTransformFn,
            bool isHighlighted, bool isSelected) {
          if (trussMesh) {
            controller.DrawMeshWithOutline(*trussMesh, r, g, b, RENDER_SCALE,
                                           isHighlighted, isSelected, cx, cy,
                                           cz, wireframe, mode,
                                           captureTransformFn, false, matrix);
          } else {
            controller.DrawWireframeBox(trussLen, trussHei, trussWid,
                                        isHighlighted, isSelected, wireframe,
                                        mode, captureTransformFn);
          }
        };

    const bool useSymbolInstancing =
        (controller.m_captureUseSymbols &&
         (controller.m_captureView == Viewer2DView::Bottom ||
          controller.m_captureView == Viewer2DView::Top ||
          controller.m_captureView == Viewer2DView::Front ||
          controller.m_captureView == Viewer2DView::Side) &&
         !highlight && !selected);
    bool placedInstance = false;
    if (useSymbolInstancing && controller.m_captureCanvas && !skipCapture) {
      std::string modelKey;
      if (!trussPath.empty())
        modelKey = NormalizeModelKey(trussPath);
      else if (!t.symbolFile.empty())
        modelKey = NormalizeModelKey(t.symbolFile);
      if (modelKey.empty() && !trussMesh) {
        std::ostringstream boxKey;
        boxKey << "box:" << t.lengthMm << "x" << trussWidthMm << "x"
               << trussHeightMm;
        modelKey = boxKey.str();
      }
      if (modelKey.empty() && !t.model.empty())
        modelKey = t.model;
      if (modelKey.empty() && !t.name.empty())
        modelKey = t.name;

      if (!modelKey.empty()) {
        SymbolKey symbolKey;
        symbolKey.modelKey = "truss:" + modelKey;
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

              controller.m_captureCanvas->SetSourceKey(
                  trussCaptureKey.empty() ? "truss" : trussCaptureKey);
              drawTrussGeometry({}, false, false);
              localCanvas->EndFrame();
              definition.bounds = ComputeSymbolBounds(definition.localCommands);

              controller.m_captureCanvas = prevCanvas;
              controller.m_captureView = prevView;
              controller.m_captureOnly = prevCaptureOnly;
              controller.m_captureIncludeGrid = prevIncludeGrid;
              return definition;
            });

        Transform2D instanceTransform =
            BuildInstanceTransform2D(captureTransform, controller.m_captureView);
        controller.m_captureCanvas->PlaceSymbolInstance(symbol.symbolId,
                                                        instanceTransform);
        placedInstance = true;
      }
    }

    if (placedInstance) {
      ICanvas2D *prevCanvas = controller.m_captureCanvas;
      bool prevCaptureOnly = controller.m_captureOnly;
      controller.m_captureCanvas = nullptr;
      controller.m_captureOnly = false;
      drawTrussGeometry(applyCapture, highlight, selected);
      controller.m_captureCanvas = prevCanvas;
      controller.m_captureOnly = prevCaptureOnly;
    } else {
      drawTrussGeometry(applyCapture, highlight, selected);
    }

    glPopMatrix();
  }
}
