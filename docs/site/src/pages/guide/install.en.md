+++
title = "Install"
summary = "Install xmouse on Linux, Windows and Android, or build it from the sources."
section = "guide"
order = 1
keywords = "install setup package dnf apt pacman windows installer zip android apk gnome extension uinput"
+++

Every device of the network runs xmouse: computers run the program, phones run the Android app (or a browser page).

## Linux {#linux}

The packages (`rpm`, `deb`, Arch) come from the package repository of xmouse, or from [my-universe](https://tsukini22.github.io/my-universe/) with the other projects:

```bash "Terminal"
curl -fsSL https://raw.githubusercontent.com/TsukiNi22/xmouse/main/setup.sh | bash -s -- --install --pre
```

| Package | Content |
| ------- | ------- |
| `xmouse` | The default package (`xmouse-op`, optimized) |
| `xmouse-db` / `xmouse-as` | Debug / AddressSanitizer builds (for debugging only) |

> [!NOTE]
> Before the first stable version, only the pre-release packages exist: `xmouse-pre` (`--pre`).

From the sources, `scripts/install.sh` installs the dependencies, builds and installs in `~/.local`:

```bash "Terminal"
git clone https://github.com/TsukiNi22/xmouse.git && cd xmouse
scripts/install.sh --deps --uinput   # --uinput: the other devices can click and type here on Wayland
scripts/install.sh --gnome           # GNOME Wayland: the xmouse extension (log out and back in once)
scripts/install.sh --autostart       # start xmouse at login (tray)
```

> [!IMPORTANT]
> On GNOME Wayland, install the **xmouse GNOME extension** (`--gnome`) then log out and back in once: without it the desktop asks to allow the input capture at every start and the cursor stays visible at the edge of the screen.

## Windows {#windows}

Download `xmouse-windows-x64-setup.exe` from the [releases](https://github.com/TsukiNi22/xmouse/releases): Start menu, firewall rule for the private networks, start with Windows as an option, uninstall from the settings. The zip (`xmouse-windows-x64.zip`) is the portable version, installed with its `install.ps1` (`-Autostart`, `-Uninstall`).

## Android {#android}

The app is on the page of every device (`https://<address>:47900/`, the link of its QR code) and on the website: [xmouse.apk](https://tsukini22.github.io/xmouse/app/xmouse.apk). It updates itself from the website and from the devices of its network.

> [!WARNING]
> Installed from a browser, Android may block it ("Play Protect") or grey out its Accessibility service: see [the troubleshooting](page:faq#android-install).

## Check a download {#check}

Every release carries `SHA256SUMS`:

```bash "Terminal"
sha256sum -c SHA256SUMS --ignore-missing
```

Next: [create the network](page:network#create).
