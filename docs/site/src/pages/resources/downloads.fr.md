+++
title = "Téléchargements"
summary = "Chaque fichier d'une version, les dépôts de paquets et l'appli."
keywords = "téléchargement version fichiers paquets rpm deb arch windows installeur zip archive apk sha256sums my-universe"
+++

Chaque version est sur la [page des versions](https://github.com/TsukiNi22/xmouse/releases) ; les pré-versions finissent par `-pre`.

## Les fichiers d'une version {#files}

| Fichier | Pour |
| ------- | ---- |
| `xmouse-windows-x64-setup.exe` | Installeur Windows |
| `xmouse-windows-x64.zip` | Windows, portable (avec `install.ps1`) |
| `xmouse-linux-x86_64.tar.gz` | Linux sans le dépôt de paquets (`tar -xzf ... -C ~/.local --strip-components=1`) |
| `*.rpm`, `*.deb`, `*.pkg.tar.zst` | Les paquets (préférez le dépôt de paquets : mises à jour) |
| `xmouse.apk` | L'appli Android |
| `SHA256SUMS` | Les sommes de contrôle de chaque fichier |

## Dépôts de paquets {#repositories}

| Dépôt | L'enregistrer |
| ----- | ------------- |
| xmouse | `curl -fsSL https://raw.githubusercontent.com/TsukiNi22/xmouse/main/setup.sh \| bash -s` |
| my-universe (tous les projets) | `curl -fsSL https://tsukini22.github.io/my-universe/install.sh \| bash -s -- --only xmouse` |

## L'appli {#app}

[xmouse.apk](https://tsukini22.github.io/xmouse/app/xmouse.apk) est la dernière appli (sa version et son sha256 dans [version.json](https://tsukini22.github.io/xmouse/app/version.json)) ; chaque appareil sert aussi celle avec laquelle il a été compilé.
