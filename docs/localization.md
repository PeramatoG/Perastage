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

Only user-facing interface labels should be marked for translation. Serialization keys, language codes, MVR/GDTF XML names, UUIDs, file paths, official enum values, project data, and technical file formats must remain stable and untranslated. UI localization must not change numeric serialization; Perastage keeps technical numeric formats locale-independent.

## Regenerating catalogs

Editable translations live in `.po` files. Generated `.mo` catalogs are build artifacts and are not committed. CMake generates the Spanish catalog at `generated/locale/es/LC_MESSAGES/perastage.mo` inside the build directory when gettext `msgfmt` is available.

Regenerate catalogs through CMake so outputs stay in the build tree:

```sh
cmake --build build --target perastage_translations
```

The application target depends on `perastage_translations` when `msgfmt` is available, then copies the generated `.mo` into the runtime locale directory for development builds and verifies that the runtime file exists. Install and packaging rules install the same generated file into the packaged runtime locale directory. If `msgfmt` is missing, normal source configuration continues with a prominent warning that Spanish translations will not be available, and the explicit translation target fails with a clear message explaining that gettext/msgfmt is required.

Localization startup diagnostics report the requested language, active language, catalog-found state, catalog-load result, and the selected or expected catalog path suffixes. For Spanish startup, a successful run reports `requested=es active=es` and `catalog_loaded=1`.
