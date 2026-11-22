+++
title = "Le réseau : maillage, relais et UDP"
summary = "Comment les membres se trouvent, partagent un même état, s'atteignent par un membre, et envoient les mouvements en UDP chiffré."
keywords = "réseau maillage découverte diffusion état répliqué relais sauts udp aes gcm prédiction tailscale"
+++

:::simple tldr
**En une phrase :** chaque appareil parle directement à chaque autre, ils gardent tous le même carnet du réseau, et quand deux ne peuvent pas se parler, un troisième fait passer les messages.
:::

:::technical tldr
**Définition :** un maillage complet de liens TLS (un par paire), un état clé-valeur répliqué (le dernier qui écrit gagne, deltas propagés), le relais par un membre relié (3 sauts au plus) et un canal UDP chiffré pour les mouvements et la présence.
:::

## L'idée {#idea}

:::simple
Imaginez des amis dans une conversation de groupe où chacun a aussi le numéro de tous les autres : pas de serveur central, pas de chef. Si deux d'entre eux ne peuvent pas s'appeler, un ami qui atteint les deux fait passer le message.
:::

:::technical
Aucun appareil n'est le serveur du réseau : n'importe quel membre peut ajouter le suivant, et chaque membre se relie à chaque autre (le lien ouvert par le plus petit identifiant gagne quand les deux appellent). L'état partagé (appareils, écrans, disposition, noms, liens, retraits) est un ensemble de clés avec une horloge logique : un delta est appliqué s'il est plus récent, et propagé aux membres non reliés à son expéditeur.
:::

## Se trouver {#discovery}

| Moyen | Quand |
| ----- | ----- |
| Diffusion locale (`47901` udp) | Même réseau local : aucune adresse à taper |
| Adresses connues | Chaque adresse publiée par un membre (local, Tailscale, internet), essayée tour à tour |
| Exposition | Tailscale, UPnP, un tunnel `bore` / `ngrok`, un port redirigé |

## Le relais {#relay}

:::html
<figure class="stepper" data-steps="4">
  <ol class="captions">
    <li><b>Pas de lien.</b> A et C ne s'atteignent pas (un autre réseau local, un pare-feu) : le lien direct manque.</li>
    <li><b>Vers B.</b> B est relié aux deux (ses <code>links</code> sont dans l'état partagé) : A envoie le message à B, enveloppé avec <code>to = C</code>.</li>
    <li><b>Transmettre.</b> B vérifie que A et C sont membres, puis le transmet à C, qui le lit comme venant de A.</li>
    <li><b>Répondre.</b> C répond de la même façon, par B : le curseur, le presse-papiers et les fichiers passent.</li>
  </ol>
  <svg viewBox="0 0 760 230" role="img" aria-label="fr-relay">
    <rect id="fr-a" class="box" x="40" y="80" width="150" height="60" rx="10" data-hl="0"/><text class="lbl" x="115" y="116" text-anchor="middle">A</text>
    <rect id="fr-b" class="box-alt" x="305" y="20" width="150" height="60" rx="10" data-hl="1-2"/><text class="lbl" x="380" y="56" text-anchor="middle">B</text>
    <rect id="fr-c" class="box" x="570" y="80" width="150" height="60" rx="10" data-hl="2-"/><text class="lbl" x="645" y="116" text-anchor="middle">C</text>
    <path class="edge" data-from="fr-a" data-to="fr-b"/>
    <path class="edge" data-from="fr-b" data-to="fr-c"/>
    <line x1="190" y1="125" x2="570" y2="125" stroke-dasharray="6 6" class="edge" data-warn="0"/>
    <text class="lbl" x="380" y="160" text-anchor="middle" data-show="0">pas de lien direct</text>
    <circle cx="200" cy="80" r="8" class="dot" data-show="1-2" data-at="1:0,0;2:330,0"/>
    <text class="lbl" x="380" y="205" text-anchor="middle" data-show="1-">{"t": "relay", "to": "C", "from": "A"}</text>
    <circle cx="470" cy="95" r="8" class="dot-2" data-show="3"/>
  </svg>
</figure>
:::

:::technical
Un message relayé porte `to`, `from` et son nombre de sauts ; un membre ne transmet qu'entre membres du réseau (les retirés refusés), seulement si son réglage `relay` le permet, 3 sauts au plus. Rien de nouveau n'est exposé : chaque membre a déjà la clé du groupe.
:::

## Les mouvements en UDP {#udp}

:::simple
Les mouvements sont petits et fréquents : en perdre un ne gêne pas, le suivant dit où est le curseur. Ils passent donc par la voie rapide (UDP), scellés pour que personne d'autre ne puisse les lire ni les imiter ; tout le reste reste sur la voie sûre (TLS).
:::

:::technical
Quand les sondes passent, les mouvements et la présence vont en UDP sur le même numéro de port que TLS : AES-256-GCM, clé `HMAC(clé du réseau, "xmouse-udp-v1")` (changée avec elle), nonce = un sel aléatoire du lancement + un compteur, l'en-tête authentifié. Un paquet plus ancien que le plus récent reçu est jeté (numéros de séquence), un rejoué aussi. Sans UDP tout passe par le lien TLS.
:::

## Pièges {#pitfalls}

:::simple
- Deux appareils sur des réseaux différents ont besoin d'un moyen de se rencontrer : Tailscale est le plus simple.
- Un téléphone membre n'a pas d'UDP et ne relaie pas : ses ordinateurs doivent l'atteindre directement.
:::

:::technical
- Le relais ajoute un saut de latence : un curseur relayé par un membre lointain est plus lent qu'un direct.
- L'état suit « le dernier qui écrit gagne » : deux membres qui déplacent le même écran au même moment gardent le dernier déplacement.
:::

## Vérifiez-vous {#quiz}

:::html
<details class="quiz"><summary>Quel appareil est le serveur du réseau ?</summary><p>Aucun : chaque membre se relie à chaque autre, n'importe quel membre peut ajouter un appareil.</p></details>
<details class="quiz"><summary>Pourquoi les mouvements peuvent-ils passer en UDP et pas les fichiers ?</summary><p>Un mouvement perdu est remplacé par le suivant ; un morceau de fichier perdu le casserait, donc les fichiers restent sur TLS.</p></details>
:::

## Glossaire {#glossary}

| Terme | Sens |
| ----- | ---- |
| Maillage | Chaque membre relié à chaque autre |
| État répliqué | Le même carnet du réseau sur chaque membre |
| Relais | Un membre qui fait passer les messages de deux autres |
| UDP | Des datagrammes rapides, sans garantie de livraison |
