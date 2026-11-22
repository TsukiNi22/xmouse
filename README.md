# xmouse

![Linux](https://img.shields.io/badge/Linux-ready-2ea44f) ![Windows](https://img.shields.io/badge/Windows-ready-2ea44f) ![Android](https://img.shields.io/badge/Android-ready-2ea44f) ![Browser pad](https://img.shields.io/badge/Browser%20pad-ready-2ea44f) ![macOS](https://img.shields.io/badge/macOS-soon-orange)

> [!TIP]
> Documentation [TsukiNi22/xmouse](https://tsukini22.github.io/xmouse/docs/) (v1.0.8): technical documentation (coming soon) and the interactive project graph.

A network of screens: link your devices once (code or QR code), place their screens on a shared layout, and **one cursor, one keyboard and one clipboard** go from one device to the other through the borders of the screens.
Every link is **TLS 1.3 with a post-quantum hybrid key exchange (X25519 + ML-KEM-768)**; a phone (Android app or any browser) is a touchpad, a member and a screen of the layout.

### Table of Contents
 - [Features](#features)
 - [Security](#security)
 - [Dependencies](#dependencies)
 - [Quick Setup - 0 (scripts)](#quick-setup---0-scripts)
 - [Packages](#packages)
 - [Quick Setup - 1 (all)](#quick-setup---1-all)
 - [Quick Setup - 2 (Limited)](#quick-setup---2-limited)
 - [Usage](#usage)
 - [Known limits](#known-limits)
 - [Unit tests](#unit-tests)
 - [Workflows/Release](#workflowsrelease)

## Features

| Feature | Content |
| ------- | ------- |
| Network | The first device creates it, any member adds the next one (one-time code or QR code); every member links to the others, LAN discovery, Tailscale / UPnP / tunnels |
| Layout & cursor | Every screen of every device on one shared canvas; past a border the cursor goes on (moves, clicks, wheel, keys), `Ctrl + Alt + Shift` brings it back |
| Foreign cursors | `pointer` mode (they move the real pointer) or `phantom` (drawn over the apps with their name, a click goes there and comes back); a phone touchpad follows the same modes with its own cursor |
| Panel protection | A foreign cursor asks before using the xmouse panel: Accept / Refuse, granted after 60 s otherwise, Revoke at any time |
| Rights & private mode | Per member: mouse, keyboard, clipboard, files, links, windows; **private mode**: the cursors only pass over this device |
| Degraded networks | Moves by **encrypted UDP** when it passes (drawn ahead while a packet is late), **relay** through a member when two devices cannot reach each other |
| Clipboard & files | Text, images and files follow the copies; files dropped on the map or dragged to an edge; links sent to a member |
| Presence | Live map of every cursor, **look here**, **laser**, **follow** a member, layout profiles |
| Phones | Android app: a member with its home (network, QR code, settings), a touchpad, its screen in the layout (Accessibility to click and type there, the clipboard of the computer to paste); any browser: the touchpad (`https://device:47900/pad`) |
| Windows | A window dragged on an edge goes to the device on the other side (streamed and driven there), or its document opens there |
| Interfaces | Qt panel (tray, light / dark) or terminal panel (`--tty`) |

## Security

| Layer | What |
| ----- | ---- |
| Transport | TLS 1.3 only, `X25519MLKEM768` first (OpenSSL >= 3.5); a member without ML-KEM says it (`X25519`, still pinned) |
| Identity & join | Self-signed ECDSA P-256 certificate per device, pinned in the network; join proof `HMAC(Argon2id(code, salt), transcript)` bound to the certificates (no offline brute force, no man in the middle), the code used once; optional local approval |
| Members | 256 bits group key, mutual proof on every link; a removal rotates the key (the removed device is refused) |
| UDP | AES-256-GCM (key derived from the group key), only the moves and the presence, replayed / late packets dropped |
| Pads | One-time code + local approval, then a 256 bits token (only its sha256 kept); the app pins the certificate of its device |
| Files & data | Received names sanitized, never overwriting, size limits; config, state, keys `0600`, folders `0700` |

> [!WARNING]
> A member can move, click and type on the devices whose rights allow it: add only your own devices (or people you trust), use the private mode or the rights to limit a device.

## Dependencies

> [!CAUTION]
> The MIT license of this project ([LICENSE](LICENSE)) only applies to the code of `xmouse` itself, not to its dependencies (Qt 6: LGPLv3, OpenSSL: Apache 2.0, qrcodegen: MIT, libei: MIT, loaded at run time).

| Name + Link | Status | Last Update |
| ----------- | ------ | ----------- |
| [libutils](https://github.com/TsukiNi22/libutils) | ![CD - Dispatch](https://github.com/TsukiNi22/libutils/actions/workflows/dispatch.yml/badge.svg) | ![](https://img.shields.io/github/last-commit/TsukiNi22/libutils) |

| Name | Version | Fedora (`dnf`) | Debian/Ubuntu (`apt`) | Arch (`pacman`) |
| ---- | ------- | -------------- | --------------------- | --------------- |
| `clang++` / `cmake` | C++20 / `>= 3.20` | `clang cmake` | `clang cmake` | `clang cmake` |
| `libutils` | `>= 4.1.0` (`>= 4.2.0` on Arch) | `libutils` | `libutils` | `libutils` |
| Qt 6 (Core Gui Widgets Network, DBus on Linux) | `>= 6.4` | `qt6-qtbase-devel` | `qt6-base-dev` | `qt6-base` |
| OpenSSL | `>= 3.2` (`3.5` for ML-KEM) | `openssl-devel` | `libssl-dev` | `openssl` |
| libei (run time, Wayland edges) | `>= 1.0` | `libei` | `libei1` | `libei` |
| XTest **[OPTIONAL]** | any | `libXtst-devel` | `libxtst-dev` | `libxtst` |
| GoogleTest (tests only) | any | `gtest-devel` | `libgtest-dev` | `gtest` |

## Quick Setup - 0 (scripts)

```bash
git clone https://github.com/TsukiNi22/xmouse.git && cd xmouse
scripts/install.sh --deps --uinput   # dependencies + libutils + build + ~/.local/bin/xmouse (--uinput: the others can click / type here on Wayland)
scripts/install.sh --gnome           # GNOME Wayland: the xmouse extension (no input capture dialog, active after a new login)
scripts/install.sh --autostart       # xmouse at login (tray)
scripts/install.sh --uninstall
```

Windows: the installer of a release (`xmouse-windows-x64-setup.exe`), or the portable zip with `install.ps1` (`-Autostart`, `-Uninstall`).
Android: the app is on the page of every device (`https://<address>:47900/`) and on the [website](https://tsukini22.github.io/xmouse/app/xmouse.apk); it updates itself.

> [!NOTE]
> Every release carries `SHA256SUMS`: `sha256sum -c SHA256SUMS --ignore-missing` in the folder of the downloaded files.

## Packages

> [!NOTE]
> Every package has a pre-release/unstable version named `<package_name>-pre`, obsoleted by any stable package of the same version or higher; the same packages exist as RPM, DEB and Arch (`.pkg.tar.zst`), and as `mingw64-*` for Windows (rpm / deb).

| File Name | Content |
| --------- | ------- |
| `xmouse` | Install the default package (`xmouse-op`) |
| `xmouse-op` | Executable compiled with the optimization options |
| `xmouse-db` | Executable compiled with the debug options (intended for debugging only) |
| `xmouse-as` | Executable compiled with the asan options (intended for debugging only) |

> [!WARNING]
> The three variants install the same `/usr/bin/xmouse`: only one of them can be installed at a time.

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
> Restriction: `fedora-based (rpm)`, `debian-based (deb)`, `arch-based (pacman)`. An `invalid hash` error: reset the cache of the package manager or run `setup.sh` again.

The script registers the package repository (`gh-pages` branch) and its GPG key for `dnf`, `apt` or `pacman` (`sudo` removed with `--no-sudo`):

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

> [!TIP]
> xmouse is also in [my-universe](https://tsukini22.github.io/my-universe/) (the packages of all my projects in one set of repositories):
> ```bash
> curl -fsSL https://tsukini22.github.io/my-universe/install.sh | bash -s -- --only xmouse
> sudo dnf install xmouse-pre    # or: sudo apt install xmouse-pre / sudo pacman -S xmouse-pre
> ```

## Usage

```bash
xmouse                                   # the panel (graphical, or the terminal panel without display)
xmouse --tty                             # terminal panel
xmouse join 192.168.1.24:47900 K7F-29Q-XM4                       # join the network of a member (its address and its code)
xmouse join 'https://192.168.1.24:47900/?code=K7F-29Q-XM4&fp=..' # the same with the link / QR code
xmouse scan                              # devices announced on the LAN
xmouse paths                             # configuration, data and log folders
```

1. On the first device: **Network** > **Create a network**.
2. On each other device: **Join a network** with the address and the code of a member (or its QR code), then accept it on the member.
3. **Layout**: drag the screens where they are on your desk, the cursor goes through the borders that touch.
4. A phone: the app opens on its home: create a network or scan the QR code of a member to join it, or **Touchpad of a computer** (on the computer: **Network** > **Pair a phone or a browser**). **This screen in the layout** shows the cursors over its apps, **Let them touch** (Accessibility) lets them click and type there.

| Setting (`config.json`) | Default | Content |
| ----------------------- | ------- | ------- |
| `name` / `port` | host name / `47900` | Name shown to the others / TLS port (LAN discovery on `47901` udp) |
| `exposure` | `lan` | `lan`, `tailscale`, `upnp`, `bore`, `ngrok`, `manual` |
| `joinApproval` | `true` | A device with the right code must also be accepted here |
| `foreignMode` | `pointer` | The cursors of the others here: `pointer` or `phantom` |
| `privateMode` | `false` | The cursors only pass over this device |
| `focusRequestDelay` | `60` | Seconds before an access to the panel is granted (unless refused) |
| `rights` | all allowed | Per member: `mouse`, `keyboard`, `clipboard`, `files`, `links`, `windows` |
| `udp` / `relay` | `true` / `true` | Moves by encrypted udp / carry the messages of the members that cannot reach each other |
| `downloadDir` | `Downloads/xmouse` | Received files |

> [!NOTE]
> `XMOUSE_HOME` uses another folder for the config and the data (several instances on one PC).

## Known limits

> [!WARNING]
> - Wayland without the GNOME extension: the desktop asks to allow the input capture at every start, the cursor stays visible at the edge, and the real pointer is not put back after a foreign click.
> - Android: the cursors only touch through the Accessibility service ("Let them touch"); an app installed from the browser needs **Allow restricted settings** first. iOS lets no app do it.
> - Windows: the screens must share one scale; the Fedora MinGW build has no ML-KEM (`X25519` links).
> - macOS: written, built once libutils builds on macOS.

## Unit tests

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=ON
cmake --build build --parallel $(nproc)                       # ./unit_tests at the root
(cd build && ctest --output-on-failure)
make tests                                                    # the three commands above
```

> [!NOTE]
> Windows build from Linux: `cmake -S . -B build-windows -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain/mingw-w64.cmake` (MinGW-w64, `mingw64-libutils`). The `.githooks/pre-commit` hook runs [xstyle](https://github.com/TsukiNi22/xstyle) on the staged lines (`git config core.hooksPath .githooks`).

## Workflows/Release
### Workflows
- `Dispatch (CI/CD)`: on every push (`main` or a `v*` tag, not for Markdown / docs only changes), decides what to run
- `Lint (CI)`: `shellcheck`, `actionlint` and `xstyle` (a `major` issue fails)
- `Unit Tests (CI)`: Linux (Debug, then AddressSanitizer) and Windows (`wine`)
- `Build - Packages (CI/CD)`: the APK, the RPM / DEB / Arch / `mingw64-*` packages, the Windows installer and zip, signed and published on `gh-pages` (with `app/` for the updates of the app)
- `Release - GitHub (CD)`: the GitHub release of a tag from its `CHANGELOG.md` section, with every file and `SHA256SUMS`
- `Build - macOS (CI)`: green with a "macOS: soon" warning while libutils does not build on macOS
- `Docs - Pages (CD)`: `docs/` on `gh-pages` (https://tsukini22.github.io/xmouse/docs/)

> [!IMPORTANT]
> The workflows need the variable `RUNNER`, the secrets `GPG_PRIVATE_KEY`, `GPG_KEY_ID`, `GPG_PASSPHRASE` (packages) and `XMOUSE_ANDROID_KEYSTORE`, `XMOUSE_ANDROID_KEYSTORE_PASS` (the signing key of the app). A commit containing `[ignore]` skips the CI.

### Pre-Release (unstable)
A tag `vx.x.x-<suffix>` (ex: `v1.0.8-pre`) or a commit containing `[build]` publishes the `-pre` packages; the tag also creates a GitHub pre-release from its `## [vx.x.x-<suffix>]` section (or `## [pre-release]`).

### Release (stable)
A tag matching exactly `vx.x.x` publishes the stable packages and release; its version must be the one of `project(... VERSION x.x.x)` in the `CMakeLists.txt` and its section must exist in the `CHANGELOG.md`.
