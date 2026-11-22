+++
title = "Limites connues"
summary = "Ce que xmouse ne sait pas encore faire, ou pas sur une plateforme."
keywords = "limites wayland portail android accessibilité ios windows échelle macos plein écran"
+++

Ce qu'il faut savoir avant de compter sur xmouse.

## Linux {#linux}

- Wayland sans l'extension GNOME (ou sous KDE, wlroots) : un dialogue de capture à chaque lancement, le curseur visible au bord, le vrai pointeur pas remis en place après un clic étranger.
- Les calques (les curseurs dessinés) tournent sous Xwayland : une appli Wayland en plein écran peut les couvrir.

## Toutes les plateformes {#all}

- Un vrai pointeur et un focus clavier par appareil : deux curseurs étrangers qui cliquent au même moment passent chacun leur tour.
- Le glisser-déposer vers un bord a besoin que la plateforme donne le bouton tenu (extension GNOME, Windows).
- Entre un Mac et un PC, <kbd>Ctrl</kbd> et <kbd>Cmd</kbd> échangent leur place pour toutes les touches : <kbd>Ctrl</kbd>+<kbd>Tab</kbd> sur le PC devient <kbd>Cmd</kbd>+<kbd>Tab</kbd> sur le Mac.

## Windows {#windows}

- Les écrans doivent avoir la même échelle.
- La version Windows n'a pas encore ML-KEM : ses liens utilisent `X25519` (toujours TLS 1.3, certificats épinglés).

## Téléphones {#phones}

- Android : les curseurs n'y cliquent que par le service d'Accessibilité ; un téléphone membre n'a ni UDP ni relais, et ne prend que le texte du presse-papiers.
- Android ne donne les vrais événements clavier qu'à un champ de texte actif (Android 13+) : ailleurs un raccourci d'appli (<kbd>Ctrl</kbd>+<kbd>T</kbd> dans un navigateur...) n'a pas d'équivalent ; recharger est un tirer vers le bas au curseur, qui marche en haut d'une liste.
- iOS : aucune appli ne peut piloter le curseur des autres appareils.

## macOS {#macos}

> [!SOON]
> La partie macOS est écrite ; elle sera compilée dès que sa bibliothèque (libutils) compilera sous macOS.
