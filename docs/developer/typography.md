# Document typography and font licensing

Perastage resolves the requested family, weight and style to the exact approved
font bytes before composition. `FontCatalog` owns immutable resource data,
version, PostScript face name, OFL provenance and SHA-256. Rendering targets use
the resolved fingerprint and glyph ID. Persisted styles keep the family and
variant authored by the user, so substituting a face does not change the project.

## Approved resources

PerastageSans is the application name for the original Noto Sans face. It is the
default family. The Noto Sans and Noto Serif files come from Debian
`fonts-noto-core 20201225-2`. Source Sans 3 comes from Adobe's 3.052R release.
[Third-party notices](../../THIRD_PARTY_NOTICES.md) identify the exact releases,
files and licenses. Only those SHA-256 identities enter the first catalog.
The files are static single-face TrueType fonts with unrestricted embedding
flags. An unrestricted `OS/2.fsType` on its own gives no license evidence.

The catalog checks packaged resources during application initialization.
When an approved file is missing or unusable, the system adapter searches the
usual font directories for that platform. It filters candidates by size before
validating format, embedding flags and SHA-256 and accepts byte-identical
matches under any filename. Bundled resources are preferred. The search covers
Windows Fonts and the per-user Microsoft Fonts directory, macOS system, local
and user Fonts directories, and Linux standard, XDG and user font directories.
Custom platform font registration paths outside these directories are outside
this discovery scope. Arbitrary installed family names and user-supplied paths
do not authorize use. Perastage does not install fonts or attach them to projects.

CFF, collections, web fonts and variable fonts are unsupported. Additions need
a verified provenance and rights record, pinned bytes, a supported outline and
cmap check, exact installed-package files and the upstream license text.
The application uses the same package contract on Windows, Linux and macOS.

## Composition and adapters

The existing TrueType reader supplies glyph advances and vertical metrics.
The text compositor works in scene metres or document points. It decodes
UTF-8 through the font's Unicode cmap (formats 4 and 12). The glyph plan
records the original character, resolved face, glyph ID, advance, position
and baseline. A missing glyph uses the approved default of the same variant
where available. Unavailable characters use the face's missing-character
contour and retain the original source. Western European text including
precomposed Spanish accents, common punctuation, Greek and Cyrillic are
supported where covered by the face. Combining-mark positioning, bidirectional
text, OpenType shaping and contextual ligatures require a shaping library.
FreeType would provide outline extraction and rasterization but would not
supply this shaping. HarfBuzz and bidi processing were deferred to avoid
introducing dependencies for an unqualified repertoire. The common compositor
reports missing or unsupported glyphs.

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
rendering support. Noto Sans Italic and Bold Italic preserve italic styling
using real faces. Requests for italic in the other two families fall back to
the corresponding Noto Sans italic face. A wx rich-text control is used for
editing and can display a native fallback on builds without private font
registration. The Layout canvas supplies the document rendering.

Legend and Event Table retain their shared point-space composition and size
formulas from the integration branch. The older split-export PDF compensation
had already been replaced there. Fixture names retain their historical
two-space-delimited-word breaks. Viewport capture and Layout raster archives
invalidate incompatible plans. The existing per-viewport label cache and
bounded shared outline cache avoid repeating glyph work.

## PDF output and persistence

The exporter uses Type 0 fonts with TrueType CIDFont descendants, an explicit
CID-to-glyph map and a ToUnicode CMap. CIDs encode both the selected glyph and
original codepoint, including surrogate pairs when required. The PDF places
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
packaged or installed resources to recover the requested appearance.

## Verification

`PdfFontMetricsEncoding` covers catalog authorization, exact installed-face
recovery, actual glyph identity, fallback, advances and mixed-family runs.
`LayoutLicensedFontsPdf` uses Poppler to validate a representative Layout's
faces, Unicode extraction, and outline/selection behavior. Layout preview
and PDF tests compare geometry in document points. The raster parity tests
compare ink geometry with tolerances for antialiasing and rasterizer
differences.
