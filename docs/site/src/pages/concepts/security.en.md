+++
title = "Security of a network"
summary = "Why only your devices get in: the post-quantum TLS links, the pinned identities, the proof of the code and the group key."
section = "concepts"
order = 3
kind = "explain"
level = "simple"
keywords = "security tls 1.3 post-quantum ml-kem x25519 certificate pinned argon2id hmac code brute force key rotation"
+++

:::simple tldr
**In one sentence:** your devices talk through locked tunnels, recognize each other by their own ID card, and a new device gets in only with the short code you show it, and your OK.
:::

:::technical tldr
**Definition:** TLS 1.3 only with a hybrid post-quantum key exchange (`X25519MLKEM768`), self-signed ECDSA P-256 certificates pinned in the replicated state, a join proof `HMAC(Argon2id(code, salt), transcript)` bound to both certificates, a 256-bit group key rotated at every removal.
:::

## The idea {#idea}

:::simple
Think of a club: the door is locked (encryption), every member has a photo ID the others know (the certificate), and to join you must say the code of the day to a member, who also has to say yes. When someone leaves, the lock changes.
:::

:::technical
Three layers: the transport (TLS 1.3, hybrid key exchange against "store now, decrypt later"), the identity (the fingerprint of each certificate is in the network state: a member whose certificate changed is refused), and the membership (the group key, given only after the join proof and the approval, used to prove membership on every link).
:::

## How a device joins {#join}

:::html
<figure class="stepper" data-steps="4">
  <ol class="captions">
    <li><b>Encrypted link.</b> The new device opens TLS 1.3 to a member, with the hybrid key exchange X25519 + ML-KEM-768.</li>
    <li><b>Transcript.</b> Both sides send a random nonce; the transcript = the nonces + the sha256 of both certificates.</li>
    <li><b>Proof.</b> The new device proves it knows the code without sending it: <code>HMAC(Argon2id(code, salt), transcript)</code>.</li>
    <li><b>Welcome.</b> The member checks the proof, asks the local approval, then sends the network key: the device is a member.</li>
  </ol>
  <svg viewBox="0 0 760 220" role="img" aria-label="en-join">
    <rect id="en-n" class="box" x="40" y="70" width="200" height="70" rx="10" data-hl="2"/><text class="lbl" x="140" y="110" text-anchor="middle">new device</text>
    <rect id="en-m" class="box-alt" x="520" y="70" width="200" height="70" rx="10" data-hl="3"/><text class="lbl" x="620" y="110" text-anchor="middle">member</text>
    <path class="edge" data-from="en-n" data-to="en-m" data-hl="0"/>
    <text class="lbl" x="380" y="60" text-anchor="middle" data-show="0-">TLS 1.3 + X25519MLKEM768</text>
    <text class="lbl" x="380" y="170" text-anchor="middle" data-show="1-">nonces + sha256(certificates)</text>
    <text class="lbl" x="380" y="195" text-anchor="middle" data-show="2-">HMAC(Argon2id(code, salt), transcript)</text>
    <text class="lbl" x="620" y="35" text-anchor="middle" data-show="3">network key</text>
  </svg>
</figure>
:::

## How strong is the code {#strength}

:::simple
The code has 9 characters chosen among 32: about 35 thousand billion possible codes. It works once, changes every 5 minutes, and a device that tries 5 wrong codes is shut out for an hour: guessing it is hopeless.
:::

:::technical
| Value | Number |
| ----- | ------ |
| Code | 9 characters, Crockford base32 (5 bits each) = 45 bits, 2^45 = 35 184 372 088 832 codes |
| Online guesses | 5 per address, then a 1 h ban and a new code; the code also changes every 300 s |
| Chance of 5 guesses | 5 / 2^45 = 1.4 x 10^-13 |
| Argon2id cost | 64 MiB, 3 passes, 1 lane per try |

The proof is bound to the transcript (nonces + both certificates): a recorded proof is useless on another session, and a man in the middle has another certificate, so his transcript differs and the proof fails. Offline brute force needs the transcript, which only travels inside TLS.
:::

## Members and removal {#members}

:::simple
Once in, a device shows on every link that it holds the key of the club. Removing a device changes the key for the others only: the removed one is left with an old key that opens nothing.
:::

:::technical
Every link runs a mutual HMAC proof with the group key bound to the TLS session; a removal publishes `removed/<id>`, rotates the key (epoch + 1) and sends it only to the remaining members. The UDP channel derives its key from the group key, so it rotates too.
:::

## Pitfalls {#pitfalls}

:::simple
- A member can move, click and type on your devices if its rights allow it: add only your own devices or people you trust.
- Show the code only to the device you want to add.
:::

:::technical
- A device whose OpenSSL has no ML-KEM (the Windows build today) links with `X25519` only: still TLS 1.3, still pinned, but not post-quantum; it says so in the network.
- Pads (phones, browsers) use a 256-bit token after their pairing; only its sha256 is kept.
:::

## Check yourself {#quiz}

:::html
<details class="quiz"><summary>Why can't someone who recorded a join reuse its proof?</summary><p>The proof is bound to the nonces and the certificates of that session: another session gives another transcript.</p></details>
<details class="quiz"><summary>What does a removed device keep?</summary><p>An old key: the others rotated it, so it opens nothing.</p></details>
:::

## Glossary & sources {#glossary}

| Term | Meaning |
| ---- | ------- |
| TLS 1.3 | The encrypted tunnel of every link |
| ML-KEM-768 | A post-quantum key exchange (NIST FIPS 203), combined with X25519 |
| Pinned certificate | An identity known in advance: another one is refused |
| Argon2id | A slow, memory-hard function: each guess costs time and memory |
| HMAC | A keyed proof: right only with the right key |
