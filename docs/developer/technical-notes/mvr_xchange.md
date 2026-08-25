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
- Distinct station instances in the `<station>.<group>._mvrxchange._tcp.local.` form, discovered through the selected group PTR owner.
- Active mDNS discovery of stations in the selected group and outgoing `MVR_JOIN` attempts to resolved stations.
- Outgoing `MVR_JOIN` message construction and TCP client support for station handshakes and commit announcements.
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

1. Perastage publishes PTR records owned by `_mvrxchange._tcp.local.` and the selected group service, such as `Default._mvrxchange._tcp.local.`, targeting one distinct station instance. SRV and TXT records are owned by that exact station instance; SRV targets a stable, station-specific `.local.` host name, and A resolves that host on the selected interface. TXT carries the user-facing `StationName` and persistent `StationUUID`.
2. Perastage tracks remote stations that are discovered through mDNS or that send an incoming `MVR_JOIN`.
3. Perastage keeps one UDP socket bound to multicast port 5353 on the selected interface, continuously consumes responses and unsolicited announcements, and periodically queries the base service. Its TTL cache associates PTR, SRV, TXT, A, and AAAA records even when answer and additional records arrive out of order, while retaining the responder and interface origin so records from independent stations cannot be combined.
4. Perastage answers incoming `MVR_JOIN` with `MVR_JOIN_RET` including local station identity and current commit metadata.
5. Perastage sends outgoing `MVR_JOIN` and parses `MVR_JOIN_RET` for resolved remote stations.
6. Incoming TCP connections may remain open after `MVR_JOIN` and carry additional JOIN, COMMIT, REQUEST, or LEAVE messages using the normal packet framing. This supports peers that retain their joined connection, while peers that close after a response and reconnect for a later operation remain supported. An incoming JOIN receives one JOIN_RET and never causes a reciprocal JOIN. A publication freezes its destination set before adding the new revision to JOIN inventory: stations already joined in either direction receive one COMMIT and are never sent a reciprocal discovery JOIN, while a station joined by discovery during that publication learns the revision only through JOIN inventory. There is no socket broadcast or periodic TCP heartbeat. The narrow grandMA3 port `42424` fallback remains isolated to endpoint resolution and is used only when an identified grandMA3 station omitted its SRV port.

The dialog uses a compact two-column settings area so the remote content remains visible without enlarging the window. It shows every known remote station separately from its advertised files, including discovery/membership state, endpoint, and StationUUID. Both remote lists scroll independently when their contents exceed the available area. These fields distinguish a raw discovery record from a completed MVR-xchange handshake. The complete protocol log can be copied with **Copy Log**. Use **Discover Now** to run an immediate discovery pass instead of waiting for the periodic discovery loop. Automatic failed JOIN attempts use a 30-second per-station retry delay, manual discovery bypasses that delay, and unchanged station counters are not logged repeatedly. If a non-grandMA3 remote station only opens an incoming join connection and does not advertise a reachable TCP endpoint, Perastage cannot push a later `MVR_COMMIT` to that station through standard TCP Mode; the latest commit list is still returned on the station's next `MVR_JOIN`.

UUID text is canonicalized to lowercase by Perastage. UUID hexadecimal digits are case-insensitive, so an uppercase grandMA3 display and a lowercase Perastage display represent the same UUID; comparison and deduplication use the canonical value rather than display casing.


## Compliance and hardening notes

Perastage implements the official TCP Mode exchange path conservatively:

- Supported official flows: mDNS/DNS-SD service advertisement, same-group discovery, `MVR_JOIN`, `MVR_JOIN_RET`, `MVR_LEAVE`, `MVR_COMMIT`, `MVR_COMMIT_RET`, `MVR_REQUEST`, and `MVR_REQUEST_RET` error responses.
- Supported packet payloads: JSON UTF-8 packets for protocol messages and MVR file packets for successful file requests. Ordinary output remains a canonical single package. Incoming multipart payloads are reassembled in zero-based order on their dedicated transaction connection with stable count/type, duplicate, overflow, cumulative-size, and missing-fragment checks.
- Message parsing rejects malformed JSON, missing required identity fields, invalid UUIDs in required UUID fields, and unsupported message types without crashing the service. JOIN and JOIN_RET emit only the canonical `Commits` member and always include `Provider`. The parser accepts legacy `Files` only when `Commits` is absent, distinguishes absent inventory from a present empty inventory, deduplicates metadata by FileUUID and StationUUID, and never allocates payload storage from a remote FileSize declaration. At the input boundary only, a JOIN_RET without `Provider` is accepted because the adjusted official MVR 1.6 response example omits that field despite Table 69 declaring it non-optional; MVR_JOIN still requires it. JOIN and JOIN_RET accept the specification-defined `0.0` new-member version used by grandMA3-class senders, while mixed `0.x` versions remain invalid and local output stays canonical 1.6. One malformed embedded commit rejects the entire JOIN or JOIN_RET with a typed error; valid entries require canonicalizable UUIDs, unsigned FileSize, supported version fields, and valid target UUIDs. Commit metadata accepts canonical 1.0–1.6 versions and the specification-defined 0/0 marker used when joining as a new member. Unsupported official session migration messages are not acted on by the TCP publisher.
- Published revisions are kept in a bounded in-memory history. Each revision records the canonical `FileUUID`, local `StationUUID`, user-friendly file name, comment, creation timestamp, file size, and MVR payload.
- Perastage only serves already-published in-memory MVR payloads. It does not export a new MVR from a network worker, and it refuses empty payloads. Manual publishing validates that the exported archive contains `GeneralSceneDescription.xml` before the revision is announced.
- Unknown `FileUUID` requests receive a standard `MVR_REQUEST_RET` error. Empty `FileUUID` requests use the specification-defined latest-file behavior and return an error when no revision is available.
- MVR 1.6 Table 74 defines `MVR_REQUEST.FromStationUUID` as an array of UUIDs, while the official request example inconsistently shows a single string. Perastage therefore emits only the normative array form, using an empty array when there is no source station, and accepts either an array or the documented string form at the input boundary. A non-empty legacy string is normalized to a one-element list, and the example's empty string is normalized to an empty list. Every non-empty UUID is validated and canonicalized before the request is handled. This compatibility does not apply to `MVR_LEAVE`: Table 70 defines its `FromStationUUID` separately as one UUID, so LEAVE output and canonical parsing remain singular.
- For outgoing requests, each `FromStationUUID` value identifies the remote station from which the advertised MVR is retrieved. Perastage uses the selected remote commit owner's StationUUID in this array, not its own local StationUUID, and logs both the requested FileUUID and remote source StationUUID for interoperability diagnostics.

The following items are intentionally outside this implementation:

- WebSocket Mode and DNS-routable session hosting.
- `MVR_NEW_SESSION_HOST` based migration or mode switching.
- Proprietary Peraviz Live Link, incremental synchronization, private messages, automatic Peraviz launch, or automatic scene-change publishing.

If a specification detail is ambiguous in the local copy of the MVR documentation, Perastage keeps the behavior conservative and documents the assumption here instead of adding private behavior to the official MVR-xchange layer.

### Resource limits and lifecycle

Control JSON is limited to 1 MiB, MVR payloads to 512 MiB, and buffered connection input to one header plus the applicable payload limit. The decoder validates the magic, protocol version, zero-based package number, package count, payload type, length conversion, and overflow before copying payload bytes. Sends use a bounded timeout, while an otherwise valid joined connection is allowed to remain idle until the peer closes, sends LEAVE, encounters a fatal error, or the service stops. The server owns at most 16 concurrent connections and deterministically shuts down idle clients. A received MVR payload must match advertised `FileSize` when inventory metadata is available.

Station membership is independent of socket lifetime. Incoming and outgoing handshake facts are retained in the station registry; explicit MVR_LEAVE is wired to the registry and suppresses publication and automatic discovery JOINs until a later valid JOIN initiated by the remote station, and inventory replacement happens only when the JOIN inventory member was present. Incoming targeted MVR_COMMIT metadata updates the associated joined station without erasing unrelated history. Canonical MVR_LEAVE output uses `FromStationUUID`; `StationUUID` is accepted only as a read-only legacy alias. DNS identities compare ASCII case-insensitively without requiring a trailing root dot. A valid StationUUID promotes and merges provisional DNS identity; endpoint reuse never merges two different known StationUUID values, and incoming COMMIT metadata is accepted only for an active protocol membership.

### Multicast discovery and DNS-SD cache

The browser owns a reusable UDP 5353 socket, joins `224.0.0.251` on the selected IPv4 interface, receives answer and additional records continuously, and uses an immediate query, one fast three-second refresh, then thirty-second steady refreshes. Background station reconciliation never forces a query; only the explicit **Discover Now** action does. Stop waits for the bounded receive poll before closing the socket, and restart creates a fresh cache for the newly selected interface. TCP listener shutdown likewise uses a bounded readiness poll rather than relying on platform-specific interruption of a blocking `accept()`. `SO_REUSEADDR` is required and `SO_REUSEPORT` is enabled where supported. Builds with `PERASTAGE_ENABLE_MVR_XCHANGE_MDNS=OFF` retain an explicit disabled backend.

PTR, SRV, TXT, A, and AAAA records retain their owner, target or address, TTL, interface identifier, responder address, last-seen time, and calculated expiry. Resolution requires SRV and TXT ownership to match the exact station instance named by PTR; it never substitutes records owned by the group service while waiting for station records. Multiple responders and multiple station instances therefore remain separate cache origins, preventing one station's TXT identity, SRV port, or host address from being combined with another station. Both group-owned and base-service PTR discovery are accepted when their target is visibly a concrete instance in the selected group. Separate TXT strings in one DNS message merge case-insensitive keys, while a refreshed TXT RRset replaces the previous set. DNS identity is ASCII case-insensitive and ignores an optional trailing root dot while display spelling is preserved. TTL zero schedules the RFC 6762 one-second goodbye grace period; expiry removes the record from resolution, and station reconciliation clears stale incoming/outgoing handshake state. Absence is determined by TTL evidence rather than one unanswered query. IPv6 records are cached and displayed, although the current TCP transport still opens IPv4 sockets, so an AAAA-only peer is not yet reachable.

The group service and concrete station instance are deliberately different names; startup refuses an ambiguous configuration if they ever compare equal. StationUUID remains the protocol identity once TXT or JOIN data is available. Perastage's SRV host label is derived from the application name, machine name, and a stable StationUUID suffix rather than the user-facing StationName, avoiding collisions with another application on the same machine without changing the persisted StationUUID. As a second defensive layer, the registry rejects its own StationUUID, service instance, or exact advertised IP-and-port endpoint while allowing other applications on the same IP when their TCP ports differ.

## UUID policy

MVR-xchange station and file identifiers use Perastage's shared `core/uuidutils.h` helpers. Locally generated `StationUUID` and `FileUUID` values are canonical lowercase UUID strings in `8-4-4-4-12` form. UUIDs read from settings, JSON messages, and mDNS TXT records are canonicalized before storage and comparison so uppercase or dashless remote station UUIDs still deduplicate correctly. Invalid UUIDs are rejected for protocol fields where the official message requires a UUID, including `StationUUID` in JOIN messages, canonical `FromStationUUID` in LEAVE/REQUEST messages, and `FileUUID`/`StationUUID` in COMMIT messages. The legacy LEAVE `StationUUID` alias is canonicalized and validated only at the input boundary.

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
