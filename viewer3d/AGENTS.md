# AGENTS.md (viewer3d)

## Scope
These rules apply to everything under `viewer3d/`.

## Rules
1. **Primary hotspot**
   - `viewer3dcontroller.cpp` should be kept contained.

2. **Design direction when extending controller logic**
   - Separate command/interaction handling from scene/resource synchronization.
   - If a new responsibility is added, create a dedicated unit in submodules (`render/`, `resources/`, `picking/`, etc.) before growing the controller.

3. **Safe changes**
   - For significant splits, cover current touched behavior first with available tests/regression checks.
