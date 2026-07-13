# Localization

Perastage uses wxWidgets gettext catalogs for user-facing interface text. English is the source language and default language. Spanish is the first translated catalog used to prove the infrastructure.

## User preference

The interface language is stored in user preferences under the stable key `ui_language`. Accepted values are:

- `en` for English
- `es` for Spanish

Missing, empty, malformed, or unsupported values fall back to English. The selected language is applied after restarting Perastage; the current UI is not rebuilt live.

## Runtime catalog locations

Catalogs use the standard gettext layout `locale/<language>/LC_MESSAGES/perastage.mo`. The application searches deterministic runtime roots for development and packaged builds:

- Development builds: `resources/locale` from the repository or nearby build directories.
- Windows packages: `resources/locale` next to the installed executable.
- Linux installs: `resources/locale` next to the executable in the current install layout, with a `../share/locale` candidate reserved for distro-style layouts.
- macOS bundles: `Contents/Resources/locale` inside the application bundle.

Only user-facing interface text should be marked for translation. Text that comes from projects, imports, user input, file names, object names, fixture names, layer names, UUIDs, protocol values, and other domain data is displayed exactly as stored. Serialization keys, language codes, MVR/GDTF XML names, UUIDs, file paths, official enum values, project data, and technical file formats must remain stable and untranslated. UI localization must not change numeric serialization; Perastage keeps technical numeric formats locale-independent.

When a value is both a stable internal identifier and a visible UI label, keep the identifier in the model or serializer unchanged and add a presentation-only display-label mapping at the UI boundary. Reusable UI mappings, such as table-column label helpers, should translate the display label with wxWidgets gettext while preserving the stable order, enum, or client-data value used by application logic. Do not compare translated text for dispatch or persistence.

Mark complete English source messages with `_()` or `wxGetTranslation()` at the presentation boundary. Translate formatted messages as complete format strings before inserting dynamic values, and keep filenames, imported names, layer names, UUIDs, paths, numeric values, and technical details as unmodified parameters. Use the wxWidgets plural gettext API for true singular/plural cases instead of concatenating translated fragments.

## Developer workflow

Mark new Perastage-created user-facing UI text in the same change that introduces it. Use `_()` for literal UI strings, `wxGetTranslation()` when translating an existing `wxString` source identifier at a UI boundary, and the wxWidgets plural gettext API for true singular/plural messages. Keep complete English sentences or labels in the source; do not build translated sentences from separately translated fragments.

For formatted UI text, translate the complete format string first and then insert dynamic values. Dynamic values such as filenames, imported names, UUIDs, protocol values, fixture names, layer names, object names, paths, numeric values, and technical error details are parameters and must not be translated or normalized.

Internal diagnostic logs are not the same as the user-visible Console panel. Diagnostic logs used for crash reports, files, debugging, protocol traces, and developer troubleshooting should normally stay stable English technical text. Console text that explains actions, errors, help, progress, or results to the user should be translated, while stable prefixes such as `[INFO]`, `[WARNING]`, `[ERROR]`, `[CMD]`, and `[METRIC]` remain unchanged for styling and parsing.

Use the terminology in [localization_glossary.md](localization_glossary.md) for recurring technical terms. Preserve official standards and identifiers such as MVR, GDTF, DMX, Art-Net, UUID, XML, Geometry3D, Symbol, and Symdef when translating would reduce precision.

## Catalog maintenance

Normal application builds compile `.po` files into generated `.mo` files in the build tree and do not rewrite committed translation sources. Use explicit developer targets for source updates and checks:

```sh
cmake --build build --target perastage_update_pot
cmake --build build --target perastage_update_po
cmake --build build --target perastage_check_translations
cmake --build build --target perastage_translations
```

The helper script behind these targets is `scripts/localization_catalog.py`. It discovers tracked repository C/C++ sources with `git ls-files`, including root files such as `main.cpp`, generates `resources/locale/perastage.pot` with `xgettext`, merges source changes into enabled PO catalogs with `msgmerge`, validates catalogs with `msgfmt --check` and accelerator checks, rejects active fuzzy or untranslated entries through gettext tooling, and compares a temporary POT with the committed POT so stale templates fail the check. The audit inspects balanced wxWidgets UI call expressions across multiple lines for unmarked string literals. `scripts/localization_audit_allowlist.txt` accepts only documented stable exceptions in `path|literal|category|reason` format; localization-debt entries are rejected.

Developers may edit PO files directly or with Poedit. External translators can submit pull requests that change only `resources/locale/<language>/LC_MESSAGES/perastage.po`; maintainers should run `perastage_check_translations` and `perastage_translations` before merging.

To add a new language, add it to the supported-language registry and native display-name list in code, initialize `resources/locale/<language>/LC_MESSAGES/perastage.po` from `resources/locale/perastage.pot`, add the language code to `PERASTAGE_TRANSLATION_LANGUAGES` in CMake, provide complete translations, and add or update localization tests for language selection, catalog loading, fallback behavior, and packaging paths.

## Regenerating catalogs

Editable translations live in `.po` files. Generated `.mo` catalogs are build artifacts and are not committed. CMake generates the Spanish catalog at `generated/locale/es/LC_MESSAGES/perastage.mo` inside the build directory when gettext `msgfmt` is available. Catalog compilation is routed through `cmake/PerastageCompileGettextCatalog.cmake` so Ninja, Visual Studio/MSBuild, and macOS bundle builds pass executable paths and catalog paths as explicit CMake arguments instead of shell-sensitive command fragments.

Regenerate catalogs through CMake so outputs stay in the build tree:

```sh
cmake --build build --target perastage_translations
```

`PERASTAGE_ENABLE_LOCALIZATION` defaults to `ON`. With localization enabled, gettext `msgfmt` is required at CMake configure time as a build-time tool only; it is not a Perastage runtime dependency. On macOS, Homebrew installs gettext as a keg-only package, so add `$(brew --prefix gettext)/bin` to `PATH` or otherwise make the tools visible before configuring CMake. On Windows, install the vcpkg classic-mode package `gettext[tools]:x64-windows`; it provides `msgfmt.exe` for catalog generation only. Do not add gettext or libintl as Perastage runtime dependencies, and end users do not need gettext installed. After adding the vcpkg package, clear the affected CMake cache and reconfigure so CMake resolves the vcpkg-provided tool. The generated `perastage.mo` file is the only localization catalog artifact used at runtime. The application target depends on `perastage_translations`, then copies the generated `.mo` into the runtime locale directory for development builds and verifies that the runtime file exists. Install and packaging rules install the same generated file into the packaged runtime locale directory. Developers may explicitly configure with `-DPERASTAGE_ENABLE_LOCALIZATION=OFF` for an English-only development build that does not expose Spanish in Preferences.

Localization startup diagnostics report the requested language, active language, catalog-found state, catalog-load result, and the selected or expected catalog path suffixes. For Spanish startup, a successful run reports `requested=es active=es` and `catalog_loaded=1`.
