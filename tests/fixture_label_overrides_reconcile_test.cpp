#include "configmanager.h"
#include "fixture.h"
#include "fixture_label_overrides.h"

#include <cassert>

int main() {
  ConfigManager &cfg = ConfigManager::Get();
  cfg.Reset();

  Fixture validFixture;
  validFixture.uuid = "fixture-valid";
  cfg.GetScene().fixtures[validFixture.uuid] = validFixture;

  cfg.SetValue("label_fixture_overrides",
               R"({
                 "fixture-valid": {
                   "showLabelName": [true, null, null]
                 },
                 "fixture-missing": {
                   "showLabelName": [false, null, null]
                 }
               })");

  viewer2d::ReconcileFixtureLabelOverridesWithScene(cfg);

  const auto overrides = viewer2d::LoadFixtureLabelOverrides(cfg);
  assert(overrides.size() == 1);
  assert(overrides.count("fixture-valid") == 1);
  assert(overrides.count("fixture-missing") == 0);
  assert(cfg.HasKey("label_fixture_overrides"));

  cfg.Reset();
  return 0;
}
