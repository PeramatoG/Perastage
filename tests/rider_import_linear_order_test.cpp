#include <algorithm>
#include <cassert>
#include <cmath>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>
#include <wx/init.h>

#include "configmanager.h"
#include "fixture.h"
#include "riderimporter.h"

namespace {
constexpr float kPositionToleranceMillimeters = 1e-3f;
using FixtureSnapshot = std::tuple<std::string, std::string, float, float>;

// Captures fixtures in deterministic left-to-right semantic order.
std::vector<FixtureSnapshot> CaptureFixtureSnapshot() {
  std::vector<FixtureSnapshot> snapshot;
  for (const auto &[uuid, fixture] : ConfigManager::Get().GetScene().fixtures) {
    (void)uuid;
    snapshot.emplace_back(fixture.positionName, fixture.typeName,
                          fixture.transform.o[0], fixture.transform.o[1]);
  }
  std::sort(snapshot.begin(), snapshot.end(),
            [](const FixtureSnapshot &left, const FixtureSnapshot &right) {
              if (std::get<0>(left) != std::get<0>(right))
                return std::get<0>(left) < std::get<0>(right);
              return std::get<2>(left) < std::get<2>(right);
            });
  return snapshot;
}

// Imports Rider text with an explicit neutral LX1 position.
std::vector<FixtureSnapshot> ImportAndCapture(const std::string &riderText) {
  auto &cfg = ConfigManager::Get();
  cfg.Reset();
  cfg.SetFloat("rider_lx1_pos", 0.0f);
  assert(RiderImporter::ImportText(riderText));
  return CaptureFixtureSnapshot();
}
} // namespace

// Verifies repeated fixture listings retain source order independently of
// defaults.
int main(int argc, char **argv) {
  wxInitializer initializer;
  assert(initializer.IsOk());
  assert(argc >= 2);

  std::ifstream input(argv[1]);
  assert(input.is_open());
  std::ostringstream buffer;
  buffer << input.rdbuf();
  const std::string riderText = buffer.str();

  const std::vector<FixtureSnapshot> direct = ImportAndCapture(riderText);
  assert(direct.size() == 9);

  const std::vector<std::string> expectedTypes = {
      "MegaPointe", "Spiider", "Spiider", "MegaPointe",
      "MegaPointe", "Spiider", "Spiider", "MegaPointe"};
  for (size_t index = 0; index < expectedTypes.size(); ++index) {
    assert(std::get<0>(direct[index]) == "LX1");
    assert(std::get<1>(direct[index]) == expectedTypes[index]);
    assert(std::abs(std::get<3>(direct[index]) + 200.0f) <
           kPositionToleranceMillimeters);
  }
  assert(std::get<0>(direct.back()) == "LX2");
  assert(std::abs(std::get<2>(direct.back())) < kPositionToleranceMillimeters);

  const std::string filtered =
      RiderImporter::BuildFixtureFilterPreview(riderText);
  assert(!filtered.empty());
  const std::vector<FixtureSnapshot> filteredSnapshot =
      ImportAndCapture(filtered);
  assert(direct == filteredSnapshot);

  const std::vector<FixtureSnapshot> symmetric = ImportAndCapture("LX1\n"
                                                                  "2 SPOT\n"
                                                                  "2 WASH\n");
  assert(symmetric.size() == 4);
  std::map<std::string, std::vector<float>> positionsByType;
  for (const FixtureSnapshot &fixture : symmetric) {
    positionsByType[std::get<1>(fixture)].push_back(std::get<2>(fixture));
  }
  assert(positionsByType.size() == 2);
  for (const auto &[typeName, positions] : positionsByType) {
    (void)typeName;
    assert(positions.size() == 2);
    assert(std::abs(positions[0] + positions[1]) <
           kPositionToleranceMillimeters);
  }

  return 0;
}
