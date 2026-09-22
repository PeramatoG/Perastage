/*
 * This file is part of Perastage.
 * Copyright (C) 2026 Luisma Peramato
 * License: GNU General Public License version 3 or later
 */

#include "viewer2d_interaction_scope_policy.h"

namespace viewer2d::interaction {

// Selects the appropriate pick route for an interaction scope.
HoverRoute
Viewer2DInteractionScopePolicy::ResolveHoverRoute(InteractionScope scope) {
  switch (scope) {
  case InteractionScope::Fixture:
    return {PickQueryKind::FixtureLabel, true, false};
  case InteractionScope::Truss:
    return {PickQueryKind::TrussLabel, true, false};
  case InteractionScope::Support:
    return {PickQueryKind::HoistLabel, false, true};
  case InteractionScope::SceneObject:
    return {PickQueryKind::SceneObjectLabel, true, false};
  case InteractionScope::CrossTable:
    return {PickQueryKind::PickUuid, true, false};
  case InteractionScope::None:
    return {};
  }
  return {};
}

// Reports whether a scene-element kind is eligible in an interaction scope.
bool Viewer2DInteractionScopePolicy::Accepts(InteractionScope scope,
                                             SceneElementKind kind) {
  if (kind == SceneElementKind::None || scope == InteractionScope::None)
    return false;
  if (scope == InteractionScope::CrossTable)
    return true;
  return (scope == InteractionScope::Fixture &&
          kind == SceneElementKind::Fixture) ||
         (scope == InteractionScope::Truss &&
          kind == SceneElementKind::Truss) ||
         (scope == InteractionScope::Support &&
          kind == SceneElementKind::Support) ||
         (scope == InteractionScope::SceneObject &&
          kind == SceneElementKind::SceneObject);
}

} // namespace viewer2d::interaction
