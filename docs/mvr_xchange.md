# MVR-xchange TCP Mode publisher

MVR-xchange lets compatible applications discover each other on a local network and exchange MVR revisions. Perastage's current integration acts as a TCP Mode publisher/server for the current scene.

## Supported in this version

- Manual TCP Mode service start and stop from **File → MVR-xchange...**.
- DNS-SD/mDNS advertisement for `_mvrxchange._tcp.local.` when the build has a Bonjour/DNS-SD backend available.
- Group discovery through the DNS-SD subtype `<group>._mvrxchange._tcp.local.` diagnostics, for example `Default._mvrxchange._tcp.local.`.
- TXT records containing `StationName` and `StationUUID`.
- Manual **Publish Current MVR** revisions using the existing Perastage MVR exporter.
- In-memory history for the most recent published revisions.
- Basic TCP Mode handling for `MVR_JOIN`, `MVR_LEAVE`, `MVR_COMMIT`, and `MVR_REQUEST` message flows.
- Requests for a known `FileUUID` return the matching MVR binary packet. Requests with an empty `FileUUID` or `latest` return the latest published revision when one exists.

## Discovery requirements

Perastage must be built with a DNS-SD backend to advertise on the network. The current backend uses Bonjour/DNS-SD (`dns_sd.h` and a DNS-SD library such as `dns_sd` or `dnssd`) when CMake can find it, registering `_mvrxchange._tcp` with the selected group as a DNS-SD subtype. If that backend is not available, the service reports a clear mDNS error and does not pretend that discovery is active.

The dialog log reports the service type, group service name, station name, station UUID, advertised TCP port, backend name, and local address diagnostics. Use this information to compare Perastage's advertised interface with the interface selected in the receiving application.

## How to use

1. Open or create a scene in Perastage.
2. Choose **File → MVR-xchange...**.
3. Review the station name, group name, station UUID, and port.
4. Click **Start** to start the TCP Mode service and mDNS advertisement.
5. Click **Publish Current MVR** to export the current scene into memory and announce it as a new MVR revision.
6. In a compatible client such as grandMA3, open the MVR-xchange view, select the same group name, select the same physical network interface, enable MVR-xchange, and request the published MVR revision.

## Testing with grandMA3

- Keep the group name identical on both applications. The default group is `Default`.
- Select the real Ethernet or Wi-Fi interface in grandMA3 when testing between computers. Loopback (`127.0.0.1`) is only useful when both applications run on the same machine and the client watches loopback.
- Accept the Windows Firewall prompt for Perastage so inbound TCP connections and mDNS traffic are not blocked.
- Verify that the Perastage dialog reports a successful mDNS backend. If it reports that DNS-SD is unavailable, grandMA3 cannot discover Perastage through MVR-xchange discovery.

## Troubleshooting

- **Windows Firewall prompt appears:** Allow Perastage on the intended private network so grandMA3 can connect to the TCP listener.
- **Service is not visible:** Check that the dialog reports successful mDNS advertisement and does not show a DNS-SD backend error.
- **Wrong interface:** Make sure grandMA3 is watching the same Ethernet/Wi-Fi interface that Perastage logs in its local address diagnostics.
- **Loopback vs LAN:** A loopback advertisement is not visible to another computer. Use a real LAN interface for cross-device testing.
- **No published files:** Start the service and click **Publish Current MVR** before requesting a file. Perastage does not export silently on request.

## Settings

Perastage persists the station name, group name, station UUID, and optional port. The default station name is `Perastage`, the default group name is `Default`, and the station UUID is generated once when no saved UUID exists.

## Limitations

- TCP Mode publisher/server only.
- Manual publishing only; scene edits and saves do not publish automatically.
- WebSocket Mode is not implemented.
- Object-level live synchronization is not implemented.
- Builds without Bonjour/DNS-SD support cannot advertise through mDNS and must report that discovery is unavailable.
