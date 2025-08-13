#include <cassert>
#include <string>
#include <unordered_map>
#include <wx/init.h>

#include "../core/configmanager.h"
#include "../core/riderimporter.h"
#include "../models/fixture.h"
#include "../models/truss.h"

int main(int argc, char **argv) {
  wxInitializer initializer;
  assert(initializer.IsOk());
  assert(argc >= 2);

  auto &cfg = ConfigManager::Get();
  RiderImporter importer;

  // Layers by position
  cfg.Reset();
  cfg.SetValue("rider_layer_mode", "position");
  assert(importer.Import(argv[1]));
  const auto &scenePos = cfg.GetScene();
  for (const auto &p : scenePos.fixtures) {
    const Fixture &f = p.second;
    if (!f.positionName.empty())
      assert(f.layer == std::string("pos ") + f.positionName);
  }
  for (const auto &p : scenePos.trusses) {
    const Truss &t = p.second;
    if (!t.positionName.empty())
      assert(t.layer == std::string("pos ") + t.positionName);
  }

  // Layers by fixture type (trusses still by position)
  cfg.Reset();
  cfg.SetValue("rider_layer_mode", "type");
  assert(importer.Import(argv[1]));
  const auto &sceneType = cfg.GetScene();
  for (const auto &p : sceneType.fixtures) {
    const Fixture &f = p.second;
    assert(f.layer == std::string("fix ") + f.typeName);
  }
  for (const auto &p : sceneType.trusses) {
    const Truss &t = p.second;
    if (!t.positionName.empty())
      assert(t.layer == std::string("truss ") + t.positionName);
  }
  return 0;
}
