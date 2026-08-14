#include "fixture_line_distribution.h"

#include <cmath>
#include <iostream>

namespace {

// Reports a failed test expectation.
bool Expect(bool condition, const char *message) {
  if (condition)
    return true;
  std::cerr << "FAILED: " << message << '\n';
  return false;
}

// Creates a fixture at the requested world position.
Fixture MakeFixture(const std::string &uuid, float x, float y, float z) {
  Fixture fixture;
  fixture.uuid = uuid;
  fixture.transform.o = {x, y, z};
  return fixture;
}

} // namespace

// Verifies line validation, margins, selection order, and endpoint placement.
int main() {
  MvrScene scene;
  Truss truss;
  truss.uuid = "truss";
  truss.lengthMm = 9000.0f;
  scene.trusses.emplace(truss.uuid, truss);
  scene.fixtures.emplace("a", MakeFixture("a", -3000.0f, 20.0f, 0.0f));
  scene.fixtures.emplace("b", MakeFixture("b", 0.0f, 10.0f, 0.0f));
  scene.fixtures.emplace("c", MakeFixture("c", 3000.0f, 0.0f, 0.0f));
  const std::vector<std::string> selection{"c", "a", "b"};

  bool ok = true;
  const auto resolved =
      fixture_line_distribution::ResolveSelectedLine(scene, selection);
  ok &= Expect(resolved.line.has_value(), "fixtures should resolve to truss");
  ok &= Expect(fixture_line_distribution::Apply(scene, selection,
                                                resolved.line->start,
                                                resolved.line->end, true),
               "margin distribution should apply");
  ok &=
      Expect(std::fabs(scene.fixtures.at("c").transform.o[0] + 2250.0f) < 0.01f,
             "selection order should be preserved");
  ok &=
      Expect(std::fabs(scene.fixtures.at("b").transform.o[0] - 2250.0f) < 0.01f,
             "equal end margins should be used");

  ok &= Expect(fixture_line_distribution::Apply(scene, selection,
                                                {-1000.0f, 0.0f, 0.0f},
                                                {2000.0f, 0.0f, 0.0f}, false),
               "endpoint distribution should apply");
  ok &= Expect(scene.fixtures.at("c").transform.o[0] == -1000.0f &&
                   scene.fixtures.at("b").transform.o[0] == 2000.0f,
               "first and last fixtures should occupy chosen endpoints");

  scene.fixtures.at("a").transform.o[1] = 500.0f;
  ok &= Expect(!fixture_line_distribution::ResolveSelectedLine(scene, selection)
                    .line.has_value(),
               "off-line fixtures should be rejected");

  Truss joined = truss;
  joined.uuid = "joined";
  joined.transform.o[0] = 9000.0f;
  scene.trusses.emplace(joined.uuid, joined);
  scene.fixtures.at("c").transform.o = {10000.0f, 25.0f, 0.0f};
  scene.fixtures.at("a").transform.o = {-2000.0f, 25.0f, 0.0f};
  scene.fixtures.at("b").transform.o = {6000.0f, 25.0f, 0.0f};
  const auto bridge =
      fixture_line_distribution::ResolveSelectedLine(scene, selection);
  ok &= Expect(bridge.line.has_value(),
               "fixtures across joined trusses should resolve");
  ok &=
      Expect(bridge.line && std::fabs(bridge.line->end[0] -
                                      bridge.line->start[0] - 18000.0f) < 0.01f,
             "joined truss line should span the complete bridge");
  return ok ? 0 : 1;
}
