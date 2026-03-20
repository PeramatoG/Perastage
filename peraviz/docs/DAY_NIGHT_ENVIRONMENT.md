# Day/Night Environment Controller

This document describes the built-in day/night environment baseline used by Peraviz.

## Files changed

- `peraviz/scripts/day_night_environment_controller.gd`
- `peraviz/scripts/load_scene.gd`
- `peraviz/test.tscn`

## How to use

1. Open `peraviz/test.tscn`.
2. Select the `DayNightEnvironmentController` node.
3. Choose one workflow:
   - Preset mode: keep `use_continuous_cycle = false` and set `current_preset` to `Dawn`, `Day`, `Dusk`, `Night`, or `BlackoutNight`.
   - Continuous mode: set `use_continuous_cycle = true` and adjust `time_of_day` between `0.0` and `1.0`.
4. Optional runtime animation:
   - Enable `auto_advance`.
   - Tune `cycle_speed`.

## Key inspector parameters

- `enabled`: global on/off switch for controller ownership.
- `use_continuous_cycle`: enables interpolation through the cycle keyframes.
- `current_preset`: active state in preset mode.
- `time_of_day`: normalized value for continuous mode.
- `auto_advance` and `cycle_speed`: automatic day/night progression.
- `day_light_intensity`, `dusk_light_intensity`, `night_light_intensity`, `moon_light_intensity`: directional light strengths.
- `ambient_energy_day`, `ambient_energy_night`: ambient floor values.
- `horizon_warmth`, `horizon_intensity`: horizon gradient tone and strength for dawn/dusk.
- `allow_blackout_night`: allows near-total darkness during blackout preset.

## Integration note

`load_scene.gd` now re-applies the controller after visual-settings updates so fixture tuning UI does not permanently override day/night sky and ambient states.

## Visual Settings controls

The Visual Settings window now includes a dedicated **Environment** section with basic controls for:

- Enabling/disabling the environment controller.
- Preset selection (`Dawn`, `Day`, `Dusk`, `Night`, `BlackoutNight`).
- Continuous cycle controls (`time_of_day`, `auto_advance`, `cycle_speed`).
- Blackout-night permission toggle.
- Core environment tuning (light intensities, ambient day/night, horizon warmth/intensity).
