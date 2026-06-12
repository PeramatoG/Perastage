#pragma once

#include "viewer3d_types.h"

#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>

class Viewer3DController;

class IdPickPass {
public:
  explicit IdPickPass(Viewer3DController &controller);
  ~IdPickPass();

  bool ReadUuidAt(int mouseX, int mouseY, int width, int height,
                  const std::unordered_set<std::string> &hiddenLayers,
                  std::string &outUuid);
  void MarkDirty();

private:
  void EnsureFramebufferSize(int width, int height);
  void RebuildIfNeeded(int width, int height,
                       const std::unordered_set<std::string> &hiddenLayers);
  uint32_t GetOrCreatePickId(const std::string &uuid);
  std::array<float, 3> GetPickColor(const std::string &uuid);

  Viewer3DController &m_controller;
  unsigned int m_fbo = 0;
  unsigned int m_colorTexture = 0;
  unsigned int m_depthRenderbuffer = 0;
  int m_width = 0;
  int m_height = 0;
  bool m_dirty = true;
  size_t m_lastSceneVersion = static_cast<size_t>(-1);
  std::unordered_set<std::string> m_lastHiddenLayers;
  std::array<int, 4> m_lastViewport = {0, 0, 0, 0};
  std::array<double, 16> m_lastModel = {};
  std::array<double, 16> m_lastProjection = {};

  uint32_t m_nextPickId = 1;
  std::unordered_map<std::string, uint32_t> m_uuidToPickId;
  std::unordered_map<uint32_t, std::string> m_pickIdToUuid;
};
