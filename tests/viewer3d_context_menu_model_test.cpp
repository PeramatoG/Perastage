#include "context_menu_model.h"

#include <cassert>
#include <set>
#include <string>

namespace {

using viewer3d::context_menu::Build;
using viewer3d::context_menu::Input;
using viewer3d::context_menu::Model;
using viewer3d::context_menu::SelectableObject;
using viewer3d::context_menu::SelectionPage;

// Verifies the general menu actions and runtime labels in every model.
void AssertValidModel(const Model &model) {
  assert(model.renderStyleAvailable);
  assert(model.exportImageAvailable);
  if (model.selectionAvailable) {
    assert(!model.selectAllLabel.empty());
    assert(!model.typeSubmenuLabel.empty());
  }
  for (const std::string &label : model.typesOrModels)
    assert(!label.empty());
  for (const std::string &label : model.positions)
    assert(!label.empty());
}

// Verifies states where fixture or truss selection actions are unavailable.
void TestUnavailableSelectionStates() {
  Input input;
  input.pickingAvailable = true;
  Model model = Build(input);
  assert(!model.selectionAvailable);
  AssertValidModel(model);

  input.page = SelectionPage::Fixtures;
  model = Build(input);
  assert(!model.selectionAvailable);
  AssertValidModel(model);

  input.page = SelectionPage::Trusses;
  model = Build(input);
  assert(!model.selectionAvailable);
  AssertValidModel(model);

  input.objects = {{"Model", "Position"}};
  input.selectionObjectHit = true;
  model = Build(input);
  assert(!model.selectionAvailable);
  assert(model.typesOrModels.empty());
  AssertValidModel(model);

  input.selectionObjectHit = false;
  input.pickingAvailable = false;
  model = Build(input);
  assert(!model.selectionAvailable);
  AssertValidModel(model);
}

// Verifies fixture labels and deterministic type and position collection.
void TestFixtureSelectionModel() {
  Input input;
  input.page = SelectionPage::Fixtures;
  input.pickingAvailable = true;
  input.objects = {{"Wash", "FOH"}, {"Spot", ""}, {"Wash", "FOH"}};
  const Model model = Build(input);

  assert(model.selectionAvailable);
  assert(model.selectAllLabel == "All fixtures");
  assert(model.typeSubmenuLabel == "Select by fixture type");
  assert((model.typesOrModels == std::vector<std::string>{"Spot", "Wash"}));
  assert((model.positions == std::vector<std::string>{"FOH"}));
  assert(model.hasNoPosition);
  AssertValidModel(model);
}

// Verifies truss labels and deterministic model and position collection.
void TestTrussSelectionModel() {
  Input input;
  input.page = SelectionPage::Trusses;
  input.pickingAvailable = true;
  input.objects = {{"F34-3000", "Upstage"}, {"F34-2000", "Downstage"}};
  const Model model = Build(input);

  assert(model.selectionAvailable);
  assert(model.selectAllLabel == "All trusses");
  assert(model.typeSubmenuLabel == "Select by model");
  assert((model.typesOrModels ==
          std::vector<std::string>{"F34-2000", "F34-3000"}));
  assert((model.positions == std::vector<std::string>{"Downstage", "Upstage"}));
  assert(!model.hasNoPosition);
  AssertValidModel(model);
}

// Verifies conversion remains independent from fixture and truss selection.
void TestSceneObjectConversionModel() {
  Input input;
  input.pickingAvailable = true;
  input.convertibleSceneObjectHit = true;
  const Model model = Build(input);

  assert(model.convertToTrussAvailable);
  assert(!model.selectionAvailable);
  AssertValidModel(model);
}

} // namespace

// Runs the Viewer3D context-menu state regression scenarios.
int main() {
  TestUnavailableSelectionStates();
  TestFixtureSelectionModel();
  TestTrussSelectionModel();
  TestSceneObjectConversionModel();
  return 0;
}
