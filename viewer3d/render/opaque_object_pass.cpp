#include "render/opaque_object_pass.h"

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

void OpaqueObjectPass::Render(
    Viewer3DController &controller, const RenderFrameContext &context,
    const Viewer3DVisibleSet &visibleSet,
    const std::function<std::array<float, 3>(const std::string &)> &getLayerColor,
    const std::function<SymbolViewKind(Viewer2DView)> &resolveSymbolView) {
  const bool wireframe = context.wireframe;
  const Viewer2DRenderMode mode = context.mode;
  const bool skipCapture = context.skipCapture;

  const auto &sceneObjects = SceneDataManager::Instance().GetSceneObjects();

  glShadeModel(GL_FLAT);
  for (const auto &uuid : visibleSet.objectUuids) {
    auto sceneIt = sceneObjects.find(uuid);
    if (sceneIt == sceneObjects.end())
      continue;
    const auto &m = sceneIt->second;
    glPushMatrix();

    std::string objectCaptureKey;
    if (controller.m_captureCanvas && !skipCapture) {
      objectCaptureKey = m.modelFile.empty() ? m.name : m.modelFile;
      if (objectCaptureKey.empty())
        objectCaptureKey = "scene_object";
      controller.m_captureCanvas->SetSourceKey(objectCaptureKey);
    }

    bool highlight = (!controller.m_highlightUuid.empty() &&
                      uuid == controller.m_highlightUuid);
    bool selected = (controller.m_selectedUuids.find(uuid) !=
                     controller.m_selectedUuids.end());

    float matrix[16];
    MatrixToArray(m.transform, matrix);
    controller.ApplyTransform(matrix, true);

    float cx = 0.0f, cy = 0.0f, cz = 0.0f;
    auto obit = controller.m_objectBounds.find(uuid);
    if (obit != controller.m_objectBounds.end()) {
      cx = (obit->second.min[0] + obit->second.max[0]) * 0.5f;
      cy = (obit->second.min[1] + obit->second.max[1]) * 0.5f;
      cz = (obit->second.min[2] + obit->second.max[2]) * 0.5f;
      cx -= m.transform.o[0] * RENDER_SCALE;
      cy -= m.transform.o[1] * RENDER_SCALE;
      cz -= m.transform.o[2] * RENDER_SCALE;
    }

    float r = 1.0f, g = 1.0f, b = 1.0f;
    if (wireframe && mode == Viewer2DRenderMode::ByLayer) {
      auto c = getLayerColor(m.layer);
      r = c[0];
      g = c[1];
      b = c[2];
    }

    Matrix captureTransform = m.transform;
    captureTransform.o[0] *= RENDER_SCALE;
    captureTransform.o[1] *= RENDER_SCALE;
    captureTransform.o[2] *= RENDER_SCALE;
    auto applyCapture = [captureTransform](const std::array<float, 3> &p) {
      return TransformPoint(captureTransform, p);
    };

    struct SceneObjectMeshPart {
      const Mesh *mesh = nullptr;
      Matrix localTransform = MatrixUtils::Identity();
      std::string modelKey;
    };
    std::vector<SceneObjectMeshPart> objectMeshParts;
    if (!m.geometries.empty()) {
      for (const auto &geo : m.geometries) {
        std::string objectPath;
        auto pathIt = controller.m_resourceSyncState.resolvedModelRefs.find(
            ResolveCacheKey(geo.modelFile));
        if (pathIt != controller.m_resourceSyncState.resolvedModelRefs.end() &&
            pathIt->second.attempted)
          objectPath = pathIt->second.resolvedPath;
        if (objectPath.empty())
          continue;
        auto it = controller.m_resourceSyncState.loadedMeshes.find(objectPath);
        if (it == controller.m_resourceSyncState.loadedMeshes.end())
          continue;

        SceneObjectMeshPart part;
        part.mesh = &it->second;
        part.localTransform = geo.localTransform;
        part.modelKey = NormalizeModelKey(objectPath);
        objectMeshParts.push_back(std::move(part));
      }
    } else if (!m.modelFile.empty()) {
      std::string objectPath;
      auto pathIt = controller.m_resourceSyncState.resolvedModelRefs.find(
          ResolveCacheKey(m.modelFile));
      if (pathIt != controller.m_resourceSyncState.resolvedModelRefs.end() &&
          pathIt->second.attempted)
        objectPath = pathIt->second.resolvedPath;
      if (!objectPath.empty()) {
        auto it = controller.m_resourceSyncState.loadedMeshes.find(objectPath);
        if (it != controller.m_resourceSyncState.loadedMeshes.end()) {
          SceneObjectMeshPart part;
          part.mesh = &it->second;
          part.modelKey = NormalizeModelKey(objectPath);
          objectMeshParts.push_back(std::move(part));
        }
      }
    }

    auto drawSceneObjectGeometry =
        [&](const std::function<std::array<float, 3>(
                const std::array<float, 3> &)> &captureTransformFn,
            bool isHighlighted, bool isSelected) {
          if (!objectMeshParts.empty()) {
            for (const auto &part : objectMeshParts) {
              Matrix worldMatrix =
                  MatrixUtils::Multiply(m.transform, part.localTransform);
              float partMatrix[16];
              MatrixToArray(worldMatrix, partMatrix);

              Matrix partCaptureMatrix = worldMatrix;
              partCaptureMatrix.o[0] *= RENDER_SCALE;
              partCaptureMatrix.o[1] *= RENDER_SCALE;
              partCaptureMatrix.o[2] *= RENDER_SCALE;
              auto partCapture = [partCaptureMatrix](const std::array<float, 3> &p) {
                return TransformPoint(partCaptureMatrix, p);
              };

              float localMatrix[16];
              MatrixToArray(part.localTransform, localMatrix);
              glPushMatrix();
              controller.ApplyTransform(localMatrix, false);
              auto partCaptureTransform = captureTransformFn;
              if (captureTransformFn)
                partCaptureTransform = partCapture;

              controller.DrawMeshWithOutline(*part.mesh, r, g, b, RENDER_SCALE,
                                             isHighlighted, isSelected, cx, cy,
                                             cz, wireframe, mode,
                                             partCaptureTransform, false,
                                             partMatrix);
              glPopMatrix();
            }
          } else {
            controller.DrawCubeWithOutline(0.3f, r, g, b, isHighlighted,
                                           isSelected, cx, cy, cz, wireframe,
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
      if (!objectMeshParts.empty())
        modelKey = objectMeshParts.front().modelKey;
      else if (!m.modelFile.empty())
        modelKey = NormalizeModelKey(m.modelFile);
      if (modelKey.empty() && !m.name.empty())
        modelKey = m.name;

      if (!modelKey.empty()) {
        SymbolKey symbolKey;
        symbolKey.modelKey = "object:" + modelKey;
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
                  objectCaptureKey.empty() ? "scene_object" : objectCaptureKey);
              drawSceneObjectGeometry({}, false, false);
              localCanvas->EndFrame();
              definition.bounds =
                  ComputeSymbolBounds(definition.localCommands);

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
      drawSceneObjectGeometry(applyCapture, highlight, selected);
      controller.m_captureCanvas = prevCanvas;
      controller.m_captureOnly = prevCaptureOnly;
    } else {
      drawSceneObjectGeometry(applyCapture, highlight, selected);
    }

    glPopMatrix();
  }
}
