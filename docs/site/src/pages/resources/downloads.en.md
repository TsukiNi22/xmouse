+++
title = "Downloads"
summary = "Every file of a release, the package repositories and the app."
section = "resources"
order = 2
keywords = "download release files packages rpm deb arch windows installer zip tarball apk sha256sums my-universe"
+++

Every version is on the [releases page](https://github.com/TsukiNi22/xmouse/releases); the pre-releases end with `-pre`.

## The files of a release {#files}

| File | For |
| ---- | --- |
| `xmouse-windows-x64-setup.exe` | Windows installer |
| `xmouse-windows-x64.zip` | Windows, portable (with `install.ps1`) |
| `xmouse-linux-x86_64.tar.gz` | Linux without the package repository (`tar -xzf ... -C ~/.local --strip-components=1`) |
| `*.rpm`, `*.deb`, `*.pkg.tar.zst` | The packages (prefer the package repository: updates) |
| `xmouse.apk` | The Android app |
| `SHA256SUMS` | The checksums of every file |

## Package repositories {#repositories}

| Repository | Register it |
| ---------- | ----------- |
| xmouse | `curl -fsSL https://raw.githubusercontent.com/TsukiNi22/xmouse/main/setup.sh \| bash -s` |
| my-universe (every project) | `curl -fsSL https://tsukini22.github.io/my-universe/install.sh \| bash -s -- --only xmouse` |

## The app {#app}

[xmouse.apk](https://tsukini22.github.io/xmouse/app/xmouse.apk) is the latest app (its version and sha256 in [version.json](https://tsukini22.github.io/xmouse/app/version.json)); every device also serves the one it was built with.
