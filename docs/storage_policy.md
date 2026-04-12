# Storage Policy (Source of Truth)

This document defines where Perastage stores data at runtime and which locations are writable.

## Intent

Perastage uses a split storage model:

- **Installation tree** is the immutable base content distributed with the application.
- **User profile tree** is the writable source of truth for user-specific and editable data.

A developer should be able to answer, without ambiguity: **Perastage stores user data in the user profile tree, not in the installation tree.**

## Installation tree (read-only at runtime)

The installation tree contains application-owned assets that ship with Perastage.

Typical contents:

- Executables and runtime binaries.
- Base resources required for the application to start.
- A seed library used as bundled defaults.

Runtime policy:

- Treat installation files as **read-only**.
- Do not persist user edits or generated artifacts into installation paths.

## User profile tree (read/write at runtime)

The user profile tree is Perastage's writable storage and user-data source of truth.

It contains:

- Editable library content:
  - `fixtures/`
  - `trusses/`
  - `scene_objects/`
  - `misc/`
  - `projects/`
  - `default_layouts/`
- Dictionaries.
- Configuration.
- Logs.
- Credentials/secrets.

Runtime policy:

- User-created or user-modified data is stored here.
- Application updates must not assume this content can be replaced.

## Runtime resolution order

When loading runtime data that may exist in both locations, Perastage resolves in this order:

1. **User profile tree first**.
2. **Installed base content second (fallback only)**.

This guarantees user overrides are respected while still allowing bundled defaults to work when user copies do not exist.

## Practical rule of thumb

- If data is shipped by the installer and expected to be static: keep it in installation content.
- If data can be edited, generated, personalized, or is operational state: keep it in user profile storage.
