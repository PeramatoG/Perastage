#pragma once

#include <cstddef>
#include <string>
#include <unordered_set>

class BasePassFramebufferCache {
public:
  BasePassFramebufferCache() = default;
  ~BasePassFramebufferCache();

  bool RestoreToDefaultFramebuffer(
      int width, int height, size_t cameraFingerprint,
      const std::unordered_set<std::string> &hiddenLayers,
      size_t sceneVersion) const;
  void CaptureFromDefaultFramebuffer(
      int width, int height, size_t cameraFingerprint,
      const std::unordered_set<std::string> &hiddenLayers,
      size_t sceneVersion);
  void Invalidate();

private:
  void EnsureFramebufferSize(int width, int height);

  unsigned int m_fbo = 0;
  unsigned int m_colorTexture = 0;
  unsigned int m_depthRenderbuffer = 0;
  int m_width = 0;
  int m_height = 0;

  bool m_hasSnapshot = false;
  size_t m_lastCameraFingerprint = 0;
  std::unordered_set<std::string> m_lastHiddenLayers;
  size_t m_lastSceneVersion = static_cast<size_t>(-1);
};
