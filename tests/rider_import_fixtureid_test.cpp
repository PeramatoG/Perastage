#include <cassert>
#include <algorithm>
#include <vector>
#include <wx/init.h>

#include "../core/riderimporter.h"
#include "../core/configmanager.h"
#include "../models/fixture.h"

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
    assert(spotIds[0] == 100 && spotIds[1] == 101);
    assert(washIds[0] == 200 && washIds[2] == 202);

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
    assert(minSpot == 100);
    assert(maxSpot == 204);
    assert(washCount == 5);
    assert(minWash == 300);
    assert(maxWash == 304);

    return 0;
}
