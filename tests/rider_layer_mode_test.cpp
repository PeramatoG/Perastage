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
#include <cassert>
#include <string>
#include <unordered_map>
#include <wx/init.h>

#include "configmanager.h"
#include "riderimporter.h"
#include "fixture.h"
#include "truss.h"

int main(int argc, char **argv) {
  wxInitializer initializer;
  assert(initializer.IsOk());
  assert(argc >= 2);

  auto &cfg = ConfigManager::Get();
  RiderImporter importer;

  // Layers by position
  cfg.Reset();
  cfg.SetValue("rider_layer_mode", "position");
  assert(importer.Import(argv[1]));
  const auto &scenePos = cfg.GetScene();
  for (const auto &p : scenePos.fixtures) {
    const Fixture &f = p.second;
    if (!f.positionName.empty())
      assert(f.layer == std::string("pos ") + f.positionName);
  }
  for (const auto &p : scenePos.trusses) {
    const Truss &t = p.second;
    if (!t.positionName.empty())
      assert(t.layer == std::string("pos ") + t.positionName);
  }

  // Layers by fixture type (trusses still by position)
  cfg.Reset();
  cfg.SetValue("rider_layer_mode", "type");
  assert(importer.Import(argv[1]));
  const auto &sceneType = cfg.GetScene();
  for (const auto &p : sceneType.fixtures) {
    const Fixture &f = p.second;
    assert(f.layer == std::string("fix ") + f.typeName);
  }
  for (const auto &p : sceneType.trusses) {
    const Truss &t = p.second;
    if (!t.positionName.empty())
      assert(t.layer == std::string("truss ") + t.positionName);
  }
  return 0;
}
