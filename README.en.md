# ConsoleLAN 1.1

[Español](README.md) · English

Windows x64 application in C++20, Qt 6 Widgets, Winsock and Npcap for selectively extending console LAN traffic over an existing VPN. Each room's host coordinates IP reservations and relays UDP traffic. No accounts or central server are required.

## New in 1.1

- **Light and Dark themes**, including tables, inputs, disabled states and dialogs.
- **English and Spanish**, switchable while the app is running.
- Both preferences are saved immediately; switching does not leave the room or change console names, IPs or reservations.
- Built-in manual in both languages, translated diagnostics and displayed logs, and an English/Spanish installer.
- Fixes a possible crash while refreshing session tables.

## Downloads

- [Windows x64 installer](https://github.com/MalfrenLasVegas/ConsoleLAN/releases/download/v1.1.0/ConsoleLAN-1.1.0-win64-setup.exe)
- [Portable package with Qt](https://github.com/MalfrenLasVegas/ConsoleLAN/releases/download/v1.1.0/ConsoleLAN-1.1.0-win64-portable.zip)
- [Full source and illustrated documentation](https://github.com/MalfrenLasVegas/ConsoleLAN/releases/download/v1.1.0/ConsoleLAN-1.1.0-source.zip)
- [SHA256 checksums](https://github.com/MalfrenLasVegas/ConsoleLAN/releases/download/v1.1.0/SHA256SUMS.txt)

Read the [1.1.0 release notes](https://github.com/MalfrenLasVegas/ConsoleLAN/releases/tag/v1.1.0). [Npcap](https://npcap.com/#download) must be installed separately. The portable package requires an up-to-date Microsoft Visual C++ Runtime x64. Qt Base sources and license notices are included in the binary packages.

## Interface

The following are screenshots of the real 1.1 UI from isolated UI tests with an **example VPN adapter label**, not an actual connection or proof of console gameplay.

![ConsoleLAN 1.1, Dark theme and English](docs/images/consolelan-1.1-dark-en.png)

![ConsoleLAN 1.1, Light theme and Spanish](docs/images/consolelan-1.1-light-es.png)

Use **Theme** and **Language** in the top right. Changes apply immediately and persist for the next launch. See the [English manual](docs/MANUAL.en.md).

## Getting started

1. Install Npcap on each Windows PC and configure a VPN with UDP connectivity between them.
2. Connect the PC and consoles by cable to the site's switch. Select the VPN and physical Ethernet separately.
3. Create a room on the host. Other PCs can discover it, or connect directly to its VPN IPv4 address when broadcast discovery is unavailable.
4. Add consoles and manually configure the IP/mask reserved by the host. MAC learning uses IPv4 source or ARP sender-IP, never an ARP question about a reservation.
5. Use the game's LAN/System Link mode and complete the [hardware acceptance checklist](docs/ACEPTACION.md).

**Actual console gameplay still requires hardware acceptance.** Promiscuous mode alone does not guarantee that a switch delivers relevant unicast traffic to the bridge PC. Console/game/firmware compatibility depends on its LAN mode. There is no Internet access, DNS, gateway, NAT, PSN integration or automatic host migration. Only one bridge PC per physical LAN is supported.

The tunnel validates room/session/endpoint and registered MAC/IP ownership. It transports Ethernet without VLAN, ARP and IPv4 up to 1514 bytes, blocks DHCP and excludes other EtherTypes. Known local unicast stays on the physical switch. Rooms rely on a trusted VPN for access security and encryption; a room ID is not user authentication.

## Build and test

Install Visual Studio C++ x64 Build Tools, CMake, Ninja and Qt with Core, Widgets and Test. The verified packaging configuration uses Qt 6.12.0 msvc2022_64.

```powershell
$env:Qt6_ROOT = 'D:\Qt\6.12.0\msvc2022_64'
.\scripts\fetch-deps.ps1 -Packaging
.\scripts\build.cmd
.\scripts\package.ps1
```

`build.cmd` runs protocol/engine checks, real Winsock loopback tests and Qt UI tests for live language switching, session state preservation, theme rendering and saved preferences. `i18n/en.json` is embedded in the executable; no external translation files or extra libraries are required. Packaging reads the version from CMake and produces versioned files in `dist/`.

Documentation: [English manual](docs/MANUAL.en.md), [Spanish manual](docs/MANUAL.md), [1.1 validation report](docs/INFORME-1.1.md), [protocol](docs/PROTOCOLO.md), [dependencies and licenses](docs/DEPENDENCIAS.md), [conceptual diagrams and clarifications](docs/IMAGENES.md).

ConsoleLAN source is MIT licensed. Dependencies retain their own licenses. Product names and console logos in the conceptual diagrams do not imply affiliation or certified compatibility.
