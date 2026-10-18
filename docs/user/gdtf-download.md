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
change successful rows too. **OK** remains enabled while the queue is idle: it
processes pending selections and closes when they finish. During analysis and
downloads it is disabled. New failures or unresolved choices keep the queue open
for review. There is no separate Apply button. Accepted choices apply to every
instance of that fixture type.

Choosing **Download** requests a repository file when there is an unambiguous
model match, even if the MVR mode name differs from the catalog. For example,
`DMX Mode` does not prevent downloading ELP PAR with `Basic` and `Compact` modes.
The DMX mode is assigned automatically after download, without a picker or
compatibility confirmation. Priority is: the MVR mode name (ignoring case,
spaces and punctuation), a unique mode with the original profile's channel count,
then a unique mode inferred from repeated equal patch spacing when that count is
unknown. Details show which evidence was used. Selecting another catalog candidate
or using full-catalog search follows the same automatic mode assignment; missing
catalog modes are resolved from the downloaded package.

If the mode remains ambiguous, the file stays downloaded and the original MVR
profile and modes remain selected. The row explains this fallback; **OK** can
continue without choosing a mode. **Choose DMX mode...** remains an optional manual
override. Several imported modes are preserved rather than collapsed into one.

Each row has mutually exclusive **MVR**, **Perastage** and **Selected file**
checkboxes. Unavailable sources are disabled. MVR preserves the imported instance
modes; Perastage selects an available library profile; Selected file restores the
last downloaded or local choice. Switching back to a downloaded file reuses its
validated package. **Downloaded; identical to original MVR** means the downloaded
bytes were compared with the embedded profile and matched exactly; a shared name
or FixtureTypeID does not establish file equality.

Use **Search GDTF Share...** to select a revision from the full existing catalog
browser, or **Load local GDTF...** to validate and copy a local profile into the
import's resources. Compatible local profiles also receive a mode automatically
when the imported evidence identifies one. Otherwise, a manual mode choice is
available. Review any model, mode or footprint differences. The original local file remains unchanged. Available fallback
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
count appear in the details. Patch inference requires at least two equal adjacent intervals in the same
universe, with no duplicate or invalid addresses or intervening fixtures of
another type. It never overrides a known original channel count or breaks a tie
between modes with the same footprint. Gaps may include unused addresses, and
equal footprints do not guarantee equal DMX channel functions.

**Cancel**, **Escape** and the window close button stop the queue and continue
importing with the original MVR profiles and modes for all its rows. This discards
all queue substitutions, including completed downloads and local selections.
Use **OK** to accept the results.

**OK** also retries failed downloads. To continue with an original profile after
a failure, select its **MVR** source and press **OK**. Canceling authentication
stops the remaining attempts without repeating the sign-in dialog for every row.
