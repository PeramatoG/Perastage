# Canonical `UserData/Data/HoistInfo` schema

This document defines the canonical Perastage payload for support hoist metadata inside MVR `GeneralSceneDescription.xml`.

## Location

- `Support/UserData/Data` with attributes:
  - `provider="Perastage"`
  - `ver="1.0"`
- Canonical payload node: `HoistInfo`
- Legacy payload node accepted on import: `MotorInfo`

## Canonical fields

Inside `HoistInfo`:

- `Capacity` (`unit="kg"`, float)
- `Weight` (`unit="kg"`, float)
- `RiggingPoint` (string, normalized hoist function)
- `MotorName` (string)
- `MotorManufacturer` (string)
- `MotorModel` (string)
- `MotorFixtureUuid` (string UUID that links to a fixture)
- `UseMotorDefaults` (`false` when explicitly disabled; omitted means `true`)
- `DummyProfileId` (string stable profile id)
- `DummyPreset` (string display name compatibility field)
- `ValueSource` (`Inherited` or `Manual`)

## Compatibility aliases

Perastage writes canonical names and also compatibility aliases for older builds:

- `Function` mirrors `RiggingPoint`.
- `DataSource` mirrors `ValueSource`.

On import, Perastage reads both canonical and compatibility aliases with this precedence:

1. `RiggingPoint`, fallback `Function`
2. `ValueSource`, fallback `DataSource`

Legacy `MotorInfo` payload blocks are still parsed and migrated internally to the canonical support fields.
