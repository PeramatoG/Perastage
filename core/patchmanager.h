#pragma once
#include <vector>

namespace PatchManager {
    struct PatchAddress {
        int universe;
        int channel;
    };

    // Compute DMX addresses for a list of fixtures given their channel counts.
    // Each fixture will be patched sequentially starting at the provided
    // universe and channel. Universes wrap when the channel range would exceed
    // 512.
    std::vector<PatchAddress> SequentialPatch(const std::vector<int>& channelCounts,
                                             int startUniverse, int startChannel);
}
