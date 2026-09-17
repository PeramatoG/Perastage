#include "../viewer2d/interaction/viewer2d_interaction_scope_policy.h"

#include <cassert>

using namespace viewer2d::interaction;

// Verifies scope-to-query routing and typed hit eligibility.
int main() {
  assert(Viewer2DInteractionScopePolicy::ResolveHoverRoute(
             InteractionScope::Fixture)
             .queryKind == PickQueryKind::FixtureLabel);
  assert(
      Viewer2DInteractionScopePolicy::ResolveHoverRoute(InteractionScope::Truss)
          .queryKind == PickQueryKind::TrussLabel);
  const auto support = Viewer2DInteractionScopePolicy::ResolveHoverRoute(
      InteractionScope::Support);
  assert(support.queryKind == PickQueryKind::HoistLabel);
  assert(support.requiresSupportLabelPath && !support.fastUuidPickAllowed);
  assert(Viewer2DInteractionScopePolicy::ResolveHoverRoute(
             InteractionScope::SceneObject)
             .queryKind == PickQueryKind::SceneObjectLabel);
  assert(Viewer2DInteractionScopePolicy::ResolveHoverRoute(
             InteractionScope::CrossTable)
             .queryKind == PickQueryKind::PickUuid);

  for (const auto kind :
       {SceneElementKind::Fixture, SceneElementKind::Truss,
        SceneElementKind::Support, SceneElementKind::SceneObject})
    assert(Viewer2DInteractionScopePolicy::Accepts(InteractionScope::CrossTable,
                                                   kind));
  assert(Viewer2DInteractionScopePolicy::Accepts(InteractionScope::Fixture,
                                                 SceneElementKind::Fixture));
  assert(!Viewer2DInteractionScopePolicy::Accepts(InteractionScope::Fixture,
                                                  SceneElementKind::Truss));
  assert(!Viewer2DInteractionScopePolicy::Accepts(InteractionScope::Support,
                                                  SceneElementKind::Fixture));
  return 0;
}
