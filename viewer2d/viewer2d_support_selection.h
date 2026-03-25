#pragma once

#include "mvrscene.h"
#include <string>
#include <unordered_set>
#include <vector>
#include <wx/gdicmn.h>
#include <wx/string.h>

namespace Viewer2DSupportSelection {

bool FindHoistAtScreenPoint(int mouseX, int mouseY, int viewportHeight,
                            const MvrScene &scene,
                            const std::unordered_set<std::string> &hiddenLayers,
                            std::string &outUuid, wxPoint &outScreenPos,
                            wxString &outLabel);

std::vector<std::string>
GetHoistsInScreenRect(int x1, int y1, int x2, int y2, int viewportWidth,
                      int viewportHeight, const MvrScene &scene,
                      const std::unordered_set<std::string> &hiddenLayers);

} // namespace Viewer2DSupportSelection
