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
`DummyPreset`, `ValueSource`, and field-level source attributes. Optional
absent values retain their standard or model defaults.

`Function` and `DataSource` remain compatibility aliases for `RiggingPoint`
and `ValueSource` respectively.

## Legacy input

The importer tolerates `Support/UserData/Data/HoistInfo` and
`Support/UserData/Data/MotorInfo`. Canonical supported root metadata takes
precedence when both shapes exist. A subsequent export migrates accepted
legacy values into `HoistInfoMap` and never emits direct Support UserData.

`GeneralSceneDescription@providerVersion` identifies the application build;
`Data@ver` independently identifies the Perastage extension schema.
