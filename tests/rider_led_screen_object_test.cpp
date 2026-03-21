#include <cassert>
#include <cmath>
#include <string>
#include <wx/init.h>

#include "configmanager.h"
#include "riderimporter.h"
#include "sceneobject.h"

int main() {
  wxInitializer initializer;
  assert(initializer.IsOk());

  auto &cfg = ConfigManager::Get();
  cfg.Reset();

  const std::string riderText =
      "ILUMINACION\n"
      "SCREEN\n"
      "1 PANTALLA LED 8X5m 1664X1040 PIXELS Y 1100Kg PARA TRASERA\n"
      "RIGGING\n"
      "1 TRUSS 40X40 14m PARA PANTALLA\n";
  assert(RiderImporter::ImportText(riderText));

  const auto &scene = cfg.GetScene();
  assert(scene.fixtures.empty());
  assert(scene.sceneObjects.size() == 1);

  const SceneObject &screen = scene.sceneObjects.begin()->second;
  constexpr float kTolerance = 1e-3f;
  assert(std::fabs(screen.transform.u[0] - (8.0f / 0.3f)) < kTolerance);
  assert(std::fabs(screen.transform.v[1] - (0.1f / 0.3f)) < kTolerance);
  assert(std::fabs(screen.transform.w[2] - (5.0f / 0.3f)) < kTolerance);

  assert(screen.transform.o[0] > -kTolerance);
  assert(screen.transform.o[0] < kTolerance);
  assert(std::fabs(screen.transform.o[2] - 6800.0f) < 1.0f);

  return 0;
}
