/*
 * Page of an xmouse device: what it is (its /api/info), the code of the link, how to install xmouse on the platform
 * of the visitor first (then the other ones), the android app served by the device itself
*/
(function () {
    "use strict";

    const REPOSITORY = "https://github.com/TsukiNi22/xmouse";
    const RELEASES = REPOSITORY + "/releases";
    const LINUX_COMMAND = "curl -fsSL https://raw.githubusercontent.com/TsukiNi22/xmouse/main/setup.sh | bash -s -- --install --pre";
    const SOURCES_COMMAND = "git clone " + REPOSITORY + ".git && cd xmouse && scripts/install.sh --deps --uinput";
    const UA = navigator.userAgent || "";
    const params = new URLSearchParams(location.search);
    const $ = (id) => document.getElementById(id);
    let apk = ""; // version of the android app this device serves ("" = none)

    // Platform of the visitor (the first card)
    function detect() {
        if (/Android/i.test(UA)) return "android";
        if (/iPhone|iPad|iPod/i.test(UA) || (/Macintosh/i.test(UA) && navigator.maxTouchPoints > 1)) return "ios";
        if (/Windows/i.test(UA)) return "windows";
        if (/Macintosh|Mac OS X/i.test(UA)) return "macos";
        if (/Linux|X11|CrOS/i.test(UA)) return "linux";
        return "other";
    }

    // One card: {id, name, tag, text, available, action: {label, href, download}, command, note}
    function platforms() {
        return [
            {
                id: "linux", name: "Linux", tag: "Fedora · Debian · Ubuntu · Arch", available: true,
                text: "The signed package repository of xmouse: this command adds it and installs xmouse (dnf, apt or pacman).",
                command: LINUX_COMMAND,
                note: "GNOME: enable the extension once (no input capture dialog): gnome-extensions enable xmouse@tsukini22.github.io, then a new login.",
            },
            {
                id: "windows", name: "Windows", tag: "Windows 10 · 11 · x64", available: true,
                text: "The zip of the release (xmouse.exe and its libraries): extract it, then run install.ps1 (Start menu, firewall, PATH).",
                action: {label: "Download xmouse-windows-x64.zip", href: RELEASES},
            },
            {
                id: "android", name: "Android", tag: "Phone · tablet", available: apk !== "",
                text: apk !== "" ? "The app served by this device: touchpad, keyboard, your cursor on every screen of the network. It updates itself from the devices of the network."
                    : "The app (touchpad, keyboard, your cursor on the screens) comes with the next version: this device does not carry it yet.",
                action: apk !== "" ? {label: "Install the app (" + apk + ")", href: "/app/xmouse.apk", download: true} : null,
            },
            {
                id: "macos", name: "macOS", tag: "Planned", available: false,
                text: "The macOS version comes after Android (v0.4 of the roadmap). Meanwhile, the sources build on macOS without the edges.",
            },
            {
                id: "ios", name: "iPhone · iPad", tag: "Not available", available: false,
                text: "iOS lets no app drive the cursor of the other devices: no xmouse app there.",
            },
            {
                id: "other", name: "From the sources", tag: "Any other system", available: true,
                text: "Build it yourself (C++20, Qt 6, OpenSSL >= 3.2): the script installs the dependencies, builds and installs xmouse.",
                command: SOURCES_COMMAND,
                action: {label: "Read the README", href: REPOSITORY + "#quick-setup---0-scripts"},
            },
        ];
    }

    function element(tag, className, text) {
        const node = document.createElement(tag);
        if (className) node.className = className;
        if (text !== undefined) node.textContent = text;
        return node;
    }

    // A command with its copy button
    function command(text) {
        const row = element("div", "command");
        const button = element("button", "btn", "Copy");

        button.type = "button";
        button.addEventListener("click", () => {
            navigator.clipboard.writeText(text).then(() => {
                button.textContent = "Copied";
                setTimeout(() => { button.textContent = "Copy"; }, 1500);
            }).catch(() => { button.textContent = "Select it"; });
        });
        const shown = element("code", "", text);
        shown.title = text;
        row.append(shown, button);
        return row;
    }

    function fill(node, p, hero) {
        node.replaceChildren();
        node.classList.toggle("off", !p.available);
        node.append(element("span", "tag", hero ? "For this device · " + p.tag : p.tag), element("h2", "", p.name), element("p", "muted", p.text));
        if (p.command) node.append(command(p.command));
        if (p.action) {
            const link = element("a", hero ? "btn primary" : "btn", p.action.label);
            const row = element("div", "row");
            link.href = p.action.href;
            if (p.action.download) link.setAttribute("download", "");
            else link.rel = "noreferrer";
            row.append(link);
            node.append(row);
        }
        if (p.note) node.append(element("p", "muted small", p.note));
    }

    function render() {
        const mine = detect();
        const list = platforms();
        const hero = list.find((p) => p.id === mine) || list[list.length - 1];

        fill($("hero"), hero, true);
        $("others").replaceChildren();
        for (const p of list) {
            if (p === hero) continue;
            const card = element("article", "platform");
            fill(card, p, false);
            $("others").append(card);
        }
    }

    // Code of the link (the QR code or the copied link of a member)
    const code = params.get("code") || "";
    if (code !== "") {
        $("code").textContent = code;
        $("codeCard").hidden = false;
    }
    $("address").textContent = location.host;
    $("copyLink").addEventListener("click", () => {
        navigator.clipboard.writeText(location.href).then(() => {
            $("copyLink").textContent = "Copied";
            setTimeout(() => { $("copyLink").textContent = "Copy the link"; }, 1500);
        }).catch(() => { $("copyLink").textContent = "Copy it from the address bar"; });
    });
    render();

    // The device, and its android app when this build carries one
    fetch("/api/info", {cache: "no-store"}).then((r) => r.json()).then((info) => {
        const network = info.network || params.get("net") || "";
        const name = element("b", "", info.name || "?");

        $("device").replaceChildren("Device ", name, network !== "" ? " in the network " : ", in no network yet");
        if (network !== "") $("device").append(element("b", "", network));
        $("footer").textContent = "xmouse " + (info.version || "") + "  ·  identity " + (info.identity || "?") + "  ·  every link is end to end TLS 1.3 with a post-quantum key exchange.";
        apk = info.apk || "";
        render();
    }).catch(() => {
        // an old device without /api/info: the steps are enough
    });
})();
