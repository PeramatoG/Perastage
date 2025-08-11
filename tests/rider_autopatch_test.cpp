#include <cassert>
#include <unordered_set>
#include <cstdio>
#include <wx/init.h>

#include "../core/riderimporter.h"
#include "../core/configmanager.h"
#include "../models/fixture.h"

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
