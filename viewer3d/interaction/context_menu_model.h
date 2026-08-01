#pragma once

#include <string>
#include <vector>

namespace viewer3d::context_menu {

enum class SelectionPage { Other, Fixtures, Trusses };

struct SelectableObject {
  std::string typeOrModel;
  std::string position;
};

struct Input {
  SelectionPage page = SelectionPage::Other;
  std::vector<SelectableObject> objects;
  bool pickingAvailable = false;
  bool selectionObjectHit = false;
  bool convertibleSceneObjectHit = false;
};

struct Model {
  bool selectionAvailable = false;
  std::string selectAllLabel;
  std::string typeSubmenuLabel;
  std::vector<std::string> typesOrModels;
  std::vector<std::string> positions;
  bool hasNoPosition = false;
  bool convertToTrussAvailable = false;
  bool renderStyleAvailable = true;
  bool exportImageAvailable = true;
};

// Calculates the valid 3D context-menu actions without constructing GUI
// objects.
Model Build(const Input &input);

} // namespace viewer3d::context_menu
