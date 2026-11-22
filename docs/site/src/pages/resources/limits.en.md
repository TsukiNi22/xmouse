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
- Between a Mac and a PC, <kbd>Ctrl</kbd> and <kbd>Cmd</kbd> change places for every key: <kbd>Ctrl</kbd>+<kbd>Tab</kbd> on the PC is <kbd>Cmd</kbd>+<kbd>Tab</kbd> on the Mac.

## Windows {#windows}

- The screens must share one scale.
- The Windows build has no ML-KEM yet: its links use `X25519` (still TLS 1.3, certificates pinned).

## Phones {#phones}

- Android: the cursors click there only through the Accessibility service; a phone member has no UDP nor relay, and takes only text from the clipboard.
- Android gives the real key events only to a focused text field (Android 13+): elsewhere a shortcut of an app (<kbd>Ctrl</kbd>+<kbd>T</kbd> in a browser...) has no equivalent; reload is a pull down at the cursor, which works at the top of a list.
- iOS: no app may drive the cursor of the other devices.

## macOS {#macos}

> [!SOON]
> The macOS part is written; it builds once its library (libutils) builds on macOS.
