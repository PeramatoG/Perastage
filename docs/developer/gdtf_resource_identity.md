# Fixture GDTF derivative ownership

## Published derivative contract

`Manufacturer@FixtureType@Perastage.gdtf` is a Perastage ownership convention,
not an official GDTF semantic. A file may use that canonical name only after it
contains exact, parseable top, bottom, front, and side SVG fixture views with a
positive view box and usable geometry. Publication validates that ownership-neutral
contract before the library dictionary accepts the derivative. External GDTFs that
already meet the contract remain authoritative regardless of their Editor metadata;
Perastage ownership is retained separately for audit and mutation policy.
Failed finalization leaves the previous source or published derivative intact.
Perastage-authored changes continue to use standard GDTF fields and Revisions;
no proprietary GDTF or MVR XML is introduced.

Symbol generation first copies the selected source to a non-canonical `.working`
archive in the project fixture directory. The existing archive rewriter applies all
four SVG views to that file, validates the published derivative contract, and only
then atomically replaces the canonical project archive. `Fixture.gdtfSpec` is rebound
after publication, including every fixture of the same source/type family. Any failure
before publication removes the working file where safe and leaves both the previous
archive and all fixture references unchanged.

During a project edit, `Fixture.gdtfSpec` and its successfully published project-owned
file are authoritative. Library synchronization is secondary: its failure is a
warning and cannot roll back a valid project result. Successful publication invalidates
the runtime parsed-SVG cache by physical path.

The project fixture GDTF editor uses the same boundary. A derivative edit is applied
to private working storage, then the four-view contract is validated before atomic
project publication. Incomplete external GDTFs remain unchanged and produce an
actionable diagnostic; the editor never uses the active library dictionary as scratch
storage and never exposes a `.working` reference to project fixtures.

## Replacement identity

Imported source identity, selected replacement identity, project ownership, and the
user-facing fixture label remain separate. Within one import transaction, an explicit
GDTF Share RID plus exact compatible mode proves that different source aliases chose
the same replacement. The download is reused and affected fixtures share one project
reference. Similar names alone never prove equivalence.

## Startup and persistence

Current projects store `gdtf_derivative_contract_version = 1` in project `config.json`.
They load their referenced SVGs directly and perform no whole-scene symbol generation
or persistent symbol-manifest validation. Project Save serializes the scene and exact
referenced resources without generating or repairing symbols and does not write
`perastage_symbol_cache_manifest.json`.

Projects without the contract version run one bounded legacy migration over unique
referenced GDTFs. The migration may unify historical numbered copies only when their
authoritative base content and exact mode match after removing the narrowly recognized
legacy Perastage symbol outputs. Unsupported or ambiguous resources remain untouched,
keep the migration incomplete, and never prevent Save. Successful migration marks the
project dirty and records contract version 1 for subsequent opens.

The parsed SVG runtime cache remains independent persistence-free infrastructure. It
keys stored presentation data by physical resource and view and is invalidated after
internal derivative replacement or cleared at project lifecycle transitions.

Automatic preparation uses runtime-only work identities made from the deterministically
resolved physical GDTF path and the exact GDTF mode. Repeated fixture and renderer
requests coalesce, while distinct exact modes remain distinct work. Publication to a
shared canonical derivative is serialized. A project epoch rejects callbacks and
publication from replaced or closed projects.

Capture progress is cooperative: each expensive render view is a separate GUI-thread
step, and CPU processing and transactional publication are explicit later states.
Missing or invalid SVGs continue to use rendered geometry immediately. Preparation
does not wait in Save, Load, MVR import/export, print, layout, or PDF operations and no
queue state or symbol manifest is persisted.
