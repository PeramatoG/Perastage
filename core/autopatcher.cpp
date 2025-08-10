#include "autopatcher.h"
#include "patchmanager.h"
#include "gdtfloader.h"
#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace AutoPatcher {

void AutoPatch(MvrScene &scene, int startUniverse, int startChannel) {
    struct FixtureInfo {
        std::string uuid;
        int channels;
        float x;
        float y;
    };

    std::vector<FixtureInfo> fixtures;
    fixtures.reserve(scene.fixtures.size());

    for (auto &pair : scene.fixtures) {
        auto &f = pair.second;
        std::string fullPath;
        if (!f.gdtfSpec.empty()) {
            fs::path p = scene.basePath.empty() ? fs::path(f.gdtfSpec)
                                               : fs::path(scene.basePath) / f.gdtfSpec;
            fullPath = p.string();
        }
        int chCount = GetGdtfModeChannelCount(fullPath, f.gdtfMode);
        if (chCount <= 0)
            continue; // skip fixtures without a valid channel count
        auto pos = f.GetPosition();
        fixtures.push_back({pair.first, chCount, pos[0], pos[1]});
    }

    std::sort(fixtures.begin(), fixtures.end(), [](const FixtureInfo &a, const FixtureInfo &b) {
        if (a.y == b.y)
            return a.x < b.x;
        return a.y < b.y;
    });

    std::vector<int> counts;
    counts.reserve(fixtures.size());
    for (const auto &f : fixtures)
        counts.push_back(f.channels);

    auto addresses = PatchManager::SequentialPatch(counts, startUniverse, startChannel);

    for (size_t i = 0; i < fixtures.size(); ++i) {
        const auto &addr = addresses[i];
        scene.fixtures[fixtures[i].uuid].address =
            std::to_string(addr.universe) + "." + std::to_string(addr.channel);
    }
}

} // namespace AutoPatcher

