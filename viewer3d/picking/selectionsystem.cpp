#include "selectionsystem.h"

void SelectionSystem::SetHighlightUuid(const std::string &uuid) {
  m_controller.SetHighlightUuidImpl(uuid);
}

void SelectionSystem::SetSelectedUuids(const std::vector<std::string> &uuids) {
  m_controller.SetSelectedUuidsImpl(uuids);
}

bool SelectionSystem::GetFixtureLabelAt(int mouseX, int mouseY, int width,
                                        int height, wxString &outLabel,
                                        wxPoint &outPos,
                                        std::string *outUuid) {
  return m_controller.GetFixtureLabelAtImpl(mouseX, mouseY, width, height,
                                            outLabel, outPos, outUuid);
}

bool SelectionSystem::GetTrussLabelAt(int mouseX, int mouseY, int width,
                                      int height, wxString &outLabel,
                                      wxPoint &outPos,
                                      std::string *outUuid) {
  return m_controller.GetTrussLabelAtImpl(mouseX, mouseY, width, height,
                                          outLabel, outPos, outUuid);
}

bool SelectionSystem::GetSceneObjectLabelAt(int mouseX, int mouseY, int width,
                                            int height, wxString &outLabel,
                                            wxPoint &outPos,
                                            std::string *outUuid) {
  return m_controller.GetSceneObjectLabelAtImpl(mouseX, mouseY, width, height,
                                                outLabel, outPos, outUuid);
}

std::vector<std::string> SelectionSystem::GetFixturesInScreenRect(
    int x1, int y1, int x2, int y2, int width, int height) const {
  return m_controller.GetFixturesInScreenRectImpl(x1, y1, x2, y2, width,
                                                  height);
}

std::vector<std::string> SelectionSystem::GetTrussesInScreenRect(
    int x1, int y1, int x2, int y2, int width, int height) const {
  return m_controller.GetTrussesInScreenRectImpl(x1, y1, x2, y2, width,
                                                 height);
}

std::vector<std::string> SelectionSystem::GetSceneObjectsInScreenRect(
    int x1, int y1, int x2, int y2, int width, int height) const {
  return m_controller.GetSceneObjectsInScreenRectImpl(x1, y1, x2, y2, width,
                                                      height);
}
