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

## [v1.0.10-pre] - 2026-11-22
### Added
- **The keys of a computer on a phone screen**: Enter runs the action of the field (send, search, next; a new line in a multi-line field), the arrows / Home / End move in the text (with Shift: select) or move the focus outside a field (Android 13+), Delete, Tab (next field), Page Up / Down (a page), Alt+Tab (recent apps), Print Screen (screenshot), play / pause; the back button of the mouse goes back, the forward one no longer taps
- **Still cursors fade out**: a foreign cursor that does not move for 3 s fades (on the computers and the phones), back at once when it moves; its clicks alone do not bring it back
- Touchpad: Back / Forward buttons of the mouse

### Fixed
- Touchpad: Shift + a letter typed the qwerty key (a Q on an azerty computer); a shortcut (Ctrl, Alt, Super + a letter, the Copy / Paste / All / Undo buttons) is now the key of that letter on the layout of the device (azerty Ctrl+A is no longer Ctrl+Q); a special key is pressed and released at once (a soft keyboard may never send its release)

## [v1.0.9-pre] - 2026-11-22
### Fixed
- GNOME extension: **never a capture while a screen of GNOME holds the input** (the screenshot tool stayed grey and stuck when an edge was pushed meanwhile, the overview, a dialog); the pointer is always shown again when a capture ends, even when GNOME took the grab back first
- Portal capture: a capture kept by the compositor is first released (its last activation, the pointer shown again where it was), disabled and armed again only when the input still comes (disabling it during a capture could leave the pointer hidden)
### Added
- **Documentation site** (`docs/site`, built into `docs/`, EN / FR, search): user guide (install, network, layout, phone, cursors and privacy, sharing, settings), concepts explained at two levels with animations (how the cursor crosses the screens, the mesh / relay / UDP, the security), troubleshooting, downloads and known limits

## [v1.0.8-pre] - 2026-11-22
### Added
- **Typing on a phone screen**: the keys of the computer arrive as the characters of its layout (azerty, Shift, AltGr read from the keyboard layout of the session, US otherwise) in the focused field; Ctrl+V / C / X / A paste, copy, cut and select all there (the letter of the layout); the special keys stay keys
- **The clipboard to the phone screens**: its text is sent to a phone in touchpad mode when it changes and when a cursor enters its screen (to paste there)
- `XkbLayout::character` (the character of a key on the layout, every key and level) with its unit tests

### Changed
- **A granted access to the panel stays until Revoke**: the banner stays with a Revoke button; the clicks of the local person in the panel no longer end a request nor a granted access

### Fixed
- **Accept did nothing**: pressing the button was taken for the local person using the panel, which ended the request before the button acted


## [v1.0.7-pre] - 2026-11-22
### Added
- A phone screen that only shows the cursors (no Accessibility service) is told: its mode in the log of the computer, a notice the first time a cursor goes there (its clicks, scroll and keys do nothing there, and how to turn the touch on), a toast on the phone at the first click
- Android: **Let them touch** guided: the steps to turn on the Accessibility service "xmouse screen", with the "Allow restricted settings" step of an app installed from the browser (buttons to the Accessibility settings and to the App info page)


## [v1.0.6-pre] - 2026-11-22
### Added
- GNOME: when the xmouse extension is installed but not loaded yet (GNOME loads a new extension only at the next login), the status and a notice say so: log out and back in once, the input capture dialog never comes back (the portal of Fedora is at version 1: no restore token, a dialog at every start)

### Fixed
- **A dead mouse and keyboard with the portal capture**: the compositor could keep the capture after a release (lost, or refused for an old activation) and every input was ignored; input coming while nothing is captured now disables the capture and arms it again (the pointer given back at once)
- The cursor comes back when the capture holds nothing any more although it is away (ended without a word, every backend)


## [v1.0.5-pre] - 2026-11-22
### Added
- Panel access request: an **Accept** button (granted at once, no waiting for the delay)

### Changed
- **The own cursor of a pad travels under its own name** (phantom / private mode): through a border it goes to the other device as the cursor of the phone (its moves, clicks, wheel, keys, text relayed with its identity), the cursor of this device stays with the person; it comes back as its own cursor, ends when its pad or its device goes; the Home button of the pad brings it back
- An access to the panel stays given to its cursor when it comes back (a pad that reconnects, a cursor that went away) until the local person uses the panel

### Fixed
- The pad of a phone in phantom mode sent the cursor of this device to the phone screen (the mouse of the computer then shown there and stuck)
- An access to the panel was asked again at the first click: the injected click of the cursor was taken for the local person using the panel (150 ms window; the map did not check at all): 600 ms or a button of a foreign cursor held


## [v1.0.4-pre] - 2026-11-22
### Changed
- **No lag of the drawn cursors**: the overlay repaints only around the cursors (a whole screen of pixels per frame lagged under Xwayland), one frame per refresh of the screen at most (at once when the last one is old enough), raised above the windows when shown then every 2 s (it was at every move, up to 250 times a second)
- A phone (pad or member) receives the moves of a cursor 60 times a second (it was 250: its screen draws at 60 Hz), the lent screen of the app draws once per frame
- A click of my cursor on a phone that only shows it is told on the desktop (Accessibility service needed to touch); the app says the mode of its lent screen (cursors shown / touch / off)

### Fixed
- **The cursor stuck away** (shown on the phone while the mouse is on the computer, or the mouse held): it comes back when the real mouse moves while it left with a pad, when the capture of the pointer is lost without a word (twice in a row), when the screen of the phone it is on goes; it stays on a device whose screens change under it (a phone turned), comes back if that device leaves the layout; the real mouse pushing an edge while the cursor is away with a pad takes it over


## [v1.0.3-pre] - 2026-11-22
### Added
- Android: a crash is written down and shown at the next launch with its trace (Copy), the lent screen and the network of the phone turned off so that it cannot come back at every launch

### Fixed
- Android: **the app stopped at every launch** once its lent screen reached its computer (1.0.2): the size of the screen was written on the network from the main thread (Android ends the app then); the websocket of the screen writes and closes on its own thread
- Android: the home was drawn again every second (a link could not be typed): only when the network or its members change; the notification of the network only when its text changes


## [v1.0.2-pre] - 2026-11-22
### Added
- Android: **a home like the panel of a computer**, the app opens on it: the network of the phone with its **QR code** and its code (time left, new code, share the link), create / join (scan a QR code or paste a link) / **disconnect - connect** / leave, the members (remove), the touchpad and the screen lent, the touchpad of a paired computer, the **settings** (name, private mode of the screen, updates at every launch, check now); every phone is a member and a server of its network by default
- Android: **updates from the xmouse website** (`app/version.json` + `app/xmouse.apk` on gh-pages, synced by the CI with the packages, the sha256 checked) and from the device, the newest one: a phone alone needs no computer to update
- Android: a QR code encoder (port of the qrcodegen of Project Nayuki, MIT)

### Changed
- **The pad of a phone / browser in phantom or private mode** has its own cursor on its device (drawn over the apps with its name, on the maps too) instead of moving the real pointer: private mode, it passes over and acts on nothing; phantom mode, a click moves the real pointer there only for the click (pointer mode unchanged)

### Fixed
- Android: the screen lent and the updates failed on a device reached by its Tailscale name (the device gives its Tailscale certificate there, the app wanted only the pinned one): a real certificate made for that name is accepted too, as the touchpad page does
- Android: the screen lent while the phone is in no network goes to the paired computer (not to the node of the phone)

## [v1.0.1-pre] - 2026-11-22
### Added
- **The Android app as a member** (This phone in a network): it creates its own network or joins one with the code / QR code, computers join it with its link, approvals by dialog or notification, its touchpad drives its own cursor across every device, its screen lent to the layout, text of the clipboard and links of the members; the member core in plain Java (`android/src/.../core`: JSON, Argon2id, the link handshake, the replicated state, the layout, the mesh, the node), its identity in the Android KeyStore, a foreground service; `scripts/check_android_core.sh` checks it against xmouse
- Foreign cursors: **pointer** mode (default: the cursor of a member moves the real pointer of the device) and **phantom** mode (drawn over the apps, the real pointer stays yours), `foreignMode` in the settings
- Android: the screen lent **without Accessibility** (phantom: the cursors drawn over the apps, "Display over other apps", a foreground service), the Accessibility service taking over to touch; the update checked at every launch / return to the app
- Map: **Recenter** button in the corner, the background dragged to move the view, wheel zoom

### Changed
- macOS shown as **soon** (orange): the macOS build runs on every push, green with a warning while libutils does not build on macOS; the notes of a release, the device page and the README say it
- **Private mode**: the cursors (members and the pads of the device) pass over a private device to reach the others, nothing is clicked / scrolled / typed there (they were sent back)
- The phones and web pads are in the members as soon as they connect (online / offline), not only once a screen is lent
- The map never recenters by itself; my cursor is followed live at home (10 Hz), an unknown position is not drawn

### Fixed
- The cursors of the map (and of the pads, the waves) follow a device moved on the layout (kept on their device, no stale dot)
- The buttons of the tables were cut (rows too low)
- Two cursors of one member could fight with the crossing of an edge in pointer mode (the injected moves never cross)


## [v1.0.0-pre] - 2026-11-22
### Added
- **Encrypted UDP** for the moves and the presence (`net::Datagrams`): AES-256-GCM with a key derived from the network key (rotated with it), nonce = salt of the run + counter, probes every second, TCP when it does not pass (`udp` setting)
- **Prediction and reordering** on lossy links: a late or replayed move is dropped (sequence numbers), a foreign cursor is drawn ahead along its speed while its next packet is late (80 ms at most), a click carries its position
- **Relay through a member**: two members that cannot reach each other go through one linked to both (`links` and `relay` in the device entries, 3 hops at most), json and binary messages (cursor, clipboard, files, windows)
- **Layout profiles** of the network (`profile/<name>`): save, apply, delete (Layout page, `profile` command)
- **Look here** (waves where the cursor is, on its screen and on the maps), **laser pointer** (a fading trail), **follow a member** (arrow at the border of the screen, halo, ring on the map)
- **Files**: copied files follow the clipboard (fetched at once up to `clipboardFilesLimit`, then pasted like local ones), files dropped on a device of the map, **Send files to...** (tray, map, `send` command), a **drop shelf** at an edge while a file is dragged there; chunked transfers with back pressure, `Downloads/xmouse`, never overwriting, limits; the Files card of the Activity page
- **Send a link** to a member (asked there by default: `links` = `ask`, `open`, `never`), rights `links` and `windows`
- **Web pad** `https://device:47900/pad`: pairing with the code + the local approval, then a token; touchpad (acceleration, tap, two fingers scroll / right click, three fingers middle, tap then hold drag), keyboard with sticky modifiers, special keys, combos, F-keys, live map, look here, laser, send a link; the Phones and web pads card of the Network page (QR code, forget)
- **Android app** (`android/`, `scripts/build_apk.sh`, no Gradle): the pad in a pinned WebView, QR scanner, updates from the device, and its **screen in the layout** (Accessibility service: the foreign cursors drawn over the apps, their clicks / drags / scrolls as touches, their text in the focused field); built by the CI, embedded in every binary, attached to the releases
- **macOS** backends: edges with an event tap (cursor dissociated and hidden), injection with CGEvent (double clicks, unicode text), key map, the Accessibility permission asked once; not built yet: libutils does not build on macOS (`Build - macOS (CI)`, by hand, says it)
- **Windows between devices** with the network wide switch: a window dragged on an edge (GNOME extension 2, Windows) or chosen on the map is streamed to the other device (JPEG frames up to `windowFps`), driven from there, given back by closing it; its document can be opened there instead (handoff); backends `WinWindows`, `ShellWindows`, `X11Windows`
- GNOME extension 2: window drags given to xmouse (or the usual border when the switch is off), held buttons, windows listed / pictured / focused
- Linux tarball `xmouse-linux-x86_64.tar.gz` (`scripts/bundle_linux.sh`: the binary stripped, menu entry, icon, GNOME extension, unpacked into `~/.local`) for the distributions without the package repository, built by the CI
- `SHA256SUMS` of every file of a release, the link on the page of a device
- `xmouse.exe` carries its icon and its version (Windows resource `packaging/windows/xmouse.rc.in`)
- Windows installer `xmouse-windows-x64-setup.exe` (NSIS, LZMA: 22 MB): Start menu, firewall of the private networks, start with Windows as an option, Add or remove programs, uninstaller; built by the CI and attached to the releases
- Page of a device: a button per file of the newest release (pre-releases included, through the GitHub API): installer and zip for Windows, `.rpm` / `.deb` / `.pkg.tar.zst` for Linux, the `.apk`; a missing file is shown disabled

### Changed
- The Windows binaries are stripped (the zip: 48 MB -> 33 MB)
- The commands of the page show in full (scrolled, no more cut)
- The hub is split by feature (`Hub.cpp`, `Hub-cursor`, `Hub-foreign`, `Hub-route`, `Hub-files`, `Hub-pads`, `Hub-windows`)
- No crossing while a button is held at an edge (a drag): the drop shelf shows there instead
- The members table tells relayed / udp links, the pads of a device; the macOS card of the device page is available (Homebrew)

### Fixed
- GNOME extension backend: the motion, button and wheel signals were never connected (`SLOT(this->...)` strings)
- `scripts/bundle_linux.sh` refuses a Debug / Asan binary (the tests build the same `./xmouse`)

## [v0.1.1-pre] - 2026-11-22
### Changed
- **[MAJOR]** The links of a device are `https://host:port/?code=..&fp=..&net=..` (QR code, copied link, addresses shown), no more `xmouse://` (still accepted when joining); joining with a link checks that the device answering has the identity of the link
- The paths of `Who can reach this device` only show once the device is in a network (like xdesk with the sharing off)

### Added
- Page of every device on its address (`web/`, embedded): the device and its network (`/api/info`), the code of the link, how to install xmouse on the platform of the visitor first (Linux packages, Windows zip, the Android app the device carries, macOS planned, iOS not available, the sources), then how to join
- Windows zip `xmouse-windows-x64.zip` (`scripts/bundle_windows.sh`: xmouse.exe, Qt, OpenSSL, `install.ps1`), built by the CI and attached to the releases; `scripts/install.ps1` (PATH, Start menu, firewall, autostart, uninstall; takes the newest release, pre-releases included)
- GNOME Shell extension `xmouse@tsukini22.github.io` and its backend `ShellCapture`: barriers and input capture on GNOME Wayland without the portal dialog, the pointer position (the real pointer goes back after a foreign click on Wayland too); only `xmouse` may call it (sender checked), unicast signals, released when xmouse stops, Ctrl + Alt + Shift and a watchdog give the input back; the portal stays the fallback
- `scripts/install.sh --gnome`, the extension installed by the packages in `share/gnome-shell/extensions`
- Network page in the way of the overview of xdesk: connection code card (countdown bar, identity, `Copy` / `QR code` / `New code`), status card (edges, input, encryption, members), `Who can reach this device` with `Open to` (LAN only, Tailscale, UPnP, bore, ngrok, manual: applied at once) and a row per address (copy, QR code), then `Connect to a network`; the QR code in its own window with the choice of the address
- The addresses of a member through a tunnel (`host:port`) are published and dialed
- Network switch at the bottom of the sidebar (like the sharing switch of xdesk): red `Leave the network` (the others stay linked, there is no server), green `Create or join`

### Fixed
- Tailscale: the page on the tailnet name was refused by the browsers (`*.ts.net` is HSTS preloaded, the self-signed certificate has no exception): the Tailscale certificate is served to the browsers asking the name, the links use the address until it is there, the members (no SNI) keep the pinned certificate
- The rows of `Who can reach this device` stayed highlighted (hover style of xdesk on rows that are not clickable)
- The `setup.sh` options `--pre` and `--windows` never changed the package installed
- Wayland: the input capture was never armed after the dialog (`session_handle` is an object path)
- Wayland: the input capture dialog only shows once an edge opens on a device online (not at every start); remembered with a restore token on the portals >= v2 (`CreateSession2` + `Start`, `persist_mode` 2), GNOME 50 implements the version 1 only
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

[v0.1.1-pre]: https://github.com/TsukiNi22/xmouse/compare/v0.1.0-pre...v0.1.1-pre
[v0.1.0-pre]: https://github.com/TsukiNi22/xmouse/releases/tag/v0.1.0-pre
