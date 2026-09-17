#include "../viewer2d/interaction/viewer2d_placement_session.h"

#include <cassert>

// Characterizes neutral continuous-placement lifecycle bookkeeping.
int main() {
  using namespace viewer2d::interaction;

  Viewer2DPlacementSession session;
  assert(!session.IsActive());
  assert(!session.IsClipboardPlacement());
  assert(!session.IsBatchPlacement());
  assert(session.Type() == ContinuousPlacementType::None);

  session.BeginNative(ContinuousPlacementType::Fixture, "fixture-a");
  assert(session.IsActive());
  assert(session.Mode() == PlacementMode::Native);
  assert(session.Type() == ContinuousPlacementType::Fixture);
  assert(session.ProvisionalUuid() == "fixture-a");
  session.RecordConfirmedUuid();
  assert(session.PlacedUuids() == std::vector<std::string>{"fixture-a"});
  session.ReplaceProvisionalUuid("fixture-b");
  session.RecordConfirmedUuid();
  assert(session.PlacedUuids() ==
         (std::vector<std::string>{"fixture-a", "fixture-b"}));
  assert(session.TakeLastConfirmedUuid() == "fixture-b");
  assert(session.ProvisionalUuid() == "fixture-b");
  assert(session.PlacedUuids() == std::vector<std::string>{"fixture-a"});
  session.RestorePlacedUuids({"fixture-c", "fixture-d"});
  assert(session.PlacedUuids() ==
         (std::vector<std::string>{"fixture-c", "fixture-d"}));

  session.BeginClipboardSingle(ContinuousPlacementType::Truss, "truss-a");
  assert(session.IsActive());
  assert(session.IsClipboardPlacement());
  assert(!session.IsBatchPlacement());
  assert(session.Type() == ContinuousPlacementType::Truss);
  assert(session.ProvisionalUuid() == "truss-a");
  assert(session.PlacedUuids().empty());

  session.BeginClipboardBatch();
  assert(session.IsActive());
  assert(session.IsClipboardPlacement());
  assert(session.IsBatchPlacement());
  assert(session.Type() == ContinuousPlacementType::None);
  assert(session.ProvisionalUuid().empty());

  session.Reset();
  assert(!session.IsActive());
  assert(!session.IsClipboardPlacement());
  assert(!session.IsBatchPlacement());
  assert(session.Type() == ContinuousPlacementType::None);
  assert(session.ProvisionalUuid().empty());
  assert(session.PlacedUuids().empty());
  assert(session.TakeLastConfirmedUuid().empty());
  return 0;
}
