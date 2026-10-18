# MVR 1.6 data integrity and round-trip audit

Status: **bounded P0 correction; universal preservation and complete fail-closed coverage remain unfinished**.

The scene-format inventory below covers every attribute/child entry in published
MVR 1.6 Tables 2–64, plus value types, archive rules, references and version policy.
An inventory entry is not a support claim. Static tracing, executed regression
evidence and independent validation are distinguished throughout. No claim of
universal losslessness or complete DIN conformance is made.

## Authority and revisions

Current continuation base: Perastage `7b4a3749` (latest fetched `origin/main`
when `codex/mvr-symdef-definition-integrity` was created, after merged PRs
#2479 and #2480). The Class continuation used `a694aa99`; the original #2479
review used `1f0e94bdd4ba4df7c6ce5a4b83fa256ba55be0eb`.
Historical verification is retained below and distinguished from this phase.
No VERSION change.

| Authority | Exact revision / interpretation |
| --- | --- |
| [Published MVR 1.6](https://github.com/mvrdevelopment/spec/blob/8e0e0ccf4d7855e190ee7f370f93ea58be490fef/mvr-spec.md), DIN SPEC 15801:2023-12 | Official `1.6-mvr` tag `8e0e0ccf4d7855e190ee7f370f93ea58be490fef`. Primary normative scene reference. |
| [Official spec main snapshot](https://github.com/mvrdevelopment/spec/blob/098d3791f77f0895bd859adf01864b4826e2006f/mvr-spec.md) | Repository HEAD `098d3791f77f0895bd859adf01864b4826e2006f`; scene section agrees with the release tag; post-publication differences concern MVR-xchange examples. Latest change to this document: `81a5e8e6b1287eb1c95c64dc43d346da158dcf79`. File SHA-256 `9fb43944c363c4b58af023a02b821bae5d4ae310aad26ffec06bae3415972db7`. Development branches (`next`, audio/cable/layer-color proposals, etc.) are excluded. |
| [Official tools/XSD](https://github.com/mvrdevelopment/tools/blob/e199c6ed635de23cb5ebf9654ee54a358775a065/mvr.xsd) | `e199c6ed635de23cb5ebf9654ee54a358775a065`; repository-pinned `resources/standards/mvr/1.6/mvr.xsd` is byte-identical. SHA-256 `85bc42015b706b2ab2bcc2ce69649e7245272b3606217263c90126cda21cb86c`. No schema modification. |
| [Official reference implementation](https://github.com/mvrdevelopment/libMVRgdtf/blob/9f5a6420c72f24fdd249cfe8133f257026c5590b/src/SceneDataExchange.cpp) | `9f5a6420c72f24fdd249cfe8133f257026c5590b`. Static comparison only: `SceneDataFocusPointObj`, `SceneDataExchange::CreateFocusPoint` and fixture `GetFocusPoint`/serialization retain a distinct scene entity and its link. Not built or used for interoperability execution. |
| [Published earlier 1.5 snapshot](https://github.com/mvrdevelopment/spec/blob/1ea072e8581f82f78a2e18ca429d893a10ea7afd/mvr-spec.md) | `1ea072e8581f82f78a2e18ca429d893a10ea7afd` identifies itself as MVR 1.5; checked for FocusPoint, FixtureTypeId and identifier/cardinality differences. `b857f5fb364567f3d69247780967c9e5400e6dea` is explicitly a **1.5 draft**, not an additional published version. |
| Local `docs/reference/mvr-spec.md` | SHA-256 `74f798834176f08b729ccf8c922d3fdf223e4b17c4a6b46946043834d6a1302b`; contains application policy paragraphs and rewritten links, so it is not treated as an unmodified authoritative standard. Left unchanged. |

The published “Only user-intended modifications … shall be processed” rule is
the acceptance criterion. Successful XML writing or successful reimport alone
cannot establish it.

### Discrepancies and version boundaries

- Published Table 26 permits Fixture/ChildList **0 or 1**. The XSD comment says
  Fixture should have ChildList in 1.6; its actual declaration is optional for
  compatibility. Do not invent an empty required Fixture child list.
- Published Table 22 permits GroupObject/ChildList **0 or 1**; the XSD requires
  it. Continuing to emit the empty container is compatible with both. Existing
  developer text calling it a published requirement was corrected.
- Published Data describes provider-owned content. The pinned XSD's `Data`
  complex type has attributes only and rejects child content, including current
  Perastage metadata. Regression tests execute the **full** XSD validation and
  explicitly expect this disagreement; a separate copy with root UserData
  removed must validate against the unchanged schema. This validates the
  standard scene projection, **not full-document XSD conformance**. Foreign
  content is independently compared semantically, not discarded from output.
- The XSD allows missing provider/providerVersion for old files; Table 2 requires
  them for 1.6. Its `positiveinteger` accepts zero. Published integer identifiers
  and persistent UUID rules are not equivalent to the exporter's stricter
  uniqueness/positive-ID policy; see remaining risks.
- Published Symdef/ChildList is a container; Table 9's Integer value-type and
  size-X description are obvious copied text. Use the normative ChildList
  definition and actual XML/XSD structure, not an integer payload.
- Official examples have inconsistencies (legacy spaced UUIDs, filename without
  extension, Network attribute casing, ScaleHandeling values); definitions and
  tables take precedence over illustrative examples. Network fields in the
  inventory use the table's exact lowercase names.
- The importer currently accepts major 1 with any minor <=6, including versions
  without a separately verified published compatibility contract. This change
  tests a narrow 1.5 FocusPoint conversion only. It does **not** certify 1.0–1.4
  or all of 1.5. Unsupported major/future minor versions may be inspected but
  canonical export/project save is now blocked; no silent downgrade.

## Lifecycle evidence

| Workflow | Actual path and authority | Result and remaining boundary |
| --- | --- | --- |
| A: MVR → MVR | `mvr_import_package` owns extraction/lease; `ReadAcquiredMvrPackage` → `ReadMvrSceneNodes` → MvrScene; `MvrExporter::SerializeSnapshotToFile` → preparation → writers/resource plan → archive | Direct FocusPoint geometry, fixture reference/scalars and the tested Class/Classing subset survive. Import provenance is scanned before normalization. See matrix for protected and unprotected partial structures. |
| B: MVR → PSTG → reopen → MVR | `ConfigManager::SaveProject` serializes **new** canonical scene.mvr through the same exporter. `ProjectSession::SaveProject` stages config, scene bytes and resources in a temporary archive and publishes only on success. Restore reacquires scene.mvr and project resources. | Test releases all source extraction leases before reopen and asserts the old directory is gone. Focus/type/geometry/transforms/references and fixture values survive. A rejected save leaves the previous PSTG readable. PSTG is not a copy of the original MVR. |
| C: edit one property | Domain instance change → private exporter snapshot → identity/transform preparation | Test changes one fixture name; independently compares focus subtrees, scalar metadata and resource hashes. Other mutation frontends have static evidence only unless listed. |
| D: merge | `AnalyzeImportedSceneMerge` plans collisions across sceneObjects and fixtures; resource rewriter copies into target lifetime; applier remaps fixture.focus and group/layer membership | FocusPoint shares existing sceneObjects ownership with a distinct kind; no second entity exists. Collision test verifies all incoming fixtures resolve to incoming remapped focus objects. Source preservation issues are unioned and remain blockers. Foreign opaque references are not generically rewritten. |
| E: copy/group/transform | Existing `scene_clipboard::Service`, `scene_grouping`, layer identity and transform integrity services copy SceneObject values (including kind) | Clipboard now remaps Focus when its target is copied in the same batch; fixture-only copy retains shared target. Group/translation tests cover nested focus transforms and local matrices. Identifier reassignment policy remains a risk, as described below. |
| F: new scene | Existing canonical exporter, deterministic identities, standard XML writers, archive plan | Existing exporter compliance tests cover new scenes. This change adds final serialized Focus dependency checks. Runtime export still does not run the entire independent XSD/semantic stack. |
| G: legacy → 1.6 | Same reader, compatibility paths and canonical exporter | Narrow 1.5 direct-focus regression; comprehensive earlier-version preservation is unverified. Legacy FixtureTypeId has no durable standard replacement and is now explicitly protected from loss. |
| H: MVR-xchange | Canonical snapshot buffer pipeline is shared with project/standalone export; `external_scene_workflow` routes import/merge | Same preservation gate applies to canonical bytes. Protocol discovery, transport, peer acceptance and live cross-application exchange were not exercised. |

## Corrected defects and protection

1. **FocusPoint entity omission**: base `mvr_scene_node_reader.cpp` routed it
   only to neutral Inspection descriptors. It is now a geometric SceneObject
   value with `SceneObjectKind::FocusPoint`, retaining UUID/name, layer UUID,
   parent group, world/local matrices and direct geometry instances/resources.
   Existing layer, clipboard, merge, undo snapshot and resource lease owners
   remain authoritative. Export writes **FocusPoint**, never a second SceneObject
   or FixtureID on that entity. FocusPoint does not consume numeric selection
   IDs belonging to other SceneObjects. Inspection filters it from generic SceneObject
   counts to avoid duplication. No new focus GUI was introduced.
2. **Fixture Focus omission**: `FixtureValues`, exporter and writer now serialize
   the UUID reference; identity recovery rewrites it with the same alias map as
   scene identities. Before recovery, missing targets block output instead of
   being silently cleared. After serialization, a separate scene-ownership walk
   verifies target UUIDs, required Geometries and prohibited Focus child lists.
3. **Fixture scalar omissions**: Function, DMXInvertPan/DMXInvertTilt and
   CustomId/CustomIdType were parsed in the base reader but not written. They
   are now written in schema order. Optional false/zero values may be omitted
   where semantically default; source lexical spelling/presence is not retained.
4. **CIE color drift**: base CIE→8-bit RGB→CIE conversion loses luminance and
   gamut. Fixture retains authored CIE text plus the imported display color;
   unchanged display color writes original CIE. An explicitly changed display
   value uses the existing conversion. Copies/PSTG/merge carry these values.
5. **Known losses lacked a shared gate**: `MvrPreservationIssue` stores node,
   original UUID/source location, original source information, reason and
   recommended action. The importer scans its existing parsed DOM before
   normalization. Evidence is never inserted as unchecked XML. It survives
   scene copies/undo and merge; Clear removes it. Export preparation emits
   structured `StandardDataPreservationBlocked` / Error / ExportFailed and
   returns before archive writing. The headless scene workflow forwards the
   exporter’s detailed findings into CLI JSON diagnostics. There is no lossy override. PSTG cannot
   persist an incomplete scene because its scene serializer fails as well. The same canonical
   MVR content is required inside PSTG and standalone export; see the shared
   serialization invariant below.

6. **Class/Classing preservation (PR #2480)**: #2479 deliberately
   blocked these valid standard structures because the editable model had no
   owner. Published Table 7 includes AUXData/Class; Table 14 requires its UUID
   and defines its name; Tables 20/22/24/26/28/30 permit one optional Classing
   UUID on supported scene objects. Table 1 forbids nil UUIDs; class checks
   enforce that constraint even though the XSD accepts them. The unchanged
   official XSD uses an optional
   name defaulting to empty and orders Classes before Symdefs/Positions.
   A schema-valid synthetic source with six classed node kinds across two layers
   reproduced the old export refusal on `a694aa99`; it now exports safely within
   this subset. Classes are a scene-wide UUID registry; each supported node owns
   its direct Classing edge. Names are retained exactly, including absence,
   empty names and whitespace; unused classes are retained. Equal names never
   coalesce distinct UUIDs. No visibility behavior is implemented.
   Pre-normalization evidence protects missing/nil/duplicate/cross-kind identities,
   unresolved or repeated Classing and unrepresented class attributes/content.
   Current model references are checked before recovery; a final projection
   comparison rejects changed definitions, assignments and Class identity
   collisions before package publication. Provider UserData is excluded from
   these walks. Merge shares a semantically equal same-UUID definition (including
   absent/empty default names), retains the target name spelling, remaps actual
   collisions with all incoming edges, and reserves class identities across
   object/lookup namespaces. Cross-kind collisions are remapped even under
   ReplaceExisting/SkipIncoming: those object policies do not authorize
   replacing a Class with a different standard node kind. Existing blockers for
   VideoScreen/Projector and all unrelated unsupported structures remain.

Known structures protected now: malformed/ambiguous/unrepresented Class/Classing;
MappingDefinition; nonidentity
Layer Matrix/elevation; multipatch; fixture CastShadow/ChildPosition, Protocols,
Alignments, CustomCommands, Overwrites, Connections, Mappings and Gobo; multiple
or nonzero DMX breaks and network assignments; noneditable VideoScreen/Projector
subtrees; unsupported generic/rigging standard fields listed in the matrix;
parametric-object child ownership that would be flattened; nested Symdefs; unrepresented/multiple Truss geometry entries and baked
Symbol/first-definition placement; malformed identities/references, unavailable or
externally dependent definition resources; Symbol geometry expanded by SceneObject/FocusPoint/Support; legacy
FixtureTypeId; foreign object-level UserData; unavailable direct focus geometry;
and unsupported future versions. Empty optional assignment containers and
identity Layer matrices do not trigger an ordinary safe-export warning.

This is **known-loss detection**, not a complete semantic provenance engine.
Evidence remains conservative after deletion of the affected object and may
require keeping the original archive and using another preserving application.
Diagnostics retain source UUIDs/locations after merge; they are source evidence,
not a remappable second scene graph. The current implementation still blocks affected PSTG saves. Removing those
limitations requires genuine standard preservation in the shared canonical
exporter; PSTG-only recovery metadata is not an alternative. Memory
cost is proportional to scanned source/evidence, without an import-export-import
cycle on routine saves. Existing recursive parser limits still apply.

## Coverage matrix

The requirement column transcribes the published table count or optional
attribute default; `R` means required. `?` means the table does not explicitly
state a count/default; the XSD adds constraints described above. All rows are
MVR **1.6** unless otherwise noted. Repeated element names are deliberately
listed separately for each owner.

Columns: I imported/parsing; D durable representation; E editable; T untouched
preservation; O correct export; P PSTG reload; M merge/copy/group; A automated
coverage. `Y` means within the explicitly bounded tested subset; `p` partial;
`N` absent; `?` unverified; `B` operation blocked if present (diagnostic evidence
is retained, **not** a preserved semantic entity). M=`p` must never be read as
full reference/resource remapping. A=`C` class integration/protection/independent tests;
`D` Symdef integration/protection/independent tests; `F` #2479 focus tests; `K` blocker tests;
`X` existing named tests; `–` no executed feature-specific proof. A test name in
this matrix identifies coverage, not proof of every possible value.

Status: `S` full semantic support **only for the named tested subset**; `V` safe
preservation without a dedicated editor; `P` partial support; `L` known remaining
loss; `B` unsupported with explicit write protection; `U` unverified. Risk/action
keys: `f` direct-focus subset; `s` fixture scalar/color subset; `h` hierarchy/local
transform path; `r` resource/reference contract; `u` root opaque UserData policy;
`c` tested Class/Classing preservation without visibility or an editor;
`b` add typed/preservation support before allowing write; `i` identifier recovery
policy requires correction; `d` direct Symdef/self-contained resource and simple Truss Symbol subset;
`a` AUXData retention requires follow-up; `v` version
and semantic validation incomplete. Detailed limitations follow the matrix.


| Table | Standard path | Requirement/default | I | D | E | T | O | P | M | A | Status | Risk/action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 2 | `GeneralSceneDescription/@verMajor` | R | Y | Y | N | p | p | p | p | K | P | P0/v |
| 2 | `GeneralSceneDescription/@verMinor` | R | Y | Y | N | p | p | p | p | K | P | P0/v |
| 2 | `GeneralSceneDescription/@provider` | R | Y | Y | N | p | p | p | p | K | P | P0/v |
| 2 | `GeneralSceneDescription/@providerVersion` | R | Y | Y | N | p | p | p | p | K | P | P0/v |
| 3 | `GeneralSceneDescription/UserData` | 0 or 1 | Y | Y | N | p | p | p | p | F,X | V | P0/u |
| 3 | `GeneralSceneDescription/Scene` | 1 | Y | Y | p | p | p | p | p | F,X | P | P0/h |
| 4 | `UserData/Data` | 0 or many | Y | Y | N | p | p | p | p | F,X | V | P0/u |
| 5 | `Data/@provider` | R | Y | Y | N | p | p | p | p | F,X | V | P0/u |
| 5 | `Data/@ver` | 1 | Y | Y | N | p | p | p | p | F,X | V | P0/u |
| 6 | `Scene/AUXData` | 0 or 1 | p | p | p | p | p | p | p | X | P | P0/a |
| 6 | `Scene/Layers` | 1 | Y | Y | p | p | p | p | p | F,X | P | P0/h |
| 7 | `AUXData/Symdef` | 0 or any | p | p | p | p | p | p | p | D | P | P0/d |
| 7 | `AUXData/Position` | 0 or any | p | p | p | p | p | p | p | X | P | P0/a |
| 7 | `AUXData/MappingDefinition` | 0 or any | N | B | N | B | B | B | B | K | B | P0/b |
| 7 | `AUXData/Class` | 0 or any | Y | Y | N | Y | Y | Y | Y | C | V | P0/c |
| 8 | `Symdef/@uuid` | R | Y | Y | N | Y | Y | Y | Y | D | V | P0/d |
| 8 | `Symdef/@name` | Empty | Y | Y | N | Y | Y | Y | Y | D | V | P0/d |
| 9 | `Symdef/ChildList` | 1 | p | p | N | p | p | p | p | D,K | P | P0/d |
| 10 | `Symdef/ChildList/Geometry3D` | ? (listed) | Y | Y | N | Y | Y | Y | Y | D | V | P0/d |
| 10 | `Symdef/ChildList/Symbol` | ? (listed) | N | B | N | B | B | B | B | K | B | P0/b |
| 11 | `Position/@uuid` | R | p | p | p | p | p | p | p | X | P | P0/a |
| 11 | `Position/@name` | Empty | p | p | p | p | p | p | p | X | P | P0/a |
| 12 | `MappingDefinition/@uuid` | R | N | B | N | B | B | B | B | K | B | P0/b |
| 12 | `MappingDefinition/@name` | ? (unspecified) | N | B | N | B | B | B | B | K | B | P0/b |
| 13 | `MappingDefinition/SizeX` | 1 | N | B | N | B | B | B | B | K | B | P0/b |
| 13 | `MappingDefinition/SizeY` | 1 | N | B | N | B | B | B | B | K | B | P0/b |
| 13 | `MappingDefinition/Source` | 1 | N | B | N | B | B | B | B | K | B | P0/b |
| 13 | `MappingDefinition/ScaleHandeling` | 0 or 1 | N | B | N | B | B | B | B | K | B | P0/b |
| 14 | `Class/@uuid` | R | Y | Y | N | Y | Y | Y | Y | C | V | P0/c |
| 14 | `Class/@name` | ? (unspecified) | Y | Y | N | Y | Y | Y | Y | C | V | P0/c |
| 15 | `Layers/Layer` | ? (listed) | Y | Y | p | p | p | p | p | F,X | P | P0/h |
| 16 | `Layer/@uuid` | R | Y | Y | p | p | p | p | p | F,X | P | P0/i |
| 16 | `Layer/@name` | Empty | Y | Y | p | p | p | p | p | F,X | P | P0/i |
| 17 | `Layer/Matrix` | 0 or 1; identity accepted, elevation blocked | N | B | N | B | B | B | B | K | B | P0/b |
| 17 | `Layer/ChildList` | 0 or 1 | Y | Y | p | p | p | p | p | F,X | P | P0/h |
| 18 | `ChildList/SceneObject` | ? (listed) | Y | Y | p | p | p | p | p | F,X | P | P0/h |
| 18 | `ChildList/GroupObject` | ? (listed) | Y | Y | p | p | p | p | p | F,X | P | P0/h |
| 18 | `ChildList/FocusPoint` | ? (listed) | Y | Y | p | Y | Y | Y | Y | F | V | P0/f |
| 18 | `ChildList/Fixture` | ? (listed) | Y | Y | p | p | p | p | p | F,X | P | P0/h |
| 18 | `ChildList/Support` | ? (listed) | Y | Y | p | p | p | p | p | F,X | P | P0/h |
| 18 | `ChildList/Truss` | ? (listed) | Y | Y | p | p | p | p | p | F,X | P | P0/h |
| 18 | `ChildList/VideoScreen` | ? (listed) | N | B | N | B | B | B | B | K | B | P0/b |
| 18 | `ChildList/Projector` | ? (listed) | N | B | N | B | B | B | B | K | B | P0/b |
| 19 | `SceneObject/@uuid` | R | Y | Y | p | p | p | p | p | F,X | P | P0/i |
| 19 | `SceneObject/@name` | Empty | Y | Y | p | p | p | p | p | F,X | P | P0/i |
| 19 | `SceneObject/@multipatch` | Empty | N | B | N | B | B | B | B | K | B | P0/b |
| 20 | `SceneObject/Matrix` | 0 or 1 | Y | Y | p | p | p | p | p | F,X | P | P0/h |
| 20 | `SceneObject/Classing` | 0 or 1 | Y | Y | N | Y | Y | Y | Y | C | V | P0/c |
| 20 | `SceneObject/Geometries` | 1 | p | p | p | p | p | p | p | F,X | P | P0/r |
| 20 | `SceneObject/GDTFSpec` | 0 or 1 | N | B | N | B | B | B | B | K | B | P0/b |
| 20 | `SceneObject/GDTFMode` | 0 or 1 | N | B | N | B | B | B | B | K | B | P0/b |
| 20 | `SceneObject/CastShadow` | 0 or 1 | N | B | N | B | B | B | B | K | B | P0/b |
| 20 | `SceneObject/Addresses` | 0 or 1 | N | B | N | B | B | B | B | K | B | P0/b |
| 20 | `SceneObject/Alignments` | 0 or 1 | N | B | N | B | B | B | B | K | B | P0/b |
| 20 | `SceneObject/CustomCommands` | 0 or 1 | N | B | N | B | B | B | B | K | B | P0/b |
| 20 | `SceneObject/Overwrites` | 0 or 1 | N | B | N | B | B | B | B | K | B | P0/b |
| 20 | `SceneObject/Connections` | 0 or 1 | N | B | N | B | B | B | B | K | B | P0/b |
| 20 | `SceneObject/FixtureID` | 1 | Y | Y | p | p | p | p | p | F,X | P | P0/i |
| 20 | `SceneObject/FixtureIDNumeric` | 1 | Y | Y | p | p | p | p | p | F,X | P | P0/i |
| 20 | `SceneObject/UnitNumber` | 0 or 1 | N | B | N | B | B | B | B | K | B | P0/b |
| 20 | `SceneObject/CustomId` | 0 or 1 | N | B | N | B | B | B | B | K | B | P0/b |
| 20 | `SceneObject/CustomIdType` | 0 or 1 | N | B | N | B | B | B | B | K | B | P0/b |
| 20 | `SceneObject/ChildList` | 0 or 1 | N | B | N | B | B | B | B | K | B | P0/b |
| 21 | `GroupObject/@uuid` | R | Y | Y | p | p | p | p | p | F,X | P | P0/i |
| 21 | `GroupObject/@name` | Empty | Y | Y | p | p | p | p | p | F,X | P | P0/i |
| 22 | `GroupObject/Matrix` | 0 or 1 | Y | Y | p | p | p | p | p | F,X | P | P0/h |
| 22 | `GroupObject/Classing` | 0 or 1 | Y | Y | N | Y | Y | Y | Y | C | V | P0/c |
| 22 | `GroupObject/ChildList` | 0 or 1; XSD requires 1 | Y | Y | p | p | p | p | p | F,X | P | P0/h |
| 23 | `FocusPoint/@uuid` | R | Y | Y | p | Y | Y | Y | Y | F | V | P0/f |
| 23 | `FocusPoint/@name` | Empty | Y | Y | p | Y | Y | Y | Y | F | V | P0/f |
| 24 | `FocusPoint/Matrix` | 0 or 1 | Y | Y | p | Y | Y | Y | Y | F | V | P0/f |
| 24 | `FocusPoint/Classing` | 0 or 1 | Y | Y | N | Y | Y | Y | Y | C | V | P0/c |
| 24 | `FocusPoint/Geometries` | 1 | Y | Y | p | Y | Y | Y | Y | F | V | P0/f |
| 25 | `Fixture/@uuid` | R | Y | Y | p | p | p | p | p | F,X | P | P0/i |
| 25 | `Fixture/@name` | Empty | Y | Y | p | p | p | p | p | F,X | P | P0/i |
| 25 | `Fixture/@multipatch` | Empty | N | B | N | B | B | B | B | K | B | P0/b |
| 26 | `Fixture/Matrix` | 0 or 1 | Y | Y | p | p | p | p | p | F,X | P | P0/h |
| 26 | `Fixture/Classing` | 0 or 1 | Y | Y | N | Y | Y | Y | Y | C | V | P0/c |
| 26 | `Fixture/GDTFSpec` | 0 or 1 | p | p | p | p | p | p | p | F,X | P | P0/r |
| 26 | `Fixture/GDTFMode` | 0 or 1 | p | p | p | p | p | p | p | F,X | P | P0/r |
| 26 | `Fixture/Focus` | 0 or 1 | Y | Y | p | Y | Y | Y | Y | F | V | P0/f |
| 26 | `Fixture/CastShadow` | 0 or 1 | N | B | N | B | B | B | B | K | B | P0/b |
| 26 | `Fixture/DMXInvertPan` | 0 or 1 | Y | Y | p | Y | Y | Y | Y | F | S | P0/s |
| 26 | `Fixture/DMXInvertTilt` | 0 or 1 | Y | Y | p | Y | Y | Y | Y | F | S | P0/s |
| 26 | `Fixture/Position` | 0 or 1 | p | p | p | p | p | p | p | X | P | P0/a |
| 26 | `Fixture/Function` | 0 or 1 | Y | Y | p | Y | Y | Y | Y | F | S | P0/s |
| 26 | `Fixture/FixtureID` | 1 | Y | Y | p | p | p | p | p | F,X | P | P0/i |
| 26 | `Fixture/FixtureIDNumeric` | 1 | Y | Y | p | p | p | p | p | F,X | P | P0/i |
| 26 | `Fixture/UnitNumber` | 1 | Y | Y | p | p | p | p | p | F,X | P | P0/i |
| 26 | `Fixture/ChildPosition` | 0 or 1 | N | B | N | B | B | B | B | K | B | P0/b |
| 26 | `Fixture/Addresses` | 0 or 1 | p | p | p | p | p | p | p | F,X | P | P0/r |
| 26 | `Fixture/Protocols` | 0 or 1 | N | B | N | B | B | B | B | K | B | P0/b |
| 26 | `Fixture/Alignments` | 0 or 1 | N | B | N | B | B | B | B | K | B | P0/b |
| 26 | `Fixture/CustomCommands` | 0 or 1 | N | B | N | B | B | B | B | K | B | P0/b |
| 26 | `Fixture/Overwrites` | 0 or 1 | N | B | N | B | B | B | B | K | B | P0/b |
| 26 | `Fixture/Connections` | 0 or 1 | N | B | N | B | B | B | B | K | B | P0/b |
| 26 | `Fixture/Color` | 0 or 1 | Y | Y | p | Y | Y | Y | Y | F | S | P0/s |
| 26 | `Fixture/CustomIdType` | 0 or 1 | Y | Y | p | Y | Y | Y | Y | F | S | P0/s |
| 26 | `Fixture/CustomId` | 0 or 1 | Y | Y | p | Y | Y | Y | Y | F | S | P0/s |
| 26 | `Fixture/Mappings` | 0 or 1 | N | B | N | B | B | B | B | K | B | P0/b |
| 26 | `Fixture/Gobo` | 0 or 1 | N | B | N | B | B | B | B | K | B | P0/b |
| 26 | `Fixture/ChildList` | 0 or 1 | N | B | N | B | B | B | B | K | B | P0/b |
| 27 | `Truss/@uuid` | R | Y | Y | p | p | p | p | p | F,X | P | P0/i |
| 27 | `Truss/@name` | Empty | Y | Y | p | p | p | p | p | F,X | P | P0/i |
| 27 | `Truss/@multipatch` | Empty | N | B | N | B | B | B | B | K | B | P0/b |
| 28 | `Truss/Matrix` | 0 or 1 | Y | Y | p | p | p | p | p | F,X | P | P0/h |
| 28 | `Truss/Classing` | 0 or 1 | Y | Y | N | Y | Y | Y | Y | C | V | P0/c |
| 28 | `Truss/Position` | 0 or 1 | p | p | p | p | p | p | p | X | P | P0/a |
| 28 | `Truss/Geometries` | 1 | p | p | p | p | p | p | p | F,X | P | P0/r |
| 28 | `Truss/Function` | 0 or 1 | Y | Y | p | p | p | p | p | F,X | P | P0/h |
| 28 | `Truss/GDTFSpec` | 0 or 1 | p | p | p | p | p | p | p | F,X | P | P0/r |
| 28 | `Truss/GDTFMode` | 0 or 1 | p | p | p | p | p | p | p | F,X | P | P0/r |
| 28 | `Truss/CastShadow` | 0 or 1 | N | B | N | B | B | B | B | K | B | P0/b |
| 28 | `Truss/Addresses` | 0 or 1 | N | B | N | B | B | B | B | K | B | P0/b |
| 28 | `Truss/Alignments` | 0 or 1 | N | B | N | B | B | B | B | K | B | P0/b |
| 28 | `Truss/CustomCommands` | 0 or 1 | N | B | N | B | B | B | B | K | B | P0/b |
| 28 | `Truss/Overwrites` | 0 or 1 | N | B | N | B | B | B | B | K | B | P0/b |
| 28 | `Truss/Connections` | 0 or 1 | N | B | N | B | B | B | B | K | B | P0/b |
| 28 | `Truss/ChildPosition` | 0 or 1 | N | B | N | B | B | B | B | K | B | P0/b |
| 28 | `Truss/ChildList` | 0 or 1 | N | B | N | B | B | B | B | K | B | P0/b |
| 28 | `Truss/FixtureID` | 1 | p | p | p | N | N | N | p | – | L | P0/i |
| 28 | `Truss/FixtureIDNumeric` | 1 | p | p | p | N | N | N | p | – | L | P0/i |
| 28 | `Truss/UnitNumber` | 0 or 1 | Y | Y | p | p | p | p | p | F,X | P | P0/i |
| 28 | `Truss/CustomIdType` | 0 or 1 | Y | Y | p | p | p | p | p | F,X | P | P0/h |
| 28 | `Truss/CustomId` | 0 or 1 | Y | Y | p | p | p | p | p | F,X | P | P0/h |
| 29 | `Support/@uuid` | R | Y | Y | p | p | p | p | p | F,X | P | P0/i |
| 29 | `Support/@name` | Empty | Y | Y | p | p | p | p | p | F,X | P | P0/i |
| 29 | `Support/@multipatch` | Empty | N | B | N | B | B | B | B | K | B | P0/b |
| 30 | `Support/Matrix` | 0 or 1 | Y | Y | p | p | p | p | p | F,X | P | P0/h |
| 30 | `Support/Classing` | 0 or 1 | Y | Y | N | Y | Y | Y | Y | C | V | P0/c |
| 30 | `Support/Position` | 0 or 1 | p | p | p | p | p | p | p | X | P | P0/a |
| 30 | `Support/Geometries` | 1 | p | p | p | p | p | p | p | F,X | P | P0/r |
| 30 | `Support/Function` | 0 or 1 | Y | Y | p | p | p | p | p | F,X | P | P0/h |
| 30 | `Support/ChainLength` | 1 | Y | Y | p | p | p | p | p | F,X | P | P0/h |
| 30 | `Support/GDTFSpec` | 0 or 1 | p | p | p | p | p | p | p | F,X | P | P0/r |
| 30 | `Support/GDTFMode` | 0 or 1 | p | p | p | p | p | p | p | F,X | P | P0/r |
| 30 | `Support/CastShadow` | 0 or 1 | N | B | N | B | B | B | B | K | B | P0/b |
| 30 | `Support/Addresses` | 0 or 1 | N | B | N | B | B | B | B | K | B | P0/b |
| 30 | `Support/Alignments` | 0 or 1 | N | B | N | B | B | B | B | K | B | P0/b |
| 30 | `Support/CustomCommands` | 0 or 1 | N | B | N | B | B | B | B | K | B | P0/b |
| 30 | `Support/Overwrites` | 0 or 1 | N | B | N | B | B | B | B | K | B | P0/b |
| 30 | `Support/Connections` | 0 or 1 | N | B | N | B | B | B | B | K | B | P0/b |
| 30 | `Support/FixtureID` | 1 | p | p | p | N | N | N | p | – | L | P0/i |
| 30 | `Support/FixtureIDNumeric` | 1 | p | p | p | N | N | N | p | – | L | P0/i |
| 30 | `Support/UnitNumber` | 0 or 1 | N | B | N | B | B | B | B | K | B | P0/b |
| 30 | `Support/CustomIdType` | 0 or 1 | N | B | N | B | B | B | B | K | B | P0/b |
| 30 | `Support/CustomId` | 0 or 1 | N | B | N | B | B | B | B | K | B | P0/b |
| 30 | `Support/ChildList` | 0 or 1 | N | B | N | B | B | B | B | K | B | P0/b |
| 31 | `VideoScreen/@uuid` | R | N | B | N | B | B | B | B | K | B | P0/b |
| 31 | `VideoScreen/@name` | Empty | N | B | N | B | B | B | B | K | B | P0/b |
| 31 | `VideoScreen/@multipatch` | Empty | N | B | N | B | B | B | B | K | B | P0/b |
| 32 | `VideoScreen/Matrix` | 0 or 1 | N | B | N | B | B | B | B | K | B | P0/b |
| 32 | `VideoScreen/Classing` | 0 or 1 | N | B | N | B | B | B | B | K | B | P0/b |
| 32 | `VideoScreen/Geometries` | 1 | N | B | N | B | B | B | B | K | B | P0/b |
| 32 | `VideoScreen/Sources` | 0 or 1 | N | B | N | B | B | B | B | K | B | P0/b |
| 32 | `VideoScreen/Function` | 0 or 1 | N | B | N | B | B | B | B | K | B | P0/b |
| 32 | `VideoScreen/GDTFSpec` | 0 or 1 | N | B | N | B | B | B | B | K | B | P0/b |
| 32 | `VideoScreen/GDTFMode` | 0 or 1 | N | B | N | B | B | B | B | K | B | P0/b |
| 32 | `VideoScreen/CastShadow` | 0 or 1 | N | B | N | B | B | B | B | K | B | P0/b |
| 32 | `VideoScreen/Addresses` | 0 or 1 | N | B | N | B | B | B | B | K | B | P0/b |
| 32 | `VideoScreen/Alignments` | 0 or 1 | N | B | N | B | B | B | B | K | B | P0/b |
| 32 | `VideoScreen/CustomCommands` | 0 or 1 | N | B | N | B | B | B | B | K | B | P0/b |
| 32 | `VideoScreen/Overwrites` | 0 or 1 | N | B | N | B | B | B | B | K | B | P0/b |
| 32 | `VideoScreen/Connections` | 0 or 1 | N | B | N | B | B | B | B | K | B | P0/b |
| 32 | `VideoScreen/ChildList` | 0 or 1 | N | B | N | B | B | B | B | K | B | P0/b |
| 32 | `VideoScreen/FixtureID` | 1 | N | B | N | B | B | B | B | K | B | P0/b |
| 32 | `VideoScreen/FixtureIDNumeric` | 1 | N | B | N | B | B | B | B | K | B | P0/b |
| 32 | `VideoScreen/UnitNumber` | 0 or 1 | N | B | N | B | B | B | B | K | B | P0/b |
| 32 | `VideoScreen/CustomIdType` | 0 or 1 | N | B | N | B | B | B | B | K | B | P0/b |
| 32 | `VideoScreen/CustomId` | 0 or 1 | N | B | N | B | B | B | B | K | B | P0/b |
| 33 | `Projector/@uuid` | R | N | B | N | B | B | B | B | K | B | P0/b |
| 33 | `Projector/@name` | Empty | N | B | N | B | B | B | B | K | B | P0/b |
| 33 | `Projector/@multipatch` | Empty | N | B | N | B | B | B | B | K | B | P0/b |
| 34 | `Projector/Matrix` | 0 or 1 | N | B | N | B | B | B | B | K | B | P0/b |
| 34 | `Projector/Classing` | 0 or 1 | N | B | N | B | B | B | B | K | B | P0/b |
| 34 | `Projector/Geometries` | 1 | N | B | N | B | B | B | B | K | B | P0/b |
| 34 | `Projector/Projections` | 1 | N | B | N | B | B | B | B | K | B | P0/b |
| 34 | `Projector/GDTFSpec` | 0 or 1 | N | B | N | B | B | B | B | K | B | P0/b |
| 34 | `Projector/GDTFMode` | 0 or 1 | N | B | N | B | B | B | B | K | B | P0/b |
| 34 | `Projector/CastShadow` | 0 or 1 | N | B | N | B | B | B | B | K | B | P0/b |
| 34 | `Projector/Addresses` | 0 or 1 | N | B | N | B | B | B | B | K | B | P0/b |
| 34 | `Projector/Alignments` | 0 or 1 | N | B | N | B | B | B | B | K | B | P0/b |
| 34 | `Projector/CustomCommands` | 0 or 1 | N | B | N | B | B | B | B | K | B | P0/b |
| 34 | `Projector/Overwrites` | 0 or 1 | N | B | N | B | B | B | B | K | B | P0/b |
| 34 | `Projector/Connections` | 0 or 1 | N | B | N | B | B | B | B | K | B | P0/b |
| 34 | `Projector/ChildList` | 0 or 1 | N | B | N | B | B | B | B | K | B | P0/b |
| 34 | `Projector/FixtureID` | 1 | N | B | N | B | B | B | B | K | B | P0/b |
| 34 | `Projector/FixtureIDNumeric` | 1 | N | B | N | B | B | B | B | K | B | P0/b |
| 34 | `Projector/UnitNumber` | 0 or 1 | N | B | N | B | B | B | B | K | B | P0/b |
| 34 | `Projector/CustomIdType` | 0 or 1 | N | B | N | B | B | B | B | K | B | P0/b |
| 34 | `Projector/CustomId` | 0 or 1 | N | B | N | B | B | B | B | K | B | P0/b |
| 36 | `Gobo/@rotation` | 0 | N | B | N | B | B | B | B | K | B | P0/b |
| 37 | `Sources/Source` | ? (listed) | N | B | N | B | B | B | B | K | B | P0/b |
| 38 | `Source/@linkedGeometry` | R | N | B | N | B | B | B | B | K | B | P0/b |
| 38 | `Source/@type` | R | N | B | N | B | B | B | B | K | B | P0/b |
| 41 | `Geometries/Geometry3D` | ? (listed) | p | p | p | p | p | p | p | F,X | P | P0/r |
| 41 | `Geometries/Symbol` | ? (listed) | p | p | p | p | p | p | p | D,F,K,X | P | P0/d |
| 42 | `Symbol/@uuid` | R | p | p | N | p | p | p | p | D,F,K,X | P | P0/d |
| 42 | `Symbol/@symdef` | R | p | p | N | p | p | p | p | D,F,K,X | P | P0/d |
| 43 | `Symbol/Matrix` | 0 or 1 | p | p | N | p | p | p | p | D,F,K,X | P | P0/d |
| 44 | `Geometry3D/@fileName` | R | p | p | p | p | p | p | p | F,X | P | P0/r |
| 45 | `Geometry3D/Matrix` | 0 or 1 | p | p | p | p | p | p | p | F,X | P | P0/r |
| 47 | `Projections/Projection` | ? (listed) | N | B | N | B | B | B | B | K | B | P0/b |
| 48 | `Projection/Source` | ? (listed) | N | B | N | B | B | B | B | K | B | P0/b |
| 48 | `Projection/ScaleHandeling` | ? (listed) | N | B | N | B | B | B | B | K | B | P0/b |
| 49 | `Addresses/Address` | ? (listed) | p | p | p | p | p | p | p | F,X | P | P0/r |
| 49 | `Addresses/Network` | ? (listed) | N | B | N | B | B | B | B | K | B | P0/b |
| 50 | `Address/@break` | 0 | p | p | p | p | p | p | p | F,X | P | P0/r |
| 52 | `Network/@geometry` | R | N | B | N | B | B | B | B | K | B | P0/b |
| 52 | `Network/@ipv4` | Optional | N | B | N | B | B | B | B | K | B | P0/b |
| 52 | `Network/@subnetmask` | optional | N | B | N | B | B | B | B | K | B | P0/b |
| 52 | `Network/@ipv6` | optional | N | B | N | B | B | B | B | K | B | P0/b |
| 52 | `Network/@dhcp` | false | N | B | N | B | B | B | B | K | B | P0/b |
| 52 | `Network/@hostname` | optional | N | B | N | B | B | B | B | K | B | P0/b |
| 53 | `Protocols/Protocol` | ? (listed) | N | B | N | B | B | B | B | K | B | P0/b |
| 54 | `Protocol/@geometry` | NetworkInOut_1 | N | B | N | B | B | B | B | K | B | P0/b |
| 54 | `Protocol/@name` | empty | N | B | N | B | B | B | B | K | B | P0/b |
| 54 | `Protocol/@type` | empty | N | B | N | B | B | B | B | K | B | P0/b |
| 54 | `Protocol/@version` | empty | N | B | N | B | B | B | B | K | B | P0/b |
| 54 | `Protocol/@transmission` | undefined | N | B | N | B | B | B | B | K | B | P0/b |
| 55 | `Alignments/Alignment` | ? (listed) | N | B | N | B | B | B | B | K | B | P0/b |
| 56 | `Alignment/@geometry` | first Beam geometry | N | B | N | B | B | B | B | K | B | P0/b |
| 56 | `Alignment/@up` | 0,0,1 | N | B | N | B | B | B | B | K | B | P0/b |
| 56 | `Alignment/@direction` | 0,0,-1 | N | B | N | B | B | B | B | K | B | P0/b |
| 57 | `CustomCommands/CustomCommand` | ? (listed) | N | B | N | B | B | B | B | K | B | P0/b |
| 58 | `Overwrites/Overwrite` | ? (listed) | N | B | N | B | B | B | B | K | B | P0/b |
| 59 | `Overwrite/@universal` | R | N | B | N | B | B | B | B | K | B | P0/b |
| 59 | `Overwrite/@target` | Empty String | N | B | N | B | B | B | B | K | B | P0/b |
| 61 | `Connections/Connection` | ? (listed) | N | B | N | B | B | B | B | K | B | P0/b |
| 61 | `Connection/@own` | R | N | B | N | B | B | B | B | K | B | P0/b |
| 61 | `Connection/@other` | R | N | B | N | B | B | B | B | K | B | P0/b |
| 61 | `Connection/@toObject` | R | N | B | N | B | B | B | B | K | B | P0/b |
| 62 | `Mappings/Mapping` | 0 or any | N | B | N | B | B | B | B | K | B | P0/b |
| 63 | `Mapping/@linkedDef` | R (XSD; UUID reference) | N | B | N | B | B | B | B | K | B | P0/b |
| 64 | `Mapping/ux` | 0 or 1 | N | B | N | B | B | B | B | K | B | P0/b |
| 64 | `Mapping/uy` | 0 or 1 | N | B | N | B | B | B | B | K | B | P0/b |
| 64 | `Mapping/ox` | 0 or 1 | N | B | N | B | B | B | B | K | B | P0/b |
| 64 | `Mapping/oy` | 0 or 1 | N | B | N | B | B | B | B | K | B | P0/b |
| 64 | `Mapping/rz` | 0 or 1 | N | B | N | B | B | B | B | K | B | P0/b |

Additional value, relationship and package requirements:

| Requirement | I / D / E | Untouched / export / PSTG / merge | Tests | Status and action |
| --- | --- | --- | --- | --- |
| Matrix value (T35): 4×3 finite floats; right-handed Z-up, scene distances mm; absent = identity | Y/Y/p | p/p/p/p | F,X | P: float32 model/formatting; exact arbitrary source precision unverified. Nonidentity layer transform blocked. |
| Gobo value: filename, rotation float default 0 (T36) | N/B/N | B/B/B/B | K | B: protected under Fixture/Gobo; image payload not represented. |
| Source value (T39): NDI/File/CITP/CaptureDevice-dependent string, required | N/B/N | B/B/B/B | K | B: parent VideoScreen/Projector/MappingDefinition protected. |
| ScaleHandeling value (T40): ScaleKeepRatio default; ScaleIgnoreRatio or KeepSizeCenter | N/B/N | B/B/B/B | K | B: exact published spelling retained in inventory; no writer. |
| Address value (T51): required absolute integer or Universe.Address; distinct break IDs default 0 | p/p/p | p/p/p/p | F,K,X | P: one break 0 supported; multiple/nonzero/network assignments blocked. Max/int overflow and unusual valid spelling need semantic validation. |
| CustomCommand value: GDTF command syntax, e.g. geometry,f value | N/B/N | B/B/B/B | K | B: no typed commands or static-property ownership. |
| Generic Integer/Float/Bool/String/UUID/Vector/CIE/FileName/IPv4/IPv6 (T1) | p/p/p | p/p/p/p | F,K,X | P: nil UUID disallowed by spec; unique identities/reference graph still incompletely validated. Strings may be trimmed. |
| Persistent UUIDs, parent ownership, referenced Class/Position/Focus/Symdef/MappingDefinition/toObject | p/p/p | p/p/p/p | F,K,X | P: direct Focus validated before recovery and after serialization; Class/Classing validated before normalization/recovery and compared after serialization; remaining reference types partial or blocked. |
| Multipatch: parent UUID; no child FixtureID/CustomID when multipatched | N/B/N | B/B/B/B | K | B: any authored multipatch attribute blocks current canonical writes. |
| GDTFMode required if GDTFSpec present; exact mode of linked GDTF | p/p/p | p/p/p/p | F,X | P: original bytes verified for unmodified synthetic GDTF; fallback and mode substitution remain risks. |
| Unchanged GDTF mandatory byte payload preservation; modified GDTF revision required | Y/Y/p | p/p/p/p | F,X | P: original bytes covered here; all mutation revision rules not exhaustively validated. |
| 3DS in mm; glTF 2.0 units per format; extensionsRequired empty (T46) | p/p/N | p/p/p/p | F,X | P: valid synthetic GLB byte integrity tested; arbitrary geometry extensions/3DS semantics not comprehensively checked. |
| Geometry external images/blobs: all present, root-relative URIs; no absolute resource URI | p/p/N | p/p/p/p | X | P: resource collection checks dependencies; rewriting dependency paths can alter bytes; incomplete geometry validation remains. |
| ZIP PKWARE 6.3.3, *.mvr; one exact root GeneralSceneDescription.xml | Y/Y/N | p/p/p/p | F,X | P: archive writer/parser exercised independently; not a complete ZIP spec audit. |
| ZIP no encryption/password; only STORE or DEFLATE | p/–/N | Y/Y/Y/p | F,X | P: independent generated-package checks; all import rejection combinations not newly tested. |
| ZIP resources at root; filenames cannot differ only in case; case-sensitive lookup | p/p/N | p/p/p/p | F,X | P: independent raw names/generated archives checked; exporter may sanitize/rename collisions. |
| Universal.gdtt optional resource for Overwrites | N/B/N | B/B/B/B | K | B: Overwrites blocked; unreferenced template archival preservation otherwise unverified. |
| Foreign root UserData child structure/content/provider/ver | Y/Y/N | p/p/p/p | F,X | V within compatibility policy: semantic synthetic XML preserved; embedded UUID/resource references remain opaque. |
| Legacy FixtureTypeId (published 1.5 only, optional default 0) | p/B/N | B/B/B/B | K | B: valid legacy information lacks a canonical replacement; detected, not silently omitted. |
| 1.5 FocusPoint direct geometry to canonical 1.6 | Y/Y/p | Y/Y/Y/p | F | V in tested subset only; earlier version full inventories remain unverified. |
| Future/unsupported major MVR → 1.6 downgrade | p/B/N | B/B/B/B | K | B: inspection/import allowed; canonical project/export writes blocked. |

## Evidence index and remaining P0 work

| Evidence owner | What was established by source review |
| --- | --- |
| `models/mvrscene.h`, `models/fixture.h`, `models/sceneobject.h` | Durable scene owns only enumerated maps/fields, root foreign UserData, and now preservation evidence; no arbitrary standard node XML is replayed. FocusPoint uses the shared geometric object value with an explicit kind. |
| `mvr/mvr_class_preservation.*`, `models/mvrscene.h`, typed scene-node Classing fields | Class registry and direct edges; pre-normalization UUID/shape/reference evidence, current-model checks and final serialized projection comparison. Optional-name presence is retained; semantic default equality is used only for merge. |
| `mvr/mvr_scene_node_reader.cpp`, `mvr/mvr_read_service.cpp` | Fixture scalar import, geometry expansion, ignored layer matrix, selected AUXData tables and recursive group ownership. Neutral VideoScreen/Projector descriptors are insufficient to save/export their standard data. |
| `mvr/mvr_xml_scene_object_writer.*`, `mvr/mvrexporter.cpp` | Complete fields actually emitted, geometry registration, placeholder/fallback behavior, resource pruning, fixed canonical 1.6 root and final structural checks. Validation ordering lists are not preservation implementations. |
| `mvr/mvr_export_preparation.cpp`, `core/mvr_identity_recovery.cpp` | Copy before recovery, deterministic repair, fixture identifiers/unit-number allocation and position normalization. New preservation checks run before repairs can conceal the loss. |
| `core/configmanager.cpp`, `core/configservices.cpp`, `mvr/mvr_import_project_application.cpp` | New canonical scene.mvr serialization and reacquisition, staged PSTG publication, retained source resource leases, typed project application. Original input XML is not a hidden durable authority. |
| `mvr/mvr_merge_analyzer.cpp`, `mvr/mvr_merge_applier.cpp`, `mvr/mvr_merge_resource_rewriter.cpp`, `core/scene_clipboard.cpp` | UUID collision maps, imported resource copies, focus reference remapping, child/layer remapping; new issue union and copied-focus remap. Foreign XML references do not share the typed alias map. |
| `core/scene_grouping.cpp`, `core/scene_transform_integrity.cpp`, `core/layer_identity.cpp` | Geometric SceneObject values participate in group ownership, transforms and Layer UUID identity. These services do not restore fields absent from the model. |
| `core/inspection/mvr_inspection.cpp`, `core/inspection/xml_schema_validation.cpp`, `resources/standards/` | Read-only independent libxml2 XSD checking plus semantic diagnostics. Inspection recognizes more source facts than editable persistence supports. Production export uses its own structural checks and does not prove universal semantic equivalence. |

**Known operations that can still alter unrelated valid data:**

- Identifier preparation assumes globally unique positive FixtureIDNumeric,
  repairs duplicates/zero, synthesizes UnitNumber, and can replace unrelated
  authored identifiers. Truss and Support export synthesize FixtureID/numeric
  IDs rather than preserving all authored values. Copy/merge tests here certify
  Focus and Class reference graphs, not a universally lossless identifier policy.
- AUXData retains the tested Class registry/edges, selected Symdef geometry facts
  and Position name tables.
  Direct Symdefs, including names and unused/empty definitions, are now retained
  with resources and collision-safe references. Nested graphs remain blocked.
  Symbol-bearing SceneObject/FocusPoint/Support still expand for display only;
  their canonical export remains protected. Other consumers need a complete audit.
- Remaining GDTF missing-resource fallback, inferred mode, geometry placeholder,
  source filename sanitization and material dependency repairs may change a
  valid authored representation. Direct focus dependencies now fail closed;
  not every other object/resource repair is covered by that rule.
- Standard strings are often trimmed; matrices are stored as float32 and may be
  normalized/rounded. Arbitrary valid source precision, whitespace-sensitive
  names/Function text, integer ranges and all matrix invariants are not proven.
- Foreign root UserData can contain object/resource references that opaque
  merging cannot remap or retain resources for. Existing malformed-provider and
  unsupported Perastage metadata compatibility behavior is not a complete
  preservation policy. This change adds no proprietary drawing metadata.
- The scanner lists **known** lossy fields. Unknown/new standard names,
  unsupported attributes or a future implementation regression are not fully
  covered by semantic provenance. It is not safe to infer that an empty issue
  list certifies losslessness. Earlier-version compatibility is incomplete.
- Existing large-project behavior and every GUI mutation path are not certified
  merely by sharing a value type. A bulk-performance observation is separate
  from cross-platform performance and interoperability validation.

These are explicitly **unfinished P0 requirements**, not waived acceptance
criteria. The branch is a coherent tested correction and protective boundary;
it is not completion of the whole mission.

### Recommended next PRs, in order

1. Complete fail-closed semantic provenance: version-specific element/attribute
   ownership, authored values and reference/resource obligations, comparison
   rules for user-intended mutation, all GUI/Core entry points, and protection
   for currently known unguarded cases above. Preserve or reject source data
   before the edit that makes restoration impossible. Retain bounded memory and
   avoid routine full round-trip cycles.
2. Correct authored identifier/UnitNumber policy across export, PSTG, copy,
   merge and grouping, without treating Annex A’s recommendation for unique selection IDs as
   permission to rewrite unrelated authored values. The informative annex says
   IDs “should” be unique and describes multipatch selection sharing. Add string/number provenance and
   version-specific semantics, including legacy FixtureTypeId conversion.
3. Extend the direct-definition phase to nested Symdef/Symbol graphs and all
   consumers, with canonical node versus display/local placement, multiple and
   mixed geometry entries, complete external dependency ownership and merge
   remapping. Add validated preservation for noneditable VideoScreen/Projector
   and their media resources through existing interchange owners.
4. Typed DMX-break/network assignments and layer elevation,
   with local/world transforms and exact identity/reference remapping. Permit
   these workflows only after their current blockers can be removed safely.
5. Complete protocols, alignments, connections, mappings, commands/overwrites,
   Gobo/Universal.gdtt and GDTF revision/resource obligations. Reconcile the
   documented schema discrepancies with upstream without weakening pinned
   official copies; run the reference implementation and receiving applications
   against a published interoperability corpus.

### Validation scope

New tests: `MvrFocusIntegrity` constructs license-compatible synthetic packages
with a valid minimal GLB and GDTF; checks unchanged/edited/repeated exports,
multiple targets and shared Focus, nested transforms, authored UUIDs/layer
membership, Position, scalar fields/CIE, foreign UserData, project reopen after
source cleanup, merge/copy/group/translation, missing geometry and target,
unsafe-save retention, future downgrade protection and narrow legacy conversion.
`MvrStandardPreservation` covers the known loss diagnostic contracts and final
Focus structure/reference negatives. `MvrFocusIndependent` runs a separate
Python `zipfile`/ElementTree reader, independently checking UUIDs/reference
resolution, root-only/case-unique ZIP names/compression/CRC, authored focus
subtrees and fixture fields, exact GLB/GDTF hashes and opaque foreign metadata.
This is a second read implementation, not an external application's rendering
or full libMVRgdtf interoperability test.

The full unmodified official XSD runs through the existing libxml2 service.
Expected rejection of Data child payload is recorded explicitly; the standard
scene projection validates separately. Whole-file XSD validity and external
interoperability remain **unverified/limited by the documented discrepancy**.

Executed commands/results and regression-before-fix evidence are recorded below
when the final verification finishes. Hosted CI (all platforms, sanitizers,
full test suite, large-project release jobs) and external MVR-xchange peers are
not implied by local focused testing.

### Communication-format review boundary

The published communication section (Tables 65–77) was also reviewed: mDNS
service/subservice, persistent StationUUID/StationName; TCP header 778682,
version 1, count/number/type and uint64 length in network byte order; WebSocket
text/binary modes; JOIN/JOIN_RET provider/version/station/Commits;
LEAVE/FromStationUUID; COMMIT version/FileSize/FileUUID/StationUUID, optional
ForStationsUUID/Comment/FileName; REQUEST FileUUID and FromStationUUID with
last-file behavior; and mutually exclusive ServiceName/ServiceURL for new
session host. Published examples and tables disagree in places (including
StationUUID vs FromStationUUID and the Table 76 ServiceURL self-reference).

Existing message/packet/registry/publication-policy owners remain unchanged.
The scene serialization gate is exercised locally; network negotiation,
WebSocket mode, session switching, remote history and third-party station
interoperability are unverified by this branch. Transport-level success is not
evidence that received MVR semantic content survived editing. Annex A’s
identifier guidance and Annex B’s persistent UUID/reference rationale inform
the remaining identity-policy work above.

## PR #2479 executed verification (historical)

Local environment: Linux, existing `build/wsl-x64-debug` toolchain. No official
schema/reference file or source-size baseline was changed.

| Check | Observed result |
| --- | --- |
| Before-fix reproduction | Restored base reader, export preparation, exporter and XML writer implementations from `1f0e94bd` while retaining the new test harness; the regression aborted at the expected missing FocusPoint-count assertion. Independent Python inspection of that base export found 0 FocusPoints, no Focus/Function/inversion/CustomId fields and CIE `0.312716,0.329001,1.000000` instead of authored `0.314303,0.328065,87.699166`. Corrected sources were restored and rebuilt. |
| Production builds | `perastage_mvr_export`, `perastage_cli` and all focused regression targets compiled and linked. Complete desktop application build was not run. |
| Focused CTest selection | **27/27 passed**: PerastageCliScene; XmlSchemaValidation; MvrInspectionStandalone; MvrMergeAnalyzerApplier; SaveLoadRoundtrip; MvrFocusIntegrity; MvrFocusIndependent; MvrDmxAddressConversion; MvrSupportUserDataRoundtrip; MvrLayerAppearanceUserData; MvrFixtureCategoryRoundtrip; MvrTrussRoundtripStructure; MvrExporterCompliance; MvrImporterArchiveStrict; MvrImporterArchiveLegacy; MvrImporterArchiveRecovery; MvrImporterPackageAcquisition; MvrImportProjectApplication; MvrInspection; RiderImportBenchmark; MvrPatchedGdtfExportMutation; SceneClipboard; MvrIdentityRecovery; MvrExportPreparation; MvrStandardPreservation; MvrExportResourceCollection; MvrLayerUuidIdentity. |
| Existing grouping regression | `SceneGroupingPreservesTransforms`: **1/1 passed**, giving **28 executed local tests** in total. |
| Large-project observation | Existing RiderImportBenchmark input `tests/data/rider_large.txt`: 1129 fixtures, approximately 1.63 seconds import in this run. Single local observation, not a baseline performance comparison or proof for every large MVR. |
| Independent read/validation | Python stdlib ZIP/XML comparison passed for unchanged, edited, PSTG/repeated, copied, grouped, transformed, merged, empty-focus and legacy packages. CRC/compression/path uniqueness, ownership, UUID references, matrices, scalar metadata and GLB/GDTF hashes checked. Libxml2 official-XSD full/projection outcomes checked as described above. |
| Required repository checks | `tests/check_perastage_tree_modules.sh` (with `PERASTAGE_TEST_PYTHON=/usr/bin/python3`), `tests/check_no_configmanager_get_in_gui.sh`, `python3 tests/check_source_file_size.py`, `python3 tests/check_repository_hygiene.py`: passed. |
| Additional ownership/integrity checks | MVR reader/XML/preparation boundary scripts, module dependency directions, include-directory ownership, official validation-schema hashes, generated docs `--check`, and `git diff --check`: passed. |

**Unverified CI:** hosted `linux-debug`, `windows-debug` and `macos-debug` jobs
in `ci-tests.yml`; full PR/release test profiles; sanitizers, installers and all
other workflows. They were not executed or certified by these local checks.
No external application interoperability session or libMVRgdtf runtime test was
performed. The expected official-XSD UserData disagreement is a documented
conformance limitation, not a green full-file schema result.

PR #2479 used `codex/mvr-16-integrity-focus` and has been merged. This continuation
uses a completely new branch, `codex/mvr-class-reference-integrity`, for a new
Draft PR against main. Do not merge automatically.

## Class preservation continuation: contract and verification

The registry is independent of Layer ownership and geometry/resource ownership.
A direct node-to-Class UUID relation survives layer changes, copies, hierarchy
edits and collision remapping. This is the identity foundation for later class
visibility filtering across layers in Viewer2D, Viewer3D and Layout Mode. This
PR adds no filtering, GUI, visibility flags or Layout/CAD implementation.
Visibility presets and documentation settings will be PSTG-owned; they do not
belong in Class definitions or proprietary MVR extensions.

`MvrClassIntegrity` covers valid source schema, unchanged/unrelated-edit exports,
all six supported classed node kinds across two layers, unused/absent-name/empty-name
classes, PSTG reopening after release of original resources, repeated import,
shared/conflicting definitions, equivalent absent/empty defaults without UUID
changes, same-name independent identities, cross-kind
collisions in both directions under all three object collision policies,
copy/group/translation, dangling model references, retained unsafe-save
publication and propagation of source evidence through merge. GLB resources are
reacquired and compared after export/PSTG. `MvrClassPreservation` exercises
pre-normalization evidence and final projection rejection. `MvrClassIndependent`
uses Python ZIP/XML readers to compare definitions/assignments, original UUIDs,
parent ownership, resource bytes, CRC/compression/path integrity and embedded
PSTG scene.mvr. The prior Focus protection regressions remain enabled.

The source fixture passes the unchanged official XSD. Exported full files have
the existing expected UserData/Data-content XSD disagreement; only an explicit
copy with root UserData removed passes the standard-scene projection check.
This is not a claim of full-file XSD conformance.

Executed locally on Linux with `build/wsl-x64-debug`:

| Check | Observed continuation result |
| --- | --- |
| Before-fix reproduction | Initial integration regression aborted at the old export refusal on `a694aa99`. Recompiled the unchanged `origin/main` preservation implementation separately against the final schema-valid source: it rejects all 3 Class definitions and 6 Classing assignments before export. No published schema was relaxed. |
| Production build | `perastage_mvr_export`, `perastage_cli` and all selected regression targets compiled successfully. |
| Selected regressions | **33/33 passed**: PerastageCliScene; XmlSchemaValidation; MvrInspectionStandalone; MvrMergeAnalyzerApplier; SaveLoadRoundtrip; MvrClassIntegrity; MvrClassIndependent; MvrClassPreservation; MvrFocusIntegrity; MvrFocusIndependent; MvrDmxAddressConversion; MvrSupportUserDataRoundtrip; MvrLayerAppearanceUserData; MvrFixtureCategoryRoundtrip; MvrTrussRoundtripStructure; MvrExporterCompliance; MvrImporterArchiveStrict; MvrImporterArchiveLegacy; MvrImporterArchiveRecovery; MvrImporterPackageAcquisition; MvrImportProjectApplication; MvrInspection; RiderImportBenchmark; MvrPatchedGdtfExportMutation; SceneClipboard; MvrIdentityRecovery; MvrExportPreparation; MvrStandardPreservation; MvrExportResourceCollection; MvrLayerUuidIdentity; SceneGroupingPreservesTransforms; MvrXmlSerialization; MvrXmlSerializationHeaderCompile. |
| Required repository checks | Tree/module contract (`PERASTAGE_TEST_PYTHON=/usr/bin/python3`), GUI singleton guard, tracked source-size ratchet and repository hygiene passed. |
| Additional checks | MVR reader/XML/preparation boundaries, module directions, include ownership, official-schema hashes, generated documentation drift and diff whitespace checks passed. |

Hosted `linux-debug`, `windows-debug` and `macos-debug` CI remain separate from
these local results; inspect the new draft PR for their current status. External
receiving applications, live MVR-xchange and libMVRgdtf runtime interoperability
were not executed. No full release profile or sanitizer claim is made. Remaining
P0 identifier/UnitNumber policy, incomplete Symdef/Symbol graph retention,
resources/fallbacks, numeric/string precision and comprehensive semantic
provenance are unchanged and unresolved. Classing within blocked VideoScreen
or Projector data remains **protected**, not supported.


## Symdef/Symbol phase 1: direct definitions and safe instances

Published MVR 1.6 Tables 8–10 require a UUID and ChildList, permit optional name
(default empty), and permit direct Geometry3D and nested Symbol references.
Tables 42–45 distinguish a Symbol instance UUID, its `symdef` reference and local
Matrix from Geometry3D's fileName/Matrix. Geometry3D has no standard geometryType
attribute. The pinned XSD agrees on these structures and orders AUXData as Class,
Symdef, Position, MappingDefinition. Table 1 forbids nil UUIDs. This phase uses
those rules without changing the standard or introducing application XML.

Confirmed corrections:

- Export used a referenced-definition allowlist and pruned valid unused/empty
  definitions and their resources. A schema-valid three-definition fixture
  failed on the pre-fix code (only one definition survived). Export now writes
  the complete canonical definition table, exact optional names, ordered direct
  geometries/matrices and all required self-contained resources.
- Four partial Symdef maps were consolidated into the existing MvrScene owner,
  not a second registry. Inspection includes empty definitions. Definitions are
  independent of GDTF SVG and PSTG project fixture symbols.
- Merge compared file spelling rather than content. Equal same-UUID definitions
  now require ordered geometry, semantic default names, matrices and resource
  SHA-256 equality. A conflict allocates a new incoming UUID and remaps its
  references, retaining the target's identity and bytes. Cross-kind collisions
  cannot overwrite definitions under ReplaceExisting/SkipIncoming either.
  Replacement identities avoid all incoming authored UUIDs. Names do not merge
  unrelated identities. Optional absent/empty names are equivalent for merge;
  target spelling is retained.
- Incoming Truss Symbol identities participate in merge planning. Clipboard
  creates a fresh instance UUID while sharing its definition. Export rejects
  ambiguous nonempty identities instead of silently repairing authored values.
  New model instances with no authored identity retain deterministic generation;
  missing source Symbol identities are protected before normalization.
- Source and final projection checks cover identity/ownership, names, geometry
  count/order, Matrix values, definition resource ownership and instance targets.
  Source double-precision values that the current float/formatter cannot retain
  are protected for definition and Symbol-bearing Truss ancestor matrices.

Supported boundary: named/unnamed, empty/unused and shared definitions containing
only direct Geometry3D with self-contained GLB or dependency-inspected 3DS;
multiple ordered geometries per definition; simple Truss instances with identity
Symbol and first-definition geometry placement, supported parent groups/layers,
and unchanged, edited, copied/grouped/transformed, merged and repeated exports.
Nonidentity matrices on unused definitions or secondary geometries are retained.
Hidden layer display state never removes owned definitions/instances from export.

Still protected: nested Symbol definitions (including unused ones), ambiguous or
missing identities/targets/resources, external geometry dependencies, malformed
or uninspectable resource formats, multiple/mixed Truss geometry entries,
nonidentity Symbol or first definition geometry placement (currently baked into
Truss display transform), source precision outside the current formatter, and
Symbol-bearing SceneObject/FocusPoint/Support. The display projections are not
canonical instance ownership. This phase intentionally does not remove those
blockers. Other standard consumers and cycle semantics remain unresolved.
General matrix provenance and identifier/UnitNumber policy remain P0 work.

Evidence: `MvrSymdefIntegrity` builds independent small schema-valid fixtures,
checks MVR/PSTG after extraction-lease release, repeat export, default names,
shared and conflicting definitions/resources, cross-kind collisions under all
policies, clipboard/group/translation, missing resources and rejected-save
retention. It also tests each valid protected structure against the official
XSD before checking rejection. `MvrSymdefPreservation` exercises malformed/nil/
duplicate identities, dangling references, source precision, 3DS opacity-map
dependencies and deliberately corrupted final projections. `MvrSymdefIndependent`
uses Python zipfile/ElementTree to compare definitions, exact model bytes,
instance/parent relationships, transforms, UUID uniqueness, archive entries and
PSTG scene.mvr without the production reader. The complete standard scene
projection and all resource bytes in PSTG are compared with standalone export
under equivalent options, beyond the Symdef subset. Reopen runs after deletion
of both original archives and lease cleanup, with a second PSTG save/reopen
cycle. It is not an external renderer.
The full/projection XSD distinction above still applies; no external receiving
application or official reference runtime was executed.

### Shared PSTG / canonical MVR serialization invariant

The reported `STG MAHOU.mvr` case (33 layers, 81 fixtures, 19 trusses, 9,661
SceneObjects and 1,038 groups on `4dc4076b2899`) exposes an unacceptable product
limitation: unsupported standard structures can prevent project saving.
Named direct Symdefs are corrected here, but other valid unsupported structures
still block writes. The real file was not supplied in this session; synthetic
tests are not a claim of successful validation of that package.

PSTG embeds the same canonical MVR scene as normal Export MVR with equivalent
options: structures, UUIDs, references, geometry, resources and semantics.
ZIP metadata/compression and irrelevant XML formatting may differ. There must
be no reduced project-only MVR representation, alternate scene model, validation
bypass or PSTG recovery metadata compensating for missing standard content.
PSTG documentation, styles, project fixture symbols and visibility settings stay
separate. The latest architectural clarification supersedes the earlier proposal
to decouple native scene storage; that proposal is not implemented.

The safe sequence is genuine support for each remaining structure in the shared
exporter: separate canonical node/display placement, typed geometry instances
for all supported consumers, nested Symdef graphs with cycle/reference checks,
and complete package dependency ownership. Remove each blocker only after MVR
and PSTG tests prove preservation. Until then keep structured diagnostics and
the previous valid destination. Do not substitute an original unedited archive,
clear issues, invent geometry, or promise that accepting a warning can produce a
lossless file. No lossy-export override is introduced here.

The Save/Save As busy-overlay lifecycle and error-dialog ordering are a separate
small GUI correction and do not change this Core serialization contract.

### Phase 1 executed verification

Local Debug build: **40/40 selected tests passed**, including all three independent
ZIP/XML readers, Symdef source/final-projection protection, existing Class/Focus/
layer identities, inspection, clipboard, transforms, CLI, merge, resource/export
preparation, archive recovery and save/load regressions. The pre-fix integration
failed because three valid definitions became one. Module-tree, GUI ConfigManager,
source-size, repository hygiene, MVR reader/XML/preparation/resource/inspection
boundaries, module directions, include ownership and official schema guards passed.
Generated Next user documentation is updated; no frozen Stable page was changed.
`node tests/check_static_docs.cjs` passed after enabling the local network
capability needed by its fixture subprocesses. Its initial restricted-sandbox
run failed with empty child output on both this branch and untouched latest
main. Browser navigation checks were unavailable because this environment has
no installed Playwright Chromium. Hosted CI status is reported in the PR.
No Windows/macOS runtime, real STG MAHOU package, or external application
interoperability result is claimed.

Hosted macOS compilation caught one remaining read of the removed partial map
in the view-cache asset fingerprint. It now traverses every canonical geometry,
including secondary/unused definition resources, and its application object
compiles locally. The ownership guard now searches all C++ sources, including
GUI/viewers, rather than only Core/MVR/models. This is an ownership migration,
not a new Layout feature. The corrected commit triggers fresh hosted CI.
