#include "context_menu_model.h"

#include <set>

namespace viewer3d::context_menu {

// Calculates the valid 3D context-menu actions without constructing GUI
// objects.
Model Build(const Input &input) {
  Model model;
  model.convertToTrussAvailable =
      input.pickingAvailable && input.convertibleSceneObjectHit;

  if (!input.pickingAvailable || input.selectionObjectHit ||
      input.objects.empty() || input.page == SelectionPage::Other) {
    return model;
  }

  model.selectionAvailable = true;
  if (input.page == SelectionPage::Fixtures) {
    model.selectAllLabel = "All fixtures";
    model.typeSubmenuLabel = "Select by fixture type";
  } else {
    model.selectAllLabel = "All trusses";
    model.typeSubmenuLabel = "Select by model";
  }

  std::set<std::string> typesOrModels;
  std::set<std::string> positions;
  for (const SelectableObject &object : input.objects) {
    if (!object.typeOrModel.empty())
      typesOrModels.insert(object.typeOrModel);
    if (object.position.empty())
      model.hasNoPosition = true;
    else
      positions.insert(object.position);
  }
  model.typesOrModels.assign(typesOrModels.begin(), typesOrModels.end());
  model.positions.assign(positions.begin(), positions.end());
  return model;
}

} // namespace viewer3d::context_menu
