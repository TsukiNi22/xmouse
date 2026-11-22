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
| Two fingers apart / together | Zoom in / out where the cursor is (<kbd>Ctrl</kbd> + wheel on a computer, a pinch on a phone screen) |
| Three fingers | Middle click |
| Three fingers to the right / left | Back / forward (the side buttons of a mouse) |
| Tap, then hold | Drag |

The keyboard of the touchpad has sticky modifiers that add up (<kbd>Ctrl</kbd>, <kbd>Alt</kbd>, <kbd>Shift</kbd>, <kbd>Super</kbd>, <kbd>AltGr</kbd>: <kbd>Ctrl</kbd> then <kbd>Shift</kbd> then a key is <kbd>Ctrl</kbd>+<kbd>Shift</kbd>+that key), the special keys (<kbd>Esc</kbd>, <kbd>Tab</kbd>, <kbd>Del</kbd>, <kbd>Ins</kbd>, the arrows, <kbd>Home</kbd> / <kbd>End</kbd>, <kbd>PgUp</kbd> / <kbd>PgDn</kbd>, <kbd>Super</kbd>, <kbd>Menu</kbd>, <kbd>PrtSc</kbd>), <kbd>F1</kbd> to <kbd>F24</kbd>, the media and volume keys, and the usual shortcuts (copy, cut, paste, undo, redo, all, reload, find, save, new tab, close tab, <kbd>Alt</kbd>+<kbd>Tab</kbd>, <kbd>Alt</kbd>+<kbd>F4</kbd>). A key button held repeats like a key of a keyboard. A shortcut keeps its letter on every layout: <kbd>Ctrl</kbd> + <kbd>A</kbd> typed on an AZERTY computer is <kbd>Ctrl</kbd> + <kbd>A</kbd> on a QWERTY one or on a phone, never <kbd>Ctrl</kbd> + <kbd>Q</kbd>. While a cursor of a computer is on a phone screen, it stays on (5 minutes after its last move). Any browser can be a touchpad too: on the computer, **Network** > **Pair a phone or a browser**, then open its QR code (`https://<address>:47900/pad`).

## The screen in the layout {#screen}

**This screen in the layout** puts the screen of the phone on the map: the cursors of the network come on it.

| Mode | What the cursors do there |
| ---- | ------------------------- |
| Show the cursors | Drawn over the apps with their name ("Display over other apps"); their clicks do nothing |
| Let them touch | Their clicks, drags and scrolls become touches, their keys type in the focused field (Accessibility service "xmouse screen") |

> [!IMPORTANT]
> Android lets an app click in the others **only through an Accessibility service**. **Let them touch** opens the steps: Accessibility > Installed apps > xmouse screen > on. Greyed out ("Restricted setting")? App info > the menu at the top right > **Allow restricted settings**, then again. Then App info > **Battery** > **Unrestricted**: else Android stops the service now and then, and the clicks do nothing until it comes back.

With **Let them touch**, a cursor of a computer **is a finger** on the phone: the left button puts it down and lifts it, a drag follows the cursor as it moves (never replayed at the end), a long press lasts what the button is held; the wheel moves the content as it turns, the right button is a long press (the menu of Android). The clipboard of the computer is sent to the phone when it changes.

## Keys on the phone screen {#keys}

In a focused text field (Android 13+), the keys arrive as the **real key events** of a keyboard: every key and every shortcut of the app works there, as with a Bluetooth keyboard (<kbd>Ctrl</kbd>+arrows by word, <kbd>Ctrl</kbd>+<kbd>Shift</kbd>+arrows to select, <kbd>Ctrl</kbd>+<kbd>Backspace</kbd>, <kbd>Ctrl</kbd>+<kbd>Z</kbd> / <kbd>Y</kbd>, <kbd>Home</kbd> / <kbd>End</kbd>, <kbd>Ctrl</kbd>+<kbd>R</kbd> or <kbd>F5</kbd> in a browser...). The characters are the ones of the layout of the computer (AZERTY included). Elsewhere, and on Android 12 and older, the keys become the actions of the phone:

| Keys of the computer | On the phone |
| -------------------- | ------------ |
| Arrows (<kbd>Ctrl</kbd>: by word, <kbd>Shift</kbd>: select), <kbd>Home</kbd> / <kbd>End</kbd> (<kbd>Ctrl</kbd>: the whole text) | Move in the text; outside a field the arrows move the focus (Android 13+), <kbd>Home</kbd> / <kbd>End</kbd> go to the top / bottom of the list under the cursor |
| <kbd>Backspace</kbd> / <kbd>Delete</kbd> (<kbd>Ctrl</kbd>: a word), <kbd>Enter</kbd> (<kbd>Shift</kbd>: a new line), <kbd>Tab</kbd> (<kbd>Shift</kbd>: back) | Delete, the action of the field (send, search, next), the next field |
| <kbd>Ctrl</kbd>+<kbd>A</kbd> / <kbd>C</kbd> / <kbd>X</kbd> / <kbd>V</kbd>, <kbd>Ctrl</kbd>/<kbd>Shift</kbd>+<kbd>Insert</kbd>, <kbd>Shift</kbd>+<kbd>Delete</kbd> | Select all, copy, cut, paste |
| <kbd>Ctrl</kbd>+<kbd>Z</kbd>, <kbd>Ctrl</kbd>+<kbd>Y</kbd> or <kbd>Ctrl</kbd>+<kbd>Shift</kbd>+<kbd>Z</kbd> | Undo, redo (what the computers typed there) |
| <kbd>Page Up</kbd> / <kbd>Page Down</kbd> | The list under the cursor by a page |
| <kbd>F5</kbd>, <kbd>Ctrl</kbd>+<kbd>R</kbd>, the reload key | Reload: a pull down at the cursor (the apps that reload by a pull, at the top of their content) |
| <kbd>Ctrl</kbd>+<kbd>+</kbd> / <kbd>-</kbd>, <kbd>Ctrl</kbd>+wheel | Zoom where the cursor is (a pinch) |
| <kbd>Esc</kbd>, <kbd>Alt</kbd>+<kbd>←</kbd>, <kbd>Alt</kbd>+<kbd>F4</kbd>, the back button of the mouse | Back |
| <kbd>Super</kbd> alone, <kbd>Ctrl</kbd>+<kbd>Esc</kbd> | All the apps (Android 12+) |
| <kbd>Super</kbd>+<kbd>D</kbd> / <kbd>H</kbd> / <kbd>Enter</kbd>, <kbd>Alt</kbd>+<kbd>Home</kbd> | Home |
| <kbd>Alt</kbd>+<kbd>Tab</kbd>, <kbd>Super</kbd>+<kbd>Tab</kbd>, <kbd>Ctrl</kbd>+<kbd>Shift</kbd>+<kbd>Esc</kbd> | Recent apps |
| <kbd>Super</kbd>+<kbd>N</kbd> / <kbd>A</kbd> / <kbd>L</kbd> / <kbd>X</kbd> / <kbd>I</kbd> | Notifications, quick settings, lock, power menu, settings |
| <kbd>Print Screen</kbd>, <kbd>Super</kbd>+<kbd>Shift</kbd>+<kbd>S</kbd> | Screenshot |
| <kbd>Super</kbd>+<kbd>B</kbd> / <kbd>C</kbd> / <kbd>E</kbd> / <kbd>K</kbd> / <kbd>M</kbd> / <kbd>P</kbd> / <kbd>S</kbd> / <kbd>U</kbd> | Browser, contacts, files, calendar, maps, music, messages, calculator |
| <kbd>Menu</kbd>, <kbd>Shift</kbd>+<kbd>F10</kbd>, the right button | Long press (the menu of Android) |
| Media keys, volume keys | The player of the phone, its volume |

A key held repeats (<kbd>Backspace</kbd>, the arrows, a letter). <kbd>Caps Lock</kbd> and <kbd>Num Lock</kbd> follow the keyboard of the computer: the numpad types its digits with <kbd>Num Lock</kbd>, else it is its arrows, <kbd>Home</kbd>, <kbd>End</kbd>... The other F-keys do nothing outside a text field: Android has no action for them.

## Updates {#updates}

At every launch the app asks the xmouse website (`app/version.json`, its sha256 checked) and the devices of its network for a newer version, and offers the newest one.
