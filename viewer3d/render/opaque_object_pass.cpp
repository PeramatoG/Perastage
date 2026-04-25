#include "opaque_object_pass.h"

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
#include "mesh.h"
#include "meshprimitives.h"
#include "opaque_pass_utils.h"
#include "scenedatamanager.h"
#include "viewer3dcontroller.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace {
void DrawBoundsSolid(const Viewer3DBoundingBox &bb) {
  glBegin(GL_QUADS);
  glVertex3f(bb.min[0], bb.min[1], bb.min[2]);
  glVertex3f(bb.max[0], bb.min[1], bb.min[2]);
  glVertex3f(bb.max[0], bb.max[1], bb.min[2]);
  glVertex3f(bb.min[0], bb.max[1], bb.min[2]);
  glVertex3f(bb.min[0], bb.min[1], bb.max[2]);
  glVertex3f(bb.max[0], bb.min[1], bb.max[2]);
  glVertex3f(bb.max[0], bb.max[1], bb.max[2]);
  glVertex3f(bb.min[0], bb.max[1], bb.max[2]);
  glVertex3f(bb.min[0], bb.min[1], bb.min[2]);
  glVertex3f(bb.max[0], bb.min[1], bb.min[2]);
  glVertex3f(bb.max[0], bb.min[1], bb.max[2]);
  glVertex3f(bb.min[0], bb.min[1], bb.max[2]);
  glVertex3f(bb.min[0], bb.max[1], bb.min[2]);
  glVertex3f(bb.max[0], bb.max[1], bb.min[2]);
  glVertex3f(bb.max[0], bb.max[1], bb.max[2]);
  glVertex3f(bb.min[0], bb.max[1], bb.max[2]);
  glVertex3f(bb.min[0], bb.min[1], bb.min[2]);
  glVertex3f(bb.min[0], bb.max[1], bb.min[2]);
  glVertex3f(bb.min[0], bb.max[1], bb.max[2]);
  glVertex3f(bb.min[0], bb.min[1], bb.max[2]);
  glVertex3f(bb.max[0], bb.min[1], bb.min[2]);
  glVertex3f(bb.max[0], bb.max[1], bb.min[2]);
  glVertex3f(bb.max[0], bb.max[1], bb.max[2]);
  glVertex3f(bb.max[0], bb.min[1], bb.max[2]);
  glEnd();
}

const Mesh &FallbackSceneObjectCubeMesh() {
  static const Mesh mesh = []() {
    Mesh cube;
    cube.vertices = {
        -0.5f, -0.5f, -0.5f, // 0
        0.5f,  -0.5f, -0.5f, // 1
        0.5f,  0.5f,  -0.5f, // 2
        -0.5f, 0.5f,  -0.5f, // 3
        -0.5f, -0.5f, 0.5f,  // 4
        0.5f,  -0.5f, 0.5f,  // 5
        0.5f,  0.5f,  0.5f,  // 6
        -0.5f, 0.5f,  0.5f   // 7
    };
    cube.indices = {
        0, 1, 2, 0, 2, 3, // back
        4, 6, 5, 4, 7, 6, // front
        0, 4, 5, 0, 5, 1, // bottom
        3, 2, 6, 3, 6, 7, // top
        0, 3, 7, 0, 7, 4, // left
        1, 5, 6, 1, 6, 2  // right
    };
    ComputeNormals(cube);
    return cube;
  }();
  return mesh;
}

const Mesh &FallbackSceneObjectCylinderMesh() {
  static const Mesh mesh = BuildCylinderMesh(0.5f, 1.0f, 24);
  return mesh;
}

bool IsPipeSceneObject(const SceneObject &object) {
  if (!object.modelFile.empty() || !object.geometries.empty())
    return false;

  std::string upperName = object.name;
  std::transform(upperName.begin(), upperName.end(), upperName.begin(),
                 [](unsigned char c) {
                   return static_cast<char>(std::toupper(c));
                 });
  return upperName.rfind("PIPE", 0) == 0;
}

bool IsScreenSceneObject(const SceneObject &object) {
  std::string lowerName = object.name;
  std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(),
                 [](unsigned char c) {
                   return static_cast<char>(std::tolower(c));
                 });
  return lowerName.find("screen") != std::string::npos ||
         lowerName.find("pantalla") != std::string::npos;
}

bool CanUseAffineSymbolInstance(const Matrix &transform, Viewer2DView view) {
  constexpr float kEpsilon = 1e-4f;
  switch (view) {
  case Viewer2DView::Top:
  case Viewer2DView::Bottom:
    return std::abs(transform.w[0]) <= kEpsilon &&
           std::abs(transform.w[1]) <= kEpsilon;
  case Viewer2DView::Front:
    return std::abs(transform.v[0]) <= kEpsilon &&
           std::abs(transform.v[2]) <= kEpsilon;
  case Viewer2DView::Side:
    return std::abs(transform.u[1]) <= kEpsilon &&
           std::abs(transform.u[2]) <= kEpsilon;
  }
  return false;
}

const Mesh *TryGetPrimitiveSceneObjectMesh(const std::string &modelRef) {
  constexpr std::string_view prefix = "primitive:";
  if (modelRef.rfind(prefix.data(), 0) != 0)
    return nullptr;

  const std::string primitiveType = modelRef.substr(prefix.size());
  if (primitiveType.empty())
    return nullptr;

  static std::unordered_map<std::string, Mesh> cache;
  auto it = cache.find(primitiveType);
  if (it != cache.end())
    return &it->second;

  Mesh mesh;
  if (!BuildPrimitiveMesh(primitiveType, mesh))
    return nullptr;
  auto [insertedIt, inserted] =
      cache.emplace(primitiveType, std::move(mesh));
  (void)inserted;
  return &insertedIt->second;
}

std::string BuildSceneObjectSymbolSignature(
    const SceneObject &object,
    const std::vector<std::pair<std::string, Matrix>> &meshParts) {
  std::ostringstream signature;
  signature << std::fixed << std::setprecision(6);

  if (!meshParts.empty()) {
    signature << "parts:" << meshParts.size();
    for (const auto &part : meshParts) {
      signature << '|';
      if (!part.first.empty())
        signature << part.first;
      else
        signature << "<unnamed>";

      const Matrix &m = part.second;
      signature << "@" << m.u[0] << ',' << m.u[1] << ',' << m.u[2] << ';'
                << m.v[0] << ',' << m.v[1] << ',' << m.v[2] << ';' << m.w[0]
                << ',' << m.w[1] << ',' << m.w[2] << ';' << m.o[0] << ','
                << m.o[1] << ',' << m.o[2];
    }
    return signature.str();
  }

  if (!object.modelFile.empty()) {
    signature << "model:" << NormalizeModelKey(object.modelFile);
    return signature.str();
  }

  signature << "fallback:";
  signature << (IsPipeSceneObject(object) ? "pipe-cylinder" : "cube");
  return signature.str();
}

} // namespace

void OpaqueObjectPass::Render(
    Viewer3DController &controller, const RenderFrameContext &context,
    const Viewer3DVisibleSet &visibleSet,
    const std::function<std::array<float, 3>(const std::string &)> &getLayerColor,
    const std::function<SymbolViewKind(Viewer2DView)> &resolveSymbolView,
    const std::function<std::array<float, 3>(const std::string &)> &getPickColor) {
  if (context.idOnlyPass) {
    glShadeModel(GL_FLAT);
    for (const auto &uuid : visibleSet.objectUuids) {
      auto bbIt = controller.m_objectBounds.find(uuid);
      if (bbIt == controller.m_objectBounds.end())
        continue;
      const auto pickColor = getPickColor(uuid);
      glColor3f(pickColor[0], pickColor[1], pickColor[2]);
      DrawBoundsSolid(bbIt->second);
    }
    return;
  }
  const bool wireframe = context.wireframe;
  const Viewer2DRenderMode mode = context.mode;
  const bool skipCapture = context.skipCapture;
  const Viewer2DView captureView = context.view;
  const bool drawRealTopInTopView =
      context.is2DViewer && captureView == Viewer2DView::Top;
  const bool disableDepthBiasFor2DViewer = context.is2DViewer;

  const auto &sceneObjects = SceneDataManager::Instance().GetSceneObjects();

  glShadeModel((context.texturedStyle && !wireframe) ? GL_SMOOTH : GL_FLAT);
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

    const bool highlight =
        context.selectionOverlayPass && !controller.m_highlightUuid.empty() &&
        uuid == controller.m_highlightUuid;
    const bool selected = context.selectionOverlayPass &&
                          controller.m_selectedUuids.find(uuid) !=
                              controller.m_selectedUuids.end();

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
    if (context.whiteModelStyle && !wireframe) {
      r = 0.95f;
      g = 0.95f;
      b = 0.95f;
    }
    if (mode == Viewer2DRenderMode::Wireframe ||
        mode == Viewer2DRenderMode::ByLayer) {
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
        if (const Mesh *primitiveMesh =
                TryGetPrimitiveSceneObjectMesh(geo.modelFile);
            primitiveMesh != nullptr) {
          SceneObjectMeshPart part;
          part.mesh = primitiveMesh;
          part.localTransform = geo.localTransform;
          part.modelKey = NormalizeModelKey(geo.modelFile);
          objectMeshParts.push_back(std::move(part));
          continue;
        }
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
          const bool disableDepthBiasForScreen =
              IsScreenSceneObject(m) && !isHighlighted && !isSelected;
          const bool disableDepthBias =
              disableDepthBiasFor2DViewer || disableDepthBiasForScreen;
          if (!objectMeshParts.empty()) {
            const bool reversePartOrder = drawRealTopInTopView;
            for (size_t offset = 0; offset < objectMeshParts.size(); ++offset) {
              const size_t partIndex =
                  reversePartOrder ? (objectMeshParts.size() - 1 - offset)
                                   : offset;
              const auto &part = objectMeshParts[partIndex];
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

              Matrix localCaptureMatrix = part.localTransform;
              localCaptureMatrix.o[0] *= RENDER_SCALE;
              localCaptureMatrix.o[1] *= RENDER_SCALE;
              localCaptureMatrix.o[2] *= RENDER_SCALE;
              auto localPartCapture =
                  [localCaptureMatrix](const std::array<float, 3> &p) {
                    return TransformPoint(localCaptureMatrix, p);
                  };

              float localMatrix[16];
              MatrixToArray(part.localTransform, localMatrix);
              glPushMatrix();
              controller.ApplyTransform(localMatrix, false);
              auto partCaptureTransform = captureTransformFn;
              if (captureTransformFn)
                partCaptureTransform = partCapture;
              else
                partCaptureTransform = localPartCapture;

              controller.DrawMeshWithOutline(*part.mesh, r, g, b, RENDER_SCALE,
                                             isHighlighted, isSelected, cx, cy,
                                             cz, wireframe, mode,
                                             partCaptureTransform, false,
                                             partMatrix, context.selectionMaskPass,
                                             disableDepthBias);
              glPopMatrix();
            }
          } else {
            // Fallback scene objects use the same mesh path as regular models
            // so every render style (white model, textured, by-layer, etc.)
            // stays visually consistent even when the object has no mesh file.
            const bool useUnlitFallbackFill =
                !isHighlighted && !isSelected &&
                context.whiteModelStyle &&
                !controller.IsSketchRenderStyleEnabled();
            const bool fallbackWireframe = wireframe;
            const Mesh &fallbackMesh = IsPipeSceneObject(m)
                                           ? FallbackSceneObjectCylinderMesh()
                                           : FallbackSceneObjectCubeMesh();
            controller.DrawMeshWithOutline(
                fallbackMesh, r, g, b, 0.3f, isHighlighted, isSelected, cx, cy,
                cz, fallbackWireframe, mode, captureTransformFn,
                useUnlitFallbackFill, matrix, context.selectionMaskPass,
                disableDepthBias);
          }
        };

    const bool useSymbolInstancing =
        (controller.m_captureUseSymbols &&
         (captureView == Viewer2DView::Bottom ||
          captureView == Viewer2DView::Top ||
          captureView == Viewer2DView::Front ||
          captureView == Viewer2DView::Side) &&
         mode != Viewer2DRenderMode::Wireframe &&
         CanUseAffineSymbolInstance(captureTransform, captureView) &&
         !highlight && !selected);
    bool placedInstance = false;
    if (useSymbolInstancing && controller.m_captureCanvas && !skipCapture) {
      std::vector<std::pair<std::string, Matrix>> symbolMeshParts;
      symbolMeshParts.reserve(objectMeshParts.size());
      for (const auto &part : objectMeshParts)
        symbolMeshParts.emplace_back(part.modelKey, part.localTransform);
      const std::string modelKey =
          BuildSceneObjectSymbolSignature(m, symbolMeshParts);

      if (!modelKey.empty()) {
        SymbolKey symbolKey;
        symbolKey.modelKey = "object:" + modelKey;
        symbolKey.viewKind = resolveSymbolView(captureView);
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
              controller.m_captureView = captureView;
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
            BuildInstanceTransform2D(captureTransform, captureView);
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
