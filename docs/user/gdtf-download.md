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

On systems where the operating-system credential store is unavailable or wxWidgets was built without secure-store support, Perastage can still validate credentials for the current operation, but it does not persist the password. You will need to enter the password again in a later session.
