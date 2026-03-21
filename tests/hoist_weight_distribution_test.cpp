#include <cassert>
#include <cmath>
#include <string>
#include <vector>

#include "hoist_weight_distribution.h"

namespace {

Fixture MakeFixture(const std::string &position, float weightKg) {
  Fixture fixture;
  fixture.uuid = position + "_fixture";
  fixture.positionName = position;
  fixture.weightKg = weightKg;
  return fixture;
}

Truss MakeTruss(const std::string &position, float weightKg) {
  Truss truss;
  truss.uuid = position + "_truss";
  truss.positionName = position;
  truss.weightKg = weightKg;
  return truss;
}

Support MakeHoist(const std::string &uuid, const std::string &position,
                  float x, float y, float z, float loadKg = 0.0f) {
  Support support;
  support.uuid = uuid;
  support.positionName = position;
  support.position = position;
  support.transform.o[0] = x;
  support.transform.o[1] = y;
  support.transform.o[2] = z;
  support.loadKg = loadKg;
  return support;
}

bool NearlyEquals(float a, float b) {
  return std::fabs(a - b) < 0.001f;
}

} // namespace

int main() {
  {
    MvrScene scene;
    scene.fixtures["f1"] = MakeFixture("LX1", 100.0f);
    scene.trusses["t1"] = MakeTruss("LX1", 50.0f);
    scene.supports["h1"] = MakeHoist("h1", "LX1", -1000.0f, 0.0f, 0.0f);
    scene.supports["h2"] = MakeHoist("h2", "LX1", 1000.0f, 200.0f, 0.0f);

    const auto roundedTotals =
        HoistWeightDistribution::BuildRoundedRiggingTotalByHangPosition(scene);
    HoistWeightDistribution::ApplyForImportedSupports(scene, {"h1", "h2"},
                                                      roundedTotals);

    assert(NearlyEquals(scene.supports["h1"].loadKg, 80.0f));
    assert(NearlyEquals(scene.supports["h2"].loadKg, 80.0f));
  }

  {
    MvrScene scene;
    scene.fixtures["f1"] = MakeFixture("LX2", 200.0f);
    scene.supports["h1"] = MakeHoist("h1", "LX2", -1000.0f, 0.0f, 0.0f);
    scene.supports["h2"] = MakeHoist("h2", "LX2", 0.0f, 0.0f, 0.0f);
    scene.supports["h3"] = MakeHoist("h3", "LX2", 1000.0f, 0.0f, 0.0f);

    const auto roundedTotals =
        HoistWeightDistribution::BuildRoundedRiggingTotalByHangPosition(scene);
    HoistWeightDistribution::ApplyForImportedSupports(scene, {"h1", "h2", "h3"},
                                                      roundedTotals);

    assert(NearlyEquals(scene.supports["h1"].loadKg, 40.0f));
    assert(NearlyEquals(scene.supports["h2"].loadKg, 135.0f));
    assert(NearlyEquals(scene.supports["h3"].loadKg, 40.0f));
  }

  {
    MvrScene scene;
    scene.fixtures["f1"] = MakeFixture("LX3", 0.0f);
    scene.trusses["t1"] = MakeTruss("LX3", 120.0f);
    scene.supports["h1"] = MakeHoist("h1", "LX3", 0.0f, 0.0f, 0.0f, 30.0f);

    const auto missingMap =
        HoistWeightDistribution::BuildMissingWeightMapByHangPosition(scene);
    auto it = missingMap.find("LX3");
    assert(it != missingMap.end());
    assert(it->second);
  }

  return 0;
}
