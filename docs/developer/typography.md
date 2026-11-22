# Document typography and font licensing

Perastage resolves the requested family, weight and style to the exact approved
font bytes before composition. `FontCatalog` owns immutable resource data,
version, PostScript face name, OFL provenance and SHA-256. Rendering targets use
the resolved fingerprint and glyph ID. Persisted styles keep the family and
variant authored by the user, so substituting a face does not change the project.

## Approved resources

PerastageSans is the application name for the original Noto Sans face. It is the
default family. PerastageSans, Noto Serif and Source Sans 3 offer Regular, Bold,
Italic and Bold Italic. Noto Sans Mono offers Regular and Bold. Noto Sans
Arabic and Noto Sans Hebrew supply upright Regular and Bold script fallback.
[Third-party notices](../../THIRD_PARTY_NOTICES.md) identify the exact releases,
files and licenses. Only these SHA-256 identities enter the catalog.
The files are static single-face TrueType fonts with unrestricted embedding
flags. An unrestricted `OS/2.fsType` on its own gives no license evidence.

The packaged catalog checks its pinned files at application initialization.
The existing targeted probes and bounded file walk can recover a missing
portable face only when its fingerprint matches a bundled approval record.

## Installed fonts

DirectWrite on Windows, CoreText on macOS and Fontconfig on Linux enumerate
registered font metadata. Windows uses DirectWrite's local file provider.
macOS resolves the CoreText PostScript name against a file's faces. Linux
uses Fontconfig's index. Remote, private or provider-only faces without a
readable file are outside this scope. Enumeration records family, style and
file identity and stats each distinct file once. The first selection validates
the face in FreeType and hashes the selected bytes. Ordinary installed faces
are independent of the bundled allowlist. No system font is redistributed
with the application or written to PSTG, MVR, GDTF or a Layout template.

The catalog holds immutable discovery data and bounded loaded-face caches.
Resolution and PDF registries keep shared ownership of any resources they
use. Glyphs carry owned fingerprint strings. FreeType outlines and HarfBuzz
shaping use the same file bytes and collection index. The cache rejects an
unloaded face if its file changed after discovery. Refresh publishes a new
snapshot, allowing open project references and render caches to resolve
against the updated font collection.

System font references keep the authored family and the per-style PostScript
names and fingerprints when available. Layout rich text records these
references in run properties. Resolution tries the exact identity, a
compatible named or same-family face, and the bundled style and script fallbacks.
Files, collection indices and system registration paths are local details.
Unavailable requested styles use real faces and never synthesize weight or slant.

| Installed font capability | Local display | PDF |
| --- | --- | --- |
| Static standalone TrueType with installable or editable embedding | Yes | Full program |
| No subsetting | Yes | Full program when other flags permit |
| Preview-and-print only | Yes | Portable fallback. The writer does not enforce read-only access. |
| Restricted, bitmap-only or ambiguous rights | Yes for supported outlines | Portable fallback |
| Static OpenType/CFF and compatible TrueType or CFF collection faces | Yes | Portable fallback |
| Variable instances, color fonts, non-scalable bitmap fonts and web containers | Unsupported | Portable fallback |

Embedding rights follow the [OpenType OS/2 specification](https://learn.microsoft.com/en-us/typography/opentype/spec/os2#fstype).
OS/2 versions 0 and 1 ignore the unassigned high bits. Versions 0 through 2
allow the least-restrictive permission among conflicting permission bits.
Modern conflicting permission bits are invalid. Font metadata does not
establish redistribution rights or override the applicable license.

## Composition and adapters

HarfBuzz applies OpenType shaping, kerning, positioning and ligatures. ICU
computes grapheme boundaries, line opportunities and visual bidi runs, with
paragraph embedding levels retained across wrapping. The renderer receives
stable face fingerprints, glyph IDs, UTF-8 cluster ranges, positions, offsets,
advances and baselines in scene metres or document points. Font selection
checks whole graphemes before shaping and groups adjacent graphemes with the
same face, direction, script and style. Unicode source is preserved as
authored. HarfBuzz handles canonical equivalents such as precomposed and
decomposed accents during shaping. FreeType reads the validated font bytes
for simple, composite and transformed contours, including quadratic and cubic
curves. A malformed outline uses the approved face's missing-glyph contour
and records a diagnostic.

The approved repertoire covers Western European text, Greek, Cyrillic, Arabic
and Hebrew where the selected or fallback face supplies the required glyphs.
Full CJK and emoji coverage are outside the bundled repertoire. Unsupported
clusters retain their source and use a missing-glyph contour where available.
Script fallback uses real upright Arabic and Hebrew faces even when the
surrounding text requests italic.

Fixture Label Editor drafts, source 2D labels and Layout capture share their
existing field composition. The default face preserves the established
NanoVG halo and the shared positions. Other approved faces use their glyph
contours. Layout replay renders the captured contours with the stored physical
outline widths. Raster antialiasing and the zoom-independent 2D halo policy
may differ from paper ink. Generic Canvas commands resolve through the same
catalog in Layout replay and PDF. They accept the approved family name, an
optional face suffix such as ` Bold Italic`, and the historic `sans` and
`perastage-sans` aliases with their bold variants.

Free text uses the same point-space text block plan for Layout and PDF. Rich
text remains serialized in its original wx XML model. Extraction preserves
families, weights, sizes, italic requests, foreground colors, underlines,
strikethrough, explicit breaks, paragraph alignment, spacing and indentation.
Edited content stores its wrapping behavior explicitly. Legacy definitions
without that setting retain font-driven compatibility wrapping for the
portable families and explicit breaks for historical styles. Legacy
native-font metrics could vary by platform. Default sizing now uses the
shipped faces, so historical free-text widths can change. Non-text rich
objects and uncommon XML style features remain in the source XML with limited
rendering support. The `Document preview` button renders the current edits with the
shared point-space composition. The native editor registers approved faces
where wxWidgets supports it. Native rendering and caret geometry can vary by
platform. Opening and accepting the dialog without editing text keeps the
original rich-text XML byte for byte. When wxWidgets cannot round-trip some
of the source, the editor displays a notice and keeps the original UTF-8 XML
as a Base64 recovery comment beside the edited XML. Subsequent edits keep the
same recovery copy. Rendering and export use the active, editable XML.

Legend and Event Table retain their shared point-space composition and size
formulas from the integration branch. The older split-export PDF compensation
had already been replaced there. Fixture names retain their historical
two-space-delimited-word breaks. Viewport capture and Layout raster archives
invalidate incompatible plans. The existing per-viewport label cache and
bounded shared outline cache avoid repeating glyph work.

## PDF output and persistence

The exporter uses Type 0 fonts with TrueType CIDFont descendants, an explicit
CID-to-glyph map and a ToUnicode CMap. CIDs encode glyph IDs and the original
UTF-16 sequence, including surrogate pairs and ligature source. A multi-glyph
cluster uses a PDF `ActualText` span around its selectable glyphs. Adjacent
left-to-right glyphs can share a span with a source space so readers preserve
the space during extraction. The PDF places
glyphs at the composed positions. It draws fixture outlines as vector paths
and leaves the glyph fill selectable. Only used embeddable faces are written. Before placing PDF glyphs, a restricted
or unsupported face is resolved to a portable face and the logical text is
recomposed. Fixture capture stores the original field text and the anchor
needed for that recomposition. The PDF registry also rejects disallowed
resources at the point where glyphs and outlines are emitted.
Missing faces and unsupported glyphs are recorded once per issue in the
export report. Font resource failure leaves scene contents and requested
style values intact.

Label presentation version 3 added the optional `fontFamily` field. Version 4
adds italic where requested. Layout reads all three versions starting at 2.
Unchanged legacy label styles continue to write version 2. Legends and Event Tables have an optional
`fontFamily` property in the existing Layout schema and use the default when
the field is absent. Free text retains its existing per-run rich-text data.
PSTG and templates store presentation choices without introducing font data
into MVR or GDTF. Reopening a project revalidates an incomplete catalog, allowing restored
packaged or installed resources to recover the requested appearance. The Layout
text editor's `Refresh fonts` action also rechecks a complete catalog. Resource
fingerprints invalidate the relevant capture and composition caches.

## Verification

`PdfFontMetricsEncoding` covers catalog authorization, exact installed-face
recovery, actual glyph identity, fallback, advances and mixed-family runs.
`LayoutLicensedFontsPdf` uses Poppler to validate a representative Layout's
faces, logical Unicode extraction in the default mode, and outline/selection
behavior. The PDF fixture uses the production fixture label compositor for
Arabic, Hebrew, ligatures and combining marks. The native smoke test exercises
editor and viewport capture of a fixture with mixed-direction text. Raw mode also checks text and spaces. Depending on the Poppler
release it may emit RTL glyphs in PDF drawing order. Official Windows, Linux
and macOS CI require Poppler. A local configuration without Poppler reports the missing validation
tools and still runs the structural test. Layout preview
and PDF tests compare geometry in document points. The raster parity tests
compare ink geometry with tolerances for antialiasing and rasterizer
differences.

HarfBuzz, FreeType and ICU are required CMake dependencies and are resolved
from the pinned vcpkg manifest for official builds. ICU supplies Unicode
segmentation and bidi through its C API. See the third-party notices for their
licenses and transitive package notices.
