#include "continuous_placement_scene.h"
#include "continuous_placement_state.h"
#include "mvrscene.h"

#include <cassert>
#include <cstdint>
#include <limits>

// Verifies cloning and removal for every supported continuous placement type.
int main() {
  // A camera revision must force absolute re-alignment without retaining a
  // temporary snap preview or adding a stale pointer delta.
  continuous_placement::ViewRevisionState viewState;
  assert(viewState.NeedsAlignment());
  viewState.CompleteAlignmentAttempt(false);
  assert(viewState.NeedsAlignment());
  float rawPosition = 4.0f;
  float displayedPosition = rawPosition + 1.5f;
  viewState.CompleteAlignmentAttempt(true);
  assert(!viewState.NeedsAlignment());
  viewState.Invalidate();
  viewState.Invalidate();
  assert(viewState.NeedsAlignment());
  viewState.CompleteAlignmentAttempt(false);
  assert(viewState.NeedsAlignment());
  viewState.CompleteAlignmentAttempt(true);
  assert(!viewState.NeedsAlignment());
  for (float pointerWorld : {8.0f, 6.0f, 9.5f}) {
    viewState.Invalidate();
    assert(viewState.NeedsAlignment());
    displayedPosition = rawPosition;
    const auto delta = continuous_placement::AbsoluteAlignmentDelta(
        {pointerWorld, 0.0f, 0.0f}, {rawPosition, 0.0f, 0.0f});
    rawPosition += delta[0];
    displayedPosition = rawPosition + 1.5f;
    viewState.MarkAligned();
    assert(!viewState.NeedsAlignment());
    assert(displayedPosition == pointerWorld + 1.5f);
  }

  continuous_placement::ViewRevisionState rollover(
      std::numeric_limits<std::uint64_t>::max());
  rollover.MarkAligned();
  rollover.Invalidate();
  assert(rollover.Revision() == 1);
  assert(rollover.NeedsAlignment());

  // Removing a preview component along the camera direction recovers the same
  // raw projection-plane point through repeated view changes without drift.
  const std::array<float, 3> rawAnchor{2.0f, 3.0f, 4.0f};
  const std::array<float, 3> previewMm{250.0f, -500.0f, 750.0f};
  std::array<float, 3> displayedAnchor{rawAnchor[0] + previewMm[0] / 1000.0f,
                                       rawAnchor[1] + previewMm[1] / 1000.0f,
                                       rawAnchor[2] + previewMm[2] / 1000.0f};
  for (int viewChange = 0; viewChange < 20; ++viewChange) {
    const auto recovered =
        continuous_placement::RawAnchorFromPreview(displayedAnchor, previewMm);
    assert(recovered == rawAnchor);
    displayedAnchor = {recovered[0] + previewMm[0] / 1000.0f,
                       recovered[1] + previewMm[1] / 1000.0f,
                       recovered[2] + previewMm[2] / 1000.0f};
  }

  MvrScene scene;

  Fixture fixture;
  fixture.uuid = "fixture-1";
  fixture.fixtureId = 7;
  fixture.transform.o = {1000.0f, 2000.0f, 3000.0f};
  scene.fixtures[fixture.uuid] = fixture;

  Truss truss;
  truss.uuid = "truss-1";
  truss.transform.o = {4000.0f, 5000.0f, 6000.0f};
  scene.trusses[truss.uuid] = truss;

  SceneObject object;
  object.uuid = "object-1";
  object.transform.o = {7000.0f, 8000.0f, 9000.0f};
  scene.sceneObjects[object.uuid] = object;

  assert(continuous_placement::CloneElement(
      scene, ContinuousPlacementType::Fixture, "fixture-1", "fixture-2"));
  assert(scene.fixtures.at("fixture-2").fixtureId == 8);
  assert(continuous_placement::CloneElement(
      scene, ContinuousPlacementType::Truss, "truss-1", "truss-2"));
  assert(continuous_placement::CloneElement(
      scene, ContinuousPlacementType::SceneObject, "object-1", "object-2"));

  // A confirmed snapped source remains snapped while its next clone starts
  // from the raw pointer anchor rather than inheriting the old preview.
  for (const auto type :
       {ContinuousPlacementType::Fixture, ContinuousPlacementType::Truss,
        ContinuousPlacementType::SceneObject}) {
    const std::string source =
        type == ContinuousPlacementType::Fixture ? "fixture-1"
        : type == ContinuousPlacementType::Truss ? "truss-1"
                                                 : "object-1";
    const std::string clone = source + "-raw-clone";
    const auto snapped =
        continuous_placement::PositionMeters(scene, type, source);
    const std::array<float, 3> preview{0.25f, -0.5f, 0.75f};
    const std::array<float, 3> raw{snapped[0] - preview[0],
                                   snapped[1] - preview[1],
                                   snapped[2] - preview[2]};
    assert(continuous_placement::CloneElement(scene, type, source, clone));
    continuous_placement::SetPositionMeters(scene, type, clone, raw);
    assert(continuous_placement::PositionMeters(scene, type, source) ==
           snapped);
    assert(continuous_placement::PositionMeters(scene, type, clone) == raw);
  }

  assert(continuous_placement::PositionMeters(
             scene, ContinuousPlacementType::Fixture, "fixture-1") ==
         (std::array<float, 3>{1.0f, 2.0f, 3.0f}));
  assert(continuous_placement::PositionMeters(
             scene, ContinuousPlacementType::Truss, "truss-1") ==
         (std::array<float, 3>{4.0f, 5.0f, 6.0f}));
  assert(continuous_placement::PositionMeters(
             scene, ContinuousPlacementType::SceneObject, "object-1") ==
         (std::array<float, 3>{7.0f, 8.0f, 9.0f}));

  continuous_placement::EraseElement(scene, ContinuousPlacementType::Fixture,
                                     "fixture-2");
  continuous_placement::EraseElement(scene, ContinuousPlacementType::Truss,
                                     "truss-2");
  continuous_placement::EraseElement(
      scene, ContinuousPlacementType::SceneObject, "object-2");
  assert(!continuous_placement::Contains(
      scene, ContinuousPlacementType::Fixture, "fixture-2"));
  assert(!continuous_placement::Contains(scene, ContinuousPlacementType::Truss,
                                         "truss-2"));
  assert(!continuous_placement::Contains(
      scene, ContinuousPlacementType::SceneObject, "object-2"));
}
