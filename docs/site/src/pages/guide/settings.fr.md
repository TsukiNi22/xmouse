+++
title = "Réglages"
summary = "Les réglages d'un appareil : dans la page Settings, ou dans son config.json."
keywords = "réglages configuration config.json port exposition approbation mode curseurs privé droits udp relais téléchargement thème dossiers xmouse_home"
+++

Chaque réglage est dans la page **Settings** ; ils sont gardés dans `config.json` (`xmouse paths` affiche son dossier).

## Les réglages principaux {#main}

| Réglage (`config.json`) | Défaut | Contenu |
| ----------------------- | ------ | ------- |
| `name` | nom de la machine | Nom montré aux autres |
| `port` | `47900` | Port TLS et UDP (découverte locale sur `47901` udp) |
| `exposure` | `lan` | `lan`, `tailscale`, `upnp`, `bore`, `ngrok`, `manual` |
| `joinApproval` | `true` | Un appareil avec le bon code doit aussi être accepté ici |
| `foreignMode` | `pointer` | Les curseurs des autres : `pointer` ou `phantom` |
| `privateMode` | `false` | Les curseurs ne font que passer sur cet appareil |
| `focusRequestDelay` | `60` | Secondes avant qu'un accès au panneau soit donné (sauf refus) |
| `rights` | tout permis | Par membre : `mouse`, `keyboard`, `clipboard`, `files`, `links`, `windows` |
| `udp` / `relay` | `true` / `true` | Mouvements en UDP chiffré / transmettre les messages des membres qui ne s'atteignent pas |
| `clipboardFilesLimit` / `filesLimit` | `64` / `4096` | Mio : fichiers copiés récupérés tout de suite / plus gros transfert accepté |
| `downloadDir` | `Downloads/xmouse` | Fichiers reçus |
| `links` | `ask` | Un lien envoyé par un membre : `ask`, `open`, `never` |
| `pads` | `true` | Un téléphone ou un navigateur peut s'associer avec le code |
| `theme` | `system` | `system`, `dark`, `light` |

## Ligne de commande {#cli}

```bash "Terminal"
xmouse                         # le panneau (ou le panneau terminal sans affichage)
xmouse --tty                   # le panneau terminal
xmouse --port 47910 --name pc  # port / nom pour ce lancement
xmouse paths                   # dossiers de configuration, de données et des journaux
```

> [!TIP]
> `XMOUSE_HOME=<dossier>` lance xmouse avec un autre dossier de configuration et de données (plusieurs instances sur un ordinateur).
