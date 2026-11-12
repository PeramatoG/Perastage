# GDTF Share catalog browser

The browser consumes only the documented GDTF Share `getList.php` catalog and
existing authentication/download endpoints. Catalog browsing does not download,
inspect, mutate, or publish GDTF/MVR resources.

## Domain and cache

`mvr/gdtf_catalog_parser` preserves the official root timestamp as a scalar
string and file size as optional bytes alongside existing revision, identity,
source, creator, dates, rating, and mode/DMX footprint records. Numeric/string
fields, null UUIDs, unavailable ratings, nested arrays, and legacy field aliases
remain supported. Invalid/absent sizes stay absent. Rating text is preserved
even when it is `N/A`; the Rating row stays visible, showing meaningful numeric
values or `--` for missing/unavailable ratings and empty selections.

`core/gdtf_catalog_service` owns validated cache snapshots. Cache version 2
keeps `list_data` and adds optional `catalog_timestamp`; older versions and
files without this metadata remain readable, deriving the marker from the
payload when present. The one-hour TTL measures local successful validation,
separately from the upstream catalog marker. Unchanged refreshes still renew
the TTL and persist the current marker, but retain existing payload/records.
Fingerprint equality recognizes identical payloads; comparison of parsed
public metadata recognizes equivalent JSON formatting/scalar representations.
The marker alone never hides changed records. Reordered rows count as a change.
Failed, empty, or unusable online responses do not overwrite the cache.

## Opening and background ownership

`gui/gdtf_share_download_workflow` first obtains `GetParsedCatalogSnapshot()`.
Usable caches open immediately without an online overlay or dialog reparse.
Fresh caches supply no refresh callback. Stale caches supply the dialog's
background callback; missing caches retain first-load sign-in/network handling.

The callback captures a credential value copy and creates its own
`GdtfShareClient` and service on the worker. Its private cookie/session cannot
race the UI-owned download client or workflow state. It invokes no GUI callbacks
or credential prompts; failed background authentication preserves the cache.
The worker returns service-parsed records through a wx thread event. The dialog
joins the worker before destruction and applies results on the UI thread.
Closing during refresh can wait for the existing bounded network timeouts.
Download remains disabled during refresh; filters and pagination stay active.
The independent download session may still need authentication after refresh.

Equivalent visible records update status/time only. Changed records rebuild
the index and results once, restoring the selected RID and its page when it
still matches the active filters. Removed/filtered selections clear details.

## Search and presentation

`core/gdtf_catalog_browser_model` has no wxWidgets or network dependency.
`SearchIndex` normalizes manufacturer, fixture, UUID, revision, and mode names
once per displayed snapshot. General query tokens are whitespace-separated and
conjunctive: every token must occur in a row, regardless of order. Dedicated
manufacturer/fixture substring filters also apply conjunctively. ASCII case
and punctuation normalization preserve existing filtering conventions; UTF-8
bytes are preserved, but non-ASCII case folding is not provided.

The same model formats file sizes and distinguishes meaningful rating text
from missing/unavailable data. `gui/gdtf_catalog_details_panel` owns translated
labels, selectable read-only UUID/RID fields, UTC date display, and the mode/footprint
table. The list shows only Manufacturer, Fixture, Revision, Source, Last Modified,
and GDTF Version. Source retains the API's `Manuf.`/`User` value; Creator is the
actual contributor username. Absent data uses a dash rather than default zero.

## Regression checks

Targeted native tests are `GdtfCatalogBrowserModel`, `GdtfCatalogCache`, and
`GdtfCatalogDialog` (requires a native GUI or Xvfb). The dialog regression
exercises browsing during a paused refresh, changed/unchanged/failing refreshes,
parsed snapshot reuse, selected RID/page retention, and Download gating.
It also verifies that dialog destruction joins an active worker safely.
`GdtfCatalogBrowserBoundary` and `MainWindowGdtfDownloadBoundary` protect domain,
presentation, and orchestration ownership. Existing import-matching and GDTF
Share security regressions protect the adjacent consumers and session contract.

Catalog storage and legacy migration use the [persistent cache service](persistent_caches.md).

## Rating parsing and exception verification

Ratings use Core's `decimal_parse.h` finite decimal conversion with complete
string consumption, classic locale, and the existing zero fallback. Leading
ASCII whitespace and an optional plus remain accepted; trailing text (including
whitespace), hex strings, non-finite strings, overflow and underflow to zero
are unavailable ratings. Numeric JSON ratings are range-checked before float
conversion. Original `ratingText` is retained independently of the numeric value.
No rating/ranking rules or DMX selection rules change for valid decimal ratings.

The CI workflows use GCC/libstdc++ on `ubuntu-latest`, MSVC on `windows-latest`,
and Apple Clang/libc++ on `macos-26`; compatibility packaging also uses
`macos-15`. Floating `from_chars` support depends on the standard library,
not just C++20 mode. The existing Core helper explicitly supports older libc++
without that overload, so catalog parsing reuses it on every platform rather
than adding a library-version condition. Its streams keep their default
non-throwing error mask and never change the global locale.

A fresh cache is validated once by `RefreshCatalogIfStale`. A stale cache is
validated and retained while a successful online response is independently
validated; unchanged results retain cached records and renew the TTL, changed
results replace them. Failed or unusable responses retain the validated cache.
The MVR queue consumes the returned parsed entries without parsing again.
The standalone browser opens a validated cache immediately, and its stale
background refresh independently reloads the cache before fetching online data.
Legacy migration additionally validates the copied file before publication.
These separate reads/versions must not be mistaken for proof that all observed
Windows exceptions came from one call site.

To verify with Visual Studio 2022:

1. Build Debug with symbols. In Debug > Windows > Exception Settings enable
   **Thrown** for C++ exceptions (or add `std::invalid_argument`).
2. With the previous revision, open GDTF Download Queue during MVR import using
   a catalog containing many `"N/A"` ratings. On a break, inspect Call Stack:
   confirm `std::stof`, `JsonToFloat`, `ParseCatalog`, and the cache or online
   service caller before attributing the exception. Record other stacks too.
3. Repeat on this revision with the same data. Run
   `gdtf_catalog_browser_model_test` under the debugger as an offline check;
   its 12,000 invalid/absent ratings must produce no conversion exceptions.
4. Exercise fresh cache, stale cache with identical/different online content,
   and failed refresh with cache fallback. Confirm search, valid-candidate
   selection, mode selection and download still work. The deterministic cache
   test covers these policies without contacting GDTF Share.
5. If any exception remains, capture its Call Stack and investigate that origin
   independently; the reported 20,686 Windows exceptions have no supplied stack
   trace and cannot all be attributed to ratings from counts alone.
