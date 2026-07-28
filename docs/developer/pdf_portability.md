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

Positioned fragments are reconstructed independently from file I/O. Fragments
are ordered stably from top to bottom and left to right. Reconstruction uses
bounding-box height when available to establish coordinate tolerance, falls
back conservatively when geometry is unavailable, uses both advance and bounds
to find the preceding fragment end, preserves literal spaces, inserts one space
for a meaningful positive horizontal gap, and inserts one newline between
distinct lines and pages.

## Deterministic fixtures

PDF fixtures are binary Git assets (`*.pdf` and `*.PDF`) and must never undergo
line-ending conversion. `tests/support/pdf_test_fixture_builder.h` creates
minimal runtime controls without timestamps, compression, platform newlines, or
external applications. It also validates checked-in and generated fixtures:

- each direct stream `/Length` matches its exact byte count;
- every in-use classic xref entry points to its object header;
- `startxref` points to the `xref` token;
- the PDF header and terminal `%%EOF` marker are present.

The checked-in `simple.pdf` and `translated.pdf` fixtures use LF bytes and exact
companion `.txt` expectations. To regenerate an equivalent fixture, use the
test builder with the documented content stream, write its returned string in
binary mode, and update companion text only when the intended extraction
contract changes. Always run `PdfTextComparison` after regeneration.

## Writer rules

PDF numeric tokens use the classic locale, regardless of the process locale.
The writer uses checked wide stream offsets, enforces the ten-digit classic xref
limit, validates the requested catalog object, checks every serialization
stage, and explicitly verifies flush and close finalization before reporting
success.
