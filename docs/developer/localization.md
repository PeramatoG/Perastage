# Localization

Perastage uses wxWidgets gettext catalogs for user-facing interface text. English is the source language and default language. Spanish and Simplified Chinese (`zh_CN`) are complete release languages.

## User preference

The interface language is stored in user preferences under the stable key `ui_language`. Accepted values are:

- `en` for English
- `es` for Spanish
- `zh_CN` for Simplified Chinese

Missing, empty, malformed, or unsupported values fall back to English. Simplified Chinese accepts `zh_CN`, `zh-cn`, `zh_cn`, `ZH_CN`, `zh-Hans`, and `zh_Hans`, but persists the canonical `zh_CN` value only. The selected language is applied after restarting Perastage; the current UI is not rebuilt live.

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

The embedded Console is a GUI shell around a future external command interface. Localize its panel caption, menus, toolbar, tooltips, preferences, and GUI-only history/search/filter dialogs. Command grammar, names, arguments, flags, examples, help and option descriptions, processor output, error codes, command echo, machine-readable values, and technical logs remain stable English. Diagnostic and protocol logs used for crash reports, files, debugging, traces, and integrations also remain English, including `[INFO]`, `[WARNING]`, `[ERROR]`, `[CMD]`, and `[METRIC]` prefixes. When one operation reports to both surfaces, provide localized text at the GUI boundary while preserving a separate stable English Console message.

Use the terminology in [localization_glossary.md](localization_glossary.md) for recurring technical terms. Preserve official standards and identifiers such as MVR, GDTF, DMX, Art-Net, UUID, XML, Geometry3D, Symbol, and Symdef when translating would reduce precision.

## Catalog maintenance

Normal application builds compile `.po` files into generated `.mo` files in the build tree and do not rewrite committed translation sources. Use explicit developer targets for source updates and checks:

```sh
cmake --build build --target perastage_update_pot
cmake --build build --target perastage_update_po
cmake --build build --target perastage_check_translations
cmake --build build --target perastage_translations
```

The helper script behind these targets is `scripts/localization_catalog.py`. It discovers tracked repository C/C++ sources with `git ls-files`, generates `resources/locale/perastage.pot`, merges PO changes, validates catalogs and accelerators, rejects fuzzy or untranslated COMPLETE entries, and rejects a stale committed POT. Its high-confidence audit automatically covers tracked presentation sources under `gui/`, `viewer2d/`, and `viewer3d/`, plus explicit root presentation entry points, so modular files cannot silently escape review. `scripts/localization_audit_allowlist.txt` accepts only narrow documented stable exceptions in `path|literal|category|reason` format; localization debt is rejected.

Developers may edit PO files directly or with Poedit. External translators can submit pull requests that change only `resources/locale/<language>/LC_MESSAGES/perastage.po`; maintainers should run `perastage_check_translations` and `perastage_translations` before merging.

To add a new language, add it to the supported-language registry and native display-name list in code, initialize `resources/locale/<language>/LC_MESSAGES/perastage.po` from `resources/locale/perastage.pot`, add the language code to `PERASTAGE_TRANSLATION_LANGUAGES` in CMake, add it to `COMPLETE_LANGUAGES` or `DRAFT_LANGUAGES` in `scripts/localization_catalog.py`, provide complete translations before moving it out of the draft list, and add or update localization tests for language selection, catalog loading, fallback behavior, and packaging paths.

## Regenerating catalogs

Editable translations live in `.po` files. Generated `.mo` catalogs are build artifacts and are not committed. CMake generates catalogs such as `generated/locale/es/LC_MESSAGES/perastage.mo` and `generated/locale/zh_CN/LC_MESSAGES/perastage.mo` inside the build directory when gettext `msgfmt` is available. Catalog compilation is routed through `cmake/PerastageCompileGettextCatalog.cmake` so Ninja, Visual Studio/MSBuild, and macOS bundle builds pass executable paths and catalog paths as explicit CMake arguments instead of shell-sensitive command fragments.

Regenerate catalogs through CMake so outputs stay in the build tree:

```sh
cmake --build build --target perastage_translations
```

`PERASTAGE_ENABLE_LOCALIZATION` defaults to `ON`. With localization enabled, gettext `msgfmt` is required at CMake configure time as a build-time tool only; it is not a Perastage runtime dependency. On macOS, Homebrew installs gettext as a keg-only package, so add `$(brew --prefix gettext)/bin` to `PATH` or otherwise make the tools visible before configuring CMake. On Windows, local builds should resolve gettext tools from the classic `C:/vcpkg/installed/x64-windows/tools/gettext/bin` tree; CI gets the same tools from the root vcpkg manifest host dependency. Do not add gettext or libintl as Perastage runtime dependencies, and end users do not need gettext installed. After dependency changes, clear the affected CMake cache or run `setup_windows.ps1 -CleanBuild -SkipBuild` so CMake resolves the vcpkg-provided tool from the active classic installation. The generated `perastage.mo` file is the only localization catalog artifact used at runtime. The application target depends on `perastage_translations`, then copies the generated `.mo` into the runtime locale directory for development builds and verifies that the runtime file exists. Install and packaging rules install the same generated file into the packaged runtime locale directory. Developers may explicitly configure with `-DPERASTAGE_ENABLE_LOCALIZATION=OFF` for an English-only development build that does not expose Spanish or Simplified Chinese in Preferences.

Localization startup diagnostics report the requested language, active language, catalog-found state, catalog-load result, and the selected or expected catalog path suffixes. For Spanish startup, a successful run reports `requested=es active=es` and `catalog_loaded=1`. For Simplified Chinese startup with the draft catalog present, a successful run reports `Localization initialized: requested=zh_CN active=zh_CN catalog_found=1 catalog_loaded=1`.


## Complete-language review

Spanish and Simplified Chinese catalogs must keep every active entry translated and non-fuzzy. Before a localization pull request is marked ready to merge:

1. Fill every active `msgstr` in `resources/locale/zh_CN/LC_MESSAGES/perastage.po`.
2. Validate placeholders, accelerator markers, line breaks, and format flags against the source text.
3. Keep release languages in `COMPLETE_LANGUAGES` in `scripts/localization_catalog.py`.
4. Run strict catalog checks with `cmake --build build --target perastage_check_translations` or `python3 scripts/localization_catalog.py check-po`.
5. Run runtime localization tests, including catalog-loading and fallback coverage.
6. Only then mark the draft pull request ready to merge.

Manual UI validation must also check CJK font coverage in native wxWidgets controls, custom-rendered tables, OpenGL/text overlays, printing, PDF/SVG output, and any bundled font path. Do not add a bundled CJK font unless a rendering failure is reproduced and licensing plus packaging impact is reviewed separately.
