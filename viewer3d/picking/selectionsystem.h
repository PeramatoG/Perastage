#pragma once

#include "iselectioncontext.h"
#include <array>
#include <chrono>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <wx/gdicmn.h>
#include <wx/string.h>

class SelectionSystem {
public:
  explicit SelectionSystem(ISelectionContext &controller) : m_controller(controller) {}

  void SetHighlightUuid(const std::string &uuid);
  void SetSelectedUuids(const std::vector<std::string> &uuids);
  bool GetFixtureLabelAt(int mouseX, int mouseY, int width, int height,
                         wxString &outLabel, wxPoint &outPos,
                         std::string *outUuid = nullptr,
                         bool confirmDepth = false);
  bool GetTrussLabelAt(int mouseX, int mouseY, int width, int height,
                       wxString &outLabel, wxPoint &outPos,
                       std::string *outUuid = nullptr,
                       bool confirmDepth = false);
  bool GetSceneObjectLabelAt(int mouseX, int mouseY, int width, int height,
                             wxString &outLabel, wxPoint &outPos,
                             std::string *outUuid = nullptr,
                             bool confirmDepth = false);
  std::vector<std::string> GetFixturesInScreenRect(int x1, int y1, int x2,
                                                   int y2, int width,
                                                   int height) const;
  std::vector<std::string> GetTrussesInScreenRect(int x1, int y1, int x2,
                                                  int y2, int width,
                                                  int height) const;
  std::vector<std::string> GetSceneObjectsInScreenRect(int x1, int y1,
                                                       int x2, int y2,
                                                       int width,
                                                       int height) const;

public:
  struct QueryCache {
    struct HoverCandidate {
      std::string uuid;
      double minX = 0.0;
      double minY = 0.0;
      double maxX = 0.0;
      double maxY = 0.0;
    };

    struct HoverGridIndex {
      struct SegmentationCell {
        int candidateIndex = -1;
        double minDepth = 1.0;
      };
      bool valid = false;
      const ISelectionContext::VisibleSet *sourceVisibleSet = nullptr;
      size_t sourceCount = 0;
      int viewportWidth = 0;
      int viewportHeight = 0;
      std::unordered_map<long long, SegmentationCell> cells;
      std::vector<HoverCandidate> candidates;
    };

    bool valid = false;
    int viewport[4]{};
    std::array<double, 16> model{};
    std::array<double, 16> projection{};
    ISelectionContext::ViewFrustumSnapshot frustum{};
    std::unordered_set<std::string> hiddenLayers;
    const ISelectionContext::VisibleSet *visibleSet = nullptr;
    bool hasDepthWorldPoint = false;
    std::array<double, 3> depthWorldPoint{0.0, 0.0, 0.0};
    int depthMouseX = -1;
    int depthMouseY = -1;
    int depthHeight = -1;
    HoverGridIndex fixtureHoverIndex;
    HoverGridIndex trussHoverIndex;
    HoverGridIndex objectHoverIndex;
  };

  struct QueryMetrics {
    std::chrono::microseconds projection{0};
    std::chrono::microseconds depthRead{0};
    std::chrono::microseconds candidateLoop{0};
  };

  mutable QueryCache m_queryCache;
  mutable int m_queryCounter = 0;

private:
  ISelectionContext &m_controller;
};
