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
