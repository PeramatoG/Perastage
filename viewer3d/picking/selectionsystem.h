#pragma once

#include <string>
#include <vector>

#include <wx/gdicmn.h>
#include <wx/string.h>

class Viewer3DController;

class SelectionSystem {
public:
  explicit SelectionSystem(Viewer3DController &controller) : m_controller(controller) {}

  void SetHighlightUuid(const std::string &uuid);
  void SetSelectedUuids(const std::vector<std::string> &uuids);
  bool GetFixtureLabelAt(int mouseX, int mouseY, int width, int height,
                         wxString &outLabel, wxPoint &outPos,
                         std::string *outUuid = nullptr);
  bool GetTrussLabelAt(int mouseX, int mouseY, int width, int height,
                       wxString &outLabel, wxPoint &outPos,
                       std::string *outUuid = nullptr);
  bool GetSceneObjectLabelAt(int mouseX, int mouseY, int width, int height,
                             wxString &outLabel, wxPoint &outPos,
                             std::string *outUuid = nullptr);
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

private:
  Viewer3DController &m_controller;
};
