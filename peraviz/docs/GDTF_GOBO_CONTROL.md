# GDTF gobo index, rotation, and shake mapping in Peraviz

This note summarizes the GDTF attributes used for gobo wheel control and
how they are mapped into Peraviz DMX runtime controls.

## GDTF reference used

Perastage includes the GDTF specification at `docs/gdtf-spec.md`.
For this implementation, the relevant attributes are:

- `Gobo(n)` for wheel slot selection.
- `Gobo(n)Pos` for indexed gobo angle.
- `Gobo(n)PosRotate` for continuous gobo rotation speed/direction.
- `Gobo(n)SelectShake` or dedicated shake attributes for gobo shake behavior.

## Runtime mapping

Peraviz exposes gobo controls when present in the fixture mode:

- `gobo_*` for slot selection.
- `gobo_index_*` for indexed angle.
- `gobo_rotation_*` for continuous rotation.
- shake metadata (`supports_shake`, `shake_active`, `shake_amplitude_deg`,
  `shake_frequency_hz`) per wheel when shake-capable functions are detected.

For multi-wheel fixtures, each wheel in `gobo_wheels` also includes:

- `index_*` channels.
- `rotation_*` channels.
- shake ranges tied to parsed DMX windows.

When the GDTF channel functions define `PhysicalFrom`/`PhysicalTo`, those
limits are propagated for index/rotation and shake interpretation when
available. Runtime uses these physical ranges to interpret values with
fixture-accurate behavior instead of a generic normalization.

## Control interpretation in Godot

Peraviz runtime applies gobo controls with this behavior:

1. Base rotation from visual settings.
2. If `gobo_index`/`index` is present (`Gobo(n)Pos`), map DMX value to a
   **fixed angular position** relative to initial orientation.
3. If `gobo_rotation`/`rotation` is present (`Gobo(n)PosRotate`), map DMX value
   to **angular speed** (including direction) and integrate over time.
4. If shake is active, apply shake as an **additional tilt modulation layer**
   on top of the resulting rotation.

Shake is therefore **not mutually exclusive** with rotation:

- Rotation defines the wheel spin/index orientation over time.
- Shake adds oscillation amplitude/frequency on top of that state.

## Shake control priority

Per-wheel shake source priority is:

1. **Dedicated shake channel** (highest priority).
2. **Same-channel select-shake window** (`Gobo(n)SelectShake`).
3. **Fallback** from rotation speed (frequency inferred from
   `abs(rotation_speed_deg_per_sec) / 360.0` if no explicit shake frequency is
   available).

Debug override behavior:

- Debug override affects shake only when explicitly enabled in debug controls.
- If debug override is disabled (or running in non-debug builds), runtime uses
  fixture/runtime shake sources only.

## Real fixture example: `Robin MegaPointe.gdtf`

The fixture file `library/fixtures/Robin MegaPointe.gdtf` provides a concrete
same-channel select-shake example:

- `Gobo1SelectShake` appears on one DMX channel and defines select-shake windows
  (for static gobo shake selection).
- `Gobo2SelectShake` appears on one DMX channel with separate windows for
  shake-index and shake-rotation behavior.

Observed DMX windows (8-bit values) in the provided fixture modes:

- `Gobo1SelectShake` starts at `88` with slot windows stepping by `8`
  (`88..95`, `96..103`, ...).
- `Gobo2SelectShake` shake-index region starts at `60` with slot windows stepping
  by `8` (`60..67`, `68..75`, ...).
- `Gobo2SelectShake` shake-rotation region starts at `130` with slot windows
  stepping by `8` (`130..137`, `138..145`, ...).

In this specific fixture, select-shake windows are the active runtime source;
if a dedicated shake channel existed for the same wheel, it would take
precedence by design.

## Approved operational limits

Peraviz enforces the following shake safety limits at runtime:

- `max amplitude = 1.0º`
- `max frequency = 7.0 Hz`

Any resolved or debug-provided shake values are clamped to these limits before
being applied.
