#pragma once

#include "iselectioncontext.h"
#include <array>
#include <chrono>
#include <string>
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
  };

  struct QueryMetrics {
    std::chrono::microseconds total{0};
    std::chrono::microseconds projection{0};
    std::chrono::microseconds depthRead{0};
    std::chrono::microseconds candidateLoop{0};
    bool usedIdPath = false;
    bool usedFallbackPath = false;
    bool reprojected = false;
  };

  struct QueryTelemetry {
    int totalQueries = 0;
    int idPathQueries = 0;
    int fallbackQueries = 0;
    int reprojections = 0;
    int repaintEstimates = 0;
    bool hadFixtureResult = false;
    bool hadTrussResult = false;
    bool hadObjectResult = false;
    std::string lastFixtureUuid;
    std::string lastTrussUuid;
    std::string lastObjectUuid;
  };

  mutable QueryCache m_queryCache;
  mutable int m_queryCounter = 0;
  mutable QueryTelemetry m_queryTelemetry;

private:
  ISelectionContext &m_controller;
};
