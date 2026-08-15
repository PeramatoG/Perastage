#include "fixture_line_distribution.h"
#include "magnet_snap.h"

#include <algorithm>
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

// Creates one straight resolved attachment chord.
truss_attachment_paths::Path MakePath(const std::string &id,
                                      const std::string &owner, float startX,
                                      float endX, float y, float z) {
  truss_attachment_paths::Path path;
  path.stableId = owner + ":" + id;
  path.ownerTrussUuid = owner;
  path.worldPointsMm = {{startX, y, z}, {endX, y, z}};
  return path;
}

} // namespace

// Verifies distribution against the same resolved chord paths used by Magnet.
int main() {
  MvrScene scene;
  scene.fixtures.emplace("a", MakeFixture("a", -3000.0f, 150.0f, 150.0f));
  scene.fixtures.emplace("b", MakeFixture("b", 0.0f, 150.0f, 150.0f));
  scene.fixtures.emplace("c", MakeFixture("c", 3000.0f, 150.0f, 150.0f));
  const std::vector<std::string> selection{"c", "a", "b"};
  std::vector<truss_attachment_paths::Path> paths;
  for (float y : {-150.0f, 150.0f})
    for (float z : {-150.0f, 150.0f})
      paths.push_back(MakePath("chord", "truss", -4500.0f, 4500.0f, y, z));

  bool ok = true;
  const auto resolved =
      fixture_line_distribution::ResolveSelectedLine(scene, selection, paths);
  ok &= Expect(resolved.line.has_value(),
               "fixtures should resolve to their attachment chord");
  ok &= Expect(resolved.line && resolved.line->start[1] == 150.0f &&
                   resolved.line->start[2] == 150.0f,
               "resolved line should remain on the selected chord");
  ok &= Expect(fixture_line_distribution::Apply(scene, selection,
                                                resolved.line->start,
                                                resolved.line->end, true),
               "margin distribution should apply");
  ok &= Expect(scene.fixtures.at("c").transform.o[1] == 150.0f &&
                   scene.fixtures.at("c").transform.o[2] == 150.0f,
               "distribution should not move fixtures off the chord");

  scene.fixtures.at("a").transform.o = {-2000.0f, -150.0f, 150.0f};
  ok &= Expect(
      !fixture_line_distribution::ResolveSelectedLine(scene, selection, paths)
           .line.has_value(),
      "fixtures on different chords should be rejected");

  paths.push_back(
      MakePath("chord", "joined", 4500.0f, 13500.0f, 150.0f, 150.0f));
  scene.fixtures.at("c").transform.o = {10000.0f, 150.0f, 150.0f};
  scene.fixtures.at("a").transform.o = {-2000.0f, 150.0f, 150.0f};
  scene.fixtures.at("b").transform.o = {6000.0f, 150.0f, 150.0f};
  const auto bridge =
      fixture_line_distribution::ResolveSelectedLine(scene, selection, paths);
  ok &= Expect(bridge.line.has_value(),
               "matching chords across joined trusses should resolve");
  ok &=
      Expect(bridge.line && std::fabs(bridge.line->end[0] -
                                      bridge.line->start[0] - 18000.0f) < 0.01f,
             "joined chord should span the complete bridge");

  const auto magnetReferences =
      magnet_snap::BuildFixtureAttachmentPathReferences(paths);
  for (const auto &path : paths) {
    const auto match =
        std::find_if(magnetReferences.begin(), magnetReferences.end(),
                     [&](const auto &reference) {
                       return reference.attachmentPathId == path.stableId &&
                              reference.worldPointsMm == path.worldPointsMm;
                     });
    ok &= Expect(match != magnetReferences.end(),
                 "Magnet and distribution should receive identical paths");
  }
  return ok ? 0 : 1;
}
