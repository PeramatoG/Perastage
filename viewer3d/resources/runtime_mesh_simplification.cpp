#include "runtime_mesh_simplification.h"
#include <meshoptimizer.h>

namespace viewer3d::resources {
Mesh BuildRuntimeRepresentation(const Mesh &source,
                                model_detail::SimplificationPolicy policy) {
  Mesh proxy = source;
  proxy.buffersReady = false;
  proxy.vao = 0;
  proxy.vboVertices = 0;
  proxy.vboNormals = 0;
  proxy.vboTexCoords = 0;
  proxy.vboFlatVertices = 0;
  proxy.vboFlatNormals = 0;
  proxy.eboTriangles = 0;
  proxy.eboLines = 0;
  proxy.textureId = 0;
  proxy.triangleIndexCount = proxy.flatVertexCount = proxy.lineIndexCount = 0;
  proxy.flatVertices.clear();
  proxy.flatNormals.clear();
  proxy.flippedIndicesCache.clear();
  proxy.flippedFlatVertices.clear();
  proxy.flippedFlatNormals.clear();

  const size_t vertexCount = proxy.vertices.size() / 3;
  if (vertexCount >= 3 && proxy.indices.size() >= 3) {
    const size_t indexCount = proxy.indices.size();
    const size_t targetIndexCount = policy.targetTriangles * 3;

    std::vector<uint32_t> simplified(indexCount);
    size_t simplifiedCount =
        meshopt_simplify(simplified.data(), proxy.indices.data(), indexCount,
                         proxy.vertices.data(), vertexCount, sizeof(float) * 3,
                         targetIndexCount, policy.maxRelativeError, 0, nullptr);
    const size_t acceptableUpperBound =
        model_detail::ProxyAcceptableIndexCount(indexCount, targetIndexCount);

    // Some dense/non-manifold fixtures (often 32-bit/high-poly assets) can be
    // too constrained for a strict low-error pass. Escalate sloppy simplify
    // error in steps before using coarse subsampling to avoid odd artifacts.
    if (policy.allowCoarseFallback &&
        (simplifiedCount < 3 || simplifiedCount > acceptableUpperBound)) {
      const auto &sloppyErrors = model_detail::kProxyFallbackErrors;
      std::vector<uint32_t> sloppyCandidate(indexCount);
      size_t bestCount = 0;
      std::vector<uint32_t> bestIndices;

      for (float sloppyError : sloppyErrors) {
        size_t candidateCount = meshopt_simplifySloppy(
            sloppyCandidate.data(), proxy.indices.data(), indexCount,
            proxy.vertices.data(), vertexCount, sizeof(float) * 3,
            targetIndexCount, sloppyError, nullptr);
        if (candidateCount < 3)
          continue;

        if (bestCount == 0 || candidateCount < bestCount) {
          bestCount = candidateCount;
          bestIndices.assign(sloppyCandidate.begin(),
                             sloppyCandidate.begin() +
                                 static_cast<std::ptrdiff_t>(candidateCount));
        }

        if (candidateCount <= acceptableUpperBound)
          break;
      }

      if (bestCount >= 3) {
        simplifiedCount = bestCount;
        simplified = std::move(bestIndices);
      }
    }
    if (policy.allowCoarseFallback &&
        model_detail::ProxyNeedsCoarseFallback(simplifiedCount, indexCount,
                                               acceptableUpperBound)) {
      // Safety net for very large/complex 32-bit meshes where meshoptimizer
      // cannot reduce enough towards the requested budget: force a coarse
      // triangle subsample so every heavy fixture still gets a lighter proxy
      // while moving the camera.
      const size_t sourceTriangles = indexCount / 3;
      const size_t targetTriangles = std::max<size_t>(1, targetIndexCount / 3);
      simplified.clear();
      simplified.reserve(targetTriangles * 3);
      for (size_t t = 0; t < targetTriangles; ++t) {
        const size_t sourceTri = (t * sourceTriangles) / targetTriangles;
        const size_t base = sourceTri * 3;
        if (base + 2 >= proxy.indices.size())
          break;
        simplified.push_back(proxy.indices[base]);
        simplified.push_back(proxy.indices[base + 1]);
        simplified.push_back(proxy.indices[base + 2]);
      }
      simplifiedCount = simplified.size();
    }
    if (simplifiedCount < 3)
      return proxy;
    simplified.resize(simplifiedCount);

    std::vector<uint32_t> cacheOptimized(simplified.size());
    meshopt_optimizeVertexCache(cacheOptimized.data(), simplified.data(),
                                simplified.size(), vertexCount);

    std::vector<uint32_t> overdrawOptimized(cacheOptimized.size());
    meshopt_optimizeOverdraw(overdrawOptimized.data(), cacheOptimized.data(),
                             cacheOptimized.size(), proxy.vertices.data(),
                             vertexCount, sizeof(float) * 3,
                             model_detail::kOverdrawThreshold);
    proxy.indices = std::move(overdrawOptimized);
  }

  return proxy;
}
} // namespace viewer3d::resources
