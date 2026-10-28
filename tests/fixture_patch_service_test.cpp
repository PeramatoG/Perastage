/* This file is part of Perastage. */
#include "fixture_patch_service.h"
#include <cassert>
using namespace perastage::patch;
int main() {
  Fixture fixture;
  fixture.uuid = "fixture";
  fixture.dmxAddresses = FixtureDmxAddresses{{
      {std::string("7"), "2049"}, {std::nullopt, "1025"}, {std::string("19"), "9.512"}}};
  fixture.address = "3.1";
  const auto initial = fixture;
  assert(ResolveFixturePatch(fixture));
  assert(SetFixturePrimaryPatch(fixture, "4.17"));
  assert(fixture.address == "4.17");
  assert(fixture.dmxAddresses->entries[0] == initial.dmxAddresses->entries[0]);
  assert(fixture.dmxAddresses->entries[2] == initial.dmxAddresses->entries[2]);
  assert(fixture.dmxAddresses->entries[1].value == "1553");
  assert(!fixture.dmxAddresses->entries[1].breakId);
  fixture.address = "5.1";
  const auto resolved = ResolveFixturePatch(fixture);
  assert(resolved && resolved.addresses->entries[1].value == "2049");
  assert(fixture.dmxAddresses->entries[1].value == "1553");
  assert(SetFixturePrimaryPatch(fixture, ""));
  assert(fixture.dmxAddresses->entries.size() == 2);
  const auto missing = fixture;
  assert(SetFixturePrimaryPatch(fixture, "1.1").conflict == FixturePatchConflict::MissingPrimary);
  assert(fixture.address == missing.address && fixture.dmxAddresses == missing.dmxAddresses);
  assert(SetFixturePrimaryPatch(fixture, ""));
  fixture = initial;
  fixture.dmxAddresses->entries.push_back({std::string("+000"), "3"});
  assert(ResolveFixturePatch(fixture).conflict == FixturePatchConflict::DuplicateBreak);
  fixture = initial;
  fixture.dmxAddresses->entries[1].value = "99999999999999999999999999999999999";
  fixture.address.clear();
  assert(ResolveFixturePatch(fixture));
  assert(SetFixturePrimaryPatch(fixture, "1.1").conflict == FixturePatchConflict::PrimaryOutsideEditableRange);
  fixture = {};
  assert(SetFixturePrimaryPatch(fixture, "2.512"));
  assert(fixture.dmxAddresses->entries[0].value == "1024");
  assert(SetFixturePrimaryPatch(fixture, ""));
  assert(fixture.dmxAddresses && fixture.dmxAddresses->entries.empty());
  assert(SetFixturePrimaryPatch(fixture, "1.513").conflict == FixturePatchConflict::InvalidPrimary);
  for (const auto *bad : {"", "0", "-1", "1.0", "1.513", "1.1.1", "nan", "1e4"})
    assert(!IsDmxValue(bad));
  assert(IsDmxValue("999999999999999999999999999999"));
  assert(IsDmxValue("999999999999999999999999999999.512"));
  assert(DmxPrimaryProjection("1025") == "3.1");
  assert(DmxPrimaryProjection("  +0003.001 ") == "3.1");
  assert(DmxInteger("-7", true) == std::optional<std::string>("-7"));
}
