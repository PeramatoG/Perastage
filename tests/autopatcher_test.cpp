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
  a.typeName = "Spot";
  a.transform.o[0] = 0.0f;
  a.transform.o[1] = 0.0f;
  scene.fixtures[a.uuid] = a;

  Fixture b;
  b.uuid = "b";
  b.typeName = "Wash";
  b.transform.o[0] = 1.0f;
  b.transform.o[1] = 0.0f;
  scene.fixtures[b.uuid] = b;

  Fixture c;
  c.uuid = "c";
  c.typeName = "Spot";
  c.transform.o[0] = 2.0f;
  c.transform.o[1] = 0.0f;
  scene.fixtures[c.uuid] = c;

  AutoPatcher::AutoPatch(scene);

  // Spot fixtures should be patched first and consecutively
  assert(scene.fixtures["a"].address == "1.1");
  assert(scene.fixtures["c"].address == "1.2");
  assert(scene.fixtures["b"].address == "1.3");

  return 0;
}
