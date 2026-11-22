+++
title = "Curseurs, panneau et confidentialité"
summary = "Ce que les curseurs des autres peuvent faire sur cet appareil : leurs deux modes, le panneau protégé, les droits et le mode privé."
keywords = "curseurs étrangers pointeur fantôme mode panneau accès accepter refuser révoquer droits mode privé passage"
+++

Les curseurs des autres membres (et des téléphones) viennent sur vos écrans avec leur nom. Cette page dit ce qu'ils peuvent faire ici.

## Pointeur ou fantôme {#modes}

| Mode (Settings) | Le curseur d'un membre ici |
| --------------- | -------------------------- |
| Pointeur (par défaut) | Déplace le vrai pointeur de cet appareil, comme une deuxième souris ; son nom est affiché à côté |
| Fantôme | Dessiné par-dessus les applis avec son nom ; votre pointeur reste le vôtre : il ne saute vers le fantôme que pour un de ses clics, puis revient |

Un curseur qui ne bouge pas pendant 3 s s'efface, et revient d'un coup quand il bouge (ses clics seuls ne le font pas revenir). Un touchpad de téléphone suit les mêmes modes : en mode fantôme il a son propre curseur (son nom), qui va sur les autres appareils sous ce nom pendant que votre souris reste avec vous.

## Les touches entre appareils {#keys}

Chaque touche part vers l'appareil où est le curseur telle qu'elle est : lettres, chiffres, <kbd>F1</kbd> à <kbd>F24</kbd>, le pavé numérique, les touches média, volume, navigateur et courrier, <kbd>Impr. écran</kbd>, <kbd>Menu</kbd>, les touches japonaises et coréennes. Cet appareil la tape avec **sa propre** disposition : chaque raccourci y marche comme sur son clavier (<kbd>F5</kbd> et <kbd>Ctrl</kbd>+<kbd>R</kbd> rechargent, <kbd>Ctrl</kbd>+<kbd>Maj</kbd>+<kbd>T</kbd>, <kbd>Alt</kbd>+<kbd>Tab</kbd>...).

| Cas | Ce que fait xmouse |
| --- | ------------------ |
| Deux dispositions différentes (AZERTY ici, QWERTY là-bas) | Un raccourci garde sa lettre : <kbd>Ctrl</kbd>+<kbd>A</kbd> tapé ici reste <kbd>Ctrl</kbd>+<kbd>A</kbd> là-bas, jamais <kbd>Ctrl</kbd>+<kbd>Q</kbd> |
| Un Mac et un PC (ou un téléphone) | <kbd>Ctrl</kbd> et <kbd>Cmd</kbd> échangent leur place : <kbd>Ctrl</kbd>+<kbd>C</kbd> sur le PC copie sur le Mac, <kbd>Cmd</kbd>+<kbd>C</kbd> sur le Mac copie sur le PC |
| Un téléphone | Les touches deviennent ses touches ou ses actions : [les touches sur l'écran du téléphone](page:phone#keys) |

<kbd>Ctrl</kbd>+<kbd>Alt</kbd>+<kbd>Maj</kbd> ramène aussitôt le curseur chez lui, quel que soit l'appareil où il est.

## Le panneau protégé {#panel}

Un curseur étranger ne clique ni ne tape jamais seul dans le panneau de xmouse : son premier clic est retenu, un second **demande l'accès**. Un bandeau propose alors **Accept** (tout de suite) ou **Refuse** ; sans réponse l'accès est donné au bout de 60 s (`focusRequestDelay`). Un accès donné reste, même quand vous utilisez le panneau, jusqu'à **Revoke**.

## Droits {#rights}

Par membre, dans le tableau **Members** : souris, clavier, presse-papiers, fichiers, liens, fenêtres. Ils sont décidés sur chaque appareil seulement, pour cet appareil.

## Mode privé {#private}

**Private mode** (Settings, zone de notification) : les curseurs des autres et des téléphones ne font que **passer** sur cet appareil pour atteindre les autres ; rien n'est cliqué, défilé ni tapé ici, votre pointeur ne bouge jamais.

> [!WARNING]
> Un membre peut déplacer, cliquer et taper sur les appareils dont les droits le permettent : n'ajoutez que vos propres appareils (ou des personnes de confiance).
