# xmouse

![Linux](https://img.shields.io/badge/Linux-ready-2ea44f) ![Windows](https://img.shields.io/badge/Windows-ready-2ea44f) ![Android](https://img.shields.io/badge/Android-ready-2ea44f) ![Browser pad](https://img.shields.io/badge/Browser%20pad-ready-2ea44f) ![macOS](https://img.shields.io/badge/macOS-soon-orange)

> [!TIP]
> Documentation [TsukiNi22/xmouse](https://tsukini22.github.io/xmouse/docs/) (v1.0.6): technical documentation (coming soon) and the interactive project graph.

A network of screens: link your devices once (one time code or QR code, like [xdesk](https://github.com/TsukiNi22/xdesk)), place their screens on a shared layout, and **one cursor, one keyboard and one clipboard** go from one device to the other through the borders of the screens.
Every member sees the cursors of the others on its screens, with their name (like a shared document); a foreign cursor clicks and types where it is, but never touches the panel of xmouse without an access request (60 s, refusable).
Every link is end to end **TLS 1.3 with a post-quantum hybrid key exchange (X25519 + ML-KEM-768)**, a device joins with an **Argon2id** proof of the code bound to the certificate of the member.
The moves go by **encrypted UDP** when it passes (drawn ahead while a packet is late), two members that cannot reach each other go **through a member**, files follow the clipboard or a drag, a **phone** (Android app or any browser) becomes a touchpad and lends its screen, and a **window** can go to another device.

### Table of Contents
 - [Features](#features)
 - [Security](#security)
 - [How it works](#how-it-works)
 - [Dependencies](#dependencies)
 - [Quick Setup - 0 (scripts)](#quick-setup---0-scripts)
 - [Packages](#packages)
 - [Quick Setup - 1 (all)](#quick-setup---1-all)
 - [Quick Setup - 2 (Limited)](#quick-setup---2-limited)
 - [Usage](#usage)
 - [Known limits](#known-limits)
 - [Roadmap](#roadmap)
 - [Windows (cross compilation)](#windows-cross-compilation)
 - [Unit tests](#unit-tests)
 - [Pre-commit hook](#pre-commit-hook)
 - [Workflows/Release](#workflowsrelease)

## Features

| Feature | Content |
| ------- | ------- |
| Network | One group per desk: the first device creates it, any member adds the next one (one time code `K7F-29Q-XM4`, rotated after each use, or the QR code / link `https://host:port/?code=..&fp=..&net=..`, whose identity is checked when joining), then every member knows it; the members link to each other directly (mesh, one link per pair, the link opened by the smaller id wins) |
| Discovery | LAN broadcast (the members find each other without an address), Tailscale addresses, UPnP / `bore` / `ngrok` / manual exposure (from xdesk), every known address of a member tried in turn |
| Layout | Every screen of every member on one canvas: drag a device, its borders snap to the others; a device may have several screens or none (a controller only); the same layout on every member (replicated state, last writer wins) |
| Cursor | Past a border shared with another device, the local pointer is captured and hidden, the cursor goes on: moves, buttons, wheel and keys go to the device it is on, from screen to screen and device to device; an offline device is a wall; `Ctrl + Alt + Shift` brings it back at once |
| Foreign cursors | Two modes per device (`foreignMode`): **pointer** (default) the cursor of a member moves the real pointer of the device, its name shown beside it; **phantom** it is drawn over every app (arrow + name in the color of the member, click-through overlay, Canva-like) and the real pointer stays yours: a click moves it there, clicks, and puts it back (teleported click), a drag follows the cursor that holds the button; the keyboard types where its cursor clicked. A phone / web pad at home follows the same modes: in phantom or private mode it has its own cursor there (its name, on the maps too) |
| Panel protection | Like xdesk: a foreign cursor never clicks / types in the xmouse panel; its first click is held, a second one asks the access, granted after `focusRequestDelay` (60 s by default) unless the person at the device refuses (banner, tray) or uses the panel |
| Live map | The Layout page shows where the cursor and the keyboard of every member are, live (a position the platform does not tell is not drawn); the view stays where you put it (drag the background, wheel zoom, **Recenter** in the corner) |
| Degraded networks | The moves and the presence go by **encrypted UDP** (AES-256-GCM, key derived from the network key) when the probes pass, TCP otherwise; a late / reordered packet is dropped (the newest position only matters) and a foreign cursor is **drawn ahead** along its speed while the next packet is late (80 ms at most); the clicks carry their position (never misplaced by a lost move) |
| Relay | Two members that cannot reach each other (another LAN, a firewall) go **through a member** linked to both (`links` of every device in the state, 3 hops at most, `relay` can be refused per device); the cursor, the clipboard, the files and the windows all pass |
| Profiles | Named layouts of the network (desk, travel...): saved, applied and deleted for every member |
| Presence | **Look here** (a wave where your cursor is, on its screen and every map), **laser** (a red trail on the screens your cursor crosses), **follow** a member (an arrow at the border of your screen points at its cursor, a halo when it is here) |
| Rights | Per member, decided on each device only: mouse, keyboard, clipboard, files, links, windows; **private mode**: the cursors (members and the pads of the device) only pass over it to reach the other devices, nothing is clicked, scrolled or typed there |
| Clipboard | Text, images (PNG) and **files** copied on a member land in the clipboard of the others (files fetched at once up to `clipboardFilesLimit`, then pasted in a file manager like local ones), no echo back |
| Files | Drop files on a device of the map, use **Send files to...**, or drag a file to an edge: a **drop shelf** opens there and sends it to the device on the other side (chunked, back pressure, through the relay too; `Downloads/xmouse`, never overwriting) |
| Links | **Send a link** to a member: opened there (asked by default, or at once, or refused) |
| Phones and web pads | Any browser (`https://device:47900/pad`) or the **Android app** pairs with the code (and the local approval): touchpad (1 finger moves with acceleration, tap clicks, 2 fingers scroll / right click, 3 fingers middle, tap then hold drags), keyboard with sticky modifiers, special keys, combos and F-keys, the live map, look here / laser; the cursor it drives crosses the borders like the real one |
| Android member | The app is **a member like a computer** (and the server of its network): its home shows its network with its **QR code** and its code, creates a network (it is its first device) or joins one (scan the QR code of a member, or its link), disconnects / connects, leaves; computers join it with its QR code or link, approvals on the phone (dialog or notification); its touchpad drives its own cursor across the screens of every device, its screen is lent to the layout; settings: name, private mode of its screen, updates; the member core is plain Java (`android/src/.../core`: Argon2id, the handshake, the replicated state, the mesh), checked against xmouse by `scripts/check_android_core.sh` |
| Android screen | The app lends the screen of the phone to the layout: **show the cursors** (phantom: drawn over its apps with their name, "Display over other apps" only) or **let them touch** (Accessibility service: their clicks / drags / scrolls become touches, their text goes in the focused field, Esc = back); reached through its network (member) or the device it is paired with (touchpad), listed in the members as soon as it is connected; the app checks the xmouse website (`app/version.json`, sha256 checked) and the device for an update at every launch, the newest wins |
| Windows between devices | With the network wide switch on, a window **dragged on an edge** (GNOME, Windows) or chosen on the map goes to the device on the other side: streamed there (JPEG frames, up to `windowFps`), driven from there (mouse, wheel, keys), given back by closing it; the document it shows can be **opened there instead** (handoff); switch off: the edge behaves as usual |
| Interfaces | Qt panel (Network, Layout, Activity, Settings, tray, Auto / Light / Dark) or terminal panel (`--tty`, automatic without display: status, code, members, join / accept / private / place / ping / laser / follow / profile / link / send / pads commands) |
| Platforms | Linux Wayland (GNOME: the xmouse GNOME Shell extension, no dialog; else the portal InputCapture + libei, as on KDE; uinput), Linux X11 (XTest / uinput, pointer grab), Windows (low level hooks, SendInput over the whole virtual desktop), macOS (event tap + CGEvent, Accessibility permission: written, its build waits for libutils on macOS), Android (app), any browser (pad) |

## Security

| Layer | What |
| ----- | ---- |
| Transport | TLS 1.3 only, `X25519MLKEM768` hybrid key exchange first (OpenSSL >= 3.5); the links between members refuse anything not post-quantum, except with a device whose OpenSSL has no ML-KEM (it tells it in the network: `X25519`, still protected by TLS 1.3 and its pinned certificate) |
| Identity | Self-signed ECDSA P-256 certificate per device, its fingerprint replicated in the network: a member whose certificate changed is refused |
| Join | `HMAC(Argon2id(code, salt), transcript)` with `transcript = nonces + sha256(certificate)` on both sides: no offline brute force, no relay by a man in the middle; the code is used once, then rotated; optional local approval (30 s, refused by default) |
| Members | 256 bits group key, the same HMAC proof bound to the TLS session, mutual (a fake member is caught) |
| Removal | The key of the network rotates (epoch + 1) and only goes to the remaining members; the removed device is refused even with the old key |
| Brute force | `maxAuthFailures` wrong codes from one address -> 1 h ban + new code |
| UDP | AES-256-GCM, key `HMAC(network key, "xmouse-udp-v1")` (rotated with it), nonce = random salt of the run + counter, the header authenticated; only from members linked right now, only the moves and the presence (everything else stays on TLS); replayed / late packets dropped |
| Relay | Only members carry messages, only for members of the network (removed ones refused), 3 hops at most; what a relay carries is in the network already (members are trusted with the group key) |
| Pads | Paired with the one-time code + the local approval (30 s, refused by default), then a 256 bits token (only its sha256 kept, `pads.json` `0600`); the Android app pins the certificate of the device (the fp of its QR code), or accepts a real certificate made for the name it asked (the Tailscale certificate of the device on its tailnet name); a pad only drives the device it is paired with |
| Files | Received names stripped of any folder / special name, never overwriting, a partial file removed; size limits per transfer and for the clipboard; `group.json`, `state.json`, `config.json`, `pads.json`, the key: `0600`, folders `0700` |
| Links | `http(s)` only, asked by default (`links`: `ask`, `open`, `never`) and per member (rights) |
| Windows | Off for the whole network with one switch; the viewer drives only the window it was given (clamped to it), per member right `windows`, refused in private mode |

> [!WARNING]
> A member can move, click and type on the devices whose rights allow it, and a pad drives its device: add only your own devices (or people you trust) to a network, and use the private mode or the rights to limit a device.

## How it works

```text
 device A (owner of a cursor)                     device B
 ----------------------------                     --------
 capture: the pointer pushes a border      -->    "enter" (x, y on B)  -> overlay "A" drawn on B
 local pointer held, deltas read                  Pointer (u16 seq, i32 x, i32 y) at 250 Hz at most, the latest only
 layout: global position -> device + local        Button / Wheel / Key / Text -> injected (uinput, XTest, SendInput)
 back on A: release the capture            -->    "leave"              -> overlay removed, keys / buttons released
 every member: Presence (20 Hz) -> live map       State deltas (layout, members) gossiped to the members not linked to the sender
 udp probes answered: Pointer / Presence by udp   AES-256-GCM, newest only; Button carries x, y; a late move is drawn ahead
 no link to C: {"t": "relay", "to": C} via B      B (linked to C, "links" in its state) forwards it, binary too (files, frames)
 phone (pad) --wss /padws--> A                    its moves drive the cursor of A; its screen is a device "via A" in the layout
```

## Dependencies

> [!CAUTION]
> The MIT license of this project ([LICENSE](LICENSE)) only applies to the code of `xmouse` itself, not to its dependencies (Qt 6: LGPLv3, OpenSSL: Apache 2.0, qrcodegen: MIT, libei header: MIT, libei: MIT, loaded at run time).

| Name + Link | Status | Last Update |
| ----------- | ------ | ----------- |
| [libutils](https://github.com/TsukiNi22/libutils) | ![CD - Dispatch](https://github.com/TsukiNi22/libutils/actions/workflows/dispatch.yml/badge.svg) | ![](https://img.shields.io/github/last-commit/TsukiNi22/libutils) |

| Name | Version | Fedora (`dnf`) | Debian/Ubuntu (`apt`) | Arch (`pacman`) |
| ---- | ------- | -------------- | --------------------- | --------------- |
| `clang++` / `cmake` | C++20 / `>= 3.20` | `clang cmake` | `clang cmake` | `clang cmake` |
| `python3` | `>= 3.8` | `python3` | `python3` | `python` |
| `libutils` | `4.x` (`>= 4.1.0`, `>= 4.2.0` on Arch) | `libutils` | `libutils` | `libutils` |
| Qt 6 (Core Gui Widgets Network, DBus on Linux) | `>= 6.4` | `qt6-qtbase-devel` | `qt6-base-dev` | `qt6-base` |
| OpenSSL | `>= 3.2` (`3.5` for ML-KEM) | `openssl-devel` | `libssl-dev` | `openssl` |
| libei (run time, Wayland edges) | `>= 1.0` | `libei` | `libei1` | `libei` |
| XTest **[OPTIONAL]** | any | `libXtst-devel` | `libxtst-dev` | `libxtst` |
| GoogleTest (tests only) | any | `gtest-devel` | `libgtest-dev` | `gtest` |
| MinGW-w64 (Windows only) | GCC `>= 13` | `mingw64-gcc-c++ mingw64-qt6-qtbase mingw64-openssl mingw64-libutils` | `g++-mingw-w64-x86-64` + libutils built for MinGW | — |
| `ccache` **[OPTIONAL]** | any | `ccache` | `ccache` | `ccache` |

> [!TIP]
> libutils packages and source install: see the [libutils README](https://github.com/TsukiNi22/libutils#packages).

## Quick Setup - 0 (scripts)

```bash
# Linux / macOS: dependencies + libutils + build + ~/.local/bin/xmouse + menu entry
git clone https://github.com/TsukiNi22/xmouse.git && cd xmouse
scripts/install.sh --deps --uinput   # --uinput: the other devices can click / type here on Wayland
scripts/install.sh --gnome           # GNOME Wayland: the xmouse extension (no input capture dialog, active after a new login)
scripts/install.sh --autostart       # xmouse at login (tray)
scripts/install.sh --uninstall
```

Windows: the installer of a release (`xmouse-windows-x64-setup.exe`, about 22 MB: Start menu, firewall of the private networks, start with Windows as an option, uninstall from the settings), or the portable zip:

```powershell
# Windows (PowerShell): the zip of a release (xmouse-windows-x64.zip: xmouse.exe and its dll), extracted, then
powershell -ExecutionPolicy Bypass -File install.ps1             # %LOCALAPPDATA%\Programs\xmouse, PATH, Start menu, firewall
powershell -ExecutionPolicy Bypass -File install.ps1 -Autostart
powershell -ExecutionPolicy Bypass -File install.ps1 -Uninstall
```

> [!NOTE]
> Every release carries `SHA256SUMS`: `sha256sum -c SHA256SUMS --ignore-missing` in the folder of the downloaded files.

> [!TIP]
> Every device serves a page on its address (`https://<address>:47900/`, the link of its QR code): what it is, its code, and how to install xmouse on the platform of the visitor (Linux packages, Windows zip, the Android app the device carries), then how to join.

| Script | Effect |
| ------ | ------ |
| `scripts/install.sh` | `--deps` (dnf / apt / pacman / brew + libutils), `--uinput` (udev `uaccess` rule on `/dev/uinput`), `--gnome` (GNOME Shell extension), `--autostart`, `--prefix DIR`, `--uninstall` |
| `scripts/install.ps1` | Windows, from the zip: `-Zip <file>`, `-Prefix`, `-Port`, `-Autostart`, `-NoFirewall`, `-Uninstall` (no zip next to it: the newest release) |
| `scripts/bundle_linux.sh` | `dist/xmouse-linux-x86_64.tar.gz` for the distributions without the package repository (`bin/xmouse` stripped, menu entry, icon, GNOME extension): `tar -xzf xmouse-linux-x86_64.tar.gz -C ~/.local --strip-components=1` (`--no-build`: the binary already built; the CI attaches it to the releases) |
| `scripts/bundle_windows.sh` | Fedora: cross compiles `xmouse.exe` and packs every dll (Qt, OpenSSL, stripped) in `dist/xmouse-windows-x64.zip`, `--installer`: `dist/xmouse-windows-x64-setup.exe` (NSIS `packaging/windows/xmouse.nsi`, `mingw32-nsis`), `--no-build`: the exe already built (the CI attaches both to the releases) |

> [!TIP]
> GNOME Wayland: the extension `xmouse@tsukini22.github.io` puts the barriers on the edges and captures the input itself: no dialog of the input capture portal (GNOME 50 asks it at every start of xmouse otherwise), and the real pointer goes back where it was after a foreign click. Only the `xmouse` program may use it, the events go to it only, Ctrl + Alt + Shift gives the input back even if xmouse does not answer. Without the extension (or disabled), xmouse uses the portal.

## Packages

> [!NOTE]
> Every package has a pre-release/unstable version named `<package_name>-pre`.
> The `-pre` packages are marked as obsolete by any release/stable package (without `-pre`) of the same version or higher.
>
> The same packages exist as RPM (Fedora), DEB (Debian/Ubuntu) and Arch Linux (pacman, `.pkg.tar.zst`): same names, content and dependencies (on Arch the libraries are in `/usr/lib`).

| File Name | Content |
| --------- | ------- |
| `xmouse` | Install the default package (`xmouse-op`) |
| `xmouse-op` | Executable compiled with the optimization options |
| `xmouse-db` | Executable compiled with the debug options (intended for debugging only) |
| `xmouse-as` | Executable compiled with the asan options (intended for debugging only) |

> [!WARNING]
> The three variants install the same `/usr/bin/xmouse`: only one of them can be installed at a time.

**Windows** (MinGW-w64 cross compilation, installed in the MinGW sysroot: `/usr/x86_64-w64-mingw32/sys-root/mingw` on Fedora, `/usr/x86_64-w64-mingw32` on Debian/Ubuntu): the same packages prefixed with `mingw64-` (ex: `mingw64-xmouse`), without the `-as` variants (no AddressSanitizer with MinGW).

> [!NOTE]
> The content of the packages is chosen at the top of the `CMakeLists.txt`: `XMOUSE_KIND` (`binary`) sets the defaults of `XMOUSE_PACKAGE_BIN` (executable), `XMOUSE_PACKAGE_LIB` (library, `XMOUSE_SHARED_LIB` for a `.so`) and `XMOUSE_PACKAGE_DEV` (headers + CMake config), each one can be forced with `-D...=ON/OFF`.
> After a change, update `PACKAGE_BIN` / `PACKAGE_LIB` / `PACKAGE_DEV` in `.github/workflows/build.yml` and `build-packages.yml`.

## Quick Setup - 1 (all)
> Setup into `/usr/local`

### Clone the repository
```bash
git clone https://github.com/TsukiNi22/xmouse.git
cd xmouse
```

### Build & install
```bash
export BUILD_DIR=build
cmake -S . -B $BUILD_DIR -DCMAKE_BUILD_TYPE=Optimized
cmake --build $BUILD_DIR --parallel $(nproc)                       # ./xmouse at the root
sudo cmake --build $BUILD_DIR --target install --parallel $(nproc) # /usr/local/bin/xmouse
```

## Quick Setup - 2 (Limited)
> Setup into `/usr`

> [!WARNING]
> If during the installation using tools like `dnf`, `apt`... you see an `invalid hash` error, reset the cache of the tool or relaunch the `setup.sh` script.
>
> If the error still persists, it might be due to an error in the repository.

> [!WARNING]
> Restriction: `fedora-based (rpm)`, `debian-based (deb)`, `arch-based (pacman)`

> [!NOTE]
> The usage of `sudo` in the script can be removed with the `--no-sudo` argument.

Run the setup script directly, without cloning the repository. It registers the package repository (`gh-pages` branch) so the system package manager can install the packages:
1. Detect the distribution from `/etc/os-release` (`rpm`, `deb` or `arch` based).
2. `rpm`: add `/etc/yum.repos.d/xmouse.repo` and import the GPG key.
3. `deb`: add the GPG key in `/etc/apt/keyrings/xmouse.gpg`, the source in `/etc/apt/sources.list.d/xmouse.list`, then `apt-get update`.
4. `arch`: add the GPG key to the pacman keyring (`pacman-key --add` + `--lsign-key`), the `[xmouse]` repository in `/etc/pacman.conf` (`SigLevel = Required`), then `pacman -Sy`.
5. With `--install` only: install the package `xmouse` (`--pre` for the unstable one, `--windows` for `mingw64-xmouse`, rpm / deb only).

```bash
wget -qO- https://raw.githubusercontent.com/TsukiNi22/xmouse/main/setup.sh | bash -s
```

or with `curl`:

```bash
curl -fsSL https://raw.githubusercontent.com/TsukiNi22/xmouse/main/setup.sh | bash -s
```

| Argument | Effect |
| -------- | ------ |
| `--no-sudo` | Run without `sudo` (the script must already run as root) |
| `--install` | Also install the package (by default only the mirror & GPG key are setup) |
| `--pre` | Select the pre-release/unstable package (`xmouse-pre`) |
| `--windows` | Select the Windows packages (`mingw64-xmouse`, MinGW sysroot, rpm / deb only) |
| `-h`, `--help` | Display the usage |

## Usage

```bash
xmouse                                   # the panel (graphical, or the terminal panel without display)
xmouse --tty                             # terminal panel (the cursors are still drawn when a display exists)
xmouse join 192.168.1.24:47900 K7F-29Q-XM4    # join the network of a member (its address and its code)
xmouse join 'https://192.168.1.24:47900/?code=K7F-29Q-XM4&fp=..'   # the same with the link / QR code (identity checked)
xmouse scan                              # devices announced on the LAN
xmouse paths                             # configuration, data and log folders
xmouse --port 47910 --name desk-pc       # port / name of this device for this run
xmouse --regen-cert                      # new identity (the members must see it join again)
```

```text
terminal panel: ping | laser on|off | follow <member>|off | profile [save|apply|delete <name>]
                link <member> <url> | send <member> <file>... | transfers | pads | padlink | pad accept|refuse|remove <name>
```

1. On the first device: **Network** > **Create a network**.
2. On each other device: **Join a network** with the address and the code shown on a member (or the QR code), accept it on the member.
3. **Layout**: drag the screens where they are on your desk (their borders snap), the cursor now goes through the borders that touch.
4. A phone: in the app, **This phone in a network** makes it a member (create a network, or scan the QR code of a member and accept it there), or **Drive a computer**: **Network** > **Pair a phone or a browser** on the computer, scan its QR code (the page opens as a touchpad, or the app takes it), accept it. **This screen in the layout** shows the cursors of the network over its apps (and lets them touch with the Accessibility service).
5. Right click a device on the map: follow its cursor, send it files or a link, show one of your windows there.

| Setting (`config.json`) | Default | Content |
| ----------------------- | ------- | ------- |
| `name` | host name | Name shown to the others |
| `port` | `47900` | TLS + websocket port (LAN discovery on `47901` udp) |
| `exposure` | `lan` | `lan`, `tailscale`, `upnp`, `bore`, `ngrok`, `manual` (+ `publicHost`) |
| `codeLength` / `codeLifetime` | `9` / `300` | Characters / seconds of the pairing code |
| `joinApproval` | `true` | A device with the right code must also be accepted here |
| `focusRequest` / `focusRequestDelay` | `true` / `60` | A foreign cursor may ask the access to the panel, granted after this delay unless refused |
| `privateMode` | `false` | Every foreign cursor is refused |
| `rights` | all allowed | Per member: `mouse`, `keyboard`, `clipboard`, `files`, `links`, `windows` |
| `udp` / `relay` | `true` / `true` | Moves by encrypted udp when it passes / carry the messages of members that cannot reach each other |
| `clipboardFilesLimit` / `filesLimit` | `64` / `4096` | MiB: copied files fetched at once / biggest transfer accepted |
| `downloadDir` | `Downloads/xmouse` | Received files |
| `links` | `ask` | A link sent by a member: `ask`, `open`, `never` |
| `pads` | `true` | A phone / browser may pair with the code and drive this device |
| `windowFps` | `12` | Frames per second of a window streamed from here |
| `foreignMode` | `pointer` | The cursors of the others here: `pointer` (they move the real pointer) or `phantom` (drawn over the apps) |
| `restorePointer` | `true` | Phantom mode: the real pointer goes back after a foreign click (X11, Windows, GNOME extension) |
| `clipboard` | `true` | The copies of this device go to the members |
| `theme` | `system` | `system`, `dark`, `light` |

| Environment | Effect |
| ----------- | ------ |
| `XMOUSE_HOME` | Another folder for the config and the data (tests, several instances on one PC) |
| `XMOUSE_QPA` | Force the Qt platform (`wayland`, `xcb`): on Wayland, xmouse runs on Xwayland by default so that the overlays stay above every app |
| `XMOUSE_SNAPSHOT` / `XMOUSE_SNAPSHOT_PAGE` / `XMOUSE_SNAPSHOT_QR` | Save the window (page `0`-`3`, or the QR code window) in this PNG after 2 s, then quit |

### Build
```bash
make                                                      # Configure & build into build/
make debug / asan / optimized                             # Build in this mode
make tests                                                # Build (Debug) and run the unit tests
make release                                              # Debug, Asan & Optimized builds
make package                                              # RPM / DEB / Arch packages of every variant
make install                                              # Install the current build (sudo make install)
make re                                                   # Full rebuild
make fclean                                               # Remove the build directory

export BUILD_DIR=build
cmake -S . -B $BUILD_DIR -DCMAKE_BUILD_TYPE=Debug         # Debug build (-g -ggdb3)
cmake -S . -B $BUILD_DIR -DCMAKE_BUILD_TYPE=Asan          # AddressSanitizer build
cmake --build $BUILD_DIR --target release                 # Debug, Asan & Optimized builds
cmake --build $BUILD_DIR --target install_release         # Install the three variants
cmake --build $BUILD_DIR --target package_release         # RPM / DEB / Arch packages of every variant (build/*.rpm, *.deb, *.pkg.tar.zst)
cmake --build $BUILD_DIR --target get_unregistered_files  # .cpp files missing from the SRC / SRC_SHARED lists

./xmouse --help                                  # Generated help (libutils ArgParser)
./xmouse -v debug                                # Verbose level: none|basic|advanced|debug
```

| Option | Default | Content |
| ------ | ------- | ------- |
| `XMOUSE_KIND` | `binary` | Project kind (`binary`, `library`, `header-only`): defaults of the three options below |
| `XMOUSE_PACKAGE_BIN` | `ON` | Build and package the executable (`xmouse-op` / `-db` / `-as`) |
| `XMOUSE_PACKAGE_LIB` | `OFF` | Build and package the library (`libxmouse-op` / `-db` / `-as`) |
| `XMOUSE_PACKAGE_DEV` | `OFF` | Package the public headers and the CMake config (`libxmouse-dev`) |
| `XMOUSE_SHARED_LIB` | `OFF` | Shared library (`.so` / `.dll`) instead of a static one |
| `XMOUSE_STABLE_RELEASE` | `OFF` | Package channel: stable (`ON`) or pre-release `-pre` (`OFF`) |
| `BUILD_TESTS` | `OFF` | Build the unit tests (`unit_tests`, GoogleTest) |

| Path | Content |
| ---- | ------- |
| `src/xmouse/` | Entry: `main.cpp`, `Node-init.cpp` (arguments), `Node.cpp` (commands) |
| `src/xmouse/core/` | `Hub` (the device in the network: `Hub.cpp` state / network / links, `Hub-cursor` my cursor and the pads driving it, `Hub-foreign` the foreign cursors and their prediction, `Hub-route` relay and udp, `Hub-files` clipboard / files / links, `Hub-pads` phones and web pads, `Hub-windows` streamed windows), `Routing`, `Layout` (screens in the global space), `State` (replicated, last writer wins), `Config`, `AuditLog` |
| `src/xmouse/net/` | `Mesh` (links to every member, LAN discovery), `Peer` (one authenticated link), `Datagrams` (encrypted udp), `Server` / `WebSocket` (TLS, `/pad`, `/padws`), `Exposure` (LAN, Tailscale, UPnP, tunnels), `Protocol` |
| `src/xmouse/security/` | `Group` (network key, proofs), `Crypto` (Argon2id, HMAC, TLS), `Certificate`, `PairingCode` |
| `src/xmouse/platform/` | Edge capture (`ShellCapture` GNOME extension, `PortalCapture` Wayland, `HookCapture` Windows, `MacCapture` macOS, `GrabCapture` X11), injection (`UInputInjector`, `XTestInjector`, `SendInputInjector`, `MacInjector`), windows (`ShellWindows`, `X11Windows`, `WinWindows`), `Clipboard`, `Keymap` |
| `src/xmouse/ui/` | `gui/` (`MainWindow`, `LayoutView`, `Overlay`, `WindowViewer`, `DropShelf`, `Theme`), `tty/` (`Tui`) |
| `android/` | Android app (Java, no Gradle): `MainActivity` (home: network, QR code, members, settings; pinned WebView of `/pad`, QR scanner, updates), `NodeService` + `core/` (the phone as a member), `ScreenService` / `OverlayService` / `ScreenLink` (the screen in the layout), `Qr` (QR encoder), `WsClient`, `Pinned` |
| `scripts/check_android_core.sh` | The member core of the app (plain Java) against the xmouse built at the root: the phone joins a network, then creates one that an xmouse joins, its cursor goes there (JDK) |
| `scripts/build_apk.sh` | `android/build/xmouse.apk` (aapt2, javac, d8, apksigner; key `android/release.jks`, embedded in the binary by CMake, built by the CI) |
| `third_party/` | `qrcodegen` (MIT), `libei/libei.h` (MIT, libei is loaded at run time) |
| `gnome-extension/` | GNOME Shell extension `xmouse@tsukini22.github.io` (barriers, capture, pointer position over D-Bus) |
| `include/xmouse/` | Public headers |
| `tests/xmouse/` | Unit tests (one file per module) |
| `cmake/config/exceptions/*.json` | Project exception codes (`utils::exception::ExternalCode`) |
| `cmake/scripts/` | Exception header generator (`generate_exception_header.py`, `const.py`), `check_files.sh` |
| `cmake/toolchain/mingw-w64.cmake` | Windows cross compilation toolchain |
| `cmake/package/` | CMake package config (`find_package(xmouse)`) and the CPack prefix of the Windows packages |
| `docs/` | HTML documentation (technical page, project graph), `logo.svg` |
| `scripts/install.sh` | Build + install in `~/.local`, menu entry, uinput access, GNOME extension, autostart |
| `web/` | Pages served by every device (embedded in the binary): `index.html` (what it is, install, join), `pad.html` / `pad.js` (the touchpad) |
| `include/xmouse/exception/generated_external_exception_header.hpp` | Generated at build time from the JSON files (not committed) |

> [!NOTE]
> Every new `.cpp` must be added to the `SRC` (executable only) or `SRC_SHARED` (executable and library) list of the `CMakeLists.txt` (`get_unregistered_files` lists the missing ones).

## Known limits

> [!WARNING]
> - Wayland without the GNOME extension (or on KDE / wlroots): the desktop asks to allow the input capture (portal dialog) once per run of xmouse, only when an edge opens on a device online (GNOME 50 implements the version 1 of the portal: no restore token; remembered on the portals >= v2); the real pointer cannot be put back after a foreign click (Wayland never tells a client where the pointer is), it stays where the foreign cursor clicked.
> - Wayland: the overlays run on Xwayland (no layer shell on GNOME): a fullscreen Wayland app may cover them.
> - One real pointer and one keyboard focus per OS: two foreign cursors clicking at the same moment on one device take turns; the keyboard of a member types where its cursor clicked last.
> - Windows: the screens must share one scale (the hooks see physical pixels); Ctrl + Alt + Del and the secure desktop end a capture; the MinGW build of Fedora ships OpenSSL 3.2 (no ML-KEM): its links are `X25519` until an OpenSSL >= 3.5 is bundled.
> - Drag and drop to an edge needs the platform to tell the held button (GNOME extension 2, Windows, macOS): elsewhere, drop the files on the map or use Send files to.
> - Windows between devices: dragged on an edge on GNOME (extension 2) and Windows; the picture of a window needs it on screen (not minimized); macOS shares its cursor, keyboard and clipboard but not its windows.
> - Android member: links by TLS only (no udp), it carries no relay, it takes no files / windows / images of the clipboard yet (text only), its computers must reach it directly (same LAN, Tailscale).
> - Android: the foreign cursors touch through the Accessibility service (taps, long presses for the right button, drags, scrolls, text in the focused field, Esc = back); a pad page in a browser drives its device but cannot lend its screen; iOS lets no app do either.
> - macOS: the backends are written (`src/xmouse/platform/macos/`, `Build - macOS (CI)` by hand) but cannot be built yet: libutils does not build on macOS (its epoll parts are Linux only, the Windows port uses WSAPoll); once it does, the Accessibility permission is asked at the first start.

## Roadmap

| Version | Content |
| ------- | ------- |
| `v0.1` ✓ | Desktop: Linux (Wayland + X11) and Windows, network (code / QR, LAN, Tailscale, UPnP), layout, cursor through the borders, named overlays, teleported clicks, keyboard, text / image clipboard, panel protection (60 s request), rights, private mode, terminal panel |
| `v0.2` ✓ | Encrypted UDP for the moves with prediction / reordering on lossy links, relay through a member, layout profiles, ping "look here", follow a member, laser pointer, files in the clipboard, drag and drop of files, send a link |
| `v0.3` ✓ | Android application (touchpad multi finger, keyboard with special keys and combos, overlay of the foreign cursors) and the web pad |
| `v0.4` ✓ | Windows between devices (handoff of a document, streamed window), with the network wide switch; macOS backends written (the build waits for libutils on macOS) |
| `v1.0.0` | Stable: desktop + Android (`v1.0.0-pre` today: every stage in, stable once checked on real phones and Macs) |

## Windows (cross compilation)
> Build for Windows (x86_64) from Linux with the MinGW-w64 toolchain (libutils: `mingw64-libutils` packages in the MinGW sysroot)

```bash
export BUILD_DIR=build-windows
cmake -S . -B $BUILD_DIR -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain/mingw-w64.cmake -DCMAKE_BUILD_TYPE=Optimized
cmake --build $BUILD_DIR --parallel $(nproc)                 # xmouse.exe at the root / build-windows/libxmouse.a
cmake --build $BUILD_DIR --target package_release           # mingw64-xmouse* RPM/DEB (MinGW sysroot)
WINEPATH=/usr/x86_64-w64-mingw32/sys-root/mingw/bin wine ./xmouse.exe --help
```

> [!NOTE]
> No AddressSanitizer with MinGW: the `Asan` mode is built like `Debug`. Portable code only: system calls through `utils::platform::`, `<windows.h>` before `<utils/utils.hpp>`.

## Unit tests

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=ON
cmake --build build --parallel $(nproc)                       # ./unit_tests at the root
(cd build && ctest --output-on-failure)
make tests                                                    # the three commands above
```

## Pre-commit hook

A shared [xstyle](https://github.com/TsukiNi22/ai-utils) hook (`.githooks/pre-commit`) checks the staged lines at each `git commit` and refuses a `major` (or worse) issue. `setup.sh` already enabled it in this clone.

```bash
git config core.hooksPath .githooks   # once per new clone
git commit --no-verify                # skip the hook once (or XSTYLE_HOOK=0 git commit ...)
xstyle --fix --staged                 # fix what can be, then git add -u
```

## Workflows/Release
### Workflows
- `Dispatch (CI/CD)`: runs on every push (branch `main` or tag `v*`, not for `*.md` / `*.txt` / `docs/` only changes) and decides what to trigger from the ref / commit message
- `Lint (CI)`: runs on every push (not with `[ignore]`): `shellcheck` on the shell scripts, `actionlint` on the workflows, `xstyle` on the project (refuses a `major` issue)
- `Unit Tests (CI)`: builds and runs the unit tests, always first (Linux in the `unit-tests` image: Debug then AddressSanitizer, Windows in the `mingw64-unit-tests` image under `wine`)
- `Build (CI)`: only builds the project (Linux in the `ci` image, Windows in the `mingw64-ci` image) and checks the outputs, without producing packages
- `Build - Packages (CI/CD)`: builds the packages (RPM/DEB/Arch: Linux in the `ci` image, Windows `mingw64-*` in the `mingw64-ci` image), signs them and publishes them on the `gh-pages` repository
- `Release - GitHub (CD)`: creates the GitHub release of a tag from its section of the `CHANGELOG.md` (`## [pre-release]` for a `-pre` tag without its own section) with the packages attached, once the packages are published
- `Docs - Pages (CD)`: copies `docs/` on the `gh-pages` branch on every push changing it (https://tsukini22.github.io/xmouse/docs/)
- `Sync Packages (CD)` (on `gh-pages`): rebuilds the metadata and signatures of the package repository after a manual push on the branch

> [!NOTE]
> `Build (CI)` only runs when `Dispatch (CI/CD)` decides **not** to build the packages, to check that the code still compiles.

> [!IMPORTANT]
> The workflows need the repository variable `RUNNER` (`gh variable set RUNNER --body <runner>`), the containers `ghcr.io/tsukini22/ci`, `unit-tests`, `mingw64-ci`, `mingw64-unit-tests`, `package`, `git`, and the secrets `GPG_PRIVATE_KEY` (base64), `GPG_KEY_ID`, `GPG_PASSPHRASE` to sign and publish the packages.

> [!TIP]
> A commit containing `[ignore]` skips the whole CI.

### Pre-Release (unstable)
The pre-release of the packages (`<package_name>-pre`) is triggered by:
- a tag with a suffix, matching `vx.x.x-<suffix>` (ex: `v1.0.1-pre`, `x` = `major`, `minor`, `fix`)
- or a commit containing `[build]`

A tag `vx.x.x-<suffix>` also creates a GitHub pre-release, whose notes are its `## [vx.x.x-<suffix>]` section of the `CHANGELOG.md`, or else the `## [pre-release]` one.

> [!NOTE]
> Only the 5 last versions of each `-pre` package are kept in the package repository, every release is kept.

### Release (stable)
The release of the packages (channel `stable`) and the stable GitHub release are only triggered by a tag matching exactly `vx.x.x` (ex: `v1.0.0`), whose section must exist in the `CHANGELOG.md`.

> [!IMPORTANT]
> The version of a tag (`vx.x.x`, suffix ignored) must be the one of `project(... VERSION x.x.x)` in the `CMakeLists.txt`: the packages are refused otherwise.

### Downstream notification
`Dispatch (CI/CD)` has a commented `notify-downstream` job (`repository_dispatch` to another repository, like libutils -> docker-image), triggered after a package publication or by a commit containing `[release]`.
