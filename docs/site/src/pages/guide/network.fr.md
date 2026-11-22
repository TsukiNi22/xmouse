+++
title = "Créer et rejoindre un réseau"
summary = "Le premier appareil crée le réseau, les autres le rejoignent avec un code à usage unique ou un QR code."
keywords = "réseau créer rejoindre code qr lien approbation accepter membres retirer quitter tailscale exposition scan"
+++

Un réseau est le groupe de vos appareils : chaque membre atteint les autres et partage une même disposition.

## Créer le réseau {#create}

Sur le premier appareil, **Network** > **Create a network**. Sa page montre alors un **code à usage unique** (`K7F-29Q-XM4`), son lien et un **QR code**. Un téléphone peut aussi être le premier appareil : [l'appli](page:phone#member) crée un réseau depuis son accueil.

## Le rejoindre {#join}

Sur chaque autre appareil, **Network** : tapez l'adresse d'un membre (`192.168.1.24:47900`) et son code, ou collez son lien, puis **Join**. Sur un téléphone, scannez le QR code d'un membre. Depuis un terminal :

```bash "Terminal"
xmouse join 192.168.1.24:47900 K7F-29Q-XM4
xmouse scan                                   # les appareils annoncés sur le réseau local
```

Puis **acceptez** le nouvel appareil sur le membre (un bandeau, une notification) : refusé par défaut au bout de 30 s.

> [!NOTE]
> Un code sert une fois, puis il est remplacé ; il change aussi toutes les 5 minutes (**New code** à tout moment). Cinq mauvais codes depuis une adresse la bannissent une heure.

## Membres {#members}

Le tableau **Members** de la page Network liste chaque appareil, en ligne ou non, avec sa latence. Chacun a ses **droits** sur cet appareil (souris, clavier, presse-papiers, fichiers, liens, fenêtres) et **Remove** : l'appareil retiré quitte le réseau et sa clé change pour les autres.

## Atteindre un appareil ailleurs {#exposure}

| Réglage | Portée |
| ------- | ------ |
| LAN only | Les appareils du même réseau local |
| LAN + Tailscale | Vos appareils connectés à votre tailnet, n'importe où |
| LAN + internet | UPnP, un tunnel `bore` / `ngrok`, ou un port redirigé à la main |

Deux membres qui ne s'atteignent pas fonctionnent quand même : leurs messages passent [par un membre](page:mesh#relay).

Ensuite : [placer les écrans](page:layout#place).
