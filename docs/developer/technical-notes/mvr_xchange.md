# MVR-xchange TCP Mode publisher

MVR-xchange lets compatible applications discover each other on a local network and exchange MVR revisions. Perastage's current integration acts as a minimal TCP Mode station and publisher for the current scene.

## Supported in this version

- Manual TCP Mode service start and stop from **File → MVR-xchange...**.
- mDNS/DNS-SD advertisement for `_mvrxchange._tcp.local.` using the vcpkg `mdns` backend when MVR-xchange discovery is enabled.
- Group discovery through `<group>._mvrxchange._tcp.local.` responses, for example `Default._mvrxchange._tcp.local.`.
- TXT records containing `StationName` and `StationUUID`.
- Manual **Publish Current MVR** revisions using the existing Perastage MVR exporter.
- In-memory history for the most recent published revisions.
- Basic TCP Mode handling for `MVR_JOIN`, `MVR_JOIN_RET`, `MVR_LEAVE`, `MVR_COMMIT`, and `MVR_REQUEST` message flows.
- Remote station tracking for discovered, incoming joined, and outgoing joined station states.
- Group-qualified DNS-SD service instances in the `<station>.<group>._mvrxchange._tcp.local.` form used by compatible stations.
- Active mDNS discovery of stations in the selected group and outgoing `MVR_JOIN` attempts to resolved stations.
- Outgoing `MVR_JOIN` message construction and short-lived TCP client support for station handshakes and commit announcements.
- Canonical lowercase UUID handling for local and remote MVR-xchange `StationUUID` and `FileUUID` values using Perastage's shared UUID utilities.
- Requests for a known `FileUUID` return the matching non-empty MVR binary packet. Requests with an empty `FileUUID` use the specification-defined latest-revision behavior and return the latest published revision when one exists.
- Requested remote MVR payloads keep the advertised MVR file name when opened as a new project, while merge imports preserve the current project name.

## Discovery requirements

Perastage uses the vcpkg `mdns` port for MVR-xchange discovery. The CMake option `PERASTAGE_ENABLE_MVR_XCHANGE_MDNS` defaults to `ON`; when it is enabled, CMake requires the `mdns` package and links the `mdns::mdns` target. If the option is turned `OFF`, Perastage builds a disabled backend that reports a clear runtime error instead of pretending discovery is active.

The dialog log reports the service type, group service name, station name, station UUID, advertised TCP port, backend name (`mdns`), host name, selected interface, advertised address, discovered stations, resolved endpoints, failed resolutions, and transfer byte counts. These diagnostics are written to the dialog log and application log without blocking modal message boxes. Use this information to compare Perastage's advertised interface with the interface selected in the receiving application.

## Build dependency

Install the vcpkg `mdns` port before configuring Perastage with MVR-xchange discovery enabled:

```bash
vcpkg install mdns:x64-windows
vcpkg install mdns:x64-linux
vcpkg install mdns:x64-osx
```

The intended backend does not require a manual Bonjour or Avahi installation. If you explicitly configure with `-DPERASTAGE_ENABLE_MVR_XCHANGE_MDNS=OFF`, the TCP publisher can still be compiled, but network discovery is unavailable and the dialog reports that state clearly.

## How to use

1. Open or create a scene in Perastage.
2. Choose **File → MVR-xchange...**.
3. Review the station name, group name, station UUID, and port. New/default installations use a station label like `Perastage on PERAMATO - DESKTOP`, preferring the full host name when available; custom station names are preserved.
4. Click **Start** to start the TCP Mode service and mDNS advertisement. The dialog status shows the advertised IP address and TCP port, for example `Running on 192.168.1.50:42424`.
5. Click **Publish Current MVR** to export the current scene into memory and announce it as a new MVR revision. Perastage announces a user-friendly file name based on the current project name and UTC publish timestamp instead of exposing the internal `FileUUID` in the suggested file name.
6. In a compatible client such as grandMA3, open the MVR-xchange view, select the same group name, select the same physical network interface, enable MVR-xchange, and request the published MVR revision.


## TCP Mode join flow

The current MVR-xchange specification says TCP Mode uses mDNS discovery, the `_mvrxchange._tcp.local.` service, a group subservice such as `Default._mvrxchange._tcp.local.`, TXT records with `StationName` and `StationUUID`, and peer-to-peer `MVR_JOIN` messages to stations registered in the same group. Perastage follows that flow for the minimal publisher path:

1. Perastage registers `_mvrxchange._tcp.local.` and the selected group subservice, such as `Default._mvrxchange._tcp.local.`.
2. Perastage tracks remote stations that are discovered through mDNS or that send an incoming `MVR_JOIN`.
3. Perastage keeps one UDP socket bound to multicast port 5353 on the selected interface, continuously consumes responses and unsolicited announcements, and periodically queries the selected group and base service. Its TTL cache associates PTR, SRV, TXT, A, and AAAA records even when answer and additional records arrive out of order.
4. Perastage answers incoming `MVR_JOIN` with `MVR_JOIN_RET` including local station identity and current commit metadata.
5. Perastage sends outgoing `MVR_JOIN` and parses `MVR_JOIN_RET` for resolved remote stations.
6. Each TCP Mode operation is a short-lived transaction: open a connection, send exactly one `MVR_JOIN`, `MVR_COMMIT`, or `MVR_REQUEST`, receive the matching response or binary payload, and close. An incoming JOIN receives one JOIN_RET and never causes a reciprocal JOIN. A publication sends one COMMIT per eligible joined station; there is no socket broadcast or JOIN refresh path. The narrow grandMA3 port `42424` fallback remains isolated to endpoint resolution and is used only when an identified grandMA3 station omitted its SRV port.

The dialog shows remote station counts and a simple station list for discovered, incoming joined, and outgoing joined stations. These counts help distinguish a raw TCP connection from a completed MVR-xchange handshake. Use **Discover Now** to run an immediate discovery pass instead of waiting for the periodic discovery loop. If a non-grandMA3 remote station only opens an incoming join connection and does not advertise a reachable TCP endpoint, Perastage cannot push a later `MVR_COMMIT` to that station through standard TCP Mode; the latest commit list is still returned on the station's next `MVR_JOIN`.


## Compliance and hardening notes

Perastage implements the official TCP Mode exchange path conservatively:

- Supported official flows: mDNS/DNS-SD service advertisement, same-group discovery, `MVR_JOIN`, `MVR_JOIN_RET`, `MVR_LEAVE`, `MVR_COMMIT`, `MVR_COMMIT_RET`, `MVR_REQUEST`, and `MVR_REQUEST_RET` error responses.
- Supported packet payloads: JSON UTF-8 packets for protocol messages and MVR file packets for successful file requests. Ordinary output remains a canonical single package. Incoming multipart payloads are reassembled in zero-based order on their dedicated transaction connection with stable count/type, duplicate, overflow, cumulative-size, and missing-fragment checks.
- Message parsing rejects malformed JSON, missing required identity fields, invalid UUIDs in required UUID fields, and unsupported message types without crashing the service. JOIN and JOIN_RET emit only the canonical `Commits` member. The parser accepts legacy `Files` only when `Commits` is absent, distinguishes absent inventory from a present empty inventory, deduplicates metadata by FileUUID and StationUUID, and never allocates payload storage from a remote FileSize declaration. Unsupported official session migration messages are not acted on by the TCP publisher.
- Published revisions are kept in a bounded in-memory history. Each revision records the canonical `FileUUID`, local `StationUUID`, user-friendly file name, comment, creation timestamp, file size, and MVR payload.
- Perastage only serves already-published in-memory MVR payloads. It does not export a new MVR from a network worker, and it refuses empty payloads. Manual publishing validates that the exported archive contains `GeneralSceneDescription.xml` before the revision is announced.
- Unknown `FileUUID` requests receive a standard `MVR_REQUEST_RET` error. Empty `FileUUID` requests use the specification-defined latest-file behavior and return an error when no revision is available.

The following items are intentionally outside this implementation:

- WebSocket Mode and DNS-routable session hosting.
- `MVR_NEW_SESSION_HOST` based migration or mode switching.
- Proprietary Peraviz Live Link, incremental synchronization, private messages, automatic Peraviz launch, or automatic scene-change publishing.

If a specification detail is ambiguous in the local copy of the MVR documentation, Perastage keeps the behavior conservative and documents the assumption here instead of adding private behavior to the official MVR-xchange layer.

### Resource limits and lifecycle

Control JSON is limited to 1 MiB, MVR payloads to 512 MiB, and buffered transaction input to one header plus the applicable payload limit. The decoder validates the magic, protocol version, zero-based package number, package count, payload type, length conversion, and overflow before copying payload bytes. Socket operations use bounded timeouts and the server accepts at most 16 concurrent transactions. A received MVR payload must match advertised `FileSize` when inventory metadata is available.

Station membership is independent of socket lifetime. Incoming and outgoing handshake facts are retained in the station registry; explicit MVR_LEAVE is wired to the registry and suppresses publication until a later successful JOIN, and inventory replacement happens only when the JOIN inventory member was present. Incoming targeted MVR_COMMIT metadata updates the associated joined station without erasing unrelated history. DNS identities compare ASCII case-insensitively without requiring a trailing root dot.

### Multicast discovery and DNS-SD cache

The browser owns a reusable UDP 5353 socket, joins `224.0.0.251` on the selected IPv4 interface, receives answer and additional records continuously, and uses an immediate query followed by periodic refresh queries. Stop waits for the bounded receive poll before closing the socket, and restart creates a fresh cache for the newly selected interface. `SO_REUSEADDR` is required and `SO_REUSEPORT` is enabled where supported. Builds with `PERASTAGE_ENABLE_MVR_XCHANGE_MDNS=OFF` retain an explicit disabled backend.

PTR, SRV, TXT, A, and AAAA records retain their owner, target or address, TTL, interface identifier, last-seen time, and calculated expiry. Resolution supports conventional instance-owned SRV/TXT records and the official MVR group-owned SRV/TXT compatibility layout. DNS identity is ASCII case-insensitive and ignores an optional trailing root dot while display spelling is preserved. TTL zero schedules the RFC 6762 one-second goodbye grace period; expiry removes the record from resolution, and station reconciliation clears stale incoming/outgoing handshake state. Absence is determined by TTL evidence rather than one unanswered query. IPv6 records are cached and displayed, although the current TCP transport still opens IPv4 sockets, so an AAAA-only peer is not yet reachable.

## UUID policy

MVR-xchange station and file identifiers use Perastage's shared `core/uuidutils.h` helpers. Locally generated `StationUUID` and `FileUUID` values are canonical lowercase UUID strings in `8-4-4-4-12` form. UUIDs read from settings, JSON messages, and mDNS TXT records are canonicalized before storage and comparison so uppercase or dashless remote station UUIDs still deduplicate correctly. Invalid UUIDs are rejected for protocol fields where the official message requires a UUID, including `StationUUID` in join/leave messages and `FileUUID`/`StationUUID` in commit messages.

## Network interface and port debugging

The dialog includes a **Network interface** selector. Use **Auto / All suitable interfaces** for normal LAN use, or choose **Loopback 127.0.0.1** when grandMA3 is running on the same machine and is watching loopback. If grandMA3 is watching Ethernet or Wi-Fi, select the matching Perastage interface so the advertised A record matches that address.

The TCP port can stay on **Auto** because the mDNS SRV record advertises the effective port. For repeatable debugging, set a fixed port such as `42424`, restart the service, and allow that port through Windows Firewall for private networks.

After Start, check the log for `MVR-xchange selected interface`, `MVR-xchange advertised A record`, `MVR_JOIN`, and `MVR_REQUEST`. If only a raw TCP connection appears and no `MVR_JOIN` follows, the client connected but did not complete the MVR-xchange handshake.

## Testing with grandMA3

- Keep the group name identical on both applications. The default group is `Default`.
- Select the real Ethernet or Wi-Fi interface in grandMA3 when testing between computers. Loopback (`127.0.0.1`) is only useful when both applications run on the same machine and the client watches loopback.
- Accept the Windows Firewall prompt for Perastage so inbound TCP connections and mDNS traffic are not blocked.
- Verify that the Perastage dialog reports the `mdns` backend. If Perastage was built without the vcpkg `mdns` backend, grandMA3 cannot discover it through MVR-xchange discovery.

## Troubleshooting

- **Windows Firewall prompt appears:** Allow Perastage on the intended private network so grandMA3 can connect to the TCP listener.
- **Service is not visible:** Check that the dialog reports the `mdns` backend, matching group name, matching selected interface, and no disabled-backend error. Also check for `MVR-xchange discovered station` logs; if grandMA3 shows only its own service, active discovery did not resolve Perastage or the selected group/interface does not match.
- **Wrong interface:** Make sure grandMA3 is watching the same Ethernet/Wi-Fi or loopback interface selected in Perastage, and that the logged advertised A record matches that interface.
- **Loopback vs LAN:** A loopback advertisement is not visible to another computer. Use loopback only for same-machine tests and a real LAN interface for cross-device testing.
- **No published files:** Start the service and click **Publish Current MVR** before requesting a file. Empty `FileUUID` requests return the latest published revision when one exists; if no revision exists, Perastage returns an error instead of exporting from the network thread. Literal placeholder values such as `latest` are not treated as a standard FileUUID request.
- **Incoming join but no visible station:** Check whether the log also shows an outgoing `MVR_JOIN`, a `MVR_JOIN_RET OK=true`, different station UUIDs, the same group name, and a matching interface. If only an incoming `MVR_JOIN` is visible, Perastage can answer the client but may not yet have a resolved endpoint for the reverse join.

## Settings

Perastage persists the station name, group name, station UUID, and optional port. The default station name is `Perastage`, the default group name is `Default`, and the station UUID is generated once when no saved UUID exists.

## Limitations

- TCP Mode publisher/server only.
- Manual publishing only; scene edits and saves do not publish automatically.
- WebSocket Mode is not implemented.
- Object-level live synchronization is not implemented.
- Builds configured with `PERASTAGE_ENABLE_MVR_XCHANGE_MDNS=OFF` cannot advertise through mDNS and must report that discovery is unavailable.
