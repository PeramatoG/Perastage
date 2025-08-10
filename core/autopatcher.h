#pragma once

#include "mvrscene.h"

namespace AutoPatcher {
// Automatically assign DMX addresses to fixtures in the scene.
// Fixtures are patched sequentially starting at the given universe and channel.
// The order is front-to-back (Y axis) then left-to-right (X axis).
void AutoPatch(MvrScene &scene, int startUniverse = 1, int startChannel = 1);
}
