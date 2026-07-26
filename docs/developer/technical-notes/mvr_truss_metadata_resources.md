# Perastage truss metadata and resource contract

MVR 1.6 permits extension data at `GeneralSceneDescription/UserData`, not as a
direct `Truss/UserData` child. Strict Perastage output stores one
`TrussInfoMap/TrussInfo` entry under `Data provider="Perastage" ver="1.0"`,
keyed by the canonical exported Truss UUID. Legacy direct `TrussInfo` remains
tolerated input and is migrated to the root map on export.

`AuxGdtf` is a portable, root-level archive filename. It must not contain a
directory, platform separator, drive prefix, traversal segment, control
character, or absolute path. The MVR ZIP must contain the referenced entry.
On import, `Truss::perastageAuxGdtfArchivePath` retains this portable archive
identity; `MvrScene::basePath` resolves it to extraction storage owned through
the scene's runtime resource lease. The lease outlives the temporary importer
stack and prevents a dangling extraction path without persisting a local path
in portable metadata.

An explicit auxiliary reference is exported only when its owned source file
exists. A missing explicit resource produces an export warning, emits no
`AuxGdtf` claim, and never selects the generic fixture fallback. Import applies
the same root-filename and existence checks before retaining the portable
identity. Unsupported schema versions, malformed UUIDs, duplicate entries,
unsafe paths, and missing entries are diagnosed; the first valid canonical
entry wins over later duplicates and legacy direct metadata.

Perastage root metadata takes precedence over legacy direct metadata. Foreign
providers are ignored by the Perastage reader. Unknown Perastage child
extensions currently have no opaque scene-model storage and therefore cannot
be reproduced after import; this limitation does not affect foreign-provider
blocks during XML inspection but a newly generated export contains only
modeled Perastage metadata.
