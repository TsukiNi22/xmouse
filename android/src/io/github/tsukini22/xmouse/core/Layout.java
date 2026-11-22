/*
 * The screens of every member in one global space (core::Layout of the desktop): where a global point lands, the moves
 * that slide along the borders, the default placement (the same on every member)
*/
package io.github.tsukini22.xmouse.core;

import java.util.ArrayList;
import java.util.Collections;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

public final class Layout {
    public static final class Rect {
        public final int x;
        public final int y;
        public final int w;
        public final int h;

        public Rect(int x, int y, int w, int h) {this.x = x; this.y = y; this.w = w; this.h = h;}

        public int right() {return this.x + this.w - 1;}

        public int bottom() {return this.y + this.h - 1;}

        public boolean contains(int px, int py) {return px >= this.x && px <= this.right() && py >= this.y && py <= this.bottom();}

        public boolean empty() {return this.w <= 0 || this.h <= 0;}

        Rect united(Rect o) {
            if (this.empty()) return o;
            if (o.empty()) return this;
            int x0 = Math.min(this.x, o.x);
            int y0 = Math.min(this.y, o.y);
            return new Rect(x0, y0, Math.max(this.x + this.w, o.x + o.w) - x0, Math.max(this.y + this.h, o.y + o.h) - y0);
        }

        Rect translated(int dx, int dy) {return new Rect(this.x + dx, this.y + dy, this.w, this.h);}
    }

    public static final class Device {
        public String id = "";
        public String name = "";
        public final List<Rect> screens = new ArrayList<>(); // logical pixels in the desktop of the device
        public int px;
        public int py; // top left in the global space
        public boolean placed;
    }

    public static final class Hit {
        public final String device;
        public final int x;
        public final int y; // local (desktop pixels of the device)

        Hit(String device, int x, int y) {this.device = device; this.x = x; this.y = y;}
    }

    private final Map<String, Device> _devices = new LinkedHashMap<>();

    // ---------------------------------------------------------------- //
    // Building (from the state: device/<id> screens, layout/<id> position)

    public static Layout fromState(StateStore state) {
        Layout layout = new Layout();
        for (String id: state.ids("device/")) {
            if (state.contains("removed/" + id)) continue;
            Map<String, Object> device = Json.map(state.value("device/" + id));
            Map<String, Object> position = Json.map(state.value("layout/" + id));
            Device d = new Device();
            d.id = id;
            d.name = Json.str(device, "name", "");
            for (Object s: Json.list(device.get("screens"))) {
                Map<String, Object> o = Json.map(s);
                Rect r = new Rect((int) Json.num(o, "x", 0), (int) Json.num(o, "y", 0), (int) Json.num(o, "w", 0), (int) Json.num(o, "h", 0));
                if (r.w < 16 || r.h < 16 || r.w > 32768 || r.h > 32768 || d.screens.size() == 16) continue;
                d.screens.add(r);
            }
            if (position.containsKey("x") && position.containsKey("y")) {
                d.px = (int) Json.num(position, "x", 0);
                d.py = (int) Json.num(position, "y", 0);
                d.placed = true;
            }
            layout._devices.put(id, d);
        }
        layout.placeUnplaced();
        return layout;
    }

    private List<String> sortedIds() {
        List<String> ids = new ArrayList<>(this._devices.keySet());
        Collections.sort(ids);
        return ids;
    }

    private void placeUnplaced() {
        List<String> ids = this.sortedIds();
        for (String id: ids) {
            Device d = this._devices.get(id);
            int right = Integer.MIN_VALUE;
            int top = 0;
            if (d.placed || d.screens.isEmpty()) continue;
            for (String other: ids) {
                Device o = this._devices.get(other);
                if (other.equals(id) || !o.placed || o.screens.isEmpty()) continue;
                Rect r = this.globalBounds(other);
                if (r.x + r.w > right) {right = r.x + r.w; top = r.y;}
            }
            d.px = right == Integer.MIN_VALUE ? 0 : right;
            d.py = right == Integer.MIN_VALUE ? 0 : top;
            d.placed = true;
        }
    }

    // ---------------------------------------------------------------- //
    // Queries

    public Map<String, Device> devices() {return this._devices;}

    public Device device(String id) {return this._devices.get(id);}

    public Rect bounds(String id) {
        Device d = this._devices.get(id);
        Rect box = new Rect(0, 0, 0, 0);
        if (d == null) return box;
        for (Rect s: d.screens) box = box.united(s);
        return box;
    }

    public Rect globalRect(String id, int screen) {
        Device d = this._devices.get(id);
        if (d == null || screen < 0 || screen >= d.screens.size()) return new Rect(0, 0, 0, 0);
        Rect b = this.bounds(id);
        return d.screens.get(screen).translated(d.px - b.x, d.py - b.y);
    }

    public Rect globalBounds(String id) {
        Device d = this._devices.get(id);
        Rect b = this.bounds(id);
        return d == null ? b : new Rect(d.px, d.py, b.w, b.h);
    }

    public int[] toGlobal(String id, int x, int y) {
        Device d = this._devices.get(id);
        if (d == null) return new int[]{x, y};
        Rect b = this.bounds(id);
        return new int[]{x - b.x + d.px, y - b.y + d.py};
    }

    public Hit hit(int gx, int gy) {
        for (String id: this.sortedIds()) {
            Device d = this._devices.get(id);
            if (!d.placed) continue;
            for (int i = 0; i < d.screens.size(); i++) {
                Rect g = this.globalRect(id, i);
                if (!g.contains(gx, gy)) continue;
                Rect s = d.screens.get(i);
                return new Hit(id, s.x + gx - g.x, s.y + gy - g.y);
            }
        }
        return null;
    }

    public int[] clampTo(String id, int gx, int gy) {
        Device d = this._devices.get(id);
        int[] best = {gx, gy};
        long bestDistance = Long.MAX_VALUE;
        if (d == null) return best;
        for (int i = 0; i < d.screens.size(); i++) {
            Rect g = this.globalRect(id, i);
            int px = Math.max(g.x, Math.min(gx, g.right()));
            int py = Math.max(g.y, Math.min(gy, g.bottom()));
            long dx = px - gx;
            long dy = py - gy;
            if (dx * dx + dy * dy < bestDistance) {bestDistance = dx * dx + dy * dy; best = new int[]{px, py};}
        }
        return best;
    }

    // Slides along the borders, never leaves the screens
    public int[] move(int fx, int fy, int dx, int dy) {
        int tx = fx + dx;
        int ty = fy + dy;
        Hit start = this.hit(fx, fy);
        if (this.hit(tx, ty) != null) return new int[]{tx, ty};
        if (dx != 0 && dy != 0) {
            if (this.hit(tx, fy) != null) return new int[]{tx, fy};
            if (this.hit(fx, ty) != null) return new int[]{fx, ty};
        }
        if (start == null) return this._devices.isEmpty() ? new int[]{fx, fy} : this.clampTo(this._devices.keySet().iterator().next(), fx, fy);
        for (int i = 0; i < this._devices.get(start.device).screens.size(); i++) {
            Rect g = this.globalRect(start.device, i);
            if (!g.contains(fx, fy)) continue;
            return new int[]{Math.max(g.x, Math.min(tx, g.right())), Math.max(g.y, Math.min(ty, g.bottom()))};
        }
        return new int[]{fx, fy};
    }
}
