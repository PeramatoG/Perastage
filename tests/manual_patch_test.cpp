/* This file is part of Perastage. License: GPL-3.0-or-later. */
#include "fixture_patch_edit_plan.h"
#include "fixturetable/patch_warning_ranges.h"
#include "fixture_patch_service.h"
#include "query/scene_query.h"
#include <cassert>
int main() {
  assert(fixture_patch_presentation::SpanRanges(1, 400, 180) == "1.400-1.512; 2.1-2.67");
  for (int channel : {1, 333, 400, 511, 512}) {
    MvrScene scene;
    auto &fixture = scene.fixtures["fixture"];
    fixture.uuid = "fixture";
    fixture.address = "1.1";
    fixture.dmxAddresses = FixtureDmxAddresses{{{std::string("0"), "1"}, {std::string("7"), "9.12"}}};
    const auto plan = perastage::patch::PlanTablePatch({180}, 1, channel);
    assert(plan.size() == 1 && plan[0].universe == 1 && plan[0].channel == channel);
    const auto requested = "1." + std::to_string(channel);
    assert(perastage::patch::SetFixturePrimaryPatch(fixture, requested));
    assert(fixture.address == requested && fixture.dmxAddresses->entries.size() == 2);
    assert(fixture.dmxAddresses->entries[1].value == "9.12");
    auto inspect = [&](int size) { return perastage::query::GetPatchStatus(scene,
        [=](const Fixture &) -> std::optional<int> { return size; }); };
    assert(inspect(180).diagnostics.empty() == (channel <= 333));
    assert(inspect(1).diagnostics.empty()); // Mode changes never move the start.
    assert(fixture.address == requested);
    if (channel > 333) assert(!inspect(180).fixtures[0].endChannel);
    if (channel == 400) {
      auto &other = scene.fixtures["other"];
      other.uuid = "other"; other.address = "1.500";
      assert(!inspect(180).conflicts.empty());
    }
  }
  assert(perastage::patch::PlanTablePatch({180}, 1, 513).empty());
  const auto automatic = PatchManager::SequentialPatch({180}, 1, 400);
  assert(automatic[0].universe == 2 && automatic[0].channel == 1);
  const auto batch = perastage::patch::PlanTablePatch({180,180}, 1, 400);
  assert(batch[0].universe == 2 && batch[0].channel == 1 && batch[1].channel == 181);
}
