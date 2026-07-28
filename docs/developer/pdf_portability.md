# PDF text and serialization portability

## Text extraction contract

`ExtractPdfTextWithResult` is the diagnostic core API. A successful result can
contain an empty `text` value, which means the PDF was parsed successfully but
contained no extractable text. A failed result has `success == false` and a
non-empty `error` describing an unreadable file, malformed PDF, or unsupported
content. PoDoFo types and exceptions remain private to the core implementation.

`ExtractPdfText` remains the compatibility wrapper and returns only the text.
Callers that need to distinguish an empty document from a parser failure must
use the diagnostic API.

For PoDoFo 0.10 and newer, extraction retains the high-level bounding-box
entries for diagnostics but reconstructs output from operation-boundary
fragments. The pinned PoDoFo 1.1.1 `PdfContentStreamReader` boundary uses public
type, error, operator, stack, and XObject accessors with explicit image-skipping
flags. It preserves text-show, positioning, text-state, and graphics-state
boundaries before the high-level extractor can merge them.
Strings and advances use the active PoDoFo font, including its public Standard
14 metrics for width-less Helvetica; missing active metrics or undecodable
strings produce an explicit extraction failure rather than a word heuristic.
Tests print raw high-level fragments only after a strict mismatch, and
production extraction does not log them.

The operation reader validates every operand stack before indexing it. Its
`q`/`Q` frames restore the CTM, active font, text-state parameters, leading,
rise, and resource context without rewinding text matrices. Form XObject frames
apply the form matrix, prefer form-local resources when present, inherit the
parent resource boundary otherwise, and restore all parent state on exit.

Positioned fragments are reconstructed independently from file I/O. Fragments
are ordered stably from top to bottom and left to right. Reconstruction uses
bounding-box height when available to establish coordinate tolerance, falls
back conservatively when geometry is unavailable, uses both advance and bounds
to find the preceding fragment end, preserves literal spaces, inserts one space
for a meaningful positive horizontal gap, and inserts one newline between
distinct lines and pages.

## Deterministic fixtures

PDF assets (`*.pdf` and `*.PDF`) must never undergo line-ending conversion.
`PdfTextComparison` first creates minimal explicit-width runtime controls in a
unique system-temporary directory and removes that directory through RAII. The
builder emits exact LF-only bytes without
timestamps, compression, platform newlines, or external applications. It
validates each in-memory fixture before writing it:

- each direct stream `/Length` matches its exact byte count;
- every in-use classic xref entry points to its object header;
- `startxref` points to the `xref` token;
- the PDF header and terminal `%%EOF` marker are present.
- the Type 1 Helvetica font uses WinAnsi encoding, the ASCII range from 32 to
  126, exactly 95 deterministic widths, and a nonzero explicit space width.

The runtime simple stream contains one `Hello World` text-show operation. The
runtime translated stream shows `Foo`, applies exactly `50 0 Td`, and then shows
`Bar`; its expected result is strictly `Foo Bar`. Companion expected-text files
are forced to LF on checkout. Their reader also canonicalizes CRLF and lone CR
line endings, preserves ordinary spaces and internal blank lines, and removes
only the historical trailing newline/form-feed sequence.

After the runtime controls, the test directly reads the unchanged checked-in
`simple.pdf` and `translated.pdf` from the source fixture directory. These
width-less Standard 14 compatibility regressions are structurally validated and
strictly compared with their companion text. CMake neither copies nor generates
them, and their binary bytes remain unchanged.

## Writer rules

PDF numeric tokens use the classic locale, regardless of the process locale.
The writer uses checked wide stream offsets, enforces the ten-digit classic xref
limit, validates the requested catalog object, checks every serialization
stage, and explicitly verifies flush and close finalization before reporting
success.
