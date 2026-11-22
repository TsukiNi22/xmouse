+++
title = "Known limits"
summary = "What xmouse cannot do yet, or cannot do on a platform."
section = "resources"
order = 3
keywords = "limits wayland portal android accessibility ios windows scale macos fullscreen"
+++

What to know before relying on xmouse.

## Linux {#linux}

- Wayland without the GNOME extension (or on KDE, wlroots): a capture dialog at every start, the cursor visible at the edge, the real pointer not put back after a foreign click.
- The overlays (the drawn cursors) run on Xwayland: a fullscreen Wayland app may cover them.

## Every platform {#all}

- One real pointer and one keyboard focus per device: two foreign cursors clicking at the same moment take turns.
- Drag and drop to an edge needs the platform to tell the held button (GNOME extension, Windows).

## Windows {#windows}

- The screens must share one scale.
- The Windows build has no ML-KEM yet: its links use `X25519` (still TLS 1.3, certificates pinned).

## Phones {#phones}

- Android: the cursors click there only through the Accessibility service; a phone member has no UDP nor relay, and takes only text from the clipboard.
- iOS: no app may drive the cursor of the other devices.

## macOS {#macos}

> [!SOON]
> The macOS part is written; it builds once its library (libutils) builds on macOS.
