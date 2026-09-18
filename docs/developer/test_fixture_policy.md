# Test fixture policy

Perastage tests classify fixtures by contract so standard conformance, legacy compatibility, tolerant recovery, Perastage extensions, and platform-specific behavior remain separate.

## CTest policy labels

Every touched test should include one policy-layer label:

- `standard-strict`: verifies current GDTF 1.2, MVR 1.6, or repository policy behavior without compatibility exemptions.
- `legacy-compatibility`: verifies intentional support for older or noncanonical inputs.
- `tolerant-recovery`: verifies recovery from readable but imperfect inputs with diagnostics.
- `perastage-extension`: verifies behavior that is owned by Perastage rather than an external standard.
- `platform-specific`: verifies behavior that only applies to a specific operating system or toolchain.

Domain labels such as `gdtf`, `mvr`, `rider`, `project`, `layout`, `pdf`, `zip`, `ui`, `policy`, and `security` should be added where they make CTest filtering clearer.

## Fixture categories

- `valid` fixtures are standards-aware inputs that should pass normal validation and canonicalization without exemptions.
- `invalid` fixtures intentionally violate a standard and are used only for rejection, security, or diagnostic tests.
- `legacy` fixtures represent historical files that Perastage intentionally keeps importable.
- `recovery` fixtures are readable archives with recoverable defects that should produce diagnostics instead of being described as standards-valid.

## Fixture sources

Normal pull-request tests must use local deterministic fixtures and must not
download live GDTF Share or schema content. Pin any official fixture or schema
needed for conformance testing in the repository according to its license and
the repository hygiene policy.

Use `tests/support/gdtf_test_fixture_builder.*` for small deterministic GDTF archives needed by unit and integration tests. Use static golden files when a test must not depend on the builder implementation itself. Use official schemas or public official fixtures when the contract being tested is schema conformance or interoperability with published standard examples.

Builder-generated archives must use deterministic entry order, `/` separators, canonical UUIDs for strict tests, and secure relative archive paths. Deliberately malformed byte-level ZIP fixtures should stay separate from ordinary builders so archive-reader edge cases remain deterministic across wxWidgets, zlib, and platforms.
