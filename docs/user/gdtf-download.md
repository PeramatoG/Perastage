# GDTF Download

Use **Tools → Download GDTF** to find and download fixture profiles from GDTF Share. Online catalog loading and downloads require a GDTF Share account and an Internet connection.

## Find a profile

1. Open **Tools → Download GDTF**. A usable cached catalog opens immediately; without one, sign in to load the catalog. Cancelling initial sign-in cancels the operation.
2. Search by manufacturer, fixture, UUID, revision or DMX mode name. All search words must match, in any order; Manufacturer and Fixture filters narrow the results.
3. Select a revision and review its creator, dates, size, identifiers and DMX modes with channel footprints before downloading.
4. Download the selected profile and check the fixture's definition and mode in your project or dictionary.

## Cached and offline browsing

A fresh catalog is reused for one hour. Stale cached data refreshes in the background while browsing remains available; a failed refresh keeps cached results usable. Cached browsing works offline without sign-in. Downloads require online authentication and wait for an active refresh to finish.

A failed or cancelled download preserves an existing destination profile.

## Credentials and local profiles

Passwords use the operating system's secure credential store when available. If the runtime store is unavailable, Perastage warns you and can use credentials for the current operation without persisting the password. You may need to enter it again in a later session. The username may be remembered as a sign-in hint.

Use **Tools → Edit dictionaries** to manage profile mappings and **Tools → Open user library folder** to find local content. See [credential troubleshooting](troubleshooting.md#gdtf-share-credentials) if passwords are not retained.

## Resolve profiles during an MVR import

Keep using the MVR/App/Download source dialog. Choosing Download opens the import
queue with safe catalog suggestions and available MVR or application fallbacks.
Select a row to inspect ranked revisions and compatibility warnings. You can
change successful rows too; **Apply selections** processes the changed proposals
before **OK** commits the results to every instance of that fixture type.

Choosing **Download** requests a repository file when there is an unambiguous
model match, even if the MVR mode name differs from the catalog. For example,
`DMX Mode` does not prevent downloading ELP PAR with `Basic` and `Compact` modes.
Confirm the actual mode after download when compatibility needs review. If you
cancel that mode choice, **Downloaded; select DMX mode** retains the package;
use **Choose DMX mode...** later or select another source. OK remains disabled
until pending mode choices are resolved.

Each row has mutually exclusive **MVR**, **Perastage** and **Selected file**
checkboxes. Unavailable sources are disabled. MVR preserves the imported instance
modes; Perastage selects an available library profile; Selected file restores the
last downloaded or local choice. Switching back to a downloaded file reuses its
validated package. **Downloaded; identical to original MVR** means the downloaded
bytes were compared with the embedded profile and matched exactly; a shared name
or FixtureTypeID does not establish file equality.

Use **Search GDTF Share...** to select a revision from the full existing catalog
browser, or **Load local GDTF...** to validate and copy a local profile into the
import's resources. Choose its DMX mode and review any model, mode or footprint
differences. The original local file remains unchanged. Available fallback
sources can be selected explicitly from the same row.

The option **Close GDTF download window automatically when finished** is available
only in **Preferences → MVR Import/Export** and defaults to off. With automatic
close enabled, safe completed choices continue the import directly. Ambiguous or
failed results remain open for review.

The queue shows catalog/sign-in activity, elapsed time, analysis progress by
fixture type and download bytes. Green indicates a successful download, reuse or
local selection; amber indicates an original MVR or application fallback; red
indicates a failure or unresolved profile.

Candidates list all available modes and their channel counts. Missing catalog
metadata is shown as unavailable, rather than zero channels, and is updated from
the actual package after downloading. The imported mode and its known channel
count appear in the details. Repeated patch spacing may suggest a mode in the
picker, but requires confirmation: gaps may include unused addresses, and an
equal footprint does not guarantee equal DMX channel functions.

**Cancel**, **Escape** and the window close button stop the queue and continue
importing with the original MVR profiles and modes for all its rows. This discards
all queue substitutions, including completed downloads and local selections.
Use **OK** to accept the results.

**Apply selections** also retries failed downloads. Canceling authentication stops the remaining attempts without repeating the sign-in dialog for every row.
