# Canonical Perastage hoist metadata in MVR

The MVR 1.6 extension point is `GeneralSceneDescription/UserData`. `Support`
does not permit a direct `UserData` child in the official schema. Perastage
therefore writes hoist metadata only in a root map:

```xml
<GeneralSceneDescription ...>
  <UserData>
    <Data provider="Perastage" ver="1.0">
      <HoistInfoMap>
        <HoistInfo uuid="canonical-support-uuid">...</HoistInfo>
      </HoistInfoMap>
    </Data>
  </UserData>
  <Scene>...</Scene>
</GeneralSceneDescription>
```

The UUID is the canonical exported Support identity. Perastage Data version
`1.0` is preferred over legacy object metadata; foreign-provider Data blocks
are not interpreted as Perastage metadata.

## Fields

`HoistInfo` can contain `Capacity`, `Weight`, a manually overridden `Load`,
`RiggingPoint`, `MotorName`, `MotorManufacturer`, `MotorModel`,
`MotorFixtureUuid`, `UseMotorDefaults`, `DummyProfileId`, the compatibility
`DummyPreset`, `ValueSource`, and field-level source elements such as
`MotorNameSource` and `CapacitySource`. Optional
absent values retain their standard or model defaults.

`Function` and `DataSource` remain compatibility aliases for `RiggingPoint`
and `ValueSource` respectively.

## Legacy input

The importer tolerates `Support/UserData/Data/HoistInfo` and
`Support/UserData/Data/MotorInfo`. Canonical supported root metadata takes
precedence when both shapes exist. A subsequent export migrates accepted
legacy values into `HoistInfoMap` and never emits direct Support UserData.
Direct legacy Data is read only for a case-insensitive `Perastage` provider.
Version `1.0` is supported; a missing direct-object version is tolerated with a
diagnostic, while foreign providers and non-empty unsupported versions are
ignored.

Duplicate canonical entries use the first valid UUID match deterministically;
later duplicates are diagnosed and ignored. Invalid or non-finite numeric
tokens do not replace existing standard values. Malformed, unknown, or
ambiguous `MotorFixtureUuid` values are never resolved by display name.
Valid links are canonicalized and resolved only against imported Fixture UUIDs;
malformed and unknown links are diagnosed and left unset.
Perastage root Data with a schema version other than `1.0` is diagnosed and
ignored. Foreign-provider Data is outside this policy and is not interpreted.

`GeneralSceneDescription@providerVersion` identifies the application build;
`Data@ver` independently identifies the Perastage extension schema.
