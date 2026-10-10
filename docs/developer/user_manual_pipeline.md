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
page-by-page to English; DOC-140C will enforce translation completeness. This
foundation stages files only and does not implement runtime fallback.

## Runtime and package ownership

`cmake/PerastageRuntimeStaging.cmake` owns development runtime copies;
`cmake/PerastageInstall.cmake` owns installation and release staging. Both copy
only top-level Markdown pages, with no HTML conversion or new build dependency:

```text
<runtime asset root>/
    help.md
    help/
        en/       # docs/user/*.md, including index.md
        es/       # docs/user/locales/es/*.md, when present
        zh_CN/    # docs/user/locales/zh_CN/*.md, when present
```

The asset root remains beside the executable on Windows/Linux and inside
`Perastage.app/Contents/Resources` on macOS. Localized source directories are
optional, including in builds with UI localization disabled; they are never
recursively copied into `help/en/`. `help/en/index.md` is the canonical-manual
package assertion. Links to developer/reference documents still resolve in the
repository/website; these documents are outside the packaged user-manual bundle.
DOC-140B must handle such links in the offline viewer.

## Transitional Help and Console extraction

Root `help.md` remains staged and installed for compatibility until DOC-140B
migrates the GUI Help viewer and Console help. This block changes neither
Help/F1 behavior nor the Console C++ consumer.

In [Shortcuts and Command Bar](../user/shortcuts-and-command-bar.md), the invisible
HTML comments `<!-- PERASTAGE_CONSOLE_HELP_BEGIN -->` and
`<!-- PERASTAGE_CONSOLE_HELP_END -->` delimit only the canonical Console command
reference, including transform spaces. Future extraction uses these stable
markers instead of a translated heading. Localized mirrors must preserve them.
General keyboard and viewer shortcuts remain outside the marked reference.
