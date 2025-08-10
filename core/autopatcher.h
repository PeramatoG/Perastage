#pragma once

#include "mvrscene.h"

namespace AutoPatcher {
// Automatically assign DMX addresses to fixtures in the scene.
// Fixtures are patched sequentially starting at the given universe and channel.
// The order is front-to-back (Y axis), prioritizing fixtures by type within each
// hang position so identical types are consecutive, and finally left-to-right (X axis).
void AutoPatch(MvrScene &scene, int startUniverse = 1, int startChannel = 1);
}
