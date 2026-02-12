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
#include <cassert>
#include <string>

// Stub GDTF channel count lookup
int GetGdtfModeChannelCount(const std::string &, const std::string &mode) {
  return mode.empty() ? 1 : std::stoi(mode);
}

int main() {
  MvrScene scene;

  Fixture a;
  a.uuid = "a";
  a.typeName = "Wash";
  a.gdtfMode = "120"; // uses 120 channels
  a.positionName = "Front";
  a.transform.o[0] = 0.0f;
  a.transform.o[1] = 0.0f;
  scene.fixtures[a.uuid] = a;

  Fixture b;
  b.uuid = "b";
  b.typeName = "Wash";
  b.gdtfMode = "120"; // uses 120 channels
  b.positionName = "Front";
  b.transform.o[0] = 1.0f;
  b.transform.o[1] = 0.0f;
  scene.fixtures[b.uuid] = b;

  // Start near the end of the universe. The block-level check should move the
  // entire group to the next universe to keep addresses contiguous.
  AutoPatcher::AutoPatch(scene, 1, 470);

  assert(scene.fixtures["a"].address == "2.1");
  assert(scene.fixtures["b"].address == "2.121");

  return 0;
}
