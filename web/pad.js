/*
 * Touchpad page of an xmouse device (/pad): pairs with the one-time code (then a token kept here), drives the cursor
 * and the keyboard of the device through /padws; the cursor crosses the borders to the other devices of the network.
 * Gestures: 1 finger moves (accelerated), tap = left click, 2 fingers = scroll / tap right click, 3 fingers tap = middle,
 * tap then hold = drag. The Android app gives its bridge (XmouseAndroid): its token is shared with its screen service.
*/
(function () {
    "use strict";

    const $ = (id) => document.getElementById(id);
    const params = new URLSearchParams(location.search);
    const android = window.XmouseAndroid || null; // the bridge of the android app (null in a browser)
    const TOKEN_KEY = "xmouse.pad.token." + location.host;
    const SEND_INTERVAL = 16; // ms between two moves sent (the sum of the deltas)
    const TAP_TIME = 220; // ms: a touch shorter than this, that did not move, is a click
    const TAP_MOVE = 10; // px a tap may move
    const DRAG_WINDOW = 300; // ms after a tap during which a new touch starts a drag
    const SCROLL_STEP = 2.2; // wheel units (1/120 of a notch) per pixel of two fingers
    const RECONNECT = [500, 1000, 2000, 4000, 8000]; // ms between two tries
    let ws = null;
    let tries = 0;
    let paired = false;
    let pending = {dx: 0, dy: 0};
    let lastSend = 0;
    let speed = Number(localStorage.getItem("xmouse.pad.speed") || "1.6");
    let laser = false;
    let map = null; // last {devices, cursors, me}
    const mods = new Set(); // sticky modifiers (Ctrl, Alt, Shift, Super)

    //----------------------------------------------------------------//
    /* STORE */

    function token() {
        try {
            return (android && android.token && android.token()) || localStorage.getItem(TOKEN_KEY) || "";
        } catch (e) {
            return "";
        }
    }

    function saveToken(value) {
        try {
            if (value) localStorage.setItem(TOKEN_KEY, value);
            else localStorage.removeItem(TOKEN_KEY);
        } catch (e) {
            // private window: paired for this visit only
        }
        if (android && android.saveToken) android.saveToken(value || "");
    }

    //----------------------------------------------------------------//
    /* LINK */

    function send(object) {
        if (ws && ws.readyState === WebSocket.OPEN) ws.send(JSON.stringify(object));
    }

    function connect() {
        ws = new WebSocket("wss://" + location.host + "/padws");
        ws.onopen = () => {
            tries = 0;
            const t = token();
            if (t) send({t: "hello", token: t, role: "pad"});
        };
        ws.onmessage = (event) => {
            let o = null;
            try {
                o = JSON.parse(event.data);
            } catch (e) {
                return;
            }
            onMessage(o);
        };
        ws.onclose = () => {
            $("link").classList.remove("ok");
            setTimeout(connect, RECONNECT[Math.min(tries++, RECONNECT.length - 1)]);
        };
    }

    function onMessage(o) {
        switch (o.t) {
            case "welcome":
                paired = true;
                showPad(o.device);
                break;
            case "paired":
                saveToken(o.token);
                paired = true;
                if (android && android.paired) android.paired(o.token, o.fp || "");
                showPad(o.device);
                break;
            case "waiting":
                $("pairStatus").textContent = "Accept this pad on " + (o.device || "the device") + "...";
                break;
            case "error":
                if (o.unpaired) saveToken("");
                showPair();
                $("pairStatus").textContent = o.text || "refused";
                break;
            case "map":
                map = o;
                drawMap();
                if (o.private && !(o.me && o.me.away)) $("where").textContent = "Private mode on " + (o.device || "the device") + ": the cursor passes over it, its clicks act on the other devices only";
                else $("where").textContent = o.me && o.me.away ? "Cursor on " + o.me.target : "Cursor on " + (deviceName(o.me && o.me.id) || o.device || "this device");
                $("surface").classList.toggle("paused", Boolean(o.private) && !(o.me && o.me.away));
                break;
            case "link":
                if (confirm((o.from || "A member") + " sends a link:\n" + o.url + "\n\nOpen it?")) window.open(o.url, "_blank", "noopener");
                break;
            default:
                break;
        }
    }

    function deviceName(id) {
        if (!map || !id) return "";
        const d = map.devices.find((x) => x.id === id);
        return d ? d.name : "";
    }

    //----------------------------------------------------------------//
    /* VIEWS */

    function showPair() {
        paired = false;
        $("padView").hidden = true;
        $("pairView").hidden = false;
        $("pairCode").value = $("pairCode").value || params.get("code") || "";
        $("pairName").value = $("pairName").value || (android ? "Phone" : (/Mobi|Android|iPhone/i.test(navigator.userAgent) ? "Phone" : "Browser"));
    }

    function showPad(device) {
        $("pairView").hidden = true;
        $("padView").hidden = false;
        $("link").classList.add("ok");
        if (device) document.title = "xmouse pad - " + device;
        if (params.has("code")) history.replaceState(null, "", "/pad"); // the code was used: not kept in the address
    }

    $("pairButton").addEventListener("click", () => {
        const code = $("pairCode").value.trim();
        if (code.length < 6) {$("pairStatus").textContent = "The code is on the device (Network page, Pair a phone)"; return;}
        $("pairStatus").textContent = "Checking the code...";
        send({t: "pair", code: code, name: $("pairName").value.trim(), kind: android ? "phone" : "web"});
    });

    //----------------------------------------------------------------//
    /* MOVES */

    function flush() {
        const now = performance.now();
        if (now - lastSend < SEND_INTERVAL || (pending.dx === 0 && pending.dy === 0)) return;
        lastSend = now;
        send({t: "move", dx: Math.round(pending.dx * 10) / 10, dy: Math.round(pending.dy * 10) / 10});
        pending = {dx: 0, dy: 0};
    }
    setInterval(flush, SEND_INTERVAL);

    // Pointer acceleration: slow fingers are precise, fast ones cross the screens
    function accelerate(dx, dy, dt) {
        const v = Math.hypot(dx, dy) / Math.max(1, dt); // px / ms
        const gain = speed * (1 + Math.min(3, v * 1.5));
        return [dx * gain, dy * gain];
    }

    function click(button) {
        send({t: "button", b: button, down: true});
        send({t: "button", b: button, down: false});
    }

    //----------------------------------------------------------------//
    /* GESTURES */

    const surface = $("surface");
    const touches = new Map(); // pointer id -> {x, y, t, sx, sy}
    let gesture = null; // {start, maxFingers, moved, scroll}
    let lastTap = 0;
    let dragging = false;

    surface.addEventListener("pointerdown", (e) => {
        surface.setPointerCapture(e.pointerId);
        touches.set(e.pointerId, {x: e.clientX, y: e.clientY, t: e.timeStamp, sx: e.clientX, sy: e.clientY});
        if (!gesture) {
            gesture = {start: e.timeStamp, maxFingers: 1, moved: false, scroll: {x: 0, y: 0}};
            // Tap then hold: the left button stays down while the finger moves
            if (e.timeStamp - lastTap < DRAG_WINDOW) {
                dragging = true;
                surface.classList.add("dragging");
                send({t: "button", b: 0, down: true});
            }
        }
        gesture.maxFingers = Math.max(gesture.maxFingers, touches.size);
        $("hint").hidden = true;
        e.preventDefault();
    });

    surface.addEventListener("pointermove", (e) => {
        const t = touches.get(e.pointerId);
        if (!t || !gesture) return;
        const dx = e.clientX - t.x;
        const dy = e.clientY - t.y;
        const dt = e.timeStamp - t.t;
        t.x = e.clientX;
        t.y = e.clientY;
        t.t = e.timeStamp;
        if (Math.hypot(e.clientX - t.sx, e.clientY - t.sy) > TAP_MOVE) gesture.moved = true;
        if (touches.size === 1) {
            const [ax, ay] = accelerate(dx, dy, dt);
            pending.dx += ax;
            pending.dy += ay;
        } else if (touches.size === 2) {
            // Two fingers: scroll (the first finger of the pair drives it)
            if (e.pointerId !== touches.keys().next().value) return;
            gesture.scroll.x += -dx * SCROLL_STEP;
            gesture.scroll.y += dy * SCROLL_STEP; // natural scrolling: the content follows the fingers (wheel > 0 = up)
            const wx = Math.trunc(gesture.scroll.x);
            const wy = Math.trunc(gesture.scroll.y);
            if (Math.abs(wx) >= 8 || Math.abs(wy) >= 8) {
                send({t: "wheel", dx: -wx, dy: wy});
                gesture.scroll.x -= wx;
                gesture.scroll.y -= wy;
            }
        }
        e.preventDefault();
    });

    function end(e) {
        if (!touches.has(e.pointerId)) return;
        touches.delete(e.pointerId);
        if (touches.size > 0 || !gesture) return;
        const short = e.timeStamp - gesture.start < TAP_TIME * (gesture.maxFingers > 1 ? 1.5 : 1);
        if (dragging) {
            dragging = false;
            surface.classList.remove("dragging");
            send({t: "button", b: 0, down: false});
            if (!gesture.moved) click(0); // a double tap: the second click
        } else if (short && !gesture.moved) {
            if (gesture.maxFingers === 1) {click(0); lastTap = e.timeStamp;}
            else if (gesture.maxFingers === 2) click(2);
            else click(1);
        }
        gesture = null;
        flush();
    }
    surface.addEventListener("pointerup", end);
    surface.addEventListener("pointercancel", end);
    surface.addEventListener("contextmenu", (e) => e.preventDefault());

    document.querySelectorAll("[data-button]").forEach((b) => {
        const button = Number(b.dataset.button);
        b.addEventListener("pointerdown", (e) => {e.preventDefault(); b.classList.add("on"); send({t: "button", b: button, down: true});});
        const up = () => {if (b.classList.contains("on")) {b.classList.remove("on"); send({t: "button", b: button, down: false});}};
        b.addEventListener("pointerup", up);
        b.addEventListener("pointerleave", up);
        b.addEventListener("pointercancel", up);
    });

    //----------------------------------------------------------------//
    /* KEYBOARD */

    function key(code, down) {
        send({t: "key", code: code, down: down});
    }

    // A key with the sticky modifiers around it (they are released after it)
    function press(code) {
        mods.forEach((m) => key(m, true));
        key(code, true);
        key(code, false);
        mods.forEach((m) => key(m, false));
        clearMods();
    }

    function clearMods() {
        mods.clear();
        document.querySelectorAll("[data-mod]").forEach((b) => b.classList.remove("on"));
    }

    // A character typed with a modifier held: its key (Ctrl + c -> ControlLeft + KeyC), a plain one: the text
    function codeOf(ch) {
        if (/^[a-z]$/i.test(ch)) return "Key" + ch.toUpperCase();
        if (/^[0-9]$/.test(ch)) return "Digit" + ch;
        return {" ": "Space", "-": "Minus", "=": "Equal", ",": "Comma", ".": "Period", "/": "Slash", ";": "Semicolon"}[ch] || "";
    }

    document.querySelectorAll("[data-mod]").forEach((b) => b.addEventListener("click", () => {
        const m = b.dataset.mod;
        if (mods.has(m)) mods.delete(m);
        else mods.add(m);
        b.classList.toggle("on", mods.has(m));
    }));
    document.querySelectorAll("[data-key]").forEach((b) => b.addEventListener("click", () => press(b.dataset.key)));
    document.querySelectorAll("[data-combo]").forEach((b) => b.addEventListener("click", () => {
        const parts = b.dataset.combo.split("+");
        parts.forEach((p) => key(p, true));
        parts.slice().reverse().forEach((p) => key(p, false));
    }));

    const fkeys = $("fkeys");
    for (let i = 1; i <= 12; ++i) {
        const b = document.createElement("button");
        b.type = "button";
        b.textContent = "F" + i;
        b.addEventListener("click", () => press("F" + i));
        fkeys.append(b);
    }
    $("fkeysToggle").addEventListener("click", () => {fkeys.hidden = !fkeys.hidden;});
    $("keyboardToggle").addEventListener("click", () => {
        $("keys").hidden = !$("keys").hidden;
        $("keyboardToggle").classList.toggle("on", !$("keys").hidden);
        if (!$("keys").hidden) $("typing").focus();
    });

    // The soft keyboard: the text inserted goes as text, the deletions / enter as keys (mobile keyboards give no key codes)
    const typing = $("typing");
    typing.addEventListener("beforeinput", (e) => {
        if (e.inputType === "deleteContentBackward") {press("Backspace"); e.preventDefault(); return;}
        if (e.inputType === "deleteContentForward") {press("Delete"); e.preventDefault(); return;}
        if (e.inputType === "insertLineBreak" || e.inputType === "insertParagraph") {press("Enter"); e.preventDefault(); return;}
        if (e.data && (e.inputType === "insertText" || e.inputType === "insertReplacementText" || e.inputType === "insertCompositionText")) {
            if (e.inputType === "insertCompositionText") return; // committed later as insertText
            if (mods.size > 0 && e.data.length === 1 && codeOf(e.data)) press(codeOf(e.data));
            else send({t: "text", text: e.data});
            e.preventDefault();
        }
    });
    typing.addEventListener("compositionend", (e) => {
        if (e.data) send({t: "text", text: e.data});
        typing.value = "";
    });
    typing.addEventListener("keydown", (e) => {
        // A real keyboard (browser on a computer, bluetooth): its codes as they are
        if (e.isComposing || e.keyCode === 229 || !e.code) return;
        if (e.code.startsWith("Key") || e.code.startsWith("Digit") || e.code === "Space") {
            if (!(e.ctrlKey || e.altKey || e.metaKey) && mods.size === 0) return; // text: beforeinput sends it
        }
        e.preventDefault();
        key(e.code, true);
    });
    typing.addEventListener("keyup", (e) => {
        if (e.isComposing || e.keyCode === 229 || !e.code) return;
        if ((e.code.startsWith("Key") || e.code.startsWith("Digit") || e.code === "Space") && !(e.ctrlKey || e.altKey || e.metaKey) && mods.size === 0) return;
        key(e.code, false);
    });

    //----------------------------------------------------------------//
    /* MENU */

    $("menuToggle").addEventListener("click", () => {$("menu").hidden = !$("menu").hidden;});
    $("mapToggle").addEventListener("click", () => {
        $("map").hidden = !$("map").hidden;
        $("mapToggle").classList.toggle("on", !$("map").hidden);
        drawMap();
    });
    $("ping").addEventListener("click", () => send({t: "ping"}));
    $("laser").addEventListener("click", () => {
        laser = !laser;
        $("laser").classList.toggle("on", laser);
        send({t: "laser", on: laser});
    });
    $("home").addEventListener("click", () => send({t: "home"}));
    $("speed").value = String(speed);
    $("speed").addEventListener("input", () => {
        speed = Number($("speed").value);
        localStorage.setItem("xmouse.pad.speed", String(speed));
    });
    $("sendLink").addEventListener("click", () => {
        if (!map) return;
        const others = map.devices.filter((d) => !d.self && d.online && d.kind !== "phone");
        if (others.length === 0) {alert("No other device online"); return;}
        const url = prompt("Link to open on another device", "https://");
        if (!url) return;
        const names = others.map((d, i) => (i + 1) + ". " + d.name).join("\n");
        const pick = others.length === 1 ? 1 : Number(prompt("On which device?\n" + names, "1"));
        if (pick >= 1 && pick <= others.length) send({t: "link", device: others[pick - 1].id, url: url});
    });
    $("unpair").addEventListener("click", () => {
        if (!confirm("Forget this device? Pairing again needs a new code.")) return;
        saveToken("");
        if (ws) ws.close();
        showPair();
    });

    // The android app: its screen lent to the network (Accessibility service), its updates, another device
    if (android) {
        ["lendScreen", "appUpdate", "changeDevice"].forEach((id) => {$(id).hidden = false;});
        $("lendScreen").addEventListener("click", () => android.lendScreen());
        $("appUpdate").addEventListener("click", () => android.checkUpdate());
        $("changeDevice").addEventListener("click", () => android.changeDevice());
        setInterval(() => {$("lendScreen").classList.toggle("on", android.screenLent());}, 2000);
    }

    //----------------------------------------------------------------//
    /* MAP */

    function drawMap() {
        const canvas = $("map");
        if (canvas.hidden || !map) return;
        const ratio = window.devicePixelRatio || 1;
        const w = canvas.clientWidth;
        const h = canvas.clientHeight;
        const ctx = canvas.getContext("2d");
        let box = null;

        canvas.width = w * ratio;
        canvas.height = h * ratio;
        ctx.scale(ratio, ratio);
        ctx.clearRect(0, 0, w, h);
        map.devices.forEach((d) => d.rects.forEach((r) => {
            box = box ? [Math.min(box[0], r[0]), Math.min(box[1], r[1]), Math.max(box[2], r[0] + r[2]), Math.max(box[3], r[1] + r[3])] : [r[0], r[1], r[0] + r[2], r[1] + r[3]];
        }));
        if (!box) return;
        const scale = Math.min((w - 20) / (box[2] - box[0]), (h - 20) / (box[3] - box[1]));
        const ox = 10 + ((w - 20) - (box[2] - box[0]) * scale) / 2 - box[0] * scale;
        const oy = 10 + ((h - 20) - (box[3] - box[1]) * scale) / 2 - box[1] * scale;
        const css = getComputedStyle(document.documentElement);
        ctx.font = "11px system-ui, sans-serif";
        map.devices.forEach((d) => {
            d.rects.forEach((r, i) => {
                ctx.fillStyle = d.online ? css.getPropertyValue("--surface-alt") : css.getPropertyValue("--bg");
                ctx.strokeStyle = d.self ? css.getPropertyValue("--accent") : css.getPropertyValue("--border");
                ctx.lineWidth = d.self ? 2 : 1;
                ctx.fillRect(ox + r[0] * scale, oy + r[1] * scale, r[2] * scale, r[3] * scale);
                ctx.strokeRect(ox + r[0] * scale, oy + r[1] * scale, r[2] * scale, r[3] * scale);
                if (i === 0) {
                    ctx.fillStyle = css.getPropertyValue("--muted");
                    ctx.fillText(d.name, ox + r[0] * scale + 4, oy + r[1] * scale + 13);
                }
            });
        });
        map.cursors.forEach((c) => {
            if ((c.flags & 2) && c.id !== map.me.id) return; // on its own device
            ctx.fillStyle = c.color;
            ctx.beginPath();
            ctx.arc(ox + c.x * scale, oy + c.y * scale, c.id === map.me.id ? 5 : 4, 0, Math.PI * 2);
            ctx.fill();
        });
    }
    window.addEventListener("resize", drawMap);

    //----------------------------------------------------------------//
    /* START */

    if ("wakeLock" in navigator) navigator.wakeLock.request("screen").catch(() => {}); // the pad stays on while it is used
    if (token()) {showPad(""); $("link").classList.remove("ok");} // paired: the pad at once, its dot turns green once linked
    else showPair();
    connect();
})();
