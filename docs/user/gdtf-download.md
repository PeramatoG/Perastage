# Download GDTF Files

Perastage can download fixture profiles from GDTF Share.

## Before you start

- You need an active GDTF Share account.
- Internet access is required for login and downloads.

## Download workflow

1. Open **Tools → Download GDTF**.
2. Sign in when prompted.
3. Search for the fixture profile you need.
4. Download and store it in your local user library.

## Related tools

- **Tools → Edit dictionaries** to manage local mapping and dictionary entries.
- **Tools → Open user library folder** to inspect downloaded files directly.

## When to use this

Use GDTF download when fixtures appear generic, missing, or visually incorrect after opening/importing MVR content.

## Credential storage

When available, Perastage stores the GDTF Share password in the operating system's secure credential store through wxWidgets. The username may remain in non-secret Perastage metadata so the sign-in field can be prefilled. The password is not written to `gdtf_credentials.json` or application configuration.

Official Perastage builds include wxSecretStore support. On Linux, password persistence also requires an active Freedesktop Secret Service provider such as GNOME Keyring or KWallet. On systems where the operating-system credential store is unavailable at runtime, or on intentionally minimal developer builds without secure-store support, Perastage can still validate credentials for the current operation, but it does not persist the password. Perastage shows a warning in this state, may keep the username as a non-secret hint, and will ask for the password again in a later session.

The GDTF search dialog identifies whether it is showing the online catalog or a cached catalog. A cached catalog can be browsed without a current authenticated session, but Perastage will ask you to sign in when you download. If the online catalog was loaded successfully in the same workflow, the authenticated session is reused for the download without asking again.

Downloads are written to temporary sibling files first and are published only after the response is successful and ZIP-compatible. If a download, cancellation, or local publication step fails, an existing destination GDTF file is preserved.
