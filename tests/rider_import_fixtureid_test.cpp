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
#include <algorithm>
#include <vector>
#include <wx/init.h>

#include "riderimporter.h"
#include "configmanager.h"
#include "fixture.h"

int main(int argc, char **argv) {
    wxInitializer initializer;
    assert(initializer.IsOk());
    assert(argc >= 3);

    auto &cfg = ConfigManager::Get();
    cfg.Reset();
    RiderImporter importer;

    // First test: basic numbering with less than 100 fixtures per type
    assert(importer.Import(argv[1]));
    const auto &scene1 = cfg.GetScene();

    std::vector<int> spotIds;
    std::vector<int> washIds;
    for (const auto &p : scene1.fixtures) {
        const Fixture &f = p.second;
        if (f.typeName == "Spot")
            spotIds.push_back(f.fixtureId);
        else if (f.typeName == "Wash")
            washIds.push_back(f.fixtureId);
    }
    std::sort(spotIds.begin(), spotIds.end());
    std::sort(washIds.begin(), washIds.end());
    assert(spotIds.size() == 2);
    assert(washIds.size() == 3);
    assert(spotIds[0] == 101 && spotIds[1] == 102);
    assert(washIds[0] == 201 && washIds[2] == 203);

    // Second test: more than 100 fixtures of one type
    cfg.Reset();
    assert(importer.Import(argv[2]));
    const auto &scene2 = cfg.GetScene();

    int minSpot = 1000, maxSpot = 0, spotCount = 0;
    int minWash = 1000, maxWash = 0, washCount = 0;
    for (const auto &p : scene2.fixtures) {
        const Fixture &f = p.second;
        if (f.typeName == "Spot") {
            ++spotCount;
            minSpot = std::min(minSpot, f.fixtureId);
            maxSpot = std::max(maxSpot, f.fixtureId);
        } else if (f.typeName == "Wash") {
            ++washCount;
            minWash = std::min(minWash, f.fixtureId);
            maxWash = std::max(maxWash, f.fixtureId);
        }
    }
    assert(spotCount == 105);
    assert(minSpot == 101);
    assert(maxSpot == 205);
    assert(washCount == 5);
    assert(minWash == 301);
    assert(maxWash == 305);

    return 0;
}
