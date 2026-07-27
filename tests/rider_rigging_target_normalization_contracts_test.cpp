#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <set>
#include <string>
#include <tuple>
#include <vector>
#include <wx/init.h>

#include "configmanager.h"
#include "riderimporter.h"

namespace {

constexpr float kEpsilon = 0.001f;

using SupportSnapshot =
    std::multiset<std::tuple<std::string, int, std::string, std::string,
                             std::string>>;
using PipeSnapshot =
    std::multiset<std::tuple<std::string, std::string, std::string, int, int>>;

// Compares scene dimensions within the importer test tolerance.
bool NearlyEqual(float lhs, float rhs) {
  return std::abs(lhs - rhs) < kEpsilon;
}

// Imports Rider text and captures stable hoist semantics without UUIDs.
SupportSnapshot ImportSupports(const std::string &text) {
  auto &cfg = ConfigManager::Get();
  cfg.Reset();
  const bool imported = RiderImporter::ImportText(text);
  assert(imported || text.empty() || text == "RIGGING");

  SupportSnapshot snapshot;
  for (const auto &[uuid, support] : cfg.GetScene().supports) {
    (void)uuid;
    snapshot.emplace(support.positionName,
                     static_cast<int>(support.capacityKg + 0.5f),
                     support.hoistFunction, support.layer, support.name);
  }
  return snapshot;
}

// Verifies one hoist alias through direct and canonical filtered imports.
void VerifyHoistAlias(const std::string &alias, const std::string &target,
                      const std::string &function, const std::string &layer,
                      const std::set<std::string> &expectedNames = {}) {
  const std::string input = "RIGGING\n2 MOTOR 500Kg PARA " + alias + "\n";
  const std::string preview = RiderImporter::BuildFixtureFilterPreview(input);
  const SupportSnapshot direct = ImportSupports(input);
  const SupportSnapshot filtered = ImportSupports(preview);
  assert(direct == filtered);

  if (target == "FLOOR") {
    assert(direct.empty());
    return;
  }
  assert(direct.size() == 2);
  std::set<std::string> names;
  for (const auto &[position, capacity, hoistFunction, supportLayer, name] :
       direct) {
    assert(position == target);
    assert(capacity == 500);
    assert(hoistFunction == function);
    assert(supportLayer == layer);
    names.insert(name);
  }
  if (!expectedNames.empty())
    assert(names == expectedNames);
}

// Captures stable pipe object semantics and validates primitive dimensions.
PipeSnapshot ImportPipes(const std::string &text, float expectedLength) {
  auto &cfg = ConfigManager::Get();
  cfg.Reset();
  assert(RiderImporter::ImportText(text));
  const auto &scene = cfg.GetScene();
  assert(scene.trusses.empty());

  PipeSnapshot snapshot;
  for (const auto &[uuid, object] : scene.sceneObjects) {
    (void)uuid;
    assert(object.geometries.size() == 1);
    const auto &geometry = object.geometries.front();
    assert(geometry.modelFile == "primitive:cylinder");
    assert(NearlyEqual(geometry.localTransform.u[0], 0.05f));
    assert(NearlyEqual(geometry.localTransform.v[1], 0.05f));
    assert(NearlyEqual(geometry.localTransform.w[2], expectedLength));
    snapshot.emplace(object.name, object.layer, geometry.modelFile,
                     static_cast<int>(geometry.localTransform.w[2] * 1000.0f),
                     static_cast<int>(geometry.localTransform.u[0] * 1000.0f));
  }
  return snapshot;
}

// Verifies direct/filtered pipe parity and canonical preview serialization.
void VerifyPipeCase(const std::string &input, const std::string &expectedPreview,
                    float expectedLength, const PipeSnapshot &expected) {
  const std::string preview = RiderImporter::BuildFixtureFilterPreview(input);
  if (preview != expectedPreview) {
    std::cerr << "Unexpected Rider rigging preview.\nExpected:\n"
              << expectedPreview << "\nActual:\n" << preview << '\n';
    assert(false);
  }
  assert(RiderImporter::BuildFixtureFilterPreview(preview) == preview);
  assert(preview.find('\r') == std::string::npos);
  assert(preview.find("\n\n") == std::string::npos);
  assert(preview.empty() || preview.back() != '\n');
  assert(ImportPipes(input, expectedLength) == expected);
  assert(ImportPipes(preview, expectedLength) == expected);
}

// Runs the complete Phase 5B hoist alias contract matrix.
void VerifyHoistAliases() {
  const std::set<std::string> sidefillNames = {"SF L", "SF R"};
  for (const std::string &alias : {"SIDE FILL", "SIDEFILL", "side   fill"})
    VerifyHoistAlias(alias, "SIDEFILL", "Audio", "rig Audio",
                     sidefillNames);
  for (const std::string &alias :
       {"SIDE", "SIDES", "LX SIDE", "LX SIDES", "CALLE", "CALLES"})
    VerifyHoistAlias(alias, "LX SIDES", "Lighting", "rig Lighting");
  VerifyHoistAlias("CALLES A SUELO", "FLOOR", "Lighting", "rig Lighting");
  VerifyHoistAlias("PA", "P.A.", "Audio", "rig Audio");
  VerifyHoistAlias("SCREEN", "SCREEN", "Video", "rig Video");
  VerifyHoistAlias("LX1", "LX1", "Lighting", "rig Lighting");
}

// Runs the complete Phase 5B pipe target expansion contract matrix.
void VerifyPipeTargets() {
  const auto pipe = [](const std::string &target, int lengthMm) {
    return std::tuple{"PIPE " + target, "pos " + target,
                      std::string("primitive:cylinder"), lengthMm, 50};
  };
  VerifyPipeCase("RIGGING\n3 VARAS PARA PUENTES LX\n",
                 "RIGGING\n1 PIPE 14m LX1\n1 PIPE 14m LX2\n1 PIPE 14m LX3",
                 14.0f, {pipe("LX1", 14000), pipe("LX2", 14000),
                         pipe("LX3", 14000)});
  VerifyPipeCase("RIGGING\n3 PIPE 10m PARA PUENTES LX\n",
                 "RIGGING\n1 PIPE 10m LX1\n1 PIPE 10m LX2\n1 PIPE 10m LX3",
                 10.0f, {pipe("LX1", 10000), pipe("LX2", 10000),
                         pipe("LX3", 10000)});
  VerifyPipeCase("RIGGING\n3 PIPE PARA LX1\n",
                 "RIGGING\n1 PIPE 14m LX1\n1 PIPE 14m LX1\n1 PIPE 14m LX1",
                 14.0f, {pipe("LX1", 14000), pipe("LX1", 14000),
                         pipe("LX1", 14000)});
  VerifyPipeCase(
      "RIGGING\n2 PIPE PARA SCREEN\n",
      "RIGGING\n1 PIPE 14m SCREEN\n1 PIPE 14m SCREEN", 14.0f,
      {pipe("SCREEN", 14000), pipe("SCREEN", 14000)});
}

} // namespace

// Verifies isolated Rider rigging target normalization contracts.
int main() {
  wxInitializer initializer;
  assert(initializer.IsOk());
  VerifyHoistAliases();
  VerifyPipeTargets();
  return 0;
}
