+++
title = "How the cursor crosses the screens"
summary = "From one device to the next through a border: the capture, the layout in one global space, the moves sent and injected."
section = "concepts"
order = 1
kind = "explain"
level = "simple"
keywords = "cursor crossing border capture layout global space enter leave injection prediction 250 hz"
+++

:::simple tldr
**In one sentence:** when your cursor pushes a border that touches another screen, your computer keeps its mouse and sends where it goes to the other device, which draws it there.
:::

:::technical tldr
**Definition:** every screen lives in one global coordinate space (the layout); a crossing captures the local pointer, converts relative moves into a global position, then into a local position on the target device, sent there at 250 Hz at most (latest position only).
:::

## The idea {#idea}

:::simple
Think of several desks pushed together under one big tablecloth: your hand slides off one desk onto the next without lifting. xmouse is the tablecloth: it knows where every screen is, so the cursor just continues.
:::

:::technical
The layout maps each device to a rectangle of a shared global space (replicated on every member, last writer wins). The owner of a cursor keeps its global position; a hit test gives the device under it and the local coordinates there. Only the owner computes: the target only receives positions and input events.
:::

## How it works {#how}

:::html
<figure class="stepper" data-steps="4">
  <ol class="captions">
    <li><b>Push.</b> Your cursor reaches the right border of A at <code>x = 1919</code>, and B touches that border on the layout.</li>
    <li><b>Hold.</b> A holds its real pointer (hidden, frozen at the border, or at the middle of its screen with the GNOME extension): from now on it only reads how far you move.</li>
    <li><b>Enter.</b> A tells B <code>enter</code> with the position on B (<code>x = 0</code>): B draws the cursor of A there.</li>
    <li><b>Move.</b> You move 150 px to the right: A sends the new position, B moves the cursor (or its real pointer) to <code>x = 150</code>.</li>
  </ol>
  <svg viewBox="0 0 760 210" role="img" aria-label="en-crossing">
    <rect id="en-a" class="box" x="40" y="40" width="300" height="130" rx="10"/><text class="lbl" x="190" y="30" text-anchor="middle">A (1920 x 1080)</text>
    <rect id="en-b" class="box-alt" x="420" y="40" width="300" height="130" rx="10" data-hl="2-"/><text class="lbl" x="570" y="30" text-anchor="middle">B (1280 x 800)</text>
    <circle id="en-ca" cx="332" cy="105" r="9" class="dot" data-hl="0" data-show="0-1"/>
    <text class="lbl" x="250" y="150" text-anchor="middle" data-show="1-">held at the border</text>
    <path class="edge" data-from="en-a" data-to="en-b" marker-end="url(#en-arr)" data-show="2-"/>
    <text class="lbl" x="380" y="195" text-anchor="middle" data-show="2-">enter (x = 0)</text>
    <circle id="en-cb" cx="430" cy="105" r="9" class="dot" data-show="2-" data-at="2:0,0;3:150,0" data-hl="3"/>
    <defs><marker id="en-arr" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="7" markerHeight="7" orient="auto"><path class="arrow" d="M0 0L10 5L0 10z"/></marker></defs>
  </svg>
</figure>
:::

Back on A (the cursor reaches the border of B that touches A), A sends <code>leave</code>, B removes the cursor and releases what it held (keys, buttons), and A releases its pointer at the matching point.

## How fast {#speed}

| What | Rate |
| ---- | ---- |
| Moves to a computer | 250 per second at most, the latest position only |
| Moves to a phone | about 66 per second (a phone screen draws 60 frames per second) |
| Presence on the maps | 20 per second while it moves |
| Drawn ahead while a packet is late | 80 ms at most |

:::simple
A late move is never replayed: only the newest position matters, so a slow network makes the cursor jump, never lag behind.
:::

:::technical
The moves go by encrypted UDP when the probes pass, else on the TLS link with back pressure (above 256 KiB waiting, a move is skipped: the next one carries the position). The target draws a foreign cursor ahead along its speed for 80 ms at most while the next packet is late; a click carries its own position, so a lost move never misplaces it.
:::

## Worked example {#example}

A is `1920 x 1080` at global `(0, 0)`, B is `1280 x 800` placed at global `(1920, 140)`. The cursor at A `(1919, 400)` moves `+5` px: global `(1924, 400)`, inside B; local on B = `(1924 - 1920, 400 - 140)` = `(4, 260)`. B receives `(4, 260)`.

## Pitfalls {#pitfalls}

:::simple
- A device has one real pointer: two cursors clicking there at the same moment take turns.
- If a device goes offline, its border becomes a wall; if the cursor was on it, it comes back home.
:::

:::technical
- Wayland gives no client the pointer position: the capture is the GNOME extension (pointer barriers, frozen pointer) or the InputCapture portal (libei); X11 grabs the pointer, Windows uses low level hooks.
- A capture can end without the owner deciding it (lock screen, the compositor): the owner then brings the cursor back; a capture kept by the compositor after a release is disabled and armed again.
- `Ctrl + Alt + Shift` is read by the capture itself: it works even when the target does not answer.
:::

## Check yourself {#quiz}

:::html
<details class="quiz"><summary>Why does a slow network make the cursor jump rather than lag?</summary><p>Only the newest position is sent and kept: older moves are dropped, never replayed late.</p></details>
<details class="quiz"><summary>Which device computes where the cursor is?</summary><p>Its owner: it holds the global position and sends the target its local position.</p></details>
:::

## Glossary {#glossary}

| Term | Meaning |
| ---- | ------- |
| Layout | Where every screen is in one shared space |
| Capture | The owner holds its real pointer and reads the moves |
| Injection | The target makes its system move / click / type (uinput, XTest, SendInput) |
| Overlay | A transparent window drawing the cursors of the others |
