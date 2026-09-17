/*
 * This file is part of Perastage.
 * Copyright (C) 2026 Luisma Peramato
 * License: GNU General Public License v3.0
 */

#pragma once

#include "viewer2d_runtime_state.h"

namespace viewer2d::interaction {

enum class SceneElementKind { None, Fixture, Truss, Support, SceneObject };
enum class InteractionScope {
  None,
  Fixture,
  Truss,
  Support,
  SceneObject,
  CrossTable
};

struct HoverRoute {
  PickQueryKind queryKind = PickQueryKind::None;
  bool fastUuidPickAllowed = false;
  bool requiresSupportLabelPath = false;
};

// Resolves picking and eligibility without depending on GUI table classes.
class Viewer2DInteractionScopePolicy {
public:
  // Selects the appropriate pick route for an interaction scope.
  static HoverRoute ResolveHoverRoute(InteractionScope scope);
  // Reports whether a scene-element kind is eligible in an interaction scope.
  static bool Accepts(InteractionScope scope, SceneElementKind kind);
};

} // namespace viewer2d::interaction
