# GDTF gobo index, rotation, and shake mapping in Peraviz

This note summarizes the GDTF attributes used for gobo wheel control and
how they are mapped into Peraviz DMX runtime controls.

## GDTF reference used

Perastage includes the GDTF specification at `docs/gdtf-spec.md`.
For this implementation, the relevant attributes are:

- `Gobo(n)` for wheel slot selection.
- `Gobo(n)Pos` for indexed gobo angle.
- `Gobo(n)PosRotate` for continuous gobo rotation speed/direction.
- `Gobo(n)SelectShake` for shake slot selection + shake frequency.

## Runtime mapping

Peraviz now exposes three independent gobo channels when present in the GDTF
mode:

- `gobo_*` for slot selection.
- `gobo_index_*` for indexed angle.
- `gobo_rotation_*` for continuous rotation.

For multi-wheel fixtures, each wheel in `gobo_wheels` also includes:

- `index_*` channels.
- `rotation_*` channels.

When the GDTF channel functions define `PhysicalFrom`/`PhysicalTo`, those
limits are also propagated for index and rotation controls. Runtime uses these
physical ranges to interpret values with fixture-accurate behavior instead of a
generic normalization.

## Control interpretation in Godot

Peraviz runtime applies gobo controls with this behavior:

1. Base rotation from visual settings.
2. If `gobo_index`/`index` is present (`Gobo(n)Pos`), map DMX value to a
   **fixed angular position** relative to initial orientation.
3. If `gobo_rotation`/`rotation` is present (`Gobo(n)PosRotate`), map DMX value
   to **angular speed** (including direction) and integrate over time.
4. If the active gobo range behavior is shake (`Gobo(n)SelectShake`), interpret
   physical values as **shake frequency** and apply a constant **triangle-wave**
   oscillation around the indexed/base angle plus an additional angular
   oscillation on the light local **Y axis** and a small local **X** lateral
   displacement, so footprint and beam show visible shake movement on projected X/Z.

For shake, Peraviz also consumes optional `SubPhysicalUnit` data from the
attribute definition:

- `Type="PlacementOffset"` + `PhysicalUnit="Angle"` -> center offset in degrees.
- `Type="Amplitude"` + `PhysicalUnit="Percent"` -> oscillation amplitude.
  - Runtime accepts both normalized `[0..1]` and percent `[0..100]` encodings
    and normalizes to `[0..1]` before applying the shake.

> Temporary debug mode: `GOBO_SHAKE_FORCE_DEBUG_ALWAYS_ON=true` enables a
> shake layer for active gobo wheels even outside `Gobo(n)SelectShake`, so
> slot/index/rotation stay active and shake is added on top for visual tuning.

Index and rotation are treated as different semantics:

- **Index**: target angle (static position).
- **Rotation/Spin**: speed command (continuous motion).
- **Shake**: periodic oscillation around a center angle at a given frequency.

This keeps backward compatibility for fixtures without explicit
`Gobo(n)Pos`/`Gobo(n)PosRotate` channels while enabling dedicated controls when
present.
