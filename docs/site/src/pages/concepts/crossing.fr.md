+++
title = "Comment le curseur traverse les écrans"
summary = "D'un appareil au suivant par un bord : la capture, la disposition dans un espace commun, les mouvements envoyés et injectés."
keywords = "curseur traverser bord capture disposition espace global entrer sortir injection prédiction 250 hz"
+++

:::simple tldr
**En une phrase :** quand votre curseur pousse un bord qui touche un autre écran, votre ordinateur garde sa souris et envoie où elle va à l'autre appareil, qui la dessine là.
:::

:::technical tldr
**Définition :** chaque écran vit dans un même espace de coordonnées (la disposition) ; un passage capture le pointeur local, convertit les mouvements relatifs en position globale, puis en position locale sur l'appareil cible, envoyée à 250 Hz au plus (la dernière position seulement).
:::

## L'idée {#idea}

:::simple
Imaginez plusieurs bureaux collés sous une grande nappe : votre main glisse d'un bureau au suivant sans se lever. xmouse est la nappe : il sait où est chaque écran, alors le curseur continue simplement.
:::

:::technical
La disposition associe à chaque appareil un rectangle d'un espace global partagé (répliqué sur chaque membre, le dernier qui écrit gagne). Le propriétaire d'un curseur garde sa position globale ; un test de position donne l'appareil dessous et les coordonnées locales là-bas. Seul le propriétaire calcule : la cible ne reçoit que des positions et des événements.
:::

## Comment ça marche {#how}

:::html
<figure class="stepper" data-steps="4">
  <ol class="captions">
    <li><b>Pousser.</b> Votre curseur atteint le bord droit de A en <code>x = 1919</code>, et B touche ce bord dans la disposition.</li>
    <li><b>Retenir.</b> A retient son vrai pointeur (caché, figé au bord, ou au milieu de son écran avec l'extension GNOME) : il ne lit plus que de combien vous bougez.</li>
    <li><b>Entrer.</b> A dit <code>enter</code> à B avec la position sur B (<code>x = 0</code>) : B dessine là le curseur de A.</li>
    <li><b>Bouger.</b> Vous bougez de 150 px vers la droite : A envoie la nouvelle position, B déplace le curseur (ou son vrai pointeur) en <code>x = 150</code>.</li>
  </ol>
  <svg viewBox="0 0 760 210" role="img" aria-label="fr-crossing">
    <rect id="fr-a" class="box" x="40" y="40" width="300" height="130" rx="10"/><text class="lbl" x="190" y="30" text-anchor="middle">A (1920 x 1080)</text>
    <rect id="fr-b" class="box-alt" x="420" y="40" width="300" height="130" rx="10" data-hl="2-"/><text class="lbl" x="570" y="30" text-anchor="middle">B (1280 x 800)</text>
    <circle id="fr-ca" cx="332" cy="105" r="9" class="dot" data-hl="0" data-show="0-1"/>
    <text class="lbl" x="250" y="150" text-anchor="middle" data-show="1-">retenu au bord</text>
    <path class="edge" data-from="fr-a" data-to="fr-b" marker-end="url(#fr-arr)" data-show="2-"/>
    <text class="lbl" x="380" y="195" text-anchor="middle" data-show="2-">enter (x = 0)</text>
    <circle id="fr-cb" cx="430" cy="105" r="9" class="dot" data-show="2-" data-at="2:0,0;3:150,0" data-hl="3"/>
    <defs><marker id="fr-arr" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="7" markerHeight="7" orient="auto"><path class="arrow" d="M0 0L10 5L0 10z"/></marker></defs>
  </svg>
</figure>
:::

De retour sur A (le curseur atteint le bord de B qui touche A), A envoie <code>leave</code>, B retire le curseur et relâche ce qu'il tenait (touches, boutons), et A relâche son pointeur au point correspondant.

## Quelle vitesse {#speed}

| Quoi | Rythme |
| ---- | ------ |
| Mouvements vers un ordinateur | 250 par seconde au plus, la dernière position seulement |
| Mouvements vers un téléphone | environ 66 par seconde (un écran de téléphone dessine 60 images par seconde) |
| Présence sur les cartes | 20 par seconde quand il bouge |
| Dessiné en avance quand un paquet est en retard | 80 ms au plus |

:::simple
Un mouvement en retard n'est jamais rejoué : seule la position la plus récente compte, donc un réseau lent fait sauter le curseur, jamais traîner.
:::

:::technical
Les mouvements passent en UDP chiffré quand les sondes passent, sinon sur le lien TLS avec contre-pression (au-delà de 256 Kio en attente, un mouvement est sauté : le suivant porte la position). La cible dessine un curseur étranger en avance selon sa vitesse pendant 80 ms au plus quand le paquet suivant est en retard ; un clic porte sa propre position, donc un mouvement perdu ne le décale jamais.
:::

## Exemple détaillé {#example}

A fait `1920 x 1080` en global `(0, 0)`, B fait `1280 x 800` placé en global `(1920, 140)`. Le curseur en A `(1919, 400)` bouge de `+5` px : global `(1924, 400)`, dans B ; local sur B = `(1924 - 1920, 400 - 140)` = `(4, 260)`. B reçoit `(4, 260)`.

## Pièges {#pitfalls}

:::simple
- Un appareil a un seul vrai pointeur : deux curseurs qui y cliquent au même moment passent chacun leur tour.
- Si un appareil se déconnecte, son bord devient un mur ; si le curseur était dessus, il revient.
:::

:::technical
- Wayland ne donne à aucun client la position du pointeur : la capture est l'extension GNOME (barrières, pointeur figé) ou le portail InputCapture (libei) ; X11 saisit le pointeur, Windows utilise des hooks bas niveau.
- Une capture peut finir sans que le propriétaire le décide (écran verrouillé, le compositeur) : le propriétaire ramène alors le curseur ; une capture gardée par le compositeur après une libération est désactivée puis réarmée.
- `Ctrl + Alt + Maj` est lu par la capture elle-même : il marche même quand la cible ne répond pas.
:::

## Vérifiez-vous {#quiz}

:::html
<details class="quiz"><summary>Pourquoi un réseau lent fait-il sauter le curseur plutôt que traîner ?</summary><p>Seule la position la plus récente est envoyée et gardée : les anciens mouvements sont jetés, jamais rejoués en retard.</p></details>
<details class="quiz"><summary>Quel appareil calcule où est le curseur ?</summary><p>Son propriétaire : il garde la position globale et envoie à la cible sa position locale.</p></details>
:::

## Glossaire {#glossary}

| Terme | Sens |
| ----- | ---- |
| Disposition | Où est chaque écran dans un même espace partagé |
| Capture | Le propriétaire retient son vrai pointeur et lit les mouvements |
| Injection | La cible fait bouger / cliquer / taper son système (uinput, XTest, SendInput) |
| Calque | Une fenêtre transparente qui dessine les curseurs des autres |
