# GDTF read/write compatibility policy

Perastage separates tolerant read/import behavior from explicit write behavior.

## Tolerant read and import

Reading a GDTF must be non-mutating and should maximize interoperability with files produced by other software. Perastage accepts the canonical archive root `description.xml` first. If that exact entry is absent, it may accept one root case-insensitive match such as `DESCRIPTION.XML` with a compatibility diagnostic. If no root candidate exists, it may accept one unique nested case-insensitive `description.xml` candidate as a non-standard compatibility fallback.

The reader rejects ambiguous or unsafe input instead of guessing. Multiple root case-insensitive candidates, multiple nested candidates when no root exists, unsafe archive paths, unreadable or empty selected descriptions, malformed XML, missing `GDTF`, missing `FixtureType`, and fixture insertion archives without usable named `DMXMode` entries fail with structured diagnostics.

Public GDTF read and fixture insertion preparation boundaries convert malformed archives, filesystem failures, ZIP failures, XML failures, and unexpected exceptions into diagnostics. These paths must not let ordinary invalid input escape into the GUI event loop as exceptions.

## Fixture insertion preparation

`PrepareGdtfFixtureInsertion(...)` is a non-GUI read-only service used before the Add Fixture dialog opens. It validates the source file, reads archive identity and modes through the shared read-only services, returns fixture display name, ordered DMX modes, optional physical values, optional model color, standards-compliance status, compatibility-fallback status, and structured diagnostics. It does not show dialogs, mutate the scene, modify the GDTF, extract files permanently, or invent missing modes.

## Standards-compliant writes

Tolerant reads do not canonicalize or rewrite input files. Explicit Perastage writes remain standards-oriented: mutation, generated derivative, canonicalization, and export paths must write `description.xml` at the archive root and use the approved Perastage canonicalization and mutation policy for the operation. Unknown resources and custom XML are preserved where the existing writer supports preservation. This policy does not broaden the current writer into a complete GDTF schema rewriter.
