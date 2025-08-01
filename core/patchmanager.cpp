#include "patchmanager.h"

namespace PatchManager {

std::vector<PatchAddress> SequentialPatch(const std::vector<int>& channelCounts,
                                          int startUniverse, int startChannel)
{
    std::vector<PatchAddress> result;
    result.reserve(channelCounts.size());

    int uni = startUniverse < 1 ? 1 : startUniverse;
    int ch  = startChannel < 1 ? 1 : startChannel;

    for (int count : channelCounts) {
        if (count < 1)
            count = 1;

        if (ch + count - 1 > 255) {
            ++uni;
            ch = 1;
        }

        result.push_back({uni, ch});

        ch += count;
        if (ch > 255) {
            ++uni;
            ch = 1;
        }
    }

    return result;
}

} // namespace PatchManager
