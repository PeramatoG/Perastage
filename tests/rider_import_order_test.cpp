#include <algorithm>
#include <cassert>
#include <vector>
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

    std::vector<const Fixture *> spots;
    std::vector<const Fixture *> washes;
    for (const auto &p : scene.fixtures) {
        const Fixture &f = p.second;
        if (f.typeName == "Spot")
            spots.push_back(&f);
        else if (f.typeName == "Wash")
            washes.push_back(&f);
    }
    assert(spots.size() == 2);
    assert(washes.size() == 4);

    std::sort(spots.begin(), spots.end(),
              [](const Fixture *a, const Fixture *b) {
                  return a->fixtureId < b->fixtureId;
              });
    std::sort(washes.begin(), washes.end(),
              [](const Fixture *a, const Fixture *b) {
                  return a->fixtureId < b->fixtureId;
              });

    assert(spots[0]->fixtureId == 101);
    assert(spots[1]->fixtureId == 102);
    assert(spots[0]->transform.o[1] < spots[1]->transform.o[1]);
    assert(spots[0]->instanceName == "Spot 1");
    assert(spots[1]->instanceName == "Spot 2");

    assert(washes[0]->fixtureId == 201);
    assert(washes[1]->fixtureId == 202);
    assert(washes[2]->fixtureId == 203);
    assert(washes[3]->fixtureId == 204);
    assert(washes[0]->transform.o[1] < washes[2]->transform.o[1]);
    assert(washes[0]->transform.o[0] < washes[1]->transform.o[0]);
    assert(washes[2]->transform.o[0] < washes[3]->transform.o[0]);
    assert(washes[0]->instanceName == "Wash 1");
    assert(washes[3]->instanceName == "Wash 4");

    return 0;
}
