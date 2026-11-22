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

The immutable catalog checks packaged resources during application
initialization. Missing or unusable resources trigger targeted filename probes
in the platform font roots, then a bounded walk of 8,192 entries and six
directory levels. Candidates are filtered by file size before format,
embedding and SHA-256 validation. A match may have a different filename.
The roots cover Windows Fonts and per-user Microsoft Fonts, macOS system,
local and user Fonts, and Linux standard, XDG and user font directories.
Custom platform registration paths outside these directories are outside
this discovery scope. Discovery results stay cached until an explicit refresh
or a project reopens with an incomplete catalog. Perastage does not install
fonts or attach them to projects.

CFF, collections, web fonts and variable fonts are unsupported. Additions need
a verified provenance and rights record, pinned bytes, a supported outline and
cmap check, exact installed-package files and the upstream license text.
The application uses the same package contract on Windows, Linux and macOS.

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
may differ from paper ink.

Free text uses the same point-space text block plan for Layout and PDF. Rich
text remains serialized in its original wx XML model. Extraction preserves
families, weights, sizes, italic requests, foreground colors, underlines,
strikethrough, explicit breaks, paragraph alignment, spacing and indentation.
Approved family selections enable word wrapping. Existing text without an
explicit approved family retains explicit line breaks and clipping. Legacy
native-font metrics could vary by platform. Default sizing now uses the
shipped faces, so historical free-text widths can change. Non-text rich
objects and uncommon XML style features remain in the source XML with limited
rendering support. The `Document preview` button renders the current edits with the
shared point-space composition. The native editor registers approved faces
where wxWidgets supports it. Native rendering and caret geometry can vary by
platform. Opening and accepting the dialog without editing text keeps the
original rich-text XML. Native editing can rewrite XML that wxWidgets does
not round-trip.

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
and leaves the glyph fill selectable. Only used approved faces are embedded.
Missing faces and unsupported glyphs are recorded once per issue in the
export report. Font resource failure leaves scene contents and requested
style values intact.

Label presentation version 3 adds the optional `fontFamily` field. Layout's
presentation wrapper reads both versions 2 and 3. Unchanged label styles
continue to write version 2. Legends and Event Tables have an optional
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
faces, logical Unicode extraction in both default and raw mode, and
outline/selection behavior. Official Windows, Linux and macOS CI require
Poppler. A local configuration without Poppler reports the missing validation
tools and still runs the structural test. Layout preview
and PDF tests compare geometry in document points. The raster parity tests
compare ink geometry with tolerances for antialiasing and rasterizer
differences.

HarfBuzz, FreeType and ICU are required CMake dependencies and are resolved
from the pinned vcpkg manifest for official builds. ICU supplies Unicode
segmentation and bidi through its C API. See the third-party notices for their
licenses and transitive package notices.
