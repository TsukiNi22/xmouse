+++
title = "Troubleshooting"
summary = "The usual problems and their fix: the capture dialog, a stuck or visible cursor, the phone app, the clicks on a phone."
section = "resources"
order = 1
keywords = "faq troubleshooting problem dialog capture portal gnome extension stuck cursor play protect restricted accessibility clicks phone update uinput"
+++

Each entry: what you see, why, what to do.

## The desktop asks to allow the input capture at every start {#capture-dialog}

On GNOME Wayland without the xmouse extension, xmouse uses the input capture portal, which asks at every start (GNOME has no "remember" for it). Install the extension and **log out and back in once**: GNOME loads a new extension only at login. Once the extension is installed, xmouse no longer uses the portal at all (the portal could leave the pointer hidden or frozen): no edge until the next login, the status says it; `XMOUSE_PORTAL=1` forces the portal.

```bash "Terminal"
scripts/install.sh --gnome
```

## The cursor stays visible at the edge {#cursor-visible}

Same cause: the portal does not hide the cursor while it is on another device; the GNOME extension does. See [above](page:faq#capture-dialog).

## The cursor is stuck on another device {#cursor-stuck}

<kbd>Ctrl</kbd> + <kbd>Alt</kbd> + <kbd>Shift</kbd>, or **Bring my cursor back**, brings it home at once. xmouse also brings it back by itself when the device it is on goes, when its screen changes under it, or when the capture is lost (the log says which).

Before 1.0.13, on GNOME with the input capture portal, the cursor also came back by itself after a few seconds without moving: each refresh of the edges ended the capture. Since 1.0.13 the edges are set again only when they change, never under a capture.

Before 1.0.15, with the GNOME extension, the cursor came back 3 seconds after it left when the mouse did not move meanwhile, and on GNOME 50 the moves, the clicks and the wheel did not reach the other device at all (only the keys did). Since 1.0.15 the extension reads every input of a capture on every GNOME version and keeps the capture while you wait; it gives the pointer back only when the input really goes elsewhere. The extension is loaded at the next login: log out and back in once after the update.

## The others cannot click or type here (Linux Wayland) {#uinput}

The injection needs access to `/dev/uinput`: `scripts/install.sh --uinput` adds the rule (log out and back in), the packages install it.

## Android blocks the app (Play Protect) {#android-install}

An app installed from a browser can be blocked by Play Protect: tap **More details** > **Install anyway** when offered; else turn off **Scan apps with Play Protect** in the Play Store for the install, then turn it on again. Its Accessibility service may be greyed out ("Restricted setting"): App info > the menu at the top right > **Allow restricted settings**.

## The clicks do nothing on the phone {#phone-clicks}

Its screen is in **Show the cursors** mode: Android lets an app click in the others only through an Accessibility service. In the app: **This screen in the layout** > **Let them touch** (see [the phone](page:phone#screen)). The computer tells it too, the first time a cursor goes there.

## The clicks work one time in two on the phone {#phone-clicks-sometimes}

Android stopped the Accessibility service (battery saving, the app swiped away) and the screen was lent without it for a while. Set App info > **Battery** > **Unrestricted** for xmouse; since 1.0.12 the app never lends the screen without the service while it is turned on.

## The phone screen is not on the map {#phone-screen}

**This screen in the layout** must be on (the notification says it), and the phone must reach the computer it is paired with (same LAN, Tailscale). Update the app: [updates](page:phone#updates).

The screen goes to the computer whose touchpad was opened last. Before 1.0.15, opening the touchpad of a computer already paired after pairing another one left the screen without a computer ("xmouse: pair the app with a device first") until the Accessibility service was turned off and on; it now goes to that computer at once.

## A foreign cursor asks to use the panel {#panel-access}

Its click on the xmouse panel is protected: **Accept**, **Refuse**, or let the 60 s pass; **Revoke** takes it back. See [the protected panel](page:cursors#panel).
