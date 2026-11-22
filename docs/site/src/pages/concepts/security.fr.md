+++
title = "Sécurité d'un réseau"
summary = "Pourquoi seuls vos appareils entrent : les liens TLS post-quantiques, les identités épinglées, la preuve du code et la clé du groupe."
keywords = "sécurité tls 1.3 post-quantique ml-kem x25519 certificat épinglé argon2id hmac code force brute rotation clé"
+++

:::simple tldr
**En une phrase :** vos appareils se parlent dans des tunnels fermés à clé, se reconnaissent à leur propre carte d'identité, et un nouvel appareil n'entre qu'avec le code court que vous lui montrez, et votre accord.
:::

:::technical tldr
**Définition :** TLS 1.3 seul avec un échange de clés hybride post-quantique (`X25519MLKEM768`), des certificats ECDSA P-256 auto-signés épinglés dans l'état répliqué, une preuve d'entrée `HMAC(Argon2id(code, sel), transcription)` liée aux deux certificats, une clé de groupe de 256 bits changée à chaque retrait.
:::

## L'idée {#idea}

:::simple
Imaginez un club : la porte est fermée à clé (le chiffrement), chaque membre a une carte d'identité que les autres connaissent (le certificat), et pour entrer il faut dire le code du jour à un membre, qui doit aussi dire oui. Quand quelqu'un part, la serrure change.
:::

:::technical
Trois couches : le transport (TLS 1.3, échange hybride contre « enregistrer maintenant, déchiffrer plus tard »), l'identité (l'empreinte de chaque certificat est dans l'état du réseau : un membre dont le certificat a changé est refusé), et l'appartenance (la clé du groupe, donnée seulement après la preuve d'entrée et l'accord, utilisée pour prouver l'appartenance sur chaque lien).
:::

## Comment un appareil entre {#join}

:::html
<figure class="stepper" data-steps="4">
  <ol class="captions">
    <li><b>Lien chiffré.</b> Le nouvel appareil ouvre TLS 1.3 vers un membre, avec l'échange de clés hybride X25519 + ML-KEM-768.</li>
    <li><b>Transcription.</b> Chaque côté envoie un nonce aléatoire ; la transcription = les nonces + le sha256 des deux certificats.</li>
    <li><b>Preuve.</b> Le nouvel appareil prouve qu'il connaît le code sans l'envoyer : <code>HMAC(Argon2id(code, sel), transcription)</code>.</li>
    <li><b>Bienvenue.</b> Le membre vérifie la preuve, demande l'accord local, puis envoie la clé du réseau : l'appareil est membre.</li>
  </ol>
  <svg viewBox="0 0 760 220" role="img" aria-label="fr-join">
    <rect id="fr-n" class="box" x="40" y="70" width="200" height="70" rx="10" data-hl="2"/><text class="lbl" x="140" y="110" text-anchor="middle">nouvel appareil</text>
    <rect id="fr-m" class="box-alt" x="520" y="70" width="200" height="70" rx="10" data-hl="3"/><text class="lbl" x="620" y="110" text-anchor="middle">membre</text>
    <path class="edge" data-from="fr-n" data-to="fr-m" data-hl="0"/>
    <text class="lbl" x="380" y="60" text-anchor="middle" data-show="0-">TLS 1.3 + X25519MLKEM768</text>
    <text class="lbl" x="380" y="170" text-anchor="middle" data-show="1-">nonces + sha256(certificats)</text>
    <text class="lbl" x="380" y="195" text-anchor="middle" data-show="2-">HMAC(Argon2id(code, salt), transcript)</text>
    <text class="lbl" x="620" y="35" text-anchor="middle" data-show="3">clé du réseau</text>
  </svg>
</figure>
:::

## Quelle force pour le code {#strength}

:::simple
Le code a 9 caractères choisis parmi 32 : environ 35 mille milliards de codes possibles. Il sert une fois, change toutes les 5 minutes, et un appareil qui essaie 5 mauvais codes est bloqué une heure : le deviner est sans espoir.
:::

:::technical
| Valeur | Nombre |
| ------ | ------ |
| Code | 9 caractères, base32 de Crockford (5 bits chacun) = 45 bits, 2^45 = 35 184 372 088 832 codes |
| Essais en ligne | 5 par adresse, puis un bannissement d'1 h et un nouveau code ; le code change aussi toutes les 300 s |
| Chance avec 5 essais | 5 / 2^45 = 1,4 x 10^-13 |
| Coût d'Argon2id | 64 Mio, 3 passes, 1 voie par essai |

La preuve est liée à la transcription (nonces + les deux certificats) : une preuve enregistrée ne sert à rien dans une autre session, et un homme du milieu a un autre certificat, donc sa transcription diffère et la preuve échoue. La force brute hors ligne a besoin de la transcription, qui ne circule que dans TLS.
:::

## Membres et retrait {#members}

:::simple
Une fois entré, un appareil montre sur chaque lien qu'il a la clé du club. Retirer un appareil change la clé pour les autres seulement : celui qui est retiré garde une vieille clé qui n'ouvre plus rien.
:::

:::technical
Chaque lien fait une preuve HMAC mutuelle avec la clé du groupe liée à la session TLS ; un retrait publie `removed/<id>`, change la clé (époque + 1) et ne l'envoie qu'aux membres restants. Le canal UDP dérive sa clé de la clé du groupe, donc elle change aussi.
:::

## Pièges {#pitfalls}

:::simple
- Un membre peut déplacer, cliquer et taper sur vos appareils si ses droits le permettent : n'ajoutez que vos propres appareils ou des personnes de confiance.
- Ne montrez le code qu'à l'appareil que vous voulez ajouter.
:::

:::technical
- Un appareil dont OpenSSL n'a pas ML-KEM (la version Windows aujourd'hui) se relie en `X25519` seulement : toujours TLS 1.3, toujours épinglé, mais pas post-quantique ; il le dit dans le réseau.
- Les pads (téléphones, navigateurs) utilisent un jeton de 256 bits après leur association ; seul son sha256 est gardé.
:::

## Vérifiez-vous {#quiz}

:::html
<details class="quiz"><summary>Pourquoi quelqu'un qui a enregistré une entrée ne peut-il pas réutiliser sa preuve ?</summary><p>La preuve est liée aux nonces et aux certificats de cette session : une autre session donne une autre transcription.</p></details>
<details class="quiz"><summary>Que garde un appareil retiré ?</summary><p>Une vieille clé : les autres l'ont changée, donc elle n'ouvre plus rien.</p></details>
:::

## Glossaire et sources {#glossary}

| Terme | Sens |
| ----- | ---- |
| TLS 1.3 | Le tunnel chiffré de chaque lien |
| ML-KEM-768 | Un échange de clés post-quantique (NIST FIPS 203), combiné à X25519 |
| Certificat épinglé | Une identité connue d'avance : une autre est refusée |
| Argon2id | Une fonction lente et gourmande en mémoire : chaque essai coûte du temps et de la mémoire |
| HMAC | Une preuve à clé : juste seulement avec la bonne clé |
