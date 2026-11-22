# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

| Scope | Content |
| ----- | ------- |
| `In Progress` | Work that hasn't been completed yet (typically listed under [pre-release]) |
| `Added`       | New features, files, or sections introduced in this release |
| `Removed`     | Existing features, files, or sections that have been deleted |
| `Changed`     | Existing features or sections that were modified (behavior, structure, or content) |
| `Fixed`       | Bugs or issues that have been resolved |

> [!NOTE]
> The `(unofficial)` version are sub-versions (typically fixes) that don't have associated tag.
> A package built at this version is not guaranteed!

---

## [pre-release]
### Fixed
- The `setup.sh` options `--pre` and `--windows` never changed the package installed
- Wayland: the input capture was never armed after the dialog (`session_handle` is an object path); the permission is now asked once (`CreateSession2` + `Start` with `persist_mode` 2 and a restore token in `<data>/portal-input-capture.token`)
- Flood of `QPixmap::fill` warnings on every refresh of the members
- Windows: the links failed with the OpenSSL 3.2 of the MinGW build (no ML-KEM): `X25519` fallback, the devices tell `pq` in the network and are dialed with the compatible groups
- Unit tests: a lone test crashed at exit (static `QCoreApplication`), the unix rights checked on Windows

## [v0.1.0-pre] - 2026-11-22
### Added
- **[MAJOR]** First version of `xmouse`: a network of screens where one cursor, one keyboard and one clipboard go from one device to the other (set up from [cpp_project_template](https://github.com/TsukiNi22/cpp_project_template), the network, security and input modules taken from [xdesk](https://github.com/TsukiNi22/xdesk))
- Network of devices (`Group`): created by the first device, a new one joins with the one time code (or the QR code `xmouse://host:port?fp=..&code=..`) of any member, `HMAC(Argon2id(code), transcript)` proofs on both sides, optional local approval (30 s, refused by default)
- Links between members (`Mesh`, `Peer`): TLS 1.3 with the `X25519MLKEM768` hybrid key exchange, 256 bits group key proof bound to the TLS session, certificates pinned in the network, one link per pair, pings and dead link detection, LAN discovery (`47901` udp), every known address dialed (LAN, Tailscale, public), 1 h ban after `maxAuthFailures` wrong codes
- Member removal with key rotation (epoch, previous key accepted once for a member that missed the rotation)
- Replicated state (`State`): last writer wins per key with Lamport clocks, gossiped to the members not linked to the sender, saved in `state.json`
- Layout of the screens (`Layout`): every screen of every member in one global space, moves sliding along the borders, the openings between devices, snapping, overlap refused; editor with the live cursors (`LayoutView`)
- Cursor through the borders: Wayland edges with the portal `InputCapture` + libei (loaded at run time, vendored header), Windows low level hooks, X11 / macOS pointer grab; moves at 250 Hz at most (latest position only, back pressure), buttons, wheel, keys, `Ctrl + Alt + Shift` to come back
- Foreign cursors: overlays above every app with the name and the color of the member, teleported clicks (the real pointer goes back on X11 / Windows), drags, keyboard where the cursor clicked, injection by uinput / XTest / SendInput (whole virtual desktop)
- Panel protection like xdesk: the clicks / keys of a foreign cursor on the panel are held, a second click asks the access, granted after `focusRequestDelay` (60 s) unless refused or the panel is used locally
- Rights per member (mouse, keyboard, clipboard, files) and the private mode
- Shared clipboard: text and PNG images, no echo back
- Graphical panel (Network with code / QR / members / join, Layout with the live map, Activity, Settings, tray) and terminal panel (`--tty`: status, members, layout, create, join, accept, place, private, remove, leave)
- Commands `xmouse`, `xmouse join <address> [code]`, `xmouse scan`, `xmouse paths`, flags `--tty`, `--gui`, `--port`, `--name`, `--regen-cert`
- `scripts/install.sh` (`--deps`, `--uinput`, `--autostart`, `--uninstall`), the xmouse logo
- Unit tests: replicated state, layout, group, the command line and real loopback links (join with the right / a wrong code, local refusal, member key, foreign key, removed member, messages)

[v0.1.0-pre]: https://github.com/TsukiNi22/xmouse/releases/tag/v0.1.0-pre
