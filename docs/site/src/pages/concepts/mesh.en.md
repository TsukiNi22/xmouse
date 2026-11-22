+++
title = "The network: mesh, relay and UDP"
summary = "How the members find each other, share one state, reach each other through a member, and send the moves by encrypted UDP."
section = "concepts"
order = 2
kind = "explain"
level = "simple"
keywords = "network mesh discovery broadcast state replicated relay hops udp aes gcm prediction tailscale"
+++

:::simple tldr
**In one sentence:** every device talks directly to every other one, they all keep the same notebook of the network, and when two cannot talk, a third one passes the messages.
:::

:::technical tldr
**Definition:** a full mesh of TLS links (one per pair), a replicated key-value state (last writer wins, deltas gossiped), relaying through a linked member (3 hops at most) and an encrypted UDP side channel for the moves and the presence.
:::

## The idea {#idea}

:::simple
Picture friends in a group chat where everyone also has everyone's phone number: no central server, nobody is the boss. If two of them cannot call each other, a friend who can reach both passes the message.
:::

:::technical
No device is a server of the network: any member can add the next one, and every member links to every other (the link opened by the smaller id wins when both dial). The shared state (devices, screens, layout, names, links, removals) is a set of keys with a logical clock: a delta is applied when it is newer, and gossiped to the members not linked to its sender.
:::

## Finding each other {#discovery}

| Way | When |
| --- | ---- |
| LAN broadcast (`47901` udp) | Same local network: no address to type |
| Known addresses | Every address a member published (LAN, Tailscale, internet), tried in turn |
| Exposure | Tailscale, UPnP, a `bore` / `ngrok` tunnel, a forwarded port |

## The relay {#relay}

:::html
<figure class="stepper" data-steps="4">
  <ol class="captions">
    <li><b>No link.</b> A and C cannot reach each other (another LAN, a firewall): the direct link is missing.</li>
    <li><b>To B.</b> B is linked to both (its <code>links</code> are in the shared state): A sends the message to B, wrapped with <code>to = C</code>.</li>
    <li><b>Forward.</b> B checks that A and C are members, then forwards it to C, which reads it as coming from A.</li>
    <li><b>Answer.</b> C answers the same way, through B: the cursor, the clipboard and the files pass.</li>
  </ol>
  <svg viewBox="0 0 760 230" role="img" aria-label="en-relay">
    <rect id="en-a" class="box" x="40" y="80" width="150" height="60" rx="10" data-hl="0"/><text class="lbl" x="115" y="116" text-anchor="middle">A</text>
    <rect id="en-b" class="box-alt" x="305" y="20" width="150" height="60" rx="10" data-hl="1-2"/><text class="lbl" x="380" y="56" text-anchor="middle">B</text>
    <rect id="en-c" class="box" x="570" y="80" width="150" height="60" rx="10" data-hl="2-"/><text class="lbl" x="645" y="116" text-anchor="middle">C</text>
    <path class="edge" data-from="en-a" data-to="en-b"/>
    <path class="edge" data-from="en-b" data-to="en-c"/>
    <line x1="190" y1="125" x2="570" y2="125" stroke-dasharray="6 6" class="edge" data-warn="0"/>
    <text class="lbl" x="380" y="160" text-anchor="middle" data-show="0">no direct link</text>
    <circle cx="200" cy="80" r="8" class="dot" data-show="1-2" data-at="1:0,0;2:330,0"/>
    <text class="lbl" x="380" y="205" text-anchor="middle" data-show="1-">{"t": "relay", "to": "C", "from": "A"}</text>
    <circle cx="470" cy="95" r="8" class="dot-2" data-show="3"/>
  </svg>
</figure>
:::

:::technical
A relayed message carries `to`, `from` and its hop count; a member forwards only between members of the network (removed ones refused), only when its `relay` setting allows it, 3 hops at most. Nothing new is exposed: every member already holds the group key.
:::

## The moves by UDP {#udp}

:::simple
Moves are small and frequent: losing one does not matter, the next one says where the cursor is. So they travel by the fast lane (UDP), sealed so nobody else can read or fake them; everything else stays on the safe lane (TLS).
:::

:::technical
When the probes pass, the moves and the presence go by UDP on the same port number as TLS: AES-256-GCM, key `HMAC(network key, "xmouse-udp-v1")` (rotated with it), nonce = a random salt of the run + a counter, the header authenticated. A packet older than the newest one received is dropped (sequence numbers), a replayed one too. Without UDP everything goes on the TLS link.
:::

## Pitfalls {#pitfalls}

:::simple
- Two devices on different networks need a way to meet: Tailscale is the simplest.
- A phone member has no UDP and does not relay: its computers must reach it directly.
:::

:::technical
- The relay adds a hop of latency: a cursor relayed through a far member is slower than a direct one.
- The state is last-writer-wins: two members moving the same screen at the same moment keep the later move.
:::

## Check yourself {#quiz}

:::html
<details class="quiz"><summary>Which device is the server of the network?</summary><p>None: every member links to every other, any member can add a device.</p></details>
<details class="quiz"><summary>Why can the moves use UDP and not the files?</summary><p>A lost move is replaced by the next one; a lost piece of a file would break it, so files stay on TLS.</p></details>
:::

## Glossary {#glossary}

| Term | Meaning |
| ---- | ------- |
| Mesh | Every member linked to every other |
| Replicated state | The same notebook of the network on every member |
| Relay | A member passing the messages of two others |
| UDP | Fast datagrams, without delivery guarantee |
