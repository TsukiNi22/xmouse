+++
title = "Create and join a network"
summary = "The first device creates the network, the others join it with a one-time code or a QR code."
section = "guide"
order = 2
keywords = "network create join code qr link approval accept members remove leave tailscale exposure scan"
+++

A network is the group of your devices: every member reaches the others and shares one layout.

## Create the network {#create}

On the first device, **Network** > **Create a network**. Its page then shows a **one-time code** (`K7F-29Q-XM4`), its link and a **QR code**. A phone can also be the first device: [the app](page:phone#member) creates a network from its home.

## Join it {#join}

On each other device, **Network**: type the address of a member (`192.168.1.24:47900`) and its code, or paste its link, then **Join**. On a phone, scan the QR code of a member. From a terminal:

```bash "Terminal"
xmouse join 192.168.1.24:47900 K7F-29Q-XM4
xmouse scan                                   # the devices announced on the LAN
```

Then **accept** the new device on the member (a banner, a notification): refused by default after 30 s.

> [!NOTE]
> A code is used once, then replaced; it also changes every 5 minutes (**New code** at any time). Five wrong codes from one address ban it for one hour.

## Members {#members}

The **Members** table of the Network page lists every device, online or not, with its latency. Each one has its **rights** on this device (mouse, keyboard, clipboard, files, links, windows) and **Remove**: the removed device leaves the network and its key changes for the others.

## Reach a device elsewhere {#exposure}

| Setting | Reach |
| ------- | ----- |
| LAN only | Devices on the same network |
| LAN + Tailscale | Your devices signed in to your tailnet, anywhere |
| LAN + internet | UPnP, a `bore` / `ngrok` tunnel, or a port forwarded by hand |

Two members that cannot reach each other still work: their messages go [through a member](page:mesh#relay).

Next: [place the screens](page:layout#place).
