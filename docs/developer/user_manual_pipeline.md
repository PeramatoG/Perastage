# User Manual Pipeline

## Source ownership and consumers

`docs/user/*.md` is the canonical English user manual. Its human-readable
navigation and inventory are owned by [the user index](../user/index.md); do not
maintain a second page list. Keep these English paths stable for repository and
website links, without moving them under `en/` or duplicating them under
`locales/en/`.

The same Markdown source is intended for GitHub repository browsing,
`perastage.luismaperamato.com`, and packaged offline Perastage help. Consumers
must not maintain independent user-workflow prose. Generated web and package
outputs are artifacts; permanent edits belong in the canonical Markdown.
Developer and reference documentation remains English-only unless explicitly
decided otherwise.

## Static website consumer

Run `node tools/docs/build_static_docs.cjs` independently of the application build.
It uses vendored Marked 4.3.0 at generation time and derives each channel's
grouped navigation from its own `docs/user/index.md`. Next is generated from
the current working tree into `docs/next/`; Stable is a frozen snapshot from
the latest published non-prerelease release tag under `docs/stable/`.
Developer Markdown follows main at adjacent `docs/developer/*.html` routes;
it is never duplicated into user channels. English HTML is committed under `docs/`; verify it
with `node tools/docs/build_static_docs.cjs --check`. Generated HTML must never
receive manual editorial fixes. Edit Markdown and regenerate instead.
Packaged Help and the website are separate consumers of the same canonical source;
the website does not fetch or render Markdown in the browser. Legacy `doc.html?md=`
bookmarks and root build/repository-layout routes remain compatibility redirects.

Normal generation preserves every Stable byte. Explicit promotion reads only
local Git objects, stages and validates all tagged user outputs, then atomically
exchanges the snapshot directory. Promotion runs on Linux or macOS with Python 3
for the operating-system directory swap; normal generation and checking use Node:

```sh
node tools/docs/build_static_docs.cjs --promote-stable 1.6.0 --source-ref v1.6.0
```

The deterministic `docs/stable/manifest.json` records the release version/ref,
exact source commit, original user index, derived page inventory and SHA-256
artifact hashes, without timestamps. `--check` rejects missing, added or edited
Stable files as well as current Next/developer/compatibility drift. Historical
pages outside the release's index and developer/source links point to that Git
tag. Required release-specific content assets are copied into Stable; shared
CSS, logo and navigation assets remain under `docs/assets/`.

`documentation.html` is the durable Online Documentation URL and defaults to
Stable. Root user bookmarks and `doc.html?md=user/...` select Stable when its
inventory contains the page, otherwise Next; fragments survive redirects.
Developer compatibility links continue to current `docs/developer/` routes.
The channel switcher opens the matching page or explains the missing page and
opens the other hub. Next carries `noindex,follow`; Stable has canonical URLs.

Only the published-release workflow promotes Stable automatically; draft
creation does not. See [Versioning Policy](versioning-policy.md#public-documentation-channels).
Packaged offline Help still corresponds to the exact application release.
The product landing, shared presentation assets and developer docs are not
versioned. Localization and broader search metadata remain separate work.

## Translated mirrors

Translated mirrors belong under `docs/user/locales/es/`,
`docs/user/locales/fr/`, `docs/user/locales/de/` and `docs/user/locales/zh_CN/`. Locale identifiers follow the existing
[localization contract](localization.md) and CMake's
`PERASTAGE_TRANSLATION_LANGUAGES` registry; English (`en`) is the implicit source
and fallback language. There is no separate manual locale registry.

Mirrors use the same filenames and preserve links and page relationships.
Translate explanatory prose and preserve exact UI labels according to
[documentation policy](documentation_policy.md) and localization policy.
Preserve Console command grammar in English, and standards identifiers such as
MVR, GDTF, DMX, XML, and UUID. The supported COMPLETE manual locales are
`en`, `es`, `fr`, `de`, and `zh_CN`. English remains the canonical source and fallback.
Translated mirrors must maintain exact filename parity with `docs/user/*.md`,
including nonempty UTF-8 content and valid localized heading fragments.
F1/Help selects the requested locale from `ui_language`; page-level English
fallback remains available for future development or missing/unreadable files
in damaged or incomplete installations.

## Runtime and package ownership

`cmake/PerastageRuntimeStaging.cmake` owns development runtime copies;
`cmake/PerastageInstall.cmake` owns installation and release staging. Both copy
only top-level Markdown pages, with no HTML conversion or new build dependency:

```text
<runtime asset root>/
    help/
        en/       # docs/user/*.md, including index.md
        es/       # docs/user/locales/es/*.md, complete mirror
        fr/       # docs/user/locales/fr/*.md, complete mirror
        de/       # docs/user/locales/de/*.md, complete mirror
        zh_CN/    # docs/user/locales/zh_CN/*.md, complete mirror
```

The asset root remains beside the executable on Windows/Linux and inside
`Perastage.app/Contents/Resources` on macOS. All five complete manuals are required release payloads, including in builds
with UI localization disabled. CMake tolerates absent localized directories
for development fixtures; release guards require every canonical filename in
`help/en/`, `help/es/`, `help/fr/`, `help/de/`, and `help/zh_CN/`, including `index.md`, with staged and
installed files byte-identical to their source pages. Locales are never
recursively copied into `help/en/`. Links to developer/reference documents still resolve in the
repository/website; these documents are outside the packaged user-manual bundle.
The offline viewer opens safe developer/reference links in the repository
through the system browser; user-manual links and heading fragments navigate
locally. Back restores the previous page, Home returns to `index.md`, and
Online Documentation remains available in both the dialog and Help menu.

## Help and Console consumers

F1/Help consumes the canonical packaged `help/<locale>/index.md` manual.
Console consumes only the English marked section from
`help/en/shortcuts-and-command-bar.md`, resolved through the same runtime resource
contract. UI language does not select Console syntax or help prose.

In [Shortcuts and Command Bar](../user/shortcuts-and-command-bar.md), the invisible
HTML comments `<!-- PERASTAGE_CONSOLE_HELP_BEGIN -->` and
`<!-- PERASTAGE_CONSOLE_HELP_END -->` delimit the canonical Console reference,
including transform spaces. Core's `user_manual/console_help` helper requires
exactly one of each marker, in order, and a nonempty section. It uses the bounded
`MarkdownToPlainText` transformation in `core/markdown` for headings, paragraphs,
lists, fenced examples, inline code, bold, links and tables; it does not implement
full CommonMark. Missing pages, invalid markers or empty/malformed presentation
return only a short English unavailability message and an Online Documentation
pointer, with no built-in command catalog. Localized mirrors preserve the markers,
but Console always loads English. General keyboard and viewer shortcuts remain
outside the marked reference.

Root `help.md` no longer exists and is neither staged nor installed. There is no
independent in-app help prose source: permanent operator-content changes belong
in `docs/user/*.md`, translated mirrors in `docs/user/locales/<locale>/*.md`, and
`help/<locale>/*.md` is generated/staged package content.

## Validation

`tests/check_docs_links.py` derives filename parity from canonical pages and
locales from CMake, checks UTF-8, Console markers and stable command examples,
and validates translated links and fragments using the offline anchor rules.
`MarkdownRenderer` verifies fragments against actual rendered headings in every
manual. `UserManualDialog` exercises localized navigation and deliberate
page-removal fallback; `UserManualStaging` verifies the production staging and
installation owners with complete, partial and absent locale fixtures.
`packaging/verify_localization.py` validates release payloads against the CMake
locale registry and canonical user index, including compiled catalogs and exact
manual bytes. Windows, macOS, Linux AppImage and Arch package guards share it;
Arch also validates the final archive inventory.
Developer links in mirrors use canonical repository URLs so they resolve both
from the nested source directories and from the flat packaged locale folders.
