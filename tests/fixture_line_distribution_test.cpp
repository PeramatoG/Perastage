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
  scene.fixtures.emplace("a", MakeFixture("a", -3.0f, 0.02f, 0.0f));
  scene.fixtures.emplace("b", MakeFixture("b", 0.0f, 0.01f, 0.0f));
  scene.fixtures.emplace("c", MakeFixture("c", 3.0f, 0.0f, 0.0f));
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
      Expect(std::fabs(scene.fixtures.at("c").transform.o[0] + 2.25f) < 0.0001f,
             "selection order should be preserved");
  ok &=
      Expect(std::fabs(scene.fixtures.at("b").transform.o[0] - 2.25f) < 0.0001f,
             "equal end margins should be used");

  ok &= Expect(fixture_line_distribution::Apply(scene, selection,
                                                {-1.0f, 0.0f, 0.0f},
                                                {2.0f, 0.0f, 0.0f}, false),
               "endpoint distribution should apply");
  ok &= Expect(scene.fixtures.at("c").transform.o[0] == -1.0f &&
                   scene.fixtures.at("b").transform.o[0] == 2.0f,
               "first and last fixtures should occupy chosen endpoints");

  scene.fixtures.at("a").transform.o[1] = 0.5f;
  ok &= Expect(!fixture_line_distribution::ResolveSelectedLine(scene, selection)
                    .line.has_value(),
               "off-line fixtures should be rejected");
  return ok ? 0 : 1;
}
