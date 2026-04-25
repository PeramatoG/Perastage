# Canonical `UserData/Data/HoistInfo` schema

This document defines the canonical Perastage payload for support hoist metadata inside MVR `GeneralSceneDescription.xml`.

## Location

- `Support/UserData/Data` with attributes:
  - `provider="Perastage"`
  - `ver="1.0"` (Perastage user-data schema version, not the app version)
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

## Versioning policy

- `GeneralSceneDescription@providerVersion` is exported from `app::kVersion` and identifies the Perastage application build that created the MVR file.
- `UserData/Data@ver` stays on the Perastage user-data schema version (`1.0`) and only changes when the Perastage custom payload schema changes.
