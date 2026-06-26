# MVR-xchange TCP Mode publisher

MVR-xchange lets compatible applications discover each other on a local network and exchange MVR revisions. Perastage's first integration focuses on acting as a TCP Mode publisher/server for the current scene.

## Supported in this version

- Manual TCP Mode service start and stop from **File → MVR-xchange...**.
- Local service advertisement through the isolated MVR-xchange mDNS integration point for `_mvrxchange._tcp`.
- Manual **Publish Current MVR** revisions using the existing Perastage MVR exporter.
- In-memory history for the most recent published revisions.
- Basic handling for `MVR_JOIN`, `MVR_LEAVE`, `MVR_COMMIT`, and `MVR_REQUEST` message flows.
- Requests for a known `FileUUID` return the matching MVR payload. Requests for the latest revision can be made with an empty `FileUUID` or `latest`.

## How to use

1. Open or create a scene in Perastage.
2. Choose **File → MVR-xchange...**.
3. Review the station name, group name, station UUID, and port.
4. Click **Start** to start the TCP Mode service.
5. Click **Publish Current MVR** to export the current scene into memory and announce it as a new MVR revision.
6. In a compatible client such as grandMA3, discover the Perastage station on the same local network and request the published MVR revision.

## Settings

Perastage persists the station name, group name, station UUID, and optional port. The default station name is `Perastage`, the default group name is `Default`, and the station UUID is generated once when no saved UUID exists.

## Limitations

- TCP Mode publisher/server only.
- Manual publishing only; scene edits and saves do not publish automatically.
- WebSocket Mode is not implemented.
- Object-level live synchronization is not implemented.
- The mDNS code is intentionally isolated so the backend can be replaced or expanded without changing GUI or protocol code.
