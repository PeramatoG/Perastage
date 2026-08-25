/*
 * This file is part of Perastage.
 * Copyright (C) 2025 Luisma Peramato
 *
 * Perastage is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Perastage is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Perastage. If not, see <https://www.gnu.org/licenses/>.
 */
#include "autopatcher.h"
#include "fixture.h"
#include "mvrscene.h"
#include <algorithm>
#include <cassert>
#include <map>
#include <string>
#include <vector>

// Supplies deterministic fixture footprints without requiring GDTF archives.
int GetGdtfModeChannelCount(const std::string &, const std::string &mode) {
  return mode.empty() ? 1 : std::stoi(mode);
}

namespace {

// Adds a compact test fixture with explicit topology and channel metadata.
void AddFixture(MvrScene &scene, const std::string &uuid,
                const std::string &positionUuid,
                const std::string &positionName, const std::string &type,
                float x, float y, int channels) {
  Fixture fixture;
  fixture.uuid = uuid;
  fixture.position = positionUuid;
  fixture.positionName = positionName;
  fixture.typeName = type;
  fixture.gdtfMode = std::to_string(channels);
  fixture.transform.o[0] = x;
  fixture.transform.o[1] = y;
  scene.fixtures[uuid] = fixture;
}

// Parses an address into universe and channel values for assertions.
std::pair<int, int> Address(const Fixture &fixture) {
  const size_t separator = fixture.address.find('.');
  assert(separator != std::string::npos);
  return {std::stoi(fixture.address.substr(0, separator)),
          std::stoi(fixture.address.substr(separator + 1))};
}

// Captures all addresses by UUID independently of unordered insertion order.
std::map<std::string, std::string> Addresses(const MvrScene &scene) {
  std::map<std::string, std::string> result;
  for (const auto &[uuid, fixture] : scene.fixtures)
    result[uuid] = fixture.address;
  return result;
}

// Creates the representative pair of longitudinal side components.
void AddSides(MvrScene &scene, int channels = 10, float noise = 0.0f,
              bool reverseInsertion = false) {
  struct Entry {
    std::string uuid;
    float x;
    float y;
  };
  std::vector<Entry> entries = {
      {"left-front", -6500.0f, 0.0f},   {"left-mid", -6500.0f, 1000.0f},
      {"left-back", -6500.0f, 2000.0f}, {"right-front", 6500.0f, 0.0f},
      {"right-mid", 6500.0f, 1000.0f},  {"right-back", 6500.0f, 2000.0f}};
  if (reverseInsertion)
    std::reverse(entries.begin(), entries.end());
  for (size_t i = 0; i < entries.size(); ++i) {
    const float coordinateNoise = i % 2 == 0 ? noise : -noise;
    AddFixture(scene, entries[i].uuid, "side-position", "Arbitrary sides",
               "Wash", entries[i].x + coordinateNoise,
               entries[i].y + coordinateNoise, channels);
  }
}

// Verifies horizontal type grouping and left-to-right traversal.
void TestHorizontalTypeGrouping() {
  MvrScene scene;
  AddFixture(scene, "spot-left", "horizontal", "Bridge", "Spot", -2, 0, 1);
  AddFixture(scene, "wash-left", "horizontal", "Bridge", "Wash", -1, 0, 1);
  AddFixture(scene, "spot-right", "horizontal", "Bridge", "Spot", 2, 0, 1);
  AddFixture(scene, "wash-right", "horizontal", "Bridge", "Wash", 1, 0, 1);
  AutoPatcher::AutoPatch(scene);
  assert(scene.fixtures.at("spot-left").address == "1.1");
  assert(scene.fixtures.at("spot-right").address == "1.2");
  assert(scene.fixtures.at("wash-left").address == "1.3");
  assert(scene.fixtures.at("wash-right").address == "1.4");
}

// Verifies component detection, coherence, and serpentine traversal.
void TestLongitudinalComponentsAndDeterminism() {
  MvrScene scene;
  AddSides(scene);
  AutoPatcher::AutoPatch(scene);
  assert(scene.fixtures.at("left-front").address == "1.1");
  assert(scene.fixtures.at("left-mid").address == "1.11");
  assert(scene.fixtures.at("left-back").address == "1.21");
  assert(scene.fixtures.at("right-back").address == "1.31");
  assert(scene.fixtures.at("right-mid").address == "1.41");
  assert(scene.fixtures.at("right-front").address == "1.51");
  const auto firstPatch = Addresses(scene);
  AutoPatcher::AutoPatch(scene);
  assert(Addresses(scene) == firstPatch);

  MvrScene reverseScene;
  AddSides(reverseScene, 10, 0.0f, true);
  AutoPatcher::AutoPatch(reverseScene);
  assert(Addresses(reverseScene) == firstPatch);
}

// Verifies transverse Positions finish before a spanning longitudinal Position.
void TestLxPositionsBeforeSides() {
  MvrScene scene;
  for (int row = 0; row < 3; ++row) {
    AddFixture(scene, "lx-" + std::to_string(row),
               "lx-position-" + std::to_string(row), "Row", "Spot", 0,
               static_cast<float>(row * 1000), 400);
  }
  AddSides(scene, 60);
  AutoPatcher::AutoPatch(scene);
  assert(Address(scene.fixtures.at("lx-0")).first == 1);
  assert(Address(scene.fixtures.at("lx-1")).first == 2);
  assert(Address(scene.fixtures.at("lx-2")).first == 3);
  for (const std::string uuid : {"left-front", "left-mid", "left-back",
                                 "right-front", "right-mid", "right-back"})
    assert(Address(scene.fixtures.at(uuid)).first == 4);
}

// Verifies protection of a complete Position that fits one universe.
void TestCompletePositionProtection() {
  MvrScene scene;
  AddFixture(scene, "a", "position", "Position", "Spot", 0, 0, 90);
  AddFixture(scene, "b", "position", "Position", "Spot", 1, 0, 90);
  AutoPatcher::AutoPatch(scene, 1, 400);
  assert(scene.fixtures.at("a").address == "2.1");
  assert(scene.fixtures.at("b").address == "2.91");
}

// Verifies component protection when a Position exceeds one universe.
void TestLargePositionComponentProtection() {
  MvrScene scene;
  for (int side = 0; side < 2; ++side) {
    for (int row = 0; row < 2; ++row)
      AddFixture(
          scene,
          "component-" + std::to_string(side) + "-" + std::to_string(row),
          "large-position", "Large", "Wash", side == 0 ? -6000.0f : 6000.0f,
          static_cast<float>(row * 1000), 170);
  }
  AutoPatcher::AutoPatch(scene);
  assert(Address(scene.fixtures.at("component-0-0")).first == 1);
  assert(Address(scene.fixtures.at("component-0-1")).first == 1);
  assert(Address(scene.fixtures.at("component-1-0")).first == 2);
  assert(Address(scene.fixtures.at("component-1-1")).first == 2);
}

// Verifies unavoidable component splitting preserves fixture boundaries.
void TestUnavoidableSplit() {
  MvrScene scene;
  for (int i = 0; i < 6; ++i)
    AddFixture(scene, "large-" + std::to_string(i), "one-component", "Long",
               "Spot", 0, static_cast<float>(i * 1000), 100);
  AutoPatcher::AutoPatch(scene);
  for (const auto &[uuid, fixture] : scene.fixtures) {
    (void)uuid;
    const auto [universe, channel] = Address(fixture);
    assert(universe >= 1);
    assert(channel >= 1 && channel + 99 <= 512);
  }
  assert(Address(scene.fixtures.at("large-5")).first == 2);
}

// Verifies tiny imported coordinate noise does not alter topology or order.
void TestCoordinateNoise() {
  MvrScene clean;
  MvrScene noisy;
  AddSides(clean);
  AddSides(noisy, 10, 0.0002f);
  AutoPatcher::AutoPatch(clean);
  AutoPatcher::AutoPatch(noisy);
  assert(Addresses(clean) == Addresses(noisy));
}

// Verifies UUID identity takes precedence and normalized names remain safe.
void TestPositionIdentity() {
  MvrScene uuidScene;
  AddFixture(uuidScene, "uuid-a", "a-position", "Same", "Spot", 10, 0, 200);
  AddFixture(uuidScene, "uuid-b", "b-position", "Same", "Spot", -10, 0, 200);
  AutoPatcher::AutoPatch(uuidScene, 1, 200);
  assert(Address(uuidScene.fixtures.at("uuid-b")).first == 1);
  assert(Address(uuidScene.fixtures.at("uuid-a")).first == 2);

  MvrScene fallbackScene;
  AddFixture(fallbackScene, "name-right", "", " bridge  one ", "Spot", 10, 0,
             1);
  AddFixture(fallbackScene, "name-left", "", "BRIDGE ONE", "Spot", -10, 0, 1);
  AutoPatcher::AutoPatch(fallbackScene);
  assert(fallbackScene.fixtures.at("name-left").address == "1.1");
  assert(fallbackScene.fixtures.at("name-right").address == "1.2");

  MvrScene metadataFreeScene;
  AddFixture(metadataFreeScene, "free-right", "", "", "Spot", 10, 0, 1);
  AddFixture(metadataFreeScene, "free-left", "", "", "Spot", -10, 0, 1);
  AutoPatcher::AutoPatch(metadataFreeScene);
  assert(metadataFreeScene.fixtures.at("free-left").address == "1.1");
  assert(metadataFreeScene.fixtures.at("free-right").address == "1.2");
}

} // namespace

// Runs topology-aware global patching regression coverage.
int main() {
  TestHorizontalTypeGrouping();
  TestLongitudinalComponentsAndDeterminism();
  TestLxPositionsBeforeSides();
  TestCompletePositionProtection();
  TestLargePositionComponentProtection();
  TestUnavoidableSplit();
  TestCoordinateNoise();
  TestPositionIdentity();
  return 0;
}
