# ConsoleLAN 1.2.1 manual

## Language and appearance

Use **Language** at the bottom of the sidebar to choose **English** or **Español**, and **Theme** to choose **Light** or **Dark**. Both changes take effect immediately, including room/console labels, statuses, diagnostics and the built-in manual. You do not need to restart or leave the room. Your room name, console names, IP reservations and selected interfaces remain unchanged.

Both preferences are saved immediately for your Windows user and restored next time. On the first start, the application uses Spanish on a Spanish-language Windows installation and English otherwise; the initial theme is Light. Upgrading from 1.0 keeps your saved consoles, reservations and other settings.

The dark theme covers the main window, tables, inputs, selection, disabled controls, diagnostic/log panels and Qt dialogs. The title bar is also adapted on supported Windows versions. Driver/Windows error details and user-defined adapter/console names retain their original language. Log files retain their original records; the viewer and exported human-readable messages use your selected language.

## Navigation

**Play over LAN** contains three steps: connection, room and consoles. **Diagnostics** holds logs and support tools; **Reservations** shows host-managed addresses. Subnet and UDP budget settings are collapsed under **Advanced options**. Select a console to view and copy its IP configuration; **Manage** groups Ethernet, MAC and removal actions.

## Prepare each site

Connect the Windows PC and consoles by Ethernet to the same switch. Configure a VPN on the PCs that permits UDP connectivity between participants. ConsoleLAN does not run a VPN on the consoles or configure your routers. Domestic subnets may differ or be identical. Use **one ConsoleLAN bridge PC per physical LAN**.

Install [Npcap](https://npcap.com/#download) separately on each PC. WinPcap compatibility mode is not required. An installation restricted to administrators may require elevation to capture. ConsoleLAN loads Npcap from its system installation, not a DLL beside the executable.

Your hardware must allow the relevant Ethernet frames to be captured and injected. Promiscuous mode alone does not make an ordinary switch send the PC every unicast between other switch ports. Validate your equipment using the acceptance guide; a compatible arrangement or port mirroring may be required.

## Create or join a room

1. Select **PC VPN**, checking its name and IPv4 address. Do not choose the domestic Ethernet as the VPN. Disconnected adapters are listed for diagnosis but cannot start a session.
2. On the host, choose **Create a room**, enter a name and, under **Advanced options**, a private /24 subnet, such as `10.253.42.0`. The app checks known interfaces and routes for overlaps. A joining PC also checks the room subnet against its local networks. ConsoleLAN does not change PC IPs, routes, NAT or Windows bridges.
3. Click **Create room**. Other PCs select their VPN and choose **Join a room** and **Find rooms on my VPN**, then double-click an advertisement. Up to five recent advertisements are displayed, without imposing a global limit on independent hosts.
4. If discovery is unavailable, enter the host's **VPN IPv4 address** and choose **Connect / reconnect**. Direct connection works with VPNs that do not carry broadcast, including many WireGuard configurations. The standard transport port is UDP 32190; discovery uses UDP 32189. The address field accepts `IP:port` for diagnosis.
5. Check the room, host and member list. Each PC joins one room at a time. The host coordinates reservations and relays client traffic; there is no central server or automatic host migration.

Rooms are open to participants of the trusted VPN. Room IDs and session tokens are not account authentication or additional encryption. Do not expose these ports directly to the Internet.

## Add and configure consoles

Choose the physical Ethernet interface or leave automatic detection selected, then click **Add console**. Each PC can remember up to 16 consoles. The host grants an authoritative IP reservation. Wait until the reserved address appears before configuring the console.

Select the console row and use **Copy IP configuration**. Set a manual wired address on the console using that IP and mask `255.255.255.0`. Assignable addresses are `.2` through `.254`; `.1` is reserved but does not run a gateway or DNS server.

Leave gateway and DNS blank or `0.0.0.0` if the console accepts that. **ConsoleLAN does not provide Internet access, PSN, DNS or a virtual router.** Do not copy gateway/DNS values from the conceptual illustrations expecting a service to exist there.

Some console firmware requires values before saving a LAN configuration. If it permits operation without those services, its own reserved IP may be used only as a configuration placeholder, knowing that there is no gateway/DNS service at that address and the Internet test will fail. This does not guarantee the firmware will accept or retain it. Firmware/game combinations that insist on successful online validation need an additional networking solution outside this release's scope.

An Internet test may fail while wired LAN is available. Use the game's **LAN/System Link** mode, not its online/PSN mode. Console/game/firmware compatibility, versions, content and latency limits must be tested separately.

## MAC learning, interfaces and conflicts

A MAC is learned only from a frame whose IPv4 **source** matches the reservation, or whose ARP **sender IP** matches it and whose ARP sender MAC agrees with its Ethernet source. An ARP request merely asking about the reserved address does not identify its owner.

Automatic detection opens active physical Ethernet candidates and remembers where the matching source appears. If it appears on multiple interfaces, the app shows a conflict. Choose the correct interface and press **Apply selected Ethernet** for the selected console. Changing the dropdown alone sets the choice for the next console.

Name, identity, preferred IP, MAC and interface are remembered. A remembered MAC does not prove traffic was seen this session; **Traffic observed** does not prove connectivity to a remote console. Confirm bidirectional ARP, sustained unicast and actual gameplay.

Conflicting MAC/IP associations are reported without silently replacing the previous record. Check the consoles' configuration. **Relearn MAC** creates a new reservation identity and removes the old one when synchronized; its IP may change if unavailable. **Remove / forget** removes a local console and requests release of its reservation. Active remote consoles are removed from their owner PC.

The host can review **Host reservations**, including offline sites, and explicitly release an offline reservation after checking its identity. A returning console may need a different IP after its old reservation is released.

## Reconnect or change room

Choose **Leave** before changing room or VPN. Remembered consoles request their preferred addresses from the new host. If an address changes, the app logs a reconfiguration message and shows the new IP. Always check reservations after switching rooms.

If the host closes or stops responding for approximately 6.5 seconds, forwarding stops. Restore the host/VPN and use **Connect / reconnect**. A VPN IPv4 change requires refreshing and selecting the adapter again. There is no automatic host migration.

## Diagnostics and firewall

TX counts local frames admitted to the tunnel; RX counts remote frames accepted by the engine. Neither proves delivery to the console. Capture observations and successful Npcap injections are shown separately in **Diagnostics and logs**.

Latency is control RTT **to the host**, not in-game ping. Incomplete messages count expired reassemblies. Estimated gaps count missing per-link data sequence numbers after a reorder window; a lost final packet may remain undetected. This is not a complete measurement of game packet loss.

The default UDP budget is 1100 bytes including the 48-byte application header. It is configurable from 512 to 1400 before starting or sending the next command. A smaller value helps with constrained VPN MTUs. There are no application retransmissions of game traffic.

Use **Export diagnostics and logs** to save the state and log. It includes IPs, MACs, names and identifiers, but not captured frame contents. Logs rotate around 2 MiB and retain the previous file. **Open data folder** shows `state.json` and the log files. Do not clone this state file between PCs: each needs its own identity. Corrupted saved data is preserved rather than silently overwritten.

The installer offers an optional, unchecked firewall task. With your consent it permits this executable on the selected VPN interface for UDP 32189–32190 and the chosen peers. It does not disable the firewall or open router ports. The portable package also includes `scripts/firewall.ps1` for voluntary elevated configuration; use your actual executable path and local VPN IP.

## Isolation and limits

The tunnel validates room, session, endpoint and registered source ownership. Known unicast is sent only to its destination site; known local unicast remains on the switch. ARP, broadcast and multicast reach room sites with consoles. Unknown unicast is rate-limited.

Ethernet without VLAN, ARP and IPv4 are supported up to 1514 bytes per frame. IPv6, VLAN, jumbo frames and other EtherTypes are not tunneled. DHCP is blocked. Non-initial IPv4 UDP fragments require an observed non-DHCP first fragment; orphan fragments are discarded.

The tunnel cannot isolate physical switch ports: consoles on the same switch may see each other even when their PCs use different rooms. Local duplicate-bridge beacons provide an auxiliary warning, not switch isolation. Validate a real two-PC setup using [the acceptance checklist](ACEPTACION.md). The original 1.0 [technical report](INFORME.md) and [1.1 update report](INFORME-1.1.md) distinguish simulated/software checks from hardware acceptance.

## Access from the PC to its local consoles (1.2)

This optional feature starts disabled and requires an active room. Add a console and wait for its reserved IP, MAC and Ethernet adapter to be detected. Under **Access from this PC**, click **Enable local access** and approve the Windows administrator prompt for `ConsoleLAN.Access.exe`.

ConsoleLAN creates its own **ConsoleLAN Local** Wintun adapter. It uses the room subnet's reserved `.1` address with `/32` routes only to detected local consoles. `.1` is excluded from console reservations; it is not a gateway or DNS server. Existing Ethernet/VPN addresses, DHCP, DNS and gateways are not changed. Overlapping networks and ARP conflicts on `.1` prevent activation.

Once local access is enabled, connect your FTP client to your console's reserved IP and its FTP server port. The console must already be running an FTP server. Passive FTP is recommended; this feature does not open firewall ports for servers on your PC. LAN gaming can continue at the same time. Consoles in other homes are not accessible through this feature.

Disabling local access, leaving the room or closing ConsoleLAN removes its own host routes and adapter. The helper also stops if its parent process dies, the control pipe disconnects or heartbeats stop. Wait for the adapter to disappear before changing the room subnet. The shared Wintun driver is not uninstalled, since other applications may use it.

Cancelling or denying administrator permission leaves local access disabled without stopping the room. Loss of a valid local IP/MAC/Ethernet target stops local access; enable it again after resolving the issue. This is IPv4-only and creates no Windows bridges, NAT or default route.

The automated 1.2 test uses the signed driver and real Windows TCP for passive FTP against a simulated console. This does not certify a physical PS5 FTP server or a real console gameplay session.
