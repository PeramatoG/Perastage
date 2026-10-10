#include "viewer3dcontroller.h"
#include "configmanager.h"
#include "resources/resource_sync_system.h"
#include <GL/glew.h>

namespace {
void ReleaseGpuBuffers(Mesh &mesh) {
  if (mesh.textureId != 0) {
    glDeleteTextures(1, &mesh.textureId);
    mesh.textureId = 0;
  }
  if (mesh.eboLines != 0) {
    glDeleteBuffers(1, &mesh.eboLines);
    mesh.eboLines = 0;
  }
  if (mesh.eboTriangles != 0) {
    glDeleteBuffers(1, &mesh.eboTriangles);
    mesh.eboTriangles = 0;
  }
  if (mesh.vboNormals != 0) {
    glDeleteBuffers(1, &mesh.vboNormals);
    mesh.vboNormals = 0;
  }
  if (mesh.vboTexCoords != 0) {
    glDeleteBuffers(1, &mesh.vboTexCoords);
    mesh.vboTexCoords = 0;
  }
  if (mesh.vboFlatNormals != 0) {
    glDeleteBuffers(1, &mesh.vboFlatNormals);
    mesh.vboFlatNormals = 0;
  }
  if (mesh.vboFlatVertices != 0) {
    glDeleteBuffers(1, &mesh.vboFlatVertices);
    mesh.vboFlatVertices = 0;
  }
  if (mesh.vboVertices != 0) {
    glDeleteBuffers(1, &mesh.vboVertices);
    mesh.vboVertices = 0;
  }
  if (mesh.vao != 0) {
    glDeleteVertexArrays(1, &mesh.vao);
    mesh.vao = 0;
  }

  mesh.triangleIndexCount = 0;
  mesh.flatVertexCount = 0;
  mesh.lineIndexCount = 0;
  mesh.buffersReady = false;
}
} // namespace

const Mesh &Viewer3DController::ResolveRenderMesh(const Mesh &source,
                                                bool authored, bool navigating) {
  if (authored) return source;
  auto &cache = m_resourceSyncState.runtimeMeshes;
  cache.ApplyPreferences(model_detail::ReadPreferences(ConfigManager::Get()),
                         ReleaseGpuBuffers);
  return cache.Resolve(source, navigating,
                       [this](Mesh &mesh) { EnsureMeshGpuBuffers(mesh); });
}
void Viewer3DController::ReleaseMeshBuffers(Mesh &mesh) {
  m_resourceSyncState.runtimeMeshes.Invalidate(mesh, ReleaseGpuBuffers);
  ReleaseGpuBuffers(mesh);
}
void Viewer3DController::ClearRuntimeMeshes() {
  m_resourceSyncState.runtimeMeshes.Clear(ReleaseGpuBuffers);
}
