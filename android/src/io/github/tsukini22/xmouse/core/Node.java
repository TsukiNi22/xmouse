/*
 * The phone as a member of the network (core::Hub of the desktop, without the desktop parts): its identity and network,
 * the replicated state, the links (Mesh), the one-time code and the approval of a new device, its screen in the layout
 * (the foreign cursors given to the screen as events), its own cursor driven by the touchpad of the app (it crosses the
 * borders to the other devices), the presence. Everything runs on one thread (loop); the listener is called from it.
*/
package io.github.tsukini22.xmouse.core;

import java.io.ByteArrayOutputStream;
import java.net.URI;
import java.nio.charset.StandardCharsets;
import java.security.SecureRandom;
import java.security.cert.X509Certificate;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.Executor;
import java.util.concurrent.Executors;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.TimeUnit;

import javax.net.ssl.SSLContext;
import javax.net.ssl.TrustManager;
import javax.net.ssl.X509TrustManager;

public final class Node implements Link.Context, Mesh.Events {
    public interface Listener {
        void changed(); // network, members, code, layout
        void joinRequest(String pending, String name, String os); // accept / refuse a new device (approve)
        void joinResult(boolean ok, String message);
        void notice(String text);
        void screen(Map<String, Object> event); // a cursor on the screen of the phone: enter / pointer / button / wheel / key / text / leave
        void map(Map<String, Object> map); // devices and cursors for the touchpad page
        void clipboard(String text); // a text copied on a member
        void link(String from, String url); // a link sent by a member
    }

    public static final class Member {
        public String id = "";
        public String name = "";
        public String os = "";
        public String kind = "desktop";
        public boolean self;
        public boolean online;
        public int rtt = -1;
    }

    /* binary messages (net::Binary of the desktop) */
    private static final int POINTER = 0x20;
    private static final int BUTTON = 0x21;
    private static final int WHEEL = 0x22;
    private static final int KEY = 0x23;
    private static final int TEXT = 0x24;
    private static final int PRESENCE = 0x30;
    private static final int RELAY = 0x50;
    private static final int PRESENCE_DOWN = 0x01;
    private static final int PRESENCE_HIDDEN = 0x02;
    private static final int PRESENCE_PING = 0x04;
    private static final int PRESENCE_LASER = 0x08;
    private static final long JOIN_TIMEOUT = 30000; // ms the person has to accept a new device
    private static final int CODE_LENGTH = 9;
    private static final long CODE_LIFETIME = 300000; // ms

    private final ScheduledExecutorService _loop = Executors.newSingleThreadScheduledExecutor(r -> {Thread t = new Thread(r, "xmouse-node"); t.setDaemon(true); return t;});
    private final Network _net;
    private final Network.Store _stateStore;
    private final StateStore _state = new StateStore();
    private final SSLContext _server;
    private final byte[] _fingerprint;
    private final String _version;
    private Listener _listener;
    private Mesh _mesh;
    private Layout _layout = new Layout();
    private String _name;
    private int _wantedPort;

    /* one-time code */
    private String _code = "";
    private byte[] _salt = new byte[0];
    private byte[] _codeKey = new byte[0];
    private long _expires = 0;
    private final Map<String, Link> _approvals = new HashMap<>();

    /* screen of the phone (lent to the layout) */
    private int _screenW = 0;
    private int _screenH = 0;
    private double _scale = 1;
    private final Set<String> _onScreen = new HashSet<>(); // foreign cursors on it

    /* my cursor (the touchpad of the app) */
    private boolean _away = false;
    private String _target = "";
    private double _gx = 0;
    private double _gy = 0;
    private int _tx = 0;
    private int _ty = 0;
    private boolean _moveDirty = false;
    private int _seq = 0;
    private int _buttons = 0;
    private final Set<String> _keys = new HashSet<>();
    private long _pingUntil = 0;
    private boolean _laser = false;
    private boolean _presenceDirty = false;
    private boolean _homeShown = false; // my cursor is drawn on the screen of the phone
    private final Map<String, long[]> _presence = new HashMap<>(); // id -> {x, y, flags, seen}
    private final Map<String, String> _presenceOn = new HashMap<>();
    private int _ticks = 0;
    private int _flushes = 0; // 8 ms steps: the presence every 6 (48 ms), the map of the pad every 12 (96 ms)

    public Node(Network.Store networkStore, Network.Store stateStore, SSLContext server, byte[] fingerprint, String name, String version) {
        this._net = new Network(networkStore);
        this._stateStore = stateStore;
        this._server = server;
        this._fingerprint = fingerprint;
        this._name = name;
        this._version = version;
    }

    // ---------------------------------------------------------------- //
    // Life cycle

    public void start(int port, Listener listener) {
        this._listener = listener;
        this._wantedPort = port;
        this._loop.execute(() -> {
            try {
                this._net.load();
                this._state.setSelf(this._net.self());
                String saved = this._stateStore.load();
                if (this._net.joined() && !saved.isEmpty()) this._state.merge(Json.object(saved));
                this._mesh = new Mesh(this, this, this._server, trustAll());
                this._mesh.start(port);
                this.rotateCode();
                this.publishSelf();
                this.rebuild();
            } catch (Exception e) {
                this._listener.notice("xmouse cannot start: " + e.getMessage());
            }
        });
        this._loop.scheduleWithFixedDelay(this::tick, 1, 1, TimeUnit.SECONDS);
        this._loop.scheduleWithFixedDelay(this::flush, 8, 8, TimeUnit.MILLISECONDS);
    }

    public void stop() {
        this._loop.execute(() -> {
            if (this._away) this.goHome();
            if (this._mesh != null) this._mesh.stop();
            this.save();
        });
        this._loop.shutdown();
    }

    public Executor executor() {return this._loop;}

    private static SSLContext trustAll() throws Exception {
        // The identity of a member is its pinned fingerprint (checked by the link), not a certificate authority
        TrustManager any = new X509TrustManager() {
            @Override public void checkClientTrusted(X509Certificate[] chain, String auth) { /* no client certificate */ }
            @Override public void checkServerTrusted(X509Certificate[] chain, String auth) { /* checked by the link: pinned / expected fingerprint */ }
            @Override public X509Certificate[] getAcceptedIssuers() {return new X509Certificate[0];}
        };
        SSLContext ctx = SSLContext.getInstance("TLS");
        ctx.init(null, new TrustManager[]{any}, new SecureRandom());
        return ctx;
    }

    private void tick() {
        long now = System.currentTimeMillis();
        try {
            if (this._mesh == null) return;
            this._mesh.tick(now);
            if (now >= this._expires && !this._code.isEmpty()) this.rotateCode();
            for (Map.Entry<String, long[]> it: new ArrayList<>(this._presence.entrySet()))
                if (!it.getKey().equals(this._net.self()) && now - it.getValue()[3] > 10000) this._presence.remove(it.getKey());
            if (++this._ticks % 30 == 0) this.publishSelf(); // addresses (wifi changed)
            this._listener.changed();
        } catch (Exception e) {
            this._listener.notice("xmouse: " + e.getMessage());
        }
    }

    // ---------------------------------------------------------------- //
    // Link.Context

    @Override public Network network() {return this._net;}

    @Override public Executor loop() {return this._loop;}

    @Override public byte[] fingerprint() {return this._fingerprint;}

    @Override public String name() {return this._name;}

    @Override public String os() {return "android";}

    @Override public String kind() {return "phone";}

    @Override public String version() {return this._version;}

    @Override public int port() {return this._mesh == null ? this._wantedPort : this._mesh.port();}

    @Override public byte[] codeSalt() {return this._codeKey.length == 32 ? this._salt : new byte[0];}

    @Override public byte[] codeKey() {return this._codeKey;}

    @Override public void codeUsed() {this.rotateCode();}

    @Override
    public String memberCheck(String id) {
        if (!this._net.joined()) return "not in a network";
        if (this._state.contains("removed/" + id)) return "removed from the network";
        if (!this._state.contains("device/" + id)) return "unknown member (not in the network yet)";
        if (!Json.str(Json.map(this._state.value("device/" + id)), "via", "").isEmpty()) return "a phone / pad links through its device only";
        return "";
    }

    @Override public byte[] pinned(String id) {return Crypto.unhex(Json.str(Json.map(this._state.value("device/" + id)), "fp", ""));}

    @Override public Map<String, Object> stateJson() {return this._state.toJson();}

    // ---------------------------------------------------------------- //
    // One-time code (Argon2id off the loop: 64 MiB, about a second on a phone)

    private void rotateCode() {
        String code = Crypto.randomCode(CODE_LENGTH);
        byte[] salt = Crypto.random(16);
        this._codeKey = new byte[0]; // no join while the new key is computed
        this._code = code;
        this._salt = salt;
        this._expires = System.currentTimeMillis() + CODE_LIFETIME;
        Thread t = new Thread(() -> {
            byte[] key = Crypto.codeKey(code, salt);
            this._loop.execute(() -> {if (this._code.equals(code)) this._codeKey = key;});
        }, "xmouse-code");
        t.setDaemon(true);
        t.start();
    }

    public void newCode() {this._loop.execute(this::rotateCode);}

    public String code() {return Crypto.displayCode(this._code);}

    public int codeSecondsLeft() {return (int) Math.max(0, (this._expires - System.currentTimeMillis()) / 1000);}

    // https://<address>:<port>/?code=..&fp=..&net=.. (a member pastes it in Connect to a network)
    public String joinLink() {
        List<String> addresses = Mesh.localAddresses();
        String host = addresses.isEmpty() ? "127.0.0.1" : addresses.get(0);
        return "https://" + host + ":" + this.port() + "/?code=" + this.code() + "&fp=" + Crypto.hex(this._fingerprint) + "&net=" + this._net.name().replace("&", "").replace(" ", "%20");
    }

    // ---------------------------------------------------------------- //
    // State

    private void save() {
        if (this._net.joined()) this._stateStore.save(Json.write(this._state.toJson()));
        else this._stateStore.save("");
    }

    private void broadcastDelta(Map<String, Object> delta) {
        if (delta.isEmpty()) return;
        if (this._mesh != null) this._mesh.broadcast(Json.obj("t", "state", "d", delta), "");
        this.save();
    }

    private void publishSelf() {
        String key = "device/" + this._net.self();
        Map<String, Object> old = Json.map(this._state.value(key));
        List<Object> addresses = new ArrayList<>(Mesh.localAddresses());
        List<Object> screens = new ArrayList<>();
        if (!this._net.joined()) return;
        if (this._screenW >= 100 && this._screenH >= 100) screens.add(Json.obj("name", "screen", "x", 0, "y", 0, "w", this._screenW, "h", this._screenH, "scale", this._scale));
        Map<String, Object> entry = Json.obj("name", this._name, "os", "android", "kind", "phone", "version", this._version, "fp", Crypto.hex(this._fingerprint), "pq", false,
            "port", this.port(), "addresses", addresses, "screens", screens, "links", new ArrayList<Object>(this._mesh == null ? new ArrayList<String>() : this._mesh.ids()),
            "relay", false, "udp", false, "windows", false, "joined", old.containsKey("joined") ? old.get("joined") : Long.toString(System.currentTimeMillis()));
        this.broadcastDelta(this._state.set(key, entry));
        this.rebuild();
    }

    private void rebuild() {
        String self = this._net.self();
        this._layout = Layout.fromState(this._state);
        if (this._net.joined() && this._layout.device(self) != null && !this._layout.device(self).screens.isEmpty() && !this._state.contains("layout/" + self)) {
            Layout.Device d = this._layout.device(self);
            this.broadcastDelta(this._state.set("layout/" + self, Json.obj("x", d.px, "y", d.py)));
        }
        if (this._mesh == null) return;
        for (String id: this._state.ids("device/")) {
            Map<String, Object> device = Json.map(this._state.value("device/" + id));
            List<String> addresses = new ArrayList<>();
            long port = Json.num(device, "port", 47900);
            if (id.equals(self)) continue;
            if (this._state.contains("removed/" + id) || !Json.str(device, "via", "").isEmpty()) {this._mesh.setCandidates(id, new ArrayList<>()); continue;}
            for (Object a: Json.list(device.get("addresses"))) {
                String text = a instanceof String ? (String) a : "";
                if (text.isEmpty()) continue;
                addresses.add(text.indexOf(':') > 0 && text.indexOf(':') == text.lastIndexOf(':') ? text : text + ":" + port);
            }
            this._mesh.setCandidates(id, addresses);
        }
    }

    private void applyDelta(Map<String, Object> delta, String from) {
        List<String> changed = this._state.merge(delta);
        Map<String, Object> forward = Json.obj();
        if (changed.isEmpty()) return;
        for (String key: changed) forward.put(key, delta.get(key));
        this._mesh.broadcast(Json.obj("t", "state", "d", forward), from); // gossip
        this.save();
        if (changed.contains("removed/" + this._net.self())) {
            this._listener.notice("This phone was removed from the network");
            this.leave();
            return;
        }
        for (String key: changed) {
            if (!key.startsWith("removed/") || !this._state.contains(key)) continue;
            this._mesh.drop(key.substring(8), "removed from the network");
        }
        this.rebuild();
        this._listener.changed();
    }

    // ---------------------------------------------------------------- //
    // Network

    public void createNetwork(String name) {
        this._loop.execute(() -> {
            String label = name.trim().isEmpty() ? this._name + " network" : name.trim();
            if (this._net.joined()) this.leave();
            this._net.create(label.length() > 64 ? label.substring(0, 64) : label);
            this._state.clear();
            this._state.setSelf(this._net.self());
            this._state.set("net/name", this._net.name());
            this._state.set("net/windowMove", true);
            this.publishSelf();
            this.save();
            this._listener.changed();
        });
    }

    // "https://host:port/?code=..&fp=.." (QR code / link of a member) or "host[:port]" with the code apart
    public void join(String link, String code) {
        this._loop.execute(() -> {
            String host;
            int port = 47900;
            String pin = code == null ? "" : code;
            byte[] expected = new byte[0];
            try {
                String text = link.trim();
                if (text.startsWith("https://") || text.startsWith("xmouse://")) {
                    URI uri = new URI(text);
                    host = uri.getHost();
                    if (uri.getPort() > 0) port = uri.getPort();
                    for (String pair: (uri.getRawQuery() == null ? "" : uri.getRawQuery()).split("&")) {
                        int eq = pair.indexOf('=');
                        if (eq <= 0) continue;
                        String k = pair.substring(0, eq);
                        String v = java.net.URLDecoder.decode(pair.substring(eq + 1), "UTF-8");
                        if (k.equals("code") && pin.isEmpty()) pin = v;
                        if (k.equals("fp")) expected = Crypto.unhex(v);
                    }
                } else if (text.indexOf(':') > 0) {
                    host = text.substring(0, text.lastIndexOf(':'));
                    port = Integer.parseInt(text.substring(text.lastIndexOf(':') + 1));
                } else host = text;
            } catch (Exception e) {
                this._listener.joinResult(false, "invalid link");
                return;
            }
            if (host == null || host.isEmpty()) {this._listener.joinResult(false, "invalid address"); return;}
            if (Crypto.normalizeCode(pin).length() < 6) {this._listener.joinResult(false, "the code shown on the other device is needed"); return;}
            if (this._net.joined()) this.leave();
            this._mesh.join(host, port, pin, expected.length == 32 ? expected : new byte[0]);
        });
    }

    @Override
    public void onJoined(Map<String, Object> welcome, String through) {
        Map<String, Object> g = Json.map(welcome.get("group"));
        try {
            this._net.adopt(Json.str(g, "id", ""), Json.str(g, "name", ""), Crypto.unb64(Json.str(g, "key", "")), (int) Json.num(g, "epoch", 1));
        } catch (Exception e) {
            this._listener.joinResult(false, e.getMessage());
            return;
        }
        this._state.clear();
        this._state.setSelf(this._net.self());
        this._state.merge(Json.map(welcome.get("state")));
        this.publishSelf();
        this.save();
        this._listener.joinResult(true, "Joined " + this._net.name());
        this._listener.changed();
    }

    @Override public void onJoinFailed(String reason) {this._listener.joinResult(false, reason);}

    public void leave() {
        Runnable run = () -> {
            if (!this._net.joined()) return;
            if (this._away) this.goHome();
            this.broadcastDelta(this._state.set("removed/" + this._net.self(), true));
            this._mesh.dropAll("left the network");
            this._net.leave();
            this._state.clear();
            this._presence.clear();
            this.save();
            this.rebuild();
            this._listener.changed();
        };
        if (Thread.currentThread().getName().equals("xmouse-node")) run.run();
        else this._loop.execute(run);
    }

    public void removeMember(String id) {
        this._loop.execute(() -> {
            if (id.equals(this._net.self())) {this.leave(); return;}
            if (!this._net.joined() || !this._state.contains("device/" + id)) return;
            this.broadcastDelta(this._state.set("removed/" + id, true));
            this.broadcastDelta(this._state.remove("layout/" + id));
            byte[] key = Crypto.random(Network.KEY_SIZE);
            this._net.rekey(key, this._net.epoch() + 1);
            for (String peer: this._mesh.ids())
                if (!peer.equals(id)) this._mesh.send(peer, Json.obj("t", "rekey", "key", Crypto.b64(key), "epoch", this._net.epoch()));
            this._mesh.drop(id, "removed from the network");
            this.rebuild();
            this._listener.changed();
        });
    }

    public void rename(String name) {
        this._loop.execute(() -> {
            this._name = name.trim().isEmpty() ? this._name : name.trim();
            this.publishSelf();
        });
    }

    @Override
    public void onJoinRequest(Link link) {
        String pending = Crypto.randomId(6);
        this._approvals.put(pending, link);
        this._loop.schedule(() -> this.approve(pending, false), JOIN_TIMEOUT, TimeUnit.MILLISECONDS); // nobody answered: refused
        this._listener.joinRequest(pending, link.name(), link.os());
    }

    public void approve(String pending, boolean accepted) {
        Runnable run = () -> {
            Link link = this._approvals.remove(pending);
            if (link != null) link.approve(accepted);
        };
        if (Thread.currentThread().getName().equals("xmouse-node")) run.run();
        else this._loop.execute(run);
    }

    // ---------------------------------------------------------------- //
    // Members (read from the UI thread: copies)

    public boolean joined() {return this._net.joined();}

    public String networkName() {
        Object v = this._state.value("net/name");
        return v instanceof String ? (String) v : this._net.name();
    }

    public List<Member> members() {
        List<Member> out = new ArrayList<>();
        for (String id: this._state.ids("device/")) {
            Map<String, Object> d = Json.map(this._state.value("device/" + id));
            Member m = new Member();
            if (this._state.contains("removed/" + id)) continue;
            m.id = id;
            m.name = Json.str(d, "name", id);
            m.os = Json.str(d, "os", "");
            m.kind = Json.str(d, "kind", "desktop");
            m.self = id.equals(this._net.self());
            m.online = m.self || (this._mesh != null && this._mesh.online(id));
            Link l = this._mesh == null ? null : this._mesh.link(id);
            m.rtt = l == null ? -1 : l.rtt();
            out.add(m);
        }
        Collections.sort(out, (a, b) -> a.self != b.self ? (a.self ? -1 : 1) : a.name.compareToIgnoreCase(b.name));
        return out;
    }

    // A computer of the network linked now (its address and its pinned identity): the app is updated from it
    public String[] updateSource() {
        try {
            for (String id: this._mesh.ids()) {
                Map<String, Object> d = Json.map(this._state.value("device/" + id));
                Link l = this._mesh.link(id);
                if (l == null || !Json.str(d, "kind", "desktop").equals("desktop") || Json.str(d, "fp", "").isEmpty()) continue;
                return new String[]{"https://" + l.address() + ":" + Json.num(d, "port", 47900), Json.str(d, "fp", "")};
            }
        } catch (Exception e) {
            // the links changed meanwhile: next time
        }
        return null;
    }

    // ---------------------------------------------------------------- //
    // Screen of the phone

    // The size of the screen (logical pixels, dp) lent to the layout, 0 = not lent
    public void setScreen(int w, int h, double scale) {
        this._loop.execute(() -> {
            this._screenW = w;
            this._screenH = h;
            this._scale = scale;
            if (w == 0) for (String id: new ArrayList<>(this._onScreen)) this._mesh.send(id, Json.obj("t", "kick"));
            this.publishSelf();
        });
    }

    // The screen refuses a cursor (phantom mode tells it why its clicks do nothing)
    public void tell(String id, String text) {this._loop.execute(() -> this._mesh.send(id, Json.obj("t", "access", "text", text)));}

    // ---------------------------------------------------------------- //
    // Messages of the members

    @Override
    public void onPeerReady(String id) {
        this._mesh.send(id, Json.obj("t", "state", "d", this._state.toJson()));
        if (this._away && this._target.equals(id)) this._mesh.send(id, Json.obj("t", "enter", "x", this._tx, "y", this._ty, "name", this._name));
        this.publishSelf(); // links
        this._listener.changed();
    }

    @Override
    public void onPeerLost(String id, String reason) {
        if (this._away && this._target.equals(id)) this.goHome();
        if (this._onScreen.remove(id)) this._listener.screen(Json.obj("t", "leave", "id", id));
        this._presence.remove(id);
        this.publishSelf();
        this._listener.changed();
    }

    @Override
    public void onMessage(String id, Map<String, Object> o) {this.message(id, o, true);}

    private void message(String id, Map<String, Object> o, boolean direct) {
        String type = Json.str(o, "t", "");
        switch (type) {
            case "relay": {
                if (!direct || !Json.str(o, "to", "").equals(this._net.self())) return; // this phone carries nothing for the others
                String from = Json.str(o, "from", "");
                if (this._state.contains("device/" + from) && !this._state.contains("removed/" + from)) this.message(from, Json.map(o.get("m")), false);
                return;
            }
            case "state": if (direct) this.applyDelta(Json.map(o.get("d")), id); return;
            case "rekey": if (direct) this._net.rekey(Crypto.unb64(Json.str(o, "key", "")), (int) Json.num(o, "epoch", 0)); return;
            case "enter": {
                if (this._screenW == 0) {this._mesh.send(id, Json.obj("t", "deny", "reason", "no screen lent on this phone")); return;}
                this._onScreen.add(id);
                this._listener.screen(Json.obj("t", "enter", "id", id, "name", this.nameOf(id, Json.str(o, "name", "")), "color", colorOf(id), "x", Json.num(o, "x", 0), "y", Json.num(o, "y", 0)));
                return;
            }
            case "leave": if (this._onScreen.remove(id)) this._listener.screen(Json.obj("t", "leave", "id", id)); return;
            case "deny":
            case "kick":
                if (this._away && this._target.equals(id)) {
                    if (type.equals("deny")) this._listener.notice(this.nameOf(id, id) + " refused the cursor: " + Json.str(o, "reason", ""));
                    this.goHome();
                }
                return;
            case "access": this._listener.notice(this.nameOf(id, id) + ": " + Json.str(o, "text", "")); return;
            case "clip":
                if (Json.str(o, "mime", "").equals("text/plain")) this._listener.clipboard(new String(Crypto.unb64(Json.str(o, "data", "")), StandardCharsets.UTF_8));
                return;
            case "link": this._listener.link(this.nameOf(id, id), Json.str(o, "url", "")); return;
            case "files": this._mesh.send(id, Json.obj("t", "fend", "offer", Json.str(o, "offer", ""), "error", "a phone does not take files yet")); return;
            case "wopen": this._mesh.send(id, Json.obj("t", "wclose", "s", Json.str(o, "s", ""), "reason", "a phone does not show windows")); return;
            default: return;
        }
    }

    @Override
    public void onBinary(String id, byte[] data) {this.binary(id, data);}

    private void binary(String id, byte[] data) {
        Reader r = new Reader(data);
        int type = data[0] & 0xff;
        switch (type) {
            case POINTER: {
                r.u16();
                int x = r.i32();
                int y = r.i32();
                if (r.ok && this._onScreen.contains(id)) this._listener.screen(Json.obj("t", "pointer", "id", id, "x", x, "y", y));
                return;
            }
            case BUTTON: {
                int b = r.u8();
                boolean down = r.u8() != 0;
                Map<String, Object> e = Json.obj("t", "button", "id", id, "b", b, "down", down);
                if (r.remaining() >= 8) {e.put("x", r.i32()); e.put("y", r.i32());}
                if (r.ok && this._onScreen.contains(id)) this._listener.screen(e);
                return;
            }
            case WHEEL: {
                int dx = (short) r.u16();
                int dy = (short) r.u16();
                if (r.ok && this._onScreen.contains(id)) this._listener.screen(Json.obj("t", "wheel", "id", id, "dx", dx, "dy", dy));
                return;
            }
            case KEY: {
                boolean down = r.u8() != 0;
                String code = r.text8();
                if (r.ok && this._onScreen.contains(id)) this._listener.screen(Json.obj("t", "key", "id", id, "code", code, "down", down));
                return;
            }
            case TEXT:
                if (this._onScreen.contains(id)) this._listener.screen(Json.obj("t", "text", "id", id, "text", new String(data, 1, data.length - 1, StandardCharsets.UTF_8)));
                return;
            case PRESENCE: {
                r.u16();
                String on = r.text8();
                int x = r.i32();
                int y = r.i32();
                int flags = r.u8();
                if (!r.ok) return;
                int[] local = this._layout.toLocal(on, x, y); // kept on its device: it follows the device moved on the layout
                this._presence.put(id, new long[]{local[0], local[1], flags, System.currentTimeMillis()});
                this._presenceOn.put(id, on);
                return;
            }
            case RELAY: {
                String to = r.text8();
                String from = r.text8();
                r.u8();
                byte[] inner = r.rest();
                if (!r.ok || inner.length == 0 || !to.equals(this._net.self()) || (inner[0] & 0xff) == RELAY) return;
                if (this._state.contains("device/" + from) && !this._state.contains("removed/" + from)) this.binary(from, inner);
                return;
            }
            default:
        }
    }

    private String nameOf(String id, String fallback) {
        Layout.Device d = this._layout.device(id);
        return d == null || d.name.isEmpty() ? fallback : d.name;
    }

    // The stable color of a member (the hue of the desktop: sha256 of the id)
    public static String colorOf(String id) {
        byte[] h = Crypto.sha256(id.getBytes(StandardCharsets.UTF_8));
        int hue = ((h[0] & 0xff) * 256 + (h[1] & 0xff)) % 360;
        float s = 190f / 255f;
        float l = 120f / 255f;
        float c = (1 - Math.abs(2 * l - 1)) * s;
        float x = c * (1 - Math.abs((hue / 60f) % 2 - 1));
        float m = l - c / 2;
        float r = 0;
        float g = 0;
        float b = 0;
        if (hue < 60) {r = c; g = x;} else if (hue < 120) {r = x; g = c;} else if (hue < 180) {g = c; b = x;}
        else if (hue < 240) {g = x; b = c;} else if (hue < 300) {r = x; b = c;} else {r = c; b = x;}
        return String.format("#%02x%02x%02x", Math.round((r + m) * 255), Math.round((g + m) * 255), Math.round((b + m) * 255));
    }

    // ---------------------------------------------------------------- //
    // My cursor (the touchpad of the app)

    // Where it starts: the screen of the phone, else the first device online with a screen (its center)
    private int[] start() {
        String self = this._net.self();
        if (this._layout.device(self) != null && !this._layout.device(self).screens.isEmpty()) {
            Layout.Rect b = this._layout.globalBounds(self);
            return new int[]{b.x + b.w / 2, b.y + b.h / 2};
        }
        List<String> ids = new ArrayList<>(this._layout.devices().keySet());
        Collections.sort(ids);
        for (String id: ids) {
            if (id.equals(self) || !this._mesh.online(id) || this._layout.device(id).screens.isEmpty()) continue;
            Layout.Rect b = this._layout.globalBounds(id);
            return new int[]{b.x + b.w / 2, b.y + b.h / 2};
        }
        return null;
    }

    public void move(double dx, double dy) {
        this._loop.execute(() -> {
            String self = this._net.self();
            if (this._layout.devices().isEmpty() || !this._net.joined()) return;
            if (!this._away && this._gx == 0 && this._gy == 0) {
                int[] s = this.start();
                if (s == null) return;
                this._gx = s[0];
                this._gy = s[1];
            }
            int fx = (int) Math.round(this._gx);
            int fy = (int) Math.round(this._gy);
            this._gx += dx;
            this._gy += dy;
            int[] to = this._layout.move(fx, fy, (int) Math.round(this._gx) - fx, (int) Math.round(this._gy) - fy);
            if (to[0] != (int) Math.round(this._gx) || to[1] != (int) Math.round(this._gy)) {this._gx = to[0]; this._gy = to[1];}
            Layout.Hit hit = this._layout.hit(to[0], to[1]);
            if (hit == null) return;
            if (hit.device.equals(self)) {
                if (this._away) this.goHome();
                if (!this._homeShown) {
                    this._homeShown = true;
                    this._listener.screen(Json.obj("t", "enter", "id", self, "name", "this phone", "color", colorOf(self), "x", hit.x, "y", hit.y));
                } else this._listener.screen(Json.obj("t", "pointer", "id", self, "x", hit.x, "y", hit.y));
                this._presenceDirty = true;
                return;
            }
            if (!this._mesh.online(hit.device)) {this._gx = fx; this._gy = fy; return;} // not linked: a wall
            if (!this._away || !hit.device.equals(this._target)) this.switchTo(hit.device, hit.x, hit.y);
            this._tx = hit.x;
            this._ty = hit.y;
            this._moveDirty = true;
            this._presenceDirty = true;
        });
    }

    private void switchTo(String device, int x, int y) {
        if (this._away) this.leaveTarget();
        else if (this._homeShown) {this._homeShown = false; this._listener.screen(Json.obj("t", "leave", "id", this._net.self()));}
        this._away = true;
        this._target = device;
        this._tx = x;
        this._ty = y;
        this._mesh.send(device, Json.obj("t", "enter", "x", x, "y", y, "name", this._name));
    }

    private void leaveTarget() {
        for (int b = 0; b <= 4; b++) if ((this._buttons & (1 << b)) != 0) this._mesh.sendBinary(this._target, new Writer(BUTTON).u8(b).u8(0).bytes());
        for (String code: this._keys) this._mesh.sendBinary(this._target, new Writer(KEY).u8(0).text8(code).bytes());
        this._buttons = 0;
        this._keys.clear();
        this._mesh.send(this._target, Json.obj("t", "leave"));
    }

    private void goHome() {
        if (!this._away) return;
        this.leaveTarget();
        this._away = false;
        this._target = "";
        int[] s = this.start();
        boolean screen = this._layout.device(this._net.self()) != null && !this._layout.device(this._net.self()).screens.isEmpty();
        this._gx = screen && s != null ? s[0] : 0;
        this._gy = screen && s != null ? s[1] : 0;
        this._presenceDirty = true;
    }

    // Back on the phone, or (without a screen lent) on the next device online
    public void home() {
        this._loop.execute(() -> {
            if (this._away) {this.goHome(); return;}
            this.move(0, 0);
        });
    }

    // Moves sent at most every 8 ms (the latest only), the presence at 20 Hz
    private void flush() {
        try {
            if (this._mesh == null) return;
            if (this._away && this._moveDirty) {
                Link l = this._mesh.link(this._target);
                if (l != null && l.pending() < 256 * 1024) {
                    this._mesh.sendBinary(this._target, new Writer(POINTER).u16(++this._seq).i32(this._tx).i32(this._ty).bytes());
                    this._moveDirty = false;
                }
            }
            this._flushes++;
            if (this._flushes % 6 == 0) this.presence();
            if (this._flushes % 12 == 0 && this._listener != null) this._listener.map(this.map());
        } catch (Exception e) {
            // a link closed meanwhile
        }
    }

    public void button(int b, boolean down) {
        this._loop.execute(() -> {
            if (!this._away) {this._listener.screen(Json.obj("t", "button", "id", this._net.self(), "b", b, "down", down)); return;}
            if (down) this._buttons |= 1 << b;
            else this._buttons &= ~(1 << b);
            this.flush();
            this._mesh.sendBinary(this._target, new Writer(BUTTON).u8(b).u8(down ? 1 : 0).i32(this._tx).i32(this._ty).bytes());
            this._presenceDirty = true;
        });
    }

    public void wheel(int dx, int dy) {
        this._loop.execute(() -> {
            if (!this._away) {this._listener.screen(Json.obj("t", "wheel", "id", this._net.self(), "dx", dx, "dy", dy)); return;}
            this._mesh.sendBinary(this._target, new Writer(WHEEL).u16(Math.max(-32000, Math.min(32000, dx))).u16(Math.max(-32000, Math.min(32000, dy))).bytes());
        });
    }

    public void key(String code, boolean down) {
        this._loop.execute(() -> {
            if (!this._away) {this._listener.screen(Json.obj("t", "key", "id", this._net.self(), "code", code, "down", down)); return;}
            if (down) this._keys.add(code);
            else this._keys.remove(code);
            this._mesh.sendBinary(this._target, new Writer(KEY).u8(down ? 1 : 0).text8(code).bytes());
        });
    }

    public void text(String text) {
        this._loop.execute(() -> {
            if (!this._away) {this._listener.screen(Json.obj("t", "text", "id", this._net.self(), "text", text)); return;}
            ByteArrayOutputStream out = new ByteArrayOutputStream();
            out.write(TEXT);
            byte[] utf8 = text.getBytes(StandardCharsets.UTF_8);
            out.write(utf8, 0, Math.min(utf8.length, 4096));
            this._mesh.sendBinary(this._target, out.toByteArray());
        });
    }

    public void ping() {this._loop.execute(() -> {this._pingUntil = System.currentTimeMillis() + 1500; this._presenceDirty = true;});}

    public void laser(boolean on) {this._loop.execute(() -> {this._laser = on; this._presenceDirty = true;});}

    public void sendLink(String device, String url) {this._loop.execute(() -> this._mesh.send(device, Json.obj("t", "link", "url", url)));}

    private void presence() {
        String on = this._away ? this._target : this._net.self();
        int flags = this._away ? (this._buttons != 0 ? PRESENCE_DOWN : 0) : PRESENCE_HIDDEN;
        if (!this._presenceDirty || !this._net.joined()) return;
        this._presenceDirty = false;
        if (System.currentTimeMillis() < this._pingUntil) {flags |= PRESENCE_PING; this._presenceDirty = true;}
        if (this._laser) flags |= PRESENCE_LASER;
        byte[] data = new Writer(PRESENCE).u16(++this._seq).text8(on).i32((int) Math.round(this._gx)).i32((int) Math.round(this._gy)).u8(flags).bytes();
        int[] local = this._layout.toLocal(on, (int) Math.round(this._gx), (int) Math.round(this._gy));
        this._presence.put(this._net.self(), new long[]{local[0], local[1], flags, System.currentTimeMillis()});
        this._presenceOn.put(this._net.self(), on);
        for (String id: this._mesh.ids()) this._mesh.sendBinary(id, data);
    }

    // The map of the touchpad page (the format of the pads of a desktop)
    private Map<String, Object> map() {
        List<Object> devices = new ArrayList<>();
        List<Object> cursors = new ArrayList<>();
        for (Layout.Device d: this._layout.devices().values()) {
            List<Object> rects = new ArrayList<>();
            for (int i = 0; i < d.screens.size(); i++) {
                Layout.Rect r = this._layout.globalRect(d.id, i);
                rects.add(java.util.Arrays.asList((Object) r.x, r.y, r.w, r.h));
            }
            boolean self = d.id.equals(this._net.self());
            devices.add(Json.obj("id", d.id, "name", d.name, "self", self, "online", self || this._mesh.online(d.id), "kind", Json.str(Json.map(this._state.value("device/" + d.id)), "kind", "desktop"), "rects", rects));
        }
        for (Map.Entry<String, long[]> it: this._presence.entrySet()) {
            String on = this._presenceOn.getOrDefault(it.getKey(), "");
            int[] at = this._layout.toGlobal(on, (int) it.getValue()[0], (int) it.getValue()[1]);
            cursors.add(Json.obj("id", it.getKey(), "name", this.nameOf(it.getKey(), ""), "color", colorOf(it.getKey()), "x", at[0], "y", at[1], "on", on, "flags", it.getValue()[2]));
        }
        return Json.obj("t", "map", "devices", devices, "cursors", cursors, "network", this._net.name(), "device", this._name, "private", false,
            "me", Json.obj("id", this._net.self(), "away", this._away, "target", this.nameOf(this._target, ""), "laser", this._laser, "x", Math.round(this._gx), "y", Math.round(this._gy)));
    }

    // ---------------------------------------------------------------- //
    // Binary messages (big endian, like net::ByteWriter / ByteReader)

    static final class Writer {
        private final ByteArrayOutputStream _out = new ByteArrayOutputStream();

        Writer(int type) {this._out.write(type);}

        Writer u8(int v) {this._out.write(v & 0xff); return this;}

        Writer u16(int v) {this.u8(v >> 8); return this.u8(v);}

        Writer i32(int v) {this.u16(v >>> 16); return this.u16(v);}

        Writer text8(String s) {
            byte[] b = s.getBytes(StandardCharsets.UTF_8);
            int n = Math.min(255, b.length);
            this.u8(n);
            this._out.write(b, 0, n);
            return this;
        }

        byte[] bytes() {return this._out.toByteArray();}
    }

    static final class Reader {
        private final byte[] _data;
        private int _pos = 1;
        boolean ok = true;

        Reader(byte[] data) {this._data = data;}

        int remaining() {return this._data.length - this._pos;}

        int u8() {
            if (this._pos + 1 > this._data.length) {this.ok = false; return 0;}
            return this._data[this._pos++] & 0xff;
        }

        int u16() {int hi = this.u8(); return (hi << 8) | this.u8();}

        int i32() {int hi = this.u16(); return (hi << 16) | this.u16();}

        String text8() {
            int n = this.u8();
            if (this._pos + n > this._data.length) {this.ok = false; return "";}
            String s = new String(this._data, this._pos, n, StandardCharsets.UTF_8);
            this._pos += n;
            return s;
        }

        byte[] rest() {
            byte[] out = new byte[Math.max(0, this._data.length - this._pos)];
            System.arraycopy(this._data, this._pos, out, 0, out.length);
            this._pos = this._data.length;
            return out;
        }
    }
}
