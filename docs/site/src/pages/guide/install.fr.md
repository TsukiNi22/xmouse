+++
title = "Installer"
summary = "Installer xmouse sous Linux, Windows et Android, ou le compiler depuis les sources."
keywords = "installer installation paquet dnf apt pacman windows installeur zip android apk extension gnome uinput"
+++

Chaque appareil du réseau lance xmouse : les ordinateurs lancent le programme, les téléphones l'appli Android (ou une page du navigateur).

## Linux {#linux}

Les paquets (`rpm`, `deb`, Arch) viennent du dépôt de paquets de xmouse, ou de [my-universe](https://tsukini22.github.io/my-universe/) avec les autres projets :

```bash "Terminal"
curl -fsSL https://raw.githubusercontent.com/TsukiNi22/xmouse/main/setup.sh | bash -s -- --install --pre
```

| Paquet | Contenu |
| ------ | ------- |
| `xmouse` | Le paquet par défaut (`xmouse-op`, optimisé) |
| `xmouse-db` / `xmouse-as` | Versions debug / AddressSanitizer (pour déboguer seulement) |

> [!NOTE]
> Avant la première version stable, seuls les paquets de pré-version existent : `xmouse-pre` (`--pre`).

Depuis les sources, `scripts/install.sh` installe les dépendances, compile et installe dans `~/.local` :

```bash "Terminal"
git clone https://github.com/TsukiNi22/xmouse.git && cd xmouse
scripts/install.sh --deps --uinput   # --uinput : les autres appareils peuvent cliquer et taper ici sous Wayland
scripts/install.sh --gnome           # GNOME Wayland : l'extension xmouse (se déconnecter et se reconnecter une fois)
scripts/install.sh --autostart       # lancer xmouse à la connexion (zone de notification)
```

> [!IMPORTANT]
> Sous GNOME Wayland, installez l'**extension GNOME de xmouse** (`--gnome`) puis déconnectez-vous et reconnectez-vous une fois : sans elle le bureau demande d'autoriser la capture des entrées à chaque lancement et le curseur reste visible au bord de l'écran.

## Windows {#windows}

Téléchargez `xmouse-windows-x64-setup.exe` dans les [versions](https://github.com/TsukiNi22/xmouse/releases) : menu Démarrer, règle du pare-feu pour les réseaux privés, lancement avec Windows en option, désinstallation depuis les paramètres. Le zip (`xmouse-windows-x64.zip`) est la version portable, installée avec son `install.ps1` (`-Autostart`, `-Uninstall`).

## Android {#android}

L'appli est sur la page de chaque appareil (`https://<adresse>:47900/`, le lien de son QR code) et sur le site : [xmouse.apk](https://tsukini22.github.io/xmouse/app/xmouse.apk). Elle se met à jour toute seule depuis le site et depuis les appareils de son réseau.

> [!WARNING]
> Installée depuis un navigateur, Android peut la bloquer (« Play Protect ») ou griser son service d'Accessibilité : voir [le dépannage](page:faq#android-install).

## Vérifier un téléchargement {#check}

Chaque version contient `SHA256SUMS` :

```bash "Terminal"
sha256sum -c SHA256SUMS --ignore-missing
```

Ensuite : [créer le réseau](page:network#create).
