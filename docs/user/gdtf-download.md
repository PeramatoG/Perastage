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

Use **Search GDTF Share...** to select a revision from the full existing catalog
browser, or **Load local GDTF...** to validate and copy a local profile into the
import's resources. Choose its DMX mode and review any model, mode or footprint
differences. The original local file remains unchanged. Available fallback
sources can be selected explicitly from the same row.

The MVR preference **Close GDTF download window automatically when finished**
defaults to off and can also be changed in the queue. With automatic close enabled,
safe completed choices continue the import directly. Ambiguous or failed results
remain open for review. Cancel stops pending transfers and retains completed
choices and valid fallbacks. A matching footprint does not guarantee that DMX
channels have the same meaning.

**Apply selections** also retries failed downloads. Canceling authentication stops the remaining attempts without repeating the sign-in dialog for every row.
