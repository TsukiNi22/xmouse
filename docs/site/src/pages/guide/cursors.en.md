+++
title = "Cursors, panel and privacy"
summary = "What the cursors of the others can do on this device: their two modes, the protected panel, the rights and the private mode."
section = "guide"
order = 5
keywords = "foreign cursors pointer phantom mode panel access accept refuse revoke rights private mode passing"
+++

The cursors of the other members (and of the phones) come on your screens with their name. This page says what they may do here.

## Pointer or phantom {#modes}

| Mode (Settings) | The cursor of a member here |
| --------------- | --------------------------- |
| Pointer (default) | Moves the real pointer of this device, like a second mouse; its name is shown beside it |
| Phantom | Drawn over the apps with its name; your pointer stays yours: it only jumps to the phantom for one of its clicks, then comes back |

A cursor that does not move for 3 s fades out, and comes back at once when it moves (its clicks alone do not bring it back). A phone touchpad follows the same modes: in phantom mode it has its own cursor (its name), which goes to the other devices under that name while your mouse stays with you.

## The protected panel {#panel}

A foreign cursor never clicks or types in the xmouse panel by itself: its first click is held, a second one **asks the access**. A banner then offers **Accept** (at once) or **Refuse**; without an answer the access is granted after 60 s (`focusRequestDelay`). A granted access stays, even when you use the panel, until **Revoke**.

## Rights {#rights}

Per member, in the **Members** table: mouse, keyboard, clipboard, files, links, windows. They are decided on each device only, for that device.

## Private mode {#private}

**Private mode** (Settings, tray): the cursors of the others and of the phones only **pass over** this device to reach the others; nothing is clicked, scrolled or typed here, your pointer never moves.

> [!WARNING]
> A member can move, click and type on the devices whose rights allow it: add only your own devices (or people you trust).
