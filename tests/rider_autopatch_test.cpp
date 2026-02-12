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
#include <unordered_set>
#include <cstdio>
#include <wx/init.h>

#include "riderimporter.h"
#include "configmanager.h"
#include "fixture.h"

int main(int argc, char **argv) {
    wxInitializer initializer;
    assert(initializer.IsOk());
    assert(argc >= 2);

    auto &cfg = ConfigManager::Get();
    cfg.Reset();
    RiderImporter importer;
    assert(importer.Import(argv[1]));

    const auto &scene = cfg.GetScene();
    std::unordered_set<int> channels;
    for (const auto &p : scene.fixtures) {
        const Fixture &f = p.second;
        assert(!f.address.empty());
        int universe = 0, channel = 0;
        std::sscanf(f.address.c_str(), "%d.%d", &universe, &channel);
        assert(universe == 1);
        channels.insert(channel);
    }
    assert(channels.size() == scene.fixtures.size());
    return 0;
}
