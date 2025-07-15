#include "mvrscene.h"

void MvrScene::Clear() {
    fixtures.clear();
    trusses.clear();
    layers.clear();
    provider.clear();
    providerVersion.clear();
    versionMajor = 1;
    versionMinor = 6;
}
