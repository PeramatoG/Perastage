# GDTF gobo index and rotation mapping in Peraviz

This note summarizes the GDTF attributes used for gobo wheel control and
how they are mapped into Peraviz DMX runtime controls.

## GDTF reference used

Perastage includes the GDTF specification at `docs/gdtf-spec.md`.
For this implementation, the relevant attributes are:

- `Gobo(n)` for wheel slot selection.
- `Gobo(n)Pos` for indexed gobo angle.
- `Gobo(n)PosRotate` for continuous gobo rotation speed/direction.

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

Index and rotation are treated as different semantics:

- **Index**: target angle (static position).
- **Rotation/Spin**: speed command (continuous motion).

This keeps backward compatibility for fixtures without explicit
`Gobo(n)Pos`/`Gobo(n)PosRotate` channels while enabling dedicated controls when
present.

## Rotation behavior detection

The GDTF parser detects rotation behavior from ChannelSet and ChannelFunction
names.  The following keywords trigger rotation classification:

- Explicit: `spin`, `rotation`, `rotate`, `posrotate`, `wheelspin`
- Direction: `cw`, `ccw`, `clockwise`, `counterclockwise` (and variants)
- Stop: `no rotation`, `stop`

When a ChannelFunction attribute indicates rotation (e.g. `Gobo1SelectSpin`)
but the function Name is generic, ChannelSets inherit the rotation behavior
from the attribute.

## Physical speed interpolation

When ChannelSets lack explicit `PhysicalFrom`/`PhysicalTo` values:

1. Named stop ranges always get physical 0/0.
2. If the parent ChannelFunction has physical values and spans both CW and CCW
   (crosses zero), direction-aware clamping is applied: each sub-range's
   proportional physical values are clamped to the correct direction half
   based on the ChannelSet name (CW → negative half, CCW → positive half).
3. Otherwise, proportional interpolation within the parent function's physical
   range is used.
4. As a last resort, speed and direction are inferred from the ChannelSet name
   using default angular speed values (20–540 deg/s).
