/*
 * This file is part of Perastage.
 * Copyright (C) 2026 Luisma Peramato
 * License: GNU General Public License version 3 or later
 */
#include "position_selection.h"
#include "matrixutils.h"
#include "position_insertion_overlay.h"

#include <cassert>
#include <cmath>
#include <limits>

using namespace perastage::command;
using Units::DistanceUnitSystem;
namespace {
constexpr auto metric = DistanceUnitSystem::Metric;
constexpr auto imperial = DistanceUnitSystem::Imperial;

class Host final : public ProjectMutationHost {
public:
  MutationPublication CommitMutation(const MvrScene &scene,
      const scene_grouping::ObjectSelection &selection,
      const std::string &label) override {
    before = scene;
    selectedBefore = selection;
    assert(label == "Position Tool");
    ++count;
    return {true, true};
  }
  MvrScene before;
  scene_grouping::ObjectSelection selectedBefore;
  int count = 0;
};

Matrix At(float x, float y, float z) {
  Matrix m = MatrixUtils::EulerToMatrix(15, 30, 45);
  m.o = {x, y, z};
  return m;
}

bool Near(double a, double b) { return std::fabs(a - b) < 0.001; }

void AbsoluteAndMixed() {
  MvrScene scene;
  Fixture a;
  a.uuid = "a";
  a.transform = At(-6605.125f, 9742.125f, 0);
  for (int axis = 0; axis < 3; ++axis) {
    a.transform.u[axis] *= 2.0f;
    a.transform.v[axis] *= 3.0f;
    a.transform.w[axis] *= 0.5f;
  }
  scene.fixtures[a.uuid] = a;
  Fixture b = a;
  b.uuid = "b";
  b.transform.o[0] = -9493.125f;
  scene.fixtures[b.uuid] = b;
  scene_grouping::ObjectSelection selection{{a.uuid}};
  Host host;
  ExecutionContext context{scene, selection, host};
  const scene_grouping::InteractiveTransformPolicy policy;
  auto read = [&] { return position_selection::Read(scene, selection, policy); };
  assert(read().commonMm[0] == a.transform.o[0]);
  assert(position_selection::FormatCoordinate(*read().commonMm[0], metric) == "-6.605");
  assert(scene.fixtures.at("a").transform.o[0] == -6605.125f);
  for (int axis = 0; axis < 3; ++axis) {
    const Matrix before = scene.fixtures.at("a").transform;
    const auto result = position_selection::SetAxis(context, policy, axis, "5", metric);
    assert(result.Success() && result.mutation.undoEntryRecorded && result.mutation.projectDirty);
    const Matrix after = scene.fixtures.at("a").transform;
    assert(after.o[axis] == 5000);
    for (int other = 0; other < 3; ++other)
      if (other != axis) assert(after.o[other] == before.o[other]);
    assert(after.u == before.u && after.v == before.v && after.w == before.w);
  }
  selection.fixtures = {"a", "b", "a"};
  auto snapshot = read();
  assert(snapshot.targets.size() == 2 && !snapshot.commonMm[0]);
  auto result = position_selection::SetAxis(context, policy, 0, "0", metric);
  assert(result.Success() && result.mutation.sceneChanged);
  assert(scene.fixtures.at("a").transform.o[0] == 0);
  assert(scene.fixtures.at("b").transform.o[0] == 0);
  assert(scene.fixtures.at("a").transform.o[1] == 5000);
  assert(scene.fixtures.at("b").transform.o[1] == b.transform.o[1]);
  assert(read().commonMm[0] == 0 && !read().commonMm[1]);
  const int committed = host.count;
  for (const auto &invalid : {"", " ", "abc", "nan", "inf", "1e300", "5 junk"}) {
    result = position_selection::SetAxis(context, policy, 0, invalid, metric);
    assert(!result.Success() && !result.mutation.sceneChanged);
  }
  assert(!position_selection::SetAxis(context, policy, 3, "1", metric).Success());
  result = position_selection::SetAxis(context, policy, 0, "0", metric);
  assert(result.Success() && !result.mutation.sceneChanged && host.count == committed);
  // Identical displayed strings must not conceal different internal origins.
  scene.fixtures.at("a").transform.o[0] = 0.125f;
  scene.fixtures.at("b").transform.o[0] = 0.126f;
  assert(position_selection::FormatCoordinate(0.125, metric) ==
         position_selection::FormatCoordinate(0.126, metric));
  assert(!read().commonMm[0]);
  scene.fixtures.at("b").transform.o = scene.fixtures.at("a").transform.o;
  for (const auto &axis : read().commonMm) assert(axis.has_value());
  scene.fixtures.erase("a");
  assert(read().targets.size() == 1);
  selection = {};
  assert(read().targets.empty());
}

void CategoriesAndUnits() {
  assert(Near(*position_selection::ParseCoordinate("3' 6\"", imperial), 1066.8));
  assert(Near(*position_selection::ParseCoordinate("-2 ft", metric), -609.6));
  assert(position_selection::FormatCoordinate(3048, imperial) == "10.000");
  MvrScene scene;
  scene.trusses["t"].transform = At(11, 22, 33);
  scene.supports["s"].transform = At(11, 22, 33);
  scene.sceneObjects["o"].transform = At(11, 22, 33);
  scene_grouping::ObjectSelection selection{{}, {"t"}, {"s"}, {"o"}};
  Host host;
  ExecutionContext context{scene, selection, host};
  const scene_grouping::InteractiveTransformPolicy policy;
  const auto snapshot = position_selection::Read(scene, selection, policy);
  assert(snapshot.commonMm[0] == 11 && snapshot.targets.size() == 3);
  assert(position_selection::SetAxis(context, policy, 2, "1", imperial).Success());
  assert(Near(scene.trusses.at("t").transform.o[2], 304.8));
  assert(scene.trusses.at("t").transform.o == scene.supports.at("s").transform.o);
  assert(scene.supports.at("s").transform.o == scene.sceneObjects.at("o").transform.o);
  assert(host.count == 1);
}

void GroupsAndProjection() {
  MvrScene scene;
  Fixture a;
  a.uuid = "a"; a.transform = At(100, 200, 300);
  scene.fixtures[a.uuid] = a;
  Truss t;
  t.uuid = "t"; t.transform = At(400, 500, 600);
  scene.trusses[t.uuid] = t;
  Support support;
  support.uuid = "support"; support.transform = At(700, 800, 900);
  scene.supports[support.uuid] = support;
  const auto group = scene_grouping::GroupSelection(scene, {{"a"}, {"t"}, {"support"}});
  scene.sceneObjects["independent"].transform = At(-100, -200, -300);
  const auto root = scene_grouping::GroupSelection(
      scene, {{}, {}, {}, {"independent"}, {group.groupUuid}});
  assert(root.changed);
  scene_grouping::ObjectSelection selection{{"a"}, {"t"}, {}, {}, {root.groupUuid, group.groupUuid}};
  const scene_grouping::InteractiveTransformPolicy exact{false, false, false, false};
  auto snapshot = position_selection::Read(scene, selection, exact);
  assert(snapshot.targets.size() == 1 && snapshot.targets.front().uuid == root.groupUuid);
  const Matrix leafLocal = scene.fixtures.at("a").localTransform;
  const Matrix nestedLocal = scene.groupObjects.at(group.groupUuid).localTransform;
  Host host;
  ExecutionContext context{scene, selection, host};
  const auto result = position_selection::SetAxis(context, exact, 0, "5", metric);
  assert(result.Success() && host.count == 1);
  const auto moved = position_selection::Read(scene, selection, exact);
  assert(moved.commonMm[0] == 5000);
  assert(scene.fixtures.at("a").localTransform.o == leafLocal.o);
  assert(scene.groupObjects.at(group.groupUuid).localTransform.o == nestedLocal.o);
  const auto deltaA = scene.fixtures.at("a").transform.o[0] - a.transform.o[0];
  const auto deltaT = scene.trusses.at("t").transform.o[0] - t.transform.o[0];
  assert(Near(deltaA, deltaT));
  selection = {{"a"}};
  assert(position_selection::Read(scene, selection, exact).targets.front().uuid == "a");
  auto promoted = exact;
  promoted.promoteFixturesToGroup = true;
  assert(position_selection::Read(scene, selection, promoted).targets.front().uuid == root.groupUuid);
  selection = {{}, {"t"}};
  assert(position_selection::Read(scene, selection, {}).targets.front().uuid == root.groupUuid);
  selection = {{}, {}, {"support"}};
  assert(position_selection::Read(scene, selection, exact).targets.front().uuid == "support");
  auto supportPolicy = exact;
  supportPolicy.promoteSupportsToGroup = true;
  assert(position_selection::Read(scene, selection, supportPolicy).targets.front().uuid == root.groupUuid);
  selection = {{}, {}, {}, {"independent"}};
  assert(position_selection::Read(scene, selection, exact).targets.front().uuid == "independent");
  auto objectPolicy = exact;
  objectPolicy.promoteSceneObjectsToGroup = true;
  assert(position_selection::Read(scene, selection, objectPolicy).targets.front().uuid == root.groupUuid);
  // A directly selected nested group stays exact, regardless of promotion preferences.
  selection = {{}, {}, {}, {}, {group.groupUuid}};
  snapshot = position_selection::Read(scene, selection, promoted);
  assert(snapshot.targets.front().uuid == group.groupUuid);
  assert(!position_selection::SameTargets(snapshot, moved));
  for (auto view : {Viewer2DView::Top, Viewer2DView::Bottom,
                    Viewer2DView::Front, Viewer2DView::Side}) {
    for (float zoom : {0.25f, 1.0f, 10.0f}) {
      viewer2d::CoordinateTransform projection{800, 600, zoom, 10, -20, view};
      const auto markers = viewer2d::BuildInsertionPointMarkers(snapshot, projection);
      assert(markers.size() == 1);
      std::array<float, 3> origin;
      for (int axis = 0; axis < 3; ++axis)
        origin[axis] = static_cast<float>(Units::DistanceMillimetersToDisplay(
            snapshot.originsMm.front()[axis], metric));
      const auto expected = viewer2d::WorldToFramebuffer(origin, projection);
      assert(expected && Near(markers.front().x, (*expected)[0]) && Near(markers.front().y, (*expected)[1]));
    }
  }
  position_selection::Snapshot origin;
  origin.originsMm = {{1000, 2000, 3000}};
  const std::array<std::array<float, 2>, 4> screen{{
      {425, 250}, {375, 250}, {425, 225}, {350, 225}}};
  int orientation = 0;
  for (auto view : {Viewer2DView::Top, Viewer2DView::Bottom,
                    Viewer2DView::Front, Viewer2DView::Side}) {
    const auto markers = viewer2d::BuildInsertionPointMarkers(
        origin, {800, 600, 1, 0, 0, view});
    assert(markers.size() == 1);
    assert(Near(markers[0].x, screen[orientation][0]));
    assert(Near(markers[0].y, screen[orientation][1]));
    ++orientation;
  }
  assert(viewer2d::ShowInsertionPointOverlay(true, true, false, false));
  assert(!viewer2d::ShowInsertionPointOverlay(false, true, false, false));
  assert(!viewer2d::ShowInsertionPointOverlay(true, false, false, false));
  assert(!viewer2d::ShowInsertionPointOverlay(true, true, true, false));
  assert(!viewer2d::ShowInsertionPointOverlay(true, true, false, true));
  assert(viewer2d::BuildInsertionPointMarkers({}, {800, 600}).empty());
}
} // namespace

int main() {
  AbsoluteAndMixed();
  CategoriesAndUnits();
  GroupsAndProjection();
}
