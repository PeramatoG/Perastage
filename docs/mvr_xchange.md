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
- Requests for a known `FileUUID` return the matching MVR binary packet. Requests with an empty `FileUUID` or `latest` return the latest published revision when one exists.

## Discovery requirements

Perastage uses the vcpkg `mdns` port for MVR-xchange discovery. The CMake option `PERASTAGE_ENABLE_MVR_XCHANGE_MDNS` defaults to `ON`; when it is enabled, CMake requires the `mdns` package and links the `mdns::mdns` target. If the option is turned `OFF`, Perastage builds a disabled backend that reports a clear runtime error instead of pretending discovery is active.

The dialog log reports the service type, group service name, station name, station UUID, advertised TCP port, backend name (`mdns`), host name, and local address diagnostics. Use this information to compare Perastage's advertised interface with the interface selected in the receiving application.

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
3. Review the station name, group name, station UUID, and port.
4. Click **Start** to start the TCP Mode service and mDNS advertisement.
5. Click **Publish Current MVR** to export the current scene into memory and announce it as a new MVR revision.
6. In a compatible client such as grandMA3, open the MVR-xchange view, select the same group name, select the same physical network interface, enable MVR-xchange, and request the published MVR revision.


## TCP Mode join flow

A TCP Mode station must both advertise itself and participate in the group handshake. The minimal Perastage flow is:

1. Perastage registers `_mvrxchange._tcp.local.` and the selected group subservice, such as `Default._mvrxchange._tcp.local.`.
2. Perastage tracks remote stations that are discovered through mDNS or that send an incoming `MVR_JOIN`.
3. Perastage actively queries the selected group service for PTR/SRV/TXT/A records and stores discovered stations.
4. Perastage answers incoming `MVR_JOIN` with `MVR_JOIN_RET` including local station identity and current commit metadata.
5. Perastage sends outgoing `MVR_JOIN` and parses `MVR_JOIN_RET` for resolved remote stations.
6. Published MVR revisions are announced through `MVR_COMMIT` to joined stations when an endpoint is available.

The dialog shows remote station counts and a simple station list for discovered, incoming joined, and outgoing joined stations. These counts help distinguish a raw TCP connection from a completed MVR-xchange handshake. Use **Discover Now** to run an immediate discovery pass instead of waiting for the periodic discovery loop.


## UUID policy

MVR-xchange station and file identifiers use Perastage's shared `core/uuidutils.h` helpers. Locally generated `StationUUID` and `FileUUID` values are canonical lowercase UUID strings in `8-4-4-4-12` form. UUIDs read from settings, JSON messages, and mDNS TXT records are canonicalized before storage and comparison so uppercase remote station UUIDs still deduplicate correctly.

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
- **No published files:** Start the service and click **Publish Current MVR** before requesting a file. Empty `FileUUID` requests return the latest published revision when one exists; if no revision exists, Perastage returns an error instead of exporting from the network thread.
- **Incoming join but no visible station:** Check whether the log also shows an outgoing `MVR_JOIN`, a `MVR_JOIN_RET OK=true`, different station UUIDs, the same group name, and a matching interface. If only an incoming `MVR_JOIN` is visible, Perastage can answer the client but may not yet have a resolved endpoint for the reverse join.

## Settings

Perastage persists the station name, group name, station UUID, and optional port. The default station name is `Perastage`, the default group name is `Default`, and the station UUID is generated once when no saved UUID exists.

## Limitations

- TCP Mode publisher/server only.
- Manual publishing only; scene edits and saves do not publish automatically.
- WebSocket Mode is not implemented.
- Object-level live synchronization is not implemented.
- Builds configured with `PERASTAGE_ENABLE_MVR_XCHANGE_MDNS=OFF` cannot advertise through mDNS and must report that discovery is unavailable.
