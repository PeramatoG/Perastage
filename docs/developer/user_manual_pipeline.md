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

## Translated mirrors

Translated mirrors belong under `docs/user/locales/es/` and
`docs/user/locales/zh_CN/`. Locale identifiers follow the existing
[localization contract](localization.md) and CMake's
`PERASTAGE_TRANSLATION_LANGUAGES` registry; English (`en`) is the implicit source
and fallback language. There is no separate manual locale registry.

Mirrors use the same filenames and preserve links and page relationships.
Translate explanatory prose and preserve exact UI labels according to
[documentation policy](documentation_policy.md) and localization policy.
Preserve Console command grammar in English, and standards identifiers such as
MVR, GDTF, DMX, XML, and UUID. Missing translated pages may temporarily fall back
page-by-page to English; DOC-140C will enforce translation completeness. F1/Help selects the requested locale from `ui_language` and falls back to
English for each missing or unreadable localized page.

## Runtime and package ownership

`cmake/PerastageRuntimeStaging.cmake` owns development runtime copies;
`cmake/PerastageInstall.cmake` owns installation and release staging. Both copy
only top-level Markdown pages, with no HTML conversion or new build dependency:

```text
<runtime asset root>/
    help/
        en/       # docs/user/*.md, including index.md
        es/       # docs/user/locales/es/*.md, when present
        zh_CN/    # docs/user/locales/zh_CN/*.md, when present
```

The asset root remains beside the executable on Windows/Linux and inside
`Perastage.app/Contents/Resources` on macOS. Localized source directories are
optional, including in builds with UI localization disabled; they are never
recursively copied into `help/en/`. `help/en/index.md` and
`help/en/shortcuts-and-command-bar.md` are required canonical-manual package
artifacts. Links to developer/reference documents still resolve in the
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
