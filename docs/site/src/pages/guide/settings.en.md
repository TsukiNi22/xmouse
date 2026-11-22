+++
title = "Settings"
summary = "The settings of a device: in the Settings page, or in its config.json."
section = "guide"
order = 7
keywords = "settings config config.json port exposure approval foreign mode private rights udp relay download theme paths xmouse_home"
+++

Every setting is in the **Settings** page; they are kept in `config.json` (`xmouse paths` prints its folder).

## The main settings {#main}

| Setting (`config.json`) | Default | Content |
| ----------------------- | ------- | ------- |
| `name` | host name | Name shown to the others |
| `port` | `47900` | TLS and UDP port (LAN discovery on `47901` udp) |
| `exposure` | `lan` | `lan`, `tailscale`, `upnp`, `bore`, `ngrok`, `manual` |
| `joinApproval` | `true` | A device with the right code must also be accepted here |
| `foreignMode` | `pointer` | The cursors of the others: `pointer` or `phantom` |
| `privateMode` | `false` | The cursors only pass over this device |
| `focusRequestDelay` | `60` | Seconds before an access to the panel is granted (unless refused) |
| `rights` | all allowed | Per member: `mouse`, `keyboard`, `clipboard`, `files`, `links`, `windows` |
| `udp` / `relay` | `true` / `true` | Moves by encrypted UDP / carry the messages of members that cannot reach each other |
| `clipboardFilesLimit` / `filesLimit` | `64` / `4096` | MiB: copied files fetched at once / biggest transfer accepted |
| `downloadDir` | `Downloads/xmouse` | Received files |
| `links` | `ask` | A link sent by a member: `ask`, `open`, `never` |
| `pads` | `true` | A phone or a browser may pair with the code |
| `theme` | `system` | `system`, `dark`, `light` |

## Command line {#cli}

```bash "Terminal"
xmouse                         # the panel (or the terminal panel without a display)
xmouse --tty                   # the terminal panel
xmouse --port 47910 --name pc  # port / name for this run
xmouse paths                   # configuration, data and log folders
```

> [!TIP]
> `XMOUSE_HOME=<folder>` runs xmouse with another configuration and data folder (several instances on one computer).
