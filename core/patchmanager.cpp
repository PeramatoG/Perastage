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

        if (ch + count - 1 > 512) {
            ++uni;
            ch = 1;
        }

        result.push_back({uni, ch});

        ch += count;
        if (ch > 512) {
            ++uni;
            ch = 1;
        }
    }

    return result;
}

} // namespace PatchManager
