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
| Deux doigts qui s'écartent / se rapprochent | Zoom avant / arrière là où est le curseur (<kbd>Ctrl</kbd> + molette sur un ordinateur, un pincement sur un écran de téléphone) |
| Trois doigts | Clic du milieu |
| Trois doigts vers la droite / la gauche | Précédent / suivant (les boutons de côté d'une souris) |
| Toucher, puis tenir | Glisser |

Le clavier du touchpad a des modificateurs maintenus qui s'additionnent (<kbd>Ctrl</kbd>, <kbd>Alt</kbd>, <kbd>Maj</kbd>, <kbd>Super</kbd>, <kbd>AltGr</kbd> : <kbd>Ctrl</kbd> puis <kbd>Maj</kbd> puis une touche donne <kbd>Ctrl</kbd>+<kbd>Maj</kbd>+cette touche), les touches spéciales (<kbd>Échap</kbd>, <kbd>Tab</kbd>, <kbd>Suppr</kbd>, <kbd>Inser</kbd>, les flèches, <kbd>Début</kbd> / <kbd>Fin</kbd>, <kbd>Page préc.</kbd> / <kbd>Page suiv.</kbd>, <kbd>Super</kbd>, <kbd>Menu</kbd>, <kbd>Impr. écran</kbd>), <kbd>F1</kbd> à <kbd>F24</kbd>, les touches média et volume, et les raccourcis courants (copier, couper, coller, annuler, rétablir, tout, recharger, rechercher, enregistrer, nouvel onglet, fermer l'onglet, <kbd>Alt</kbd>+<kbd>Tab</kbd>, <kbd>Alt</kbd>+<kbd>F4</kbd>). Un bouton de touche tenu se répète comme une touche de clavier. Un raccourci garde sa lettre sur toutes les dispositions : <kbd>Ctrl</kbd> + <kbd>A</kbd> tapé sur un ordinateur AZERTY reste <kbd>Ctrl</kbd> + <kbd>A</kbd> sur un QWERTY ou sur un téléphone, jamais <kbd>Ctrl</kbd> + <kbd>Q</kbd>. Tant qu'un curseur d'ordinateur est sur un écran de téléphone, celui-ci reste allumé (5 minutes après son dernier mouvement). N'importe quel navigateur peut aussi être un touchpad : sur l'ordinateur, **Network** > **Pair a phone or a browser**, puis ouvrez son QR code (`https://<adresse>:47900/pad`).

## L'écran dans la disposition {#screen}

**Cet écran dans la disposition** met l'écran du téléphone sur la carte : les curseurs du réseau viennent dessus.

| Mode | Ce que les curseurs y font |
| ---- | -------------------------- |
| Afficher les curseurs | Dessinés par-dessus les applis avec leur nom (« Afficher par-dessus les autres applis ») ; leurs clics ne font rien |
| Les laisser toucher | Leurs clics, glisser et défilements deviennent des touchers, leurs touches tapent dans le champ actif (service d'Accessibilité « xmouse screen ») |

> [!IMPORTANT]
> Android ne laisse une appli cliquer dans les autres **que par un service d'Accessibilité**. **Les laisser toucher** ouvre les étapes : Accessibilité > Applications installées > xmouse screen > activé. Grisé (« Paramètre restreint ») ? Infos de l'appli > le menu en haut à droite > **Autoriser les paramètres restreints**, puis à nouveau. Ensuite Infos de l'appli > **Batterie** > **Sans restriction** : sinon Android arrête le service de temps en temps, et les clics ne font rien jusqu'à son retour.

Avec **Les laisser toucher**, un curseur d'ordinateur **est un doigt** sur le téléphone : le bouton gauche le pose et le lève, un glisser suit le curseur pendant qu'il bouge (jamais rejoué à la fin), un appui long dure le temps où le bouton est tenu ; la molette fait glisser le contenu pendant qu'elle tourne, en accélérant puis en ralentissant (84 dp par cran, il s'arrête avec la molette), le bouton droit est un appui long (le menu d'Android). Un curseur ne clique jamais sur la page du pavé de l'appli : ce peut être le curseur que ce pavé pilote, chaque tap y cliquerait encore. Le presse-papiers de l'ordinateur est envoyé au téléphone quand il change.

## Les touches sur l'écran du téléphone {#keys}

Dans un champ de texte actif (Android 13+), les touches arrivent comme les **vrais événements clavier** d'un clavier : chaque touche et chaque raccourci de l'appli y marchent, comme avec un clavier Bluetooth (<kbd>Ctrl</kbd>+flèches par mot, <kbd>Ctrl</kbd>+<kbd>Maj</kbd>+flèches pour sélectionner, <kbd>Ctrl</kbd>+<kbd>Retour arrière</kbd>, <kbd>Ctrl</kbd>+<kbd>Z</kbd> / <kbd>Y</kbd>, <kbd>Début</kbd> / <kbd>Fin</kbd>, <kbd>Ctrl</kbd>+<kbd>R</kbd> ou <kbd>F5</kbd> dans un navigateur...). Les caractères sont ceux de la disposition de l'ordinateur (AZERTY compris). Ailleurs, et sous Android 12 ou plus ancien, les touches deviennent les actions du téléphone :

| Touches de l'ordinateur | Sur le téléphone |
| ----------------------- | ---------------- |
| Flèches (<kbd>Ctrl</kbd> : par mot, <kbd>Maj</kbd> : sélection), <kbd>Début</kbd> / <kbd>Fin</kbd> (<kbd>Ctrl</kbd> : tout le texte) | Se déplacer dans le texte ; hors d'un champ les flèches déplacent le focus (Android 13+), <kbd>Début</kbd> / <kbd>Fin</kbd> vont en haut / en bas de la liste sous le curseur |
| <kbd>Retour arrière</kbd> / <kbd>Suppr</kbd> (<kbd>Ctrl</kbd> : un mot), <kbd>Tab</kbd> (<kbd>Maj</kbd> : en arrière) | Effacer, le champ suivant |
| <kbd>Entrée</kbd> | Comme le clavier du téléphone : l'action que sa touche <kbd>Entrée</kbd> montre pour le champ (envoyer, rechercher, aller, suivant), sinon un retour à la ligne (une note, une discussion sans touche d'envoi) |
| <kbd>Maj</kbd>+<kbd>Entrée</kbd>, <kbd>Ctrl</kbd>+<kbd>Entrée</kbd> | Un retour à la ligne, jamais l'envoi (un champ d'une seule ligne n'en a pas : son action) |
| <kbd>Ctrl</kbd>+<kbd>A</kbd> / <kbd>C</kbd> / <kbd>X</kbd> / <kbd>V</kbd>, <kbd>Ctrl</kbd>/<kbd>Maj</kbd>+<kbd>Inser</kbd>, <kbd>Maj</kbd>+<kbd>Suppr</kbd> | Tout sélectionner, copier, couper, coller |
| <kbd>Ctrl</kbd>+<kbd>Z</kbd>, <kbd>Ctrl</kbd>+<kbd>Y</kbd> ou <kbd>Ctrl</kbd>+<kbd>Maj</kbd>+<kbd>Z</kbd> | Annuler, rétablir (ce que les ordinateurs y ont tapé) |
| <kbd>Page préc.</kbd> / <kbd>Page suiv.</kbd> | La liste sous le curseur d'une page |
| <kbd>F5</kbd>, <kbd>Ctrl</kbd>+<kbd>R</kbd>, la touche d'actualisation | Recharger : un tirer vers le bas au curseur (les applis qui se rechargent en tirant, en haut de leur contenu) |
| <kbd>Ctrl</kbd>+<kbd>+</kbd> / <kbd>-</kbd>, <kbd>Ctrl</kbd>+molette | Zoom là où est le curseur (un pincement) |
| <kbd>Échap</kbd>, <kbd>Alt</kbd>+<kbd>←</kbd>, <kbd>Alt</kbd>+<kbd>F4</kbd>, le bouton précédent de la souris | Retour |
| <kbd>Super</kbd> seule, <kbd>Ctrl</kbd>+<kbd>Échap</kbd> | Toutes les applis (Android 12+) |
| <kbd>Super</kbd>+<kbd>D</kbd> / <kbd>H</kbd> / <kbd>Entrée</kbd>, <kbd>Alt</kbd>+<kbd>Début</kbd> | Accueil |
| <kbd>Alt</kbd>+<kbd>Tab</kbd>, <kbd>Super</kbd>+<kbd>Tab</kbd>, <kbd>Ctrl</kbd>+<kbd>Maj</kbd>+<kbd>Échap</kbd> | Applis récentes |
| <kbd>Super</kbd>+<kbd>N</kbd> / <kbd>A</kbd> / <kbd>L</kbd> / <kbd>X</kbd> / <kbd>I</kbd> | Notifications, réglages rapides, verrouillage, menu marche/arrêt, paramètres |
| <kbd>Impr. écran</kbd>, <kbd>Super</kbd>+<kbd>Maj</kbd>+<kbd>S</kbd> | Capture d'écran |
| <kbd>Super</kbd>+<kbd>B</kbd> / <kbd>C</kbd> / <kbd>E</kbd> / <kbd>K</kbd> / <kbd>M</kbd> / <kbd>P</kbd> / <kbd>S</kbd> / <kbd>U</kbd> | Navigateur, contacts, fichiers, agenda, cartes, musique, messages, calculatrice |
| <kbd>Menu</kbd>, <kbd>Maj</kbd>+<kbd>F10</kbd>, le bouton droit | Appui long (le menu d'Android) |
| Touches média, touches de volume | Le lecteur du téléphone, son volume |

Une touche tenue se répète (<kbd>Retour arrière</kbd>, les flèches, une lettre). <kbd>Verr. Maj</kbd> et <kbd>Verr. Num</kbd> suivent le clavier de l'ordinateur : le pavé numérique tape ses chiffres avec <kbd>Verr. Num</kbd>, sinon ce sont ses flèches, <kbd>Début</kbd>, <kbd>Fin</kbd>... Les autres touches F ne font rien hors d'un champ de texte : Android n'a pas d'action pour elles.

## Mises à jour {#updates}

À chaque lancement, l'appli demande au site de xmouse (`app/version.json`, son sha256 vérifié) et aux appareils de son réseau s'il existe une version plus récente, et propose la plus récente.
