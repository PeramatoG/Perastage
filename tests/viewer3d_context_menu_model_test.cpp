/* This file is part of Perastage. License: GPL-3.0-or-later. */
#include "context_menu_model.h"
#include <algorithm>
#include <cassert>

using namespace viewport_context;
namespace {
bool Has(const Model &model, Action action) {
  return std::find(model.actions.begin(), model.actions.end(), action) != model.actions.end();
}
Input FixtureInput() {
  Input input;
  input.hit = {Kind::Fixture, "fixture-a"};
  input.capabilities = {true, true, true, true, true, true, true, true, true, true, true, true};
  return input;
}
void TestClickedIdentityAndChronology() {
  auto input = FixtureInput();
  auto model = Build(input);
  assert(model.replaceSelection);
  assert(model.selection == std::vector<Target>{input.hit});
  assert(Has(model, Action::EditFixture) && Has(model, Action::EditPatch));
  assert(!Has(model, Action::EditTruss) && !Has(model, Action::EditPrimitive));
  input.selection = {{Kind::Truss, "truss-b"}, input.hit, {Kind::Fixture, "fixture-c"}};
  model = Build(input);
  assert(!model.replaceSelection);
  assert(model.selection == input.selection);
  assert(Has(model, Action::Group) && Has(model, Action::Copy) && Has(model, Action::Delete));
  assert(Build(input).actions == model.actions);
  // A promoted group carries all clicked members while retaining the hit UUID.
  input.selection.clear();
  input.clickedSelection = {input.hit, {Kind::Truss, "group-sibling"}};
  model = Build(input);
  assert(model.replaceSelection && model.selection == input.clickedSelection);
  input.selection = input.clickedSelection;
  assert(!Build(input).replaceSelection);
}
void TestCategoriesAndCapabilities() {
  auto input = FixtureInput();
  for (const auto kind : {Kind::Truss, Kind::Support, Kind::SceneObject, Kind::Group}) {
    input.hit = {kind, "clicked"};
    const auto model = Build(input);
    assert(model.replaceSelection && model.selection.front().uuid == "clicked");
    assert(Has(model, Action::EditTruss) == (kind == Kind::Truss));
    assert(Has(model, Action::EditPrimitive) == (kind == Kind::SceneObject));
    assert(Has(model, Action::ConvertToTruss) == (kind == Kind::SceneObject));
    assert(Has(model, Action::PositionTool));
    assert(!Has(model, Action::EditFixture));
  }
  input.hit = {Kind::SceneObject, "non-primitive"};
  input.capabilities.editPrimitive = false;
  input.capabilities.convertToTruss = false;
  input.capabilities.ungroup = false;
  const auto model = Build(input);
  assert(!Has(model, Action::EditPrimitive));
  assert(!Has(model, Action::ConvertToTruss));
  assert(!Has(model, Action::Ungroup));
}
void TestEmptyAndInteractions() {
  Input input;
  input.selection = {{Kind::Fixture, "keep"}};
  auto model = Build(input);
  assert(!model.replaceSelection && model.selection == input.selection);
  assert(!Has(model, Action::RenderStyle) && !Has(model, Action::ExportImage));
  input.capabilities.renderStyle = input.capabilities.exportImage = true;
  model = Build(input);
  assert(Has(model, Action::RenderStyle) && Has(model, Action::ExportImage));
  input.capabilities.selectionShortcuts = true;
  assert(Has(Build(input), Action::SelectionShortcuts));
  input.hit = {Kind::Fixture, "hit"};
  assert(!Has(Build(input), Action::SelectionShortcuts));
  // Failed/filtered picks have no individual action target.
  input.hit = {Kind::Fixture, {}};
  input.capabilities.editFixture = true;
  assert(!Has(Build(input), Action::EditFixture));
  input.interactionActive = true;
  assert(Build(input).actions.empty());
  input.interactionActive = false;
  input.dragged = true;
  assert(Build(input).actions.empty());
}
void TestGestureRecovery() {
  ClickGesture gesture;
  assert(!gesture.Finish());
  gesture.Begin(10, 10); gesture.Move(14, 10); gesture.Move(10, 10);
  assert(!gesture.Finish());
  gesture.Begin(10, 10); gesture.Move(12, 12);
  assert(gesture.Finish());
  gesture.Begin(10, 10, true); assert(!gesture.Finish());
  gesture.Begin(10, 10); gesture.Cancel(); assert(!gesture.Finish());
  gesture.Begin(10, 10); assert(gesture.Finish());
}
} // namespace
int main() {
  TestClickedIdentityAndChronology(); TestCategoriesAndCapabilities();
  TestEmptyAndInteractions(); TestGestureRecovery();
}
