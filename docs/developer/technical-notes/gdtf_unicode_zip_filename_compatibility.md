# GDTF Unicode ZIP filename compatibility

Perastage reads and extracts GDTF archives with a shared archive reader that inventories ZIP entries before higher-level description, insertion, preview, and resource code consumes them. The reader keeps archive reads non-mutating: opening or inserting a GDTF never rewrites ZIP metadata, renames resources, changes timestamps, or normalizes XML values.

## Compatibility case

Some otherwise usable GDTF archives store Unicode wheel, gobo, thumbnail, or model resource names as UTF-8 bytes but omit ZIP general-purpose bit 11, the standard UTF-8 filename flag. wxWidgets decodes such names with the local legacy converter by default. On Windows this can fail for characters outside the active code page and previously allowed a conversion exception to escape into the GUI event loop before `description.xml` was reached.

## Decoding policy

The archive reader now reads raw central-directory filename bytes and applies one decoding policy for all GDTF read and extraction paths:

- Entries with ZIP UTF-8 bit 11 set are decoded strictly as UTF-8. Invalid UTF-8 is reported as `GDTF_ARCHIVE_FILENAME_DECODE_FAILED` and the archive read fails cleanly.
- Unflagged ASCII names are accepted without a compatibility warning.
- Unflagged names whose raw bytes are valid UTF-8 are decoded as UTF-8, marked as a compatibility fallback, and reported once per archive with the affected entry count.
- Unflagged names that are not valid UTF-8 are not passed through a lossy platform ANSI conversion. They fail with a structured diagnostic until a safe reversible legacy decoder is added.
- Ambiguous filename interpretations must fail without guessing.

Raw central-directory identities are validated before wxWidgets streams any
payload. A malformed identity therefore cannot be skipped by the ZIP stream and
shift the sequential association between later raw names and payloads. The
reader never substitutes a platform-decoded name for an identity that is
explicitly marked as UTF-8 but contains invalid bytes.

The compatibility warning uses `GDTF_ARCHIVE_UTF8_FALLBACK_USED` semantics and reports a message such as: `GDTF archive uses valid UTF-8 filenames without the ZIP UTF-8 flag; compatibility fallback applied to N entries.`

## Path safety and Windows behavior

Filename decoding happens before path normalization, extraction, and safety checks. The decoded archive path must still be relative, must not contain drive or root syntax, must not contain `..` traversal, and must not contain unsafe empty paths. Unicode fallback does not weaken traversal protection.

On Windows, callers keep filesystem paths as `std::filesystem::path` for native
operations and pass them to wxWidgets with
`WxPathUtils::WxStringFromFilesystemPath`. UTF-8 conversion is reserved for
serialization, storage, or logging boundaries; converting a native path through
`path.string()` or `path.generic_string()` at a wxWidgets filesystem boundary is
not permitted. GDTF archive entry names are stored as UTF-8 text after safe
decoding, independent of the operating-system locale, and extraction writes
through native `std::filesystem::path` objects.

## Fixture classifications

- `standard-strict`: canonical `description.xml`, ASCII names without bit 11,
  and valid Unicode names with bit 11.
- `legacy-compatibility`: independently documented legacy formats with a safe,
  reversible interpretation.
- `tolerant-recovery`: valid UTF-8 Unicode names missing bit 11 and recoverable
  case or nesting variants of `description.xml`; each recovery is diagnosed.
- `malformed-byte`: invalid UTF-8 bytes with bit 11 set, patched structurally in
  the intended local and central filename records; the archive identity is
  rejected with `FilenameDecodeFailed`.
- `platform-specific`: Unicode outer filesystem paths, created as native
  `std::filesystem::path` values and opened through the shared wx path helper.

## Read/write separation

Perastage reads non-standard but unambiguous GDTF archives permissively and reports diagnostics. Reads remain byte-preserving. Explicit Perastage-generated GDTF ZIP output continues to use wxWidgets UTF-8 entry names so Unicode resource names are written with standards-compliant UTF-8 filename metadata and can be reopened by compliant applications.

## Regression coverage

Focused read-service tests generate small temporary archives with Unicode names containing Greek Phi and Chinese characters, then read and extract them through the shared policy. The tests deliberately clear ZIP UTF-8 flags in both local and central headers to verify fallback recovery, aggregate warning counts, description lookup after Unicode entries, wheel resource resolution, strict invalid flagged UTF-8 failure, DMX mode whitespace preservation, and UTF-8 flag metadata on explicit Unicode ZIP output.
