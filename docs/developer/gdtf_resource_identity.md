# Fixture GDTF derivative ownership

## Published derivative contract

`Manufacturer@FixtureType@Perastage.gdtf` is a Perastage ownership convention,
not an official GDTF semantic. A file may use that canonical name only after it
contains readable top, bottom, front, and side SVG fixture views. Publication
validates that contract before the library dictionary accepts the derivative.
Failed finalization leaves the previous source or published derivative intact.
Perastage-authored changes continue to use standard GDTF fields and Revisions;
no proprietary GDTF or MVR XML is introduced.

During a project edit, `Fixture.gdtfSpec` and its project-owned file are
authoritative. Library synchronization is secondary. Explicit symbol regeneration
updates that same derivative and invalidates the runtime parsed-SVG cache by physical
path.

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
