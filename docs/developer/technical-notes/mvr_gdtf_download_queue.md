# MVR GDTF matching and resolution

The initial MVR/App/Download source dialog remains the import entry point.
Download rows subsequently use the existing `GdtfDownloadQueueDialog` presentation
and its single native modal boundary. `MvrGdtfResolutionDialog` extends that
presentation with a selected-row candidate list, available resource choices,
full-catalog search and local-file selection. The importer delegates to
`RunMvrGdtfResolution` and applies only final validated resource/mode pairs.

## Ownership

- `mvr/gdtf_catalog_matcher.*`: identity normalization and technical scoring.
- `mvr/gdtf_candidate_ranking.cpp`: one catalog evaluation implementation,
  structured candidates and conservative automatic recommendation. The existing
  best-candidate adapter remains available to legacy consumers.
- `mvr/gdtf_patch_evidence.*`: conservative repeated-address-spacing hints for manual review.
- `perastage_gdtf_matching`: standard-library-only production target.
- `core/gdtf_resolution.*`: typed source, proposed/applied choices, origin,
  fallback precedence, statuses and completion/cancellation transitions.
- `core/gdtf_resolution_resources.*`: validation through the existing archive,
  description and mode readers; immutable copies into the import-owned scene
  workspace. The separate `perastage_gdtf_resolution_resources` adapter links
  existing readers. `perastage_gdtf_resolution` remains standard-library-only;
  the resource readers retain the existing wxBase ZIP dependency, not wxWidgets GUI controls.
- `gui/mvr_gdtf_resolution_workflow.*`: catalog service, credentials, session
  retry, staging and composition of callbacks.
- `gui/mvr_gdtf_resolution_worker.cpp`: bounded queue scheduling, background
  catalog analysis/download and event delivery; joins each worker before reuse.
- `gui/gdtf_download_queue_table.*`: native data-view presentation, row colours and
  mutually exclusive source toggles. Unavailable sources cannot be activated.
- `gui/mvr_gdtf_resolution_sources.cpp`: completed-package mode review and reversible
  source selection; the source table never owns ranking or network work.
- `gui/mvr_gdtf_resolution_actions.cpp`: native picker, mode/review prompts and
  the existing `GdtfSearchDialog` in selection-only mode.

## Ranking and confidence

Evaluate the parsed catalog once per fixture type, never during widget refresh.
Keep eight useful revisions by default. Exact canonical identity precedes partial
model evidence; safe candidates precede review candidates. Within a tier, retain
stable technical ordering (authoritative identity, name, numeric compatibility,
manufacturer, mode, footprint), then uploader metadata, modification time, rating
and lexical revision ID. Reputation, recency and rating cannot promote a model
variant to automatic eligibility. Roman numerals retain their established numeric
normalization. Existing descriptive/version stripping remains a legacy search
compatibility rule, not proof that two revisions are safe replacements.

`Custom`, `Generic`, `Unknown`, empty and `No Manufacturer` metadata are unknown.
A catalog manufacturer's literal prefix may be removed from the imported model
for canonical comparison; substring matches do not establish manufacturer identity.
When exactly one manufacturer prefix is supported by exact model evidence,
conflicting catalog manufacturer metadata requires review. Multiple supported
manufacturer prefixes remain ambiguous. Known conflicting manufacturers require review. Catalog UUID and FixtureTypeID
are logged separately; neither establishes revision identity or model equivalence.

Automatic recommendation requires exact canonical model identity, no manufacturer
or numeric conflict, an identified compatible mode and agreement with any known
footprint. Mode-digit or footprint-only evidence remains available for review.
A matching footprint never proves channel semantics. Tied competing model
identities or manufacturers without supporting prefix evidence are ambiguous.
Confidence is `Safe`, `Review`, `Ambiguous` or `NoMatch`; only `Safe` can supply a
recommendation. Full-catalog manual selection can intentionally choose another
model, after a compatibility prompt and explicit mode selection.

`CandidateRanking.downloadRid` separately identifies an unambiguous exact model
without requiring mode equivalence. `CatalogIntent::DownloadRequested` is passed
only for the rows the user already assigned to Download. It selects that revision
with `SelectionOrigin::DownloadRequested`; a mode mismatch leaves the proposed
mode empty and does not suppress the transfer. Other `Analyze` callers keep the
conservative automatic-apply default. Manufacturer/model ambiguity and numeric or
variant conflicts still require an explicit candidate selection.

The legacy `SelectBestDownloadMatch` retains its established fuzzy best-candidate
contract for existing consumers. It is not the automatic-apply policy. New import
code consumes `RankDownloadCandidates` through `gdtf_resolution::Analyze`.

## Selection and application lifecycle

`Resolution.selected` is the proposal; `Resolution.applied` is the validated
resource to commit. Explicit user selection takes precedence over automatic
recommendation. Choosing an existing MVR, application or local profile also
records it as the fallback for later catalog attempts. A failed revision retains
that valid fallback and the proposed revision for inspection/retry. The low-level
`Cancel` transition preserves applied choices for other consumers. The interactive
MVR queue instead treats cancellation as rollback: every row returns its original
MVR path with an empty mode override, preserving each instance's imported mode.
The workflow also checks the modal return code before applying results. Late completion cannot replace
a different proposal. Packages are cached by revision; each application validates
the actual selected mode and checks the retained file. Changing modes or returning
from MVR/library to the downloaded package does not repeat its transfer.

Only structurally usable resources/modes appear in the existing-source selector.
No fallback mode is guessed from matching footprint or address spacing. Local
files and downloaded packages are read with shared Core services and copied to
unique names under `resolved-gdtf/` in the import workspace. The scene's existing
resource lease owns their lifetime; original files, the MVR archive and the
application library are not overwritten. A downloaded automatic replacement is
validated against actual package identity, manufacturer, mode and footprint before
commit. Missing catalog mode metadata is resolved from the downloaded package
for explicit user selections.

After a requested download, actual identity is validated independently of mode.
Uncertain modes use the native mode picker and compatibility confirmation. A
canceled mode choice records `ModeRequired`, retains the package separately from
the applied fallback, and disables acknowledgement until a mode or another source
is selected. Exact archive-byte comparison runs in the worker after download;
`Identical` still applies the downloaded source and explicitly reports equality.
Neither catalog UUID nor matching XML identity is used as a file-equality test.

The table exposes MVR, Perastage and Selected file as exclusive source checkboxes.
Switching to MVR preserves each imported mode; switching back restores the validated
download/local choice. Available package modes also appear in the existing mode
selector. Revisions without a reliable model match give an explicit review reason
instead of implying that an original file was reused because it was identical.

After processing, the user can select any row, including successful rows, revise
its proposal, and apply again. Acknowledgement is disabled while proposals are
pending or work is active. The existing MVR preference
`mvr_auto_close_gdtf_download_window` retains its default **off** and its existing
Preferences → MVR Import/Export location; the queue has no preference checkbox. Enabled
automatic close commits safe final choices without a confirmation prompt.
Ambiguity, unresolved resources or failures keep the queue open for review.

Download, reuse and local selection use green success styling; original MVR and
application fallback and downloaded packages awaiting a mode use amber; failures and unresolved profiles use red; canceled
rows use muted styling. Candidate labels list every known mode/footprint and
explicitly mark absent metadata as unavailable. Actual downloaded modes enrich
the row. Original package mode counts supplement imported evidence. Repeated
same-universe patch spacing (at least two equal adjacent intervals for one type
and mode, no duplicate/invalid addresses) can preselect a mode for manual review;
it never changes authoritative footprint evidence or automatic eligibility.

The modal queue appears before credential-store reads. Catalog, saved-credential
login and downloads run in its worker. Only native credential entry and warnings
run on the GUI thread. A 250 ms activity timer shows elapsed phase time; row events
publish analysis results and progress incrementally. Original/application profiles
are parsed once per unique path; canonical request models are reused per
manufacturer during ranking. Ranking checks cancellation between catalog entries. Technical logs record catalog
load, per-row profile-read and ranking times separately.
Login, catalog and download use libcurl's multi interface with 100 ms polling for
cancellation, including connection setup, while retaining established timeouts.
Synchronous secure-store calls and individual archive/JSON reads are not
interruptible; cancellation checkpoints surround those operations.

Cancel, Escape and window close request rollback both during work and after
completion. The worker unwinds and joins before the single modal boundary returns;
late results cannot commit a substitution after cancellation. Only OK or successful
automatic close accepts the queue choices. Other explicitly chosen sources in the
preceding MVR/App/Download dialog retain their independent choices.

Application updates every instance with the originating fixture-type key. Explicit
queue modes bypass the importer's permissive mode resolver after validation.
Fixture UUIDs, labels/type keys, addresses, transforms, groups and unrelated types
are not rewritten. Existing project resource consolidation/export owns MVR
publication; no extension XML or alternate package format is introduced.

## Future command and protocol boundary

The existing CLI reads files through Inspection Core and dispatches typed semantic
operations through Command Core and local IPC. OSC is a frontend adapter; the MCP
adapter delegates to the CLI. None needs a dialog to call these new domain APIs.

A future adapter can expose analysis inputs (identity evidence, immutable catalog
snapshot, existing validated resources and optional explicit `Choice`) and serialize
`CandidateRanking`/`Resolution` to JSON. A separate mutation command would acquire
an import/project resource lease, resolve a revision and mode, validate bytes,
and atomically apply the final resource map through the normal project transaction.
Inspection Core remains read-only; catalog acquisition/download is outside it.

No new CLI command, OSC route or MCP tool is registered. Blockers are a versioned
JSON schema for evidence/reasons/statuses, a headless credential/cancellation
policy, an import-session/resource handle, and a Command Core transaction adapter
for applying replacements to an active scene. The standard-only matching target
can already be linked independently; package validation uses the existing Core
read target and wxBase ZIP runtime.

## Validation

`MvrGdtfImportMatching` retains the existing regressions. `GdtfResolution` covers
ranked revisions, generic/prefixed manufacturers, Roman numerals, ambiguity,
model/mode/footprint mismatches, UUID non-equivalence and manual/fallback/reuse/
cancellation precedence. `GdtfResolutionResources` covers local package identity,
modes, immutable ownership, malformed/missing definitions and failed publication.
`MvrGdtfResolutionDialog` exercises automatic completion, successful-row changes,
independent aliases, exact revision reuse, cancellation and offline fallback with
an injected deterministic backend. Existing catalog and queue lifecycle tests
remain in place. `GdtfResolutionBoundary` guards the new separation.

The Tivoli binary was not available. Tests use explicitly synthetic metadata and
small generated packages and do not contact GDTF Share.

Types with several imported DMX modes require review before a catalog replacement.
Validated MVR/application fallbacks can preserve the full imported mode set; a
mode explicitly chosen by the user applies to every instance of that type. This
prevents an automatic suggestion based on the first instance from silently
rewriting other instances' modes. Address spacing is not treated as a verified
GDTF footprint.

Apply selections retries failed catalog rows. An authentication cancellation or failure stops pending attempts as one batch, retaining every valid fallback and completed result without repeating the sign-in dialog for each row.
