#include <cassert>
#include "../core/autopatcher.h"
#include "../models/mvrscene.h"
#include "../models/fixture.h"

// Stub GDTF channel count lookup
int GetGdtfModeChannelCount(const std::string&, const std::string&)
{
    return 1; // every fixture uses one channel
}

int main()
{
    MvrScene scene;

    Fixture a;
    a.uuid = "a";
    a.typeName = "Spot";
    a.transform.o[0] = 0.0f;
    a.transform.o[1] = 0.0f;
    scene.fixtures[a.uuid] = a;

    Fixture b;
    b.uuid = "b";
    b.typeName = "Wash";
    b.transform.o[0] = 1.0f;
    b.transform.o[1] = 0.0f;
    scene.fixtures[b.uuid] = b;

    Fixture c;
    c.uuid = "c";
    c.typeName = "Spot";
    c.transform.o[0] = 2.0f;
    c.transform.o[1] = 0.0f;
    scene.fixtures[c.uuid] = c;

    AutoPatcher::AutoPatch(scene);

    // Spot fixtures should be patched first and consecutively
    assert(scene.fixtures["a"].address == "1.1");
    assert(scene.fixtures["c"].address == "1.2");
    assert(scene.fixtures["b"].address == "1.3");

    return 0;
}
