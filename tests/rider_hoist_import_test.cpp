#include <cassert>
#include <cmath>
#include <map>
#include <string>
#include <wx/init.h>

#include "configmanager.h"
#include "riderimporter.h"

int main() {
  wxInitializer initializer;
  assert(initializer.IsOk());

  auto &cfg = ConfigManager::Get();
  cfg.Reset();

  const std::string riderText =
      "RIGGING Y ESTRUCTURAS\n"
      "4 MOTOR 2TO + GRILLETES + ESLINGAS PARA PA\n"
      "2 MOTOR 500Kg + GRILLETES + ESLINGAS PARA SIDE FILL\n"
      "3 TRUSS 40X40 14m PARA PUENTES LX\n"
      "6 MOTOR 500Kg + GRILLETES + ESLINGAS PARA PUENTES LX\n"
      "1 TRUSS 40X40 PRO 14m PARA PANTALLA\n"
      "4 MOTOR 1TO + GRILLETES + ESLINGAS PARA PANTALLA\n"
      "CABLEADO Y CONTROL DE MOTORES\n";

  assert(RiderImporter::ImportText(riderText));

  const auto &scene = cfg.GetScene();
  assert(scene.supports.size() == 16);

  std::map<std::string, int> countByPosition;
  std::map<std::string, int> countByCapacity;
  std::map<std::string, int> countByFunction;
  for (const auto &[uuid, support] : scene.supports) {
    (void)uuid;
    countByPosition[support.positionName]++;
    const int cap = static_cast<int>(support.capacityKg + 0.5f);
    countByCapacity[std::to_string(cap)]++;
    countByFunction[support.positionName + ":" + support.hoistFunction]++;
  }

  assert(countByPosition["P.A."] == 4);
  assert(countByPosition["SIDEFILL"] == 2);
  assert(countByPosition["LX1"] == 2);
  assert(countByPosition["LX2"] == 2);
  assert(countByPosition["LX3"] == 2);
  assert(countByPosition["SCREEN"] == 4);

  assert(countByCapacity["2000"] == 4);
  assert(countByCapacity["1000"] == 4);
  assert(countByCapacity["500"] == 8);
  assert(countByFunction["P.A.:Audio"] == 4);
  assert(countByFunction["SIDEFILL:Audio"] == 2);
  assert(countByFunction["SCREEN:Video"] == 4);
  assert(countByFunction["LX1:Lighting"] == 2);
  assert(countByFunction["LX2:Lighting"] == 2);
  assert(countByFunction["LX3:Lighting"] == 2);

  bool foundScreenTruss = false;
  for (const auto &[uuid, truss] : scene.trusses) {
    (void)uuid;
    if (truss.positionName != "SCREEN")
      continue;
    foundScreenTruss = true;
    assert(std::abs(truss.transform.o[1] - 3000.0f) < 0.001f);
    assert(std::abs(truss.transform.o[2] - 9000.0f) < 0.001f);
  }
  assert(foundScreenTruss);

  return 0;
}
