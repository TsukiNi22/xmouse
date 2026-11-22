+++
title = "Le téléphone"
summary = "L'appli Android : un membre du réseau avec son accueil, un touchpad, et son écran dans la disposition."
keywords = "téléphone android appli touchpad écran accessibilité laisser toucher afficher curseurs qr membre pad navigateur presse-papiers saisie mise à jour"
+++

L'appli s'ouvre sur son **accueil**, comme le panneau d'un ordinateur : son réseau, ses membres, son touchpad, son écran et ses réglages.

## L'accueil {#home}

| Carte | Contenu |
| ----- | ------- |
| Réseau | Créer un réseau ou en rejoindre un (scanner un QR code, coller un lien) ; dans un réseau : son QR code et son code, **Partager le lien**, **Nouveau code**, **Se déconnecter** / **Se connecter**, **Quitter le réseau** |
| Membres | Chaque appareil du réseau, en ligne ou non, **Retirer** |
| Ce téléphone | **Ouvrir le touchpad**, **Cet écran dans la disposition** (son mode) |
| Touchpad d'un ordinateur | Le touchpad d'un ordinateur associé, sans le téléphone dans son réseau |
| Réglages | Nom, mode privé de l'écran, mises à jour à chaque lancement, **Chercher les mises à jour maintenant** |

## Le téléphone comme membre {#member}

Comme un ordinateur, le téléphone crée un réseau (il en est alors le premier appareil : les ordinateurs le rejoignent avec son QR code) ou en rejoint un avec le QR code d'un membre, puis y est accepté. Son touchpad pilote son propre curseur sur les écrans de tous les appareils.

## Le touchpad {#touchpad}

| Geste | Effet |
| ----- | ----- |
| Un doigt | Déplace le curseur (plus vite quand le doigt va vite) |
| Toucher | Clic gauche |
| Deux doigts | Défilement ; toucher : clic droit |
| Trois doigts | Clic du milieu |
| Toucher, puis tenir | Glisser |

Le clavier du touchpad a les modificateurs maintenus, les touches spéciales et les touches F. N'importe quel navigateur peut aussi être un touchpad : sur l'ordinateur, **Network** > **Pair a phone or a browser**, puis ouvrez son QR code (`https://<adresse>:47900/pad`).

## L'écran dans la disposition {#screen}

**Cet écran dans la disposition** met l'écran du téléphone sur la carte : les curseurs du réseau viennent dessus.

| Mode | Ce que les curseurs y font |
| ---- | -------------------------- |
| Afficher les curseurs | Dessinés par-dessus les applis avec leur nom (« Afficher par-dessus les autres applis ») ; leurs clics ne font rien |
| Les laisser toucher | Leurs clics, glisser et défilements deviennent des touchers, leurs touches tapent dans le champ actif (service d'Accessibilité « xmouse screen ») |

> [!IMPORTANT]
> Android ne laisse une appli cliquer dans les autres **que par un service d'Accessibilité**. **Les laisser toucher** ouvre les étapes : Accessibilité > Applications installées > xmouse screen > activé. Grisé (« Paramètre restreint ») ? Infos de l'appli > le menu en haut à droite > **Autoriser les paramètres restreints**, puis à nouveau.

Avec **Les laisser toucher**, les touches tapées sur un ordinateur arrivent comme les caractères de **sa** disposition de clavier (azerty compris), <kbd>Ctrl</kbd>+<kbd>V</kbd> / <kbd>C</kbd> / <kbd>X</kbd> / <kbd>A</kbd> collent, copient, coupent et sélectionnent tout, et le presse-papiers de l'ordinateur est envoyé au téléphone quand il change.

## Mises à jour {#updates}

À chaque lancement, l'appli demande au site de xmouse (`app/version.json`, son sha256 vérifié) et aux appareils de son réseau s'il existe une version plus récente, et propose la plus récente.
