+++
title = "The phone"
summary = "The Android app: a member of the network with its home, a touchpad, and its screen in the layout."
section = "guide"
order = 4
keywords = "phone android app touchpad screen accessibility let them touch show cursors qr member pad browser clipboard typing update"
+++

The app opens on its **home**, like the panel of a computer: its network, its members, its touchpad, its screen and its settings.

## The home {#home}

| Card | Content |
| ---- | ------- |
| Network | Create a network or join one (scan a QR code, paste a link); in a network: its QR code and its code, **Share the link**, **New code**, **Disconnect** / **Connect**, **Leave the network** |
| Members | Every device of the network, online or not, **Remove** |
| This phone | **Open the touchpad**, **This screen in the layout** (its mode) |
| Touchpad of a computer | The touchpad of one paired computer, without the phone in its network |
| Settings | Name, private mode of the screen, updates at every launch, **Check for updates now** |

## The phone as a member {#member}

Like a computer, the phone creates a network (it is then its first device: computers join it with its QR code) or joins one with the QR code of a member, then is accepted there. Its touchpad drives its own cursor across the screens of every device.

## The touchpad {#touchpad}

| Gesture | Effect |
| ------- | ------ |
| One finger | Moves the cursor (faster when the finger moves fast) |
| Tap | Left click |
| Two fingers | Scroll; tap: right click |
| Three fingers | Middle click |
| Tap, then hold | Drag |

The keyboard of the touchpad has sticky modifiers, the special keys and the F-keys. Any browser can be a touchpad too: on the computer, **Network** > **Pair a phone or a browser**, then open its QR code (`https://<address>:47900/pad`).

## The screen in the layout {#screen}

**This screen in the layout** puts the screen of the phone on the map: the cursors of the network come on it.

| Mode | What the cursors do there |
| ---- | ------------------------- |
| Show the cursors | Drawn over the apps with their name ("Display over other apps"); their clicks do nothing |
| Let them touch | Their clicks, drags and scrolls become touches, their keys type in the focused field (Accessibility service "xmouse screen") |

> [!IMPORTANT]
> Android lets an app click in the others **only through an Accessibility service**. **Let them touch** opens the steps: Accessibility > Installed apps > xmouse screen > on. Greyed out ("Restricted setting")? App info > the menu at the top right > **Allow restricted settings**, then again.

With **Let them touch**, the keys typed on a computer arrive as the characters of **its** keyboard layout (azerty included), and the clipboard of the computer is sent to the phone when it changes.

| Key / button of the computer | On the phone |
| ---------------------------- | ------------ |
| <kbd>Enter</kbd> | The action of the field (send, search, next); a new line in a multi-line field |
| <kbd>Ctrl</kbd>+<kbd>V</kbd> / <kbd>C</kbd> / <kbd>X</kbd> / <kbd>A</kbd> | Paste, copy, cut, select all |
| Arrows, <kbd>Home</kbd> / <kbd>End</kbd> (with <kbd>Shift</kbd>: select) | Move in the text; outside a text field, ↑ ↓ ← → move the focus (Android 13+) |
| <kbd>Backspace</kbd> / <kbd>Delete</kbd>, <kbd>Tab</kbd> | Delete, next field |
| <kbd>Page Up</kbd> / <kbd>Page Down</kbd> | Scroll a page |
| <kbd>Esc</kbd> or the back button of the mouse, <kbd>Super</kbd> | Back, home |
| <kbd>Alt</kbd>+<kbd>Tab</kbd>, <kbd>Print Screen</kbd>, play / pause | Recent apps, screenshot, media |

## Updates {#updates}

At every launch the app asks the xmouse website (`app/version.json`, its sha256 checked) and the devices of its network for a newer version, and offers the newest one.
