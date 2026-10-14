#include "performance_metrics.h"
#include "viewer3dcontroller.h"
#include "configmanager.h"
#include "resources/resource_sync_system.h"
#include "logger.h"
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
  m_resourceSyncState.meshResolvedThisFrame |= !source.indices.empty();
  if (authored) return source;
  auto &cache = m_resourceSyncState.runtimeMeshes;
  return cache.Resolve(source, navigating,
                       [this](Mesh &mesh) { EnsureMeshGpuBuffers(mesh); });
}
void Viewer3DController::RefreshModelDetailPreferences() {
  m_resourceSyncState.runtimeMeshes.ApplyPreferences(
      model_detail::ReadPreferences(ConfigManager::Get()), ReleaseGpuBuffers);
}
void Viewer3DController::ReleaseMeshBuffers(Mesh &mesh) {
  m_resourceSyncState.runtimeMeshes.Invalidate(mesh, ReleaseGpuBuffers);
  ReleaseGpuBuffers(mesh);
}
void Viewer3DController::ClearRuntimeMeshes() {
  m_resourceSyncState.runtimeMeshes.Clear(ReleaseGpuBuffers);
}

// Optional quality work runs only after presentation, with a valid GL context.
bool Viewer3DController::FinishPresentedFrame(bool navigating) {
  if (navigating) performance::Count(performance::Work::MovingFrame);
  auto &cache = m_resourceSyncState.runtimeMeshes;
  if (!m_resourceSyncState.firstVisibleRecorded && m_resourceSyncState.meshResolvedThisFrame &&
      m_resourceSyncState.firstSyncTime != std::chrono::steady_clock::time_point{}) {
    auto &sample = performance::samples[static_cast<size_t>(performance::Work::FirstVisible)];
    ++sample.count;
    sample.nanoseconds += std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now() - m_resourceSyncState.firstSyncTime).count();
    m_resourceSyncState.firstVisibleRecorded = true;
    if (performance::Enabled()) Logger::Instance().Log(performance::Summary());
  }
  return cache.HasPending();
}

viewer3d::resources::ResourceLoadProgress Viewer3DController::GetResourceLoadProgress() const {
  return {m_resourceSyncState.workRequests.Completed(), m_resourceSyncState.workRequests.Size(),
          m_resourceSyncState.resourcesFailed, m_resourceSyncState.runtimeMeshes.PendingCount()};
}

// Source loading takes precedence over optional navigation/quality processing.
bool Viewer3DController::PreparePendingRenderMeshes() {
  if (IsResourceSyncPending()) return false;
  RefreshModelDetailPreferences();
  return m_resourceSyncState.runtimeMeshes.PreparePending(
      [this](Mesh &mesh) { EnsureMeshGpuBuffers(mesh); },
      viewer3d::resources::ProgressiveLoadCadence::kMaximumResources,
      viewer3d::resources::ProgressiveLoadCadence::kWorkSlice) > 0;
}
