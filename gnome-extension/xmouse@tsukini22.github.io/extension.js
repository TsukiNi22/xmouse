/*
 * Edition:
 *  22/11/2026 by Tsukini
 *
 * File Name:
 *  extension.js
 *
 * File Description:
 *  GNOME Shell side of xmouse: pointer barriers on the edges opening on the other devices, capture of the mouse and
 *  the keyboard once the pointer pushes one (modal grab, hidden and held pointer), pointer position; no portal dialog
*/

//----------------------------------------------------------------//
/* IMPORT */

import {Extension} from "resource:///org/gnome/shell/extensions/extension.js";
import * as Main from "resource:///org/gnome/shell/ui/main.js";
import Clutter from "gi://Clutter";
import Shell from "gi://Shell";
import GLib from "gi://GLib";
import Meta from "gi://Meta";
import Gio from "gi://Gio";
import St from "gi://St";

//----------------------------------------------------------------//
/* CONSTANT */

const VERSION = 1; // of the D-Bus interface (xmouse checks it)
const OBJECT_PATH = "/io/github/tsukini22/xmouse";
const INTERFACE_NAME = "io.github.tsukini22.xmouse.Shell1";
const CALLER_NAME = "xmouse"; // only this program may arm the edges (basename of /proc/<pid>/exe)
const EVDEV_OFFSET = 8; // X / Clutter key code = evdev code + 8
const WHEEL_NOTCH = 120; // 1/120 of a notch, like Qt angleDelta
const WATCHDOG_SECONDS = 3; // a capture that receives no event at all is given back (the input is never lost)

const SIDE_LEFT = 0;
const SIDE_RIGHT = 1;
const SIDE_TOP = 2;
const SIDE_BOTTOM = 3;

/*
 * SetBarriers(ai): 5 integers per edge: x1, y1, x2, y2, side (0 left, 1 right, 2 top, 3 bottom), global stage pixels
 * The events of a capture are signals sent to the caller only (unicast): no other client of the bus sees the keys
*/
const INTERFACE = `
<node>
    <interface name="${INTERFACE_NAME}">
        <method name="Version"><arg type="u" direction="out" name="version"/></method>
        <method name="GetPointer"><arg type="i" direction="out" name="x"/><arg type="i" direction="out" name="y"/></method>
        <method name="SetBarriers"><arg type="ai" direction="in" name="edges"/></method>
        <method name="Release"><arg type="i" direction="in" name="x"/><arg type="i" direction="in" name="y"/></method>
        <signal name="Crossed"><arg type="i" name="x"/><arg type="i" name="y"/><arg type="i" name="dx"/><arg type="i" name="dy"/></signal>
        <signal name="Motion"><arg type="d" name="dx"/><arg type="d" name="dy"/></signal>
        <signal name="Button"><arg type="u" name="button"/><arg type="b" name="down"/></signal>
        <signal name="Wheel"><arg type="i" name="dx"/><arg type="i" name="dy"/></signal>
        <signal name="Key"><arg type="u" name="evdev"/><arg type="b" name="down"/></signal>
        <signal name="Ended"/>
    </interface>
</node>`;

//----------------------------------------------------------------//
/* TOOLS */

// Clutter button -> xmouse MouseButton (left 0, middle 1, right 2, back 3, forward 4), -1 = ignored
function buttonOf(button) {
    switch (button) {
        case Clutter.BUTTON_PRIMARY:   return 0;
        case Clutter.BUTTON_MIDDLE:    return 1;
        case Clutter.BUTTON_SECONDARY: return 2;
        case 8:                        return 3; // back
        case 9:                        return 4; // forward
        default:                       return -1;
    }
}

// Program behind a bus name ("" when it cannot be read)
function executableOf(sender) {
    try {
        const reply = Gio.DBus.session.call_sync("org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus",
            "GetConnectionUnixProcessID", new GLib.Variant("(s)", [sender]), new GLib.VariantType("(u)"),
            Gio.DBusCallFlags.NONE, 1000, null);
        const [pid] = reply.deepUnpack();
        const path = GLib.file_read_link(`/proc/${pid}/exe`).replace(/ \(deleted\)$/, ""); // a rebuilt binary still runs
        return GLib.path_get_basename(path);
    } catch (e) {
        return "";
    }
}

//----------------------------------------------------------------//
/* SERVICE */

class XmouseService {
    constructor() {
        this._impl = Gio.DBusExportedObject.wrapJSObject(INTERFACE, this);
        this._impl.export(Gio.DBus.session, OBJECT_PATH);
        this._owner = null; // unique bus name of the xmouse that armed the edges
        this._ownerWatch = 0;
        this._barriers = []; // [{barrier, side, hitId}]
        this._actor = null; // invisible actor holding the grab
        this._grab = null;
        this._origin = [0, 0]; // where the pointer is held during a capture
        this._cursorHidden = false;
        this._events = 0; // events received by the current capture
        this._watchdog = 0;
        this._lockId = Main.sessionMode.connect("updated", () => {
            if (Main.sessionMode.isLocked) this._end(); // the lock screen takes the input back
        });
    }

    destroy() {
        this._end();
        this._clearBarriers();
        this._unwatch();
        Main.sessionMode.disconnect(this._lockId);
        this._impl.unexport();
    }

    /* methods (the *Async forms get the invocation: the caller is checked) */
    VersionAsync(params, invocation) {
        invocation.return_value(new GLib.Variant("(u)", [VERSION]));
    }

    GetPointerAsync(params, invocation) {
        if (!this._allowed(invocation)) return;
        const [x, y] = global.get_pointer();
        invocation.return_value(new GLib.Variant("(ii)", [Math.round(x), Math.round(y)]));
    }

    SetBarriersAsync(params, invocation) {
        const [edges] = params;

        if (!this._allowed(invocation)) return;
        this._watch(invocation.get_sender());
        this._clearBarriers();
        for (let i = 0; i + 4 < edges.length; i += 5)
            this._addBarrier(edges[i], edges[i + 1], edges[i + 2], edges[i + 3], edges[i + 4]);
        invocation.return_value(null);
    }

    ReleaseAsync(params, invocation) {
        const [x, y] = params;

        if (!this._allowed(invocation)) return;
        this._release(x, y);
        invocation.return_value(null);
    }

    /* access */
    _allowed(invocation) {
        const sender = invocation.get_sender();

        if (sender === this._owner) return true; // checked when it armed the edges (a bus name is never reused)
        if (this._owner !== null) {
            invocation.return_dbus_error("org.freedesktop.DBus.Error.AccessDenied", "another xmouse holds the edges");
            return false;
        }
        if (executableOf(sender) !== CALLER_NAME) {
            invocation.return_dbus_error("org.freedesktop.DBus.Error.AccessDenied", "only xmouse may use this extension");
            return false;
        }
        return true;
    }

    _watch(sender) {
        if (this._owner === sender) return;
        this._unwatch();
        this._owner = sender;
        this._ownerWatch = Gio.bus_watch_name(Gio.BusType.SESSION, sender, Gio.BusNameWatcherFlags.NONE, null, () => {
            // xmouse stopped (or crashed): the pointer is given back, the edges removed
            this._end();
            this._clearBarriers();
            this._unwatch();
        });
    }

    _unwatch() {
        if (this._ownerWatch !== 0) Gio.bus_unwatch_name(this._ownerWatch);
        this._ownerWatch = 0;
        this._owner = null;
    }

    _emit(name, signature, values) {
        if (this._owner === null) return;
        try {
            Gio.DBus.session.emit_signal(this._owner, OBJECT_PATH, INTERFACE_NAME, name, signature === null ? null : new GLib.Variant(signature, values));
        } catch (e) {
            logError(e, "xmouse: signal " + name);
        }
    }

    /* barriers */
    _addBarrier(x1, y1, x2, y2, side) {
        const directions = { // the way back inside stays open, the push outside is a hit
            [SIDE_LEFT]: Meta.BarrierDirection.POSITIVE_X,
            [SIDE_RIGHT]: Meta.BarrierDirection.NEGATIVE_X,
            [SIDE_TOP]: Meta.BarrierDirection.POSITIVE_Y,
            [SIDE_BOTTOM]: Meta.BarrierDirection.NEGATIVE_Y,
        }[side];
        const props = {x1, y1, x2, y2, directions};
        let barrier = null;

        if (directions === undefined) return;
        try {
            barrier = new Meta.Barrier({backend: global.backend, ...props}); // GNOME 46+
        } catch (e) {
            barrier = new Meta.Barrier({display: global.display, ...props}); // GNOME 45
        }
        const hitId = barrier.connect("hit", (b, event) => this._onHit(side, event));
        this._barriers.push({barrier, side, hitId});
    }

    _clearBarriers() {
        for (const entry of this._barriers) {
            entry.barrier.disconnect(entry.hitId);
            entry.barrier.destroy();
        }
        this._barriers = [];
    }

    /* capture */
    _onHit(side, event) {
        const push = {[SIDE_LEFT]: [-1, 0], [SIDE_RIGHT]: [1, 0], [SIDE_TOP]: [0, -1], [SIDE_BOTTOM]: [0, 1]}[side];

        if (this._grab !== null || this._owner === null || Main.sessionMode.isLocked) return;
        this._origin = [Math.round(event.x), Math.round(event.y)];
        this._actor = new St.Widget({reactive: true, can_focus: true, opacity: 0, x: 0, y: 0, width: global.stage.width, height: global.stage.height});
        Main.uiGroup.add_child(this._actor);
        this._grab = Main.pushModal(this._actor, {actionMode: Shell.ActionMode.NONE}); // a stage grab always succeeds (GNOME 50): the watchdog below covers a grab that gets no event
        this._actor.connect("event", (actor, ev) => this._onEvent(ev));
        global.backend.get_cursor_tracker().inhibit_cursor_visibility();
        this._cursorHidden = true;
        this._events = 0;
        this._watchdog = GLib.timeout_add_seconds(GLib.PRIORITY_DEFAULT, WATCHDOG_SECONDS, () => {
            this._watchdog = 0;
            if (this._events === 0) this._end(); // the events do not reach the grab: the pointer and the keyboard come back
            return GLib.SOURCE_REMOVE;
        });
        this._emit("Crossed", "(iiii)", [this._origin[0], this._origin[1], push[0], push[1]]);
    }

    _onEvent(ev) {
        const type = ev.type();

        this._events += 1;
        switch (type) {
            case Clutter.EventType.MOTION: this._onMotion(ev); break;
            case Clutter.EventType.BUTTON_PRESS:
            case Clutter.EventType.BUTTON_RELEASE: {
                const button = buttonOf(ev.get_button());
                if (button >= 0) this._emit("Button", "(ub)", [button, type === Clutter.EventType.BUTTON_PRESS]);
                break;
            }
            case Clutter.EventType.SCROLL: this._onScroll(ev); break;
            case Clutter.EventType.KEY_PRESS:
            case Clutter.EventType.KEY_RELEASE: this._onKey(ev, type === Clutter.EventType.KEY_PRESS); break;
            default: break;
        }
        return Clutter.EVENT_STOP;
    }

    _onMotion(ev) {
        const [x, y] = ev.get_coords();
        let dx = x - this._origin[0];
        let dy = y - this._origin[1];

        // The device move itself when Clutter gives it (no acceleration lost on the border), else the distance to the held point
        try {
            const relative = ev.get_relative_motion();
            if (relative[0]) {dx = relative[1]; dy = relative[2];}
        } catch (e) {
            // older Clutter: the absolute distance is used
        }
        if (dx !== 0 || dy !== 0) this._emit("Motion", "(dd)", [dx, dy]);
        if (x !== this._origin[0] || y !== this._origin[1]) global.backend.get_default_seat().warp_pointer(this._origin[0], this._origin[1]);
    }

    _onScroll(ev) {
        if (ev.get_flags() & Clutter.EventFlags.FLAG_POINTER_EMULATED) return; // the same scroll arrives as smooth: once
        switch (ev.get_scroll_direction()) {
            case Clutter.ScrollDirection.UP:    this._emit("Wheel", "(ii)", [0, WHEEL_NOTCH]);  break;
            case Clutter.ScrollDirection.DOWN:  this._emit("Wheel", "(ii)", [0, -WHEEL_NOTCH]); break;
            case Clutter.ScrollDirection.LEFT:  this._emit("Wheel", "(ii)", [WHEEL_NOTCH, 0]);  break;
            case Clutter.ScrollDirection.RIGHT: this._emit("Wheel", "(ii)", [-WHEEL_NOTCH, 0]); break;
            case Clutter.ScrollDirection.SMOOTH: {
                const [sx, sy] = ev.get_scroll_delta();
                const dx = Math.round(-sx * WHEEL_NOTCH);
                const dy = Math.round(-sy * WHEEL_NOTCH);
                if (dx !== 0 || dy !== 0) this._emit("Wheel", "(ii)", [dx, dy]);
                break;
            }
            default: break;
        }
    }

    _onKey(ev, down) {
        const state = ev.get_state();
        const escape = Clutter.ModifierType.CONTROL_MASK | Clutter.ModifierType.MOD1_MASK | Clutter.ModifierType.SHIFT_MASK;

        // Ctrl + Alt + Shift: the pointer comes back even when xmouse does not answer anymore
        if (down && (state & escape) === escape) {
            this._end();
            return;
        }
        this._emit("Key", "(ub)", [ev.get_key_code() - EVDEV_OFFSET, down]);
    }

    _release(x, y) {
        if (this._watchdog !== 0) GLib.source_remove(this._watchdog);
        this._watchdog = 0;
        if (this._grab === null) return;
        Main.popModal(this._grab);
        this._grab = null;
        if (this._actor !== null) this._actor.destroy();
        this._actor = null;
        if (this._cursorHidden) global.backend.get_cursor_tracker().uninhibit_cursor_visibility();
        this._cursorHidden = false;
        global.backend.get_default_seat().warp_pointer(x, y);
    }

    // Capture ended here (lock screen, escape, xmouse gone): xmouse is told
    _end() {
        if (this._grab === null) return;
        this._release(this._origin[0], this._origin[1]);
        this._emit("Ended", null, null);
    }
}

//----------------------------------------------------------------//
/* EXTENSION */

export default class XmouseExtension extends Extension {
    enable() {
        this._service = new XmouseService();
    }

    disable() {
        this._service.destroy();
        this._service = null;
    }
}
