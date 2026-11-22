+++
title = "Dépannage"
summary = "Les problèmes habituels et leur solution : le dialogue de capture, un curseur bloqué ou visible, l'appli du téléphone, les clics sur un téléphone."
keywords = "faq dépannage problème dialogue capture portail extension gnome curseur bloqué play protect restreint accessibilité clics téléphone mise à jour uinput"
+++

Chaque entrée : ce que vous voyez, pourquoi, quoi faire.

## Le bureau demande d'autoriser la capture des entrées à chaque lancement {#capture-dialog}

Sous GNOME Wayland sans l'extension xmouse, xmouse utilise le portail de capture des entrées, qui demande à chaque lancement (GNOME n'a pas de « se souvenir » pour lui). Installez l'extension et **déconnectez-vous puis reconnectez-vous une fois** : GNOME ne charge une nouvelle extension qu'à la connexion. Une fois l'extension installée, xmouse n'utilise plus du tout le portail (il pouvait laisser le pointeur caché ou figé) : pas de bord jusqu'à la prochaine connexion, le statut le dit ; `XMOUSE_PORTAL=1` force le portail.

```bash "Terminal"
scripts/install.sh --gnome
```

## Le curseur reste visible au bord {#cursor-visible}

Même cause : le portail ne cache pas le curseur pendant qu'il est sur un autre appareil ; l'extension GNOME le fait. Voir [plus haut](page:faq#capture-dialog).

## Le curseur est bloqué sur un autre appareil {#cursor-stuck}

<kbd>Ctrl</kbd> + <kbd>Alt</kbd> + <kbd>Maj</kbd>, ou **Bring my cursor back**, le ramène aussitôt. xmouse le ramène aussi tout seul quand l'appareil où il est s'en va, quand son écran change sous lui, ou quand la capture est perdue (le journal dit laquelle).

Avant la 1.0.13, sous GNOME avec le portail de capture des entrées, le curseur revenait aussi tout seul après quelques secondes sans bouger : chaque rafraîchissement des bords mettait fin à la capture. Depuis la 1.0.13, les bords ne sont reposés que quand ils changent, jamais pendant une capture.

## Les autres ne peuvent ni cliquer ni taper ici (Linux Wayland) {#uinput}

L'injection a besoin d'accéder à `/dev/uinput` : `scripts/install.sh --uinput` ajoute la règle (se déconnecter et se reconnecter), les paquets l'installent.

## Android bloque l'appli (Play Protect) {#android-install}

Une appli installée depuis un navigateur peut être bloquée par Play Protect : touchez **Plus d'informations** > **Installer quand même** quand c'est proposé ; sinon désactivez **Analyser les applis avec Play Protect** dans le Play Store le temps de l'installation, puis réactivez-le. Son service d'Accessibilité peut être grisé (« Paramètre restreint ») : Infos de l'appli > le menu en haut à droite > **Autoriser les paramètres restreints**.

## Les clics ne font rien sur le téléphone {#phone-clicks}

Son écran est en mode **Afficher les curseurs** : Android ne laisse une appli cliquer dans les autres que par un service d'Accessibilité. Dans l'appli : **Cet écran dans la disposition** > **Les laisser toucher** (voir [le téléphone](page:phone#screen)). L'ordinateur le dit aussi, la première fois qu'un curseur y va.

## Les clics marchent un coup sur deux sur le téléphone {#phone-clicks-sometimes}

Android a arrêté le service d'Accessibilité (économie de batterie, appli balayée des récentes) et l'écran a été prêté sans lui un moment. Mettez Infos de l'appli > **Batterie** > **Sans restriction** pour xmouse ; depuis la 1.0.12, l'appli ne prête jamais l'écran sans le service tant qu'il est activé.

## L'écran du téléphone n'est pas sur la carte {#phone-screen}

**Cet écran dans la disposition** doit être activé (la notification le dit), et le téléphone doit atteindre l'ordinateur auquel il est associé (même réseau local, Tailscale). Mettez l'appli à jour : [mises à jour](page:phone#updates).

## Un curseur étranger demande à utiliser le panneau {#panel-access}

Son clic sur le panneau de xmouse est protégé : **Accept**, **Refuse**, ou laissez passer les 60 s ; **Revoke** le reprend. Voir [le panneau protégé](page:cursors#panel).
