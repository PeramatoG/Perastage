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
#include "../core/autopatcher.h"
#include "../models/fixture.h"
#include "../models/mvrscene.h"
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
  a.typeName = "Spot";
  a.gdtfMode = "400"; // uses 400 channels
  a.positionName = "Front";
  a.transform.o[0] = 0.0f;
  a.transform.o[1] = 0.0f;
  scene.fixtures[a.uuid] = a;

  Fixture b1;
  b1.uuid = "b1";
  b1.typeName = "Wash";
  b1.gdtfMode = "100"; // uses 100 channels
  b1.positionName = "Front";
  b1.transform.o[0] = 1.0f;
  b1.transform.o[1] = 0.0f;
  scene.fixtures[b1.uuid] = b1;

  Fixture b2;
  b2.uuid = "b2";
  b2.typeName = "Wash";
  b2.gdtfMode = "100"; // uses 100 channels
  b2.positionName = "Front";
  b2.transform.o[0] = 2.0f;
  b2.transform.o[1] = 0.0f;
  scene.fixtures[b2.uuid] = b2;

  AutoPatcher::AutoPatch(scene);

  // Fixture a occupies channels 1-400 in universe 1. The Wash fixtures should
  // start in a new universe together instead of being split across universes.
  assert(scene.fixtures["a"].address == "1.1");
  assert(scene.fixtures["b1"].address == "2.1");
  assert(scene.fixtures["b2"].address == "2.101");

  return 0;
}
