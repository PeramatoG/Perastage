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

## Control interpretation in Godot

`BeamOpticsController.BuildGoboControls(...)` now computes `gobo_rotation_deg`
using this order:

1. Base rotation from visual settings.
2. Override by `gobo_index_norm` (or wheel-specific `index_norm`) as
   `0..360°` index angle.
3. Add continuous rotation offset from `gobo_rotation_norm` (or wheel-specific
   `rotation_norm`) around a center stop value at `0.5`.

This keeps backward compatibility for fixtures without explicit
`Gobo(n)Pos`/`Gobo(n)PosRotate` channels while enabling dedicated controls when
present.
