/*
 * Page of an xmouse device: what it is (its /api/info), the code of the link, how to install xmouse on the platform
 * of the visitor first (then the other ones): the files of the latest release (pre-releases included) and the android
 * app served by the device itself
*/
(function () {
    "use strict";

    const REPOSITORY = "https://github.com/TsukiNi22/xmouse";
    const RELEASES_API = "https://api.github.com/repos/TsukiNi22/xmouse/releases?per_page=10";
    const LINUX_COMMAND = "curl -fsSL https://raw.githubusercontent.com/TsukiNi22/xmouse/main/setup.sh | bash -s -- --install --pre";
    const SOURCES_COMMAND = "git clone " + REPOSITORY + ".git && cd xmouse && scripts/install.sh --deps --uinput --gnome";
    const FILES = { // file of a release -> its button (the -op variant holds the program, the mingw64- ones are packages of the sysroot)
        setup: /^xmouse-windows-x64-setup\.exe$/,
        zip: /^xmouse-windows-x64\.zip$/,
        rpm: /^xmouse-op(-pre)?-\d[^/]*\.rpm$/,
        deb: /^xmouse-op(-pre)?-\d[^/]*\.deb$/,
        arch: /^xmouse-op(-pre)?-\d[^/]*\.pkg\.tar\.zst$/,
        tarball: /^xmouse-linux-x86_64\.tar\.gz$/,
        apk: /\.apk$/,
        sums: /^SHA256SUMS$/,
    };
    const UA = navigator.userAgent || "";
    const params = new URLSearchParams(location.search);
    const $ = (id) => document.getElementById(id);
    let apk = ""; // version of the android app this device serves ("" = none)
    let release = null; // {tag, url, assets: {setup: {url, size}, ...}} of the newest release with files (null = not known yet)
    let releaseState = "loading"; // loading | ok | none (offline, no release yet)

    // Platform of the visitor (the first card)
    function detect() {
        if (/Android/i.test(UA)) return "android";
        if (/iPhone|iPad|iPod/i.test(UA) || (/Macintosh/i.test(UA) && navigator.maxTouchPoints > 1)) return "ios";
        if (/Windows/i.test(UA)) return "windows";
        if (/Macintosh|Mac OS X/i.test(UA)) return "macos";
        if (/Linux|X11|CrOS/i.test(UA)) return "linux";
        return "other";
    }

    // A button on a file of the release: {label, file} | {label, href} ; missing file -> shown disabled
    function asset(key) {
        return release && release.assets[key] ? release.assets[key] : null;
    }

    function platforms() {
        const deviceApk = apk !== "" ? {label: "Install the app (" + apk + ")", href: "/app/xmouse.apk", download: true} : null;
        return [
            {
                id: "linux", name: "Linux", tag: "Fedora · Debian · Ubuntu · Arch", available: true,
                text: "The signed package repository of xmouse: this command adds it and installs xmouse (dnf, apt or pacman), the updates come with the system ones. Or a package of the release:",
                command: LINUX_COMMAND,
                buttons: [{label: ".rpm (Fedora)", file: "rpm"}, {label: ".deb (Debian, Ubuntu)", file: "deb"}, {label: ".pkg.tar.zst (Arch)", file: "arch"}, {label: ".tar.gz (other distributions, into ~/.local)", file: "tarball"}],
                note: "GNOME: enable the extension once (no input capture dialog): gnome-extensions enable xmouse@tsukini22.github.io, then a new login.",
            },
            {
                id: "windows", name: "Windows", tag: "Windows 10 · 11 · x64", available: true,
                text: "The installer: Start menu, firewall of the private networks, start with Windows (optional), uninstall from the settings. Or the zip (portable, install.ps1 inside).",
                buttons: [{label: "Download the installer", file: "setup", primary: true}, {label: "Zip", file: "zip"}],
            },
            {
                id: "android", name: "Android", tag: "Phone · tablet", available: deviceApk !== null || asset("apk") !== null,
                text: "The app: touchpad, keyboard, your cursor on every screen of the network. It updates itself from the devices of the network.",
                buttons: deviceApk ? [Object.assign(deviceApk, {primary: true})] : [{label: "Install the app (.apk)", file: "apk", primary: true}],
            },
            {
                id: "macos", name: "macOS", tag: "Planned", available: false,
                text: "The macOS version comes after Android (v0.4 of the roadmap). Meanwhile, the sources build on macOS without the edges.",
                buttons: [{label: "Not available yet", file: "none"}],
            },
            {
                id: "ios", name: "iPhone · iPad", tag: "Not available", available: false,
                text: "iOS lets no app drive the cursor of the other devices: no xmouse app there.",
            },
            {
                id: "other", name: "From the sources", tag: "Any other system", available: true,
                text: "Build it yourself (C++20, Qt 6, OpenSSL >= 3.2): the script installs the dependencies, builds and installs xmouse.",
                command: SOURCES_COMMAND,
                buttons: [{label: "Read the README", href: REPOSITORY + "#quick-setup---0-scripts"}],
            },
        ];
    }

    function element(tag, className, text) {
        const node = document.createElement(tag);
        if (className) node.className = className;
        if (text !== undefined) node.textContent = text;
        return node;
    }

    function megabytes(bytes) {
        return (bytes / 1048576).toFixed(bytes < 10485760 ? 1 : 0) + " MB";
    }

    // A command (the whole text, scrolled) with its copy button
    function command(text) {
        const row = element("div", "command");
        const shown = element("code", "", text);
        const button = element("button", "btn", "Copy");

        shown.title = text;
        button.type = "button";
        button.addEventListener("click", () => {
            navigator.clipboard.writeText(text).then(() => {
                button.textContent = "Copied";
                setTimeout(() => { button.textContent = "Copy"; }, 1500);
            }).catch(() => { button.textContent = "Select it"; });
        });
        row.append(shown, button);
        return row;
    }

    function button(b, hero) {
        const link = element("a", "btn" + (b.primary && hero ? " primary" : ""));
        const file = b.file ? asset(b.file) : null;

        link.append(b.label);
        if (b.href) {
            link.href = b.href;
            if (b.download) link.setAttribute("download", "");
            else link.rel = "noreferrer";
            return link;
        }
        if (!file) {
            // Not in the release (yet): shown, not clickable, the reason said
            link.classList.add("off");
            link.setAttribute("aria-disabled", "true");
            if (b.file !== "none") link.append(element("span", "size", releaseState === "loading" ? "  · looking..." : "  · not available yet"));
            return link;
        }
        link.href = file.url;
        link.rel = "noreferrer";
        link.append(element("span", "size", "  · " + megabytes(file.size)));
        return link;
    }

    function fill(node, p, hero) {
        node.replaceChildren();
        node.classList.toggle("off", !p.available);
        node.append(element("span", "tag", hero ? "For this device · " + p.tag : p.tag), element("h2", "", p.name), element("p", "muted", p.text));
        if (p.command) node.append(command(p.command));
        if (p.buttons && p.buttons.length > 0) {
            const row = element("div", "row");
            for (const b of p.buttons)
                row.append(button(b, hero));
            node.append(row);
        }
        if (p.note) node.append(element("p", "muted small", p.note));
        if (hero && release && p.buttons && p.buttons.some((b) => b.file)) {
            const from = element("p", "muted small", "From the release " + release.tag + ". ");
            const sums = asset("sums");
            if (sums) {
                const link = element("a", "", "SHA256SUMS");
                link.href = sums.url;
                link.rel = "noreferrer";
                from.append("Check a file: sha256sum -c ", link, " --ignore-missing.");
            }
            node.append(from);
        }
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

    // The newest release that carries files (the pre-releases too: xmouse has no stable one yet)
    fetch(RELEASES_API, {cache: "no-store"}).then((r) => r.json()).then((list) => {
        const found = Array.isArray(list) ? list.find((r) => !r.draft && r.assets && r.assets.length > 0) : null;
        if (found) {
            release = {tag: found.tag_name, url: found.html_url, assets: {}};
            for (const a of found.assets)
                for (const [key, pattern] of Object.entries(FILES))
                    if (pattern.test(a.name) && !release.assets[key]) release.assets[key] = {url: a.browser_download_url, size: a.size};
        }
        releaseState = found ? "ok" : "none";
        render();
    }).catch(() => {
        releaseState = "none"; // offline (a LAN without internet): the command and the device app stay
        render();
    });

    // The device, and its android app when this build carries one
    fetch("/api/info", {cache: "no-store"}).then((r) => r.json()).then((info) => {
        const network = info.network || params.get("net") || "";

        $("device").replaceChildren("Device ", element("b", "", info.name || "?"), network !== "" ? " in the network " : ", in no network yet");
        if (network !== "") $("device").append(element("b", "", network));
        $("footer").textContent = "xmouse " + (info.version || "") + "  ·  identity " + (info.identity || "?") + "  ·  every link is end to end TLS 1.3 with a post-quantum key exchange.";
        apk = info.apk || "";
        render();
    }).catch(() => {
        // an old device without /api/info: the steps are enough
    });
})();
