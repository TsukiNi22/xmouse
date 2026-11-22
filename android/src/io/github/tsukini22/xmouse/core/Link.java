/*
 * One authenticated link to another member (net::Peer of the desktop), the same handshake byte for byte:
 *  server -> hello {v, app, version, id, name, os, port, group, nonce, salt}
 *  client -> auth  {mode: member | join, id, name, os, port, kind, epoch, version, nonce, proof}
 *            member: HMAC(network key, transcript "client")  join: HMAC(Argon2id(code, salt), transcript "client")
 *  server -> welcome {mode, proof [, group, state]} (the proof of the server side, "server" transcript)
 * The transcript binds both nonces and the certificate of the server side (no relay by a man in the middle).
 * Every callback runs on the loop of the node (one thread).
*/
package io.github.tsukini22.xmouse.core;

import java.nio.charset.StandardCharsets;
import java.security.cert.Certificate;
import java.util.Map;
import java.util.concurrent.Executor;

import javax.net.ssl.SSLSocket;

public final class Link implements Ws.Listener {
    public static final int PROTOCOL_VERSION = 1;
    public static final long PING_INTERVAL = 2000; // ms
    public static final long TIMEOUT = 8000; // ms without anything received: the link is dropped

    public enum Phase {CONNECTING, HELLO, AUTH, APPROVAL, READY, CLOSED}

    // What a link needs from its node
    public interface Context {
        Network network();
        Executor loop();
        byte[] fingerprint(); // sha256 of the certificate of this device
        String name();
        String os();
        String kind(); // "phone"
        String version();
        int port();
        byte[] codeSalt(); // "" = no new member accepted here
        byte[] codeKey(); // Argon2id(code, salt) of the current code
        void codeUsed(); // a join succeeded: the code rotates
        String memberCheck(String id); // "" = a member that may connect, else why not
        byte[] pinned(String id); // its known certificate fingerprint (empty = unknown)
        Map<String, Object> stateJson(); // full replicated state (a new member)
    }

    public interface Listener {
        void onReady(Link link);
        void onJoinRequest(Link link); // server: valid code, approve() expected
        void onJoined(Link link, Map<String, Object> welcome); // client of a join: the network given
        void onMessage(Link link, Map<String, Object> o);
        void onBinary(Link link, byte[] data);
        void onClosed(Link link, String reason);
    }

    private final Context _ctx;
    private final Listener _listener;
    private final boolean _outgoing;
    private Ws _ws;
    private Phase _phase = Phase.CONNECTING;
    private String _mode = "member";
    private String _code = "";
    private String _device = "";
    private String _name = "";
    private String _os = "";
    private int _remotePort = 0;
    private String _host = "";
    private byte[] _serverNonce = new byte[0];
    private byte[] _clientNonce = new byte[0];
    private byte[] _serverFingerprint = new byte[0];
    private byte[] _proofKey = new byte[0];
    private byte[] _expected = new byte[0];
    private boolean _stale = false;
    private long _lastSeen = System.currentTimeMillis();
    private long _lastPing = 0;
    private int _rtt = -1;
    private String _closeReason = "";
    private Object _tag; // what the mesh dialed (its target id)

    private Link(Context ctx, Listener listener, boolean outgoing) {
        this._ctx = ctx;
        this._listener = listener;
        this._outgoing = outgoing;
    }

    // ---------------------------------------------------------------- //
    // Opening

    // Server side: the websocket of an incoming connection, the hello sent at once
    public static Link accept(Ws ws, Context ctx, Listener listener) {
        Link link = new Link(ctx, listener, false);
        link._ws = ws;
        link._serverFingerprint = ctx.fingerprint();
        link._serverNonce = Crypto.random(32);
        link._phase = Phase.HELLO;
        ws.start(link);
        link.sendJson(Json.obj("t", "hello", "v", PROTOCOL_VERSION, "app", "xmouse", "version", ctx.version(), "id", ctx.network().self(), "name", ctx.name(), "os", ctx.os(),
            "port", ctx.port(), "group", ctx.network().tag(), "nonce", Crypto.b64(link._serverNonce), "salt", ctx.codeSalt().length == 16 ? Crypto.b64(ctx.codeSalt()) : ""));
        return link;
    }

    // Client side: an opened websocket (the TLS session gives the certificate of the server), code = join
    public static Link dialed(Ws ws, String host, int port, Context ctx, Listener listener, String code, byte[] expected) {
        Link link = new Link(ctx, listener, true);
        Certificate[] chain;
        link._ws = ws;
        link._host = host;
        link._remotePort = port;
        link._code = Crypto.normalizeCode(code);
        link._mode = code.isEmpty() ? "member" : "join";
        link._expected = expected == null ? new byte[0] : expected;
        try {
            chain = ((SSLSocket) ws.socket()).getSession().getPeerCertificates();
            link._serverFingerprint = Crypto.fingerprint(chain[0]);
        } catch (Exception e) {
            link._serverFingerprint = new byte[0];
        }
        link._phase = Phase.HELLO;
        ws.start(link);
        if (link._serverFingerprint.length == 0) {link.fail("the device has no certificate"); return link;}
        if (link._expected.length > 0 && !Crypto.equal(link._expected, link._serverFingerprint)) {link.fail("this is not the device of the link (another identity answered): nothing sent"); return link;}
        return link;
    }

    // ---------------------------------------------------------------- //
    // Getters

    public String id() {return this._device;}

    public String name() {return this._name;}

    public String os() {return this._os;}

    public String mode() {return this._mode;}

    public String host() {return this._host;}

    public int remotePort() {return this._remotePort;}

    public boolean outgoing() {return this._outgoing;}

    public boolean ready() {return this._phase == Phase.READY;}

    public Phase phase() {return this._phase;}

    public int rtt() {return this._rtt;}

    public long pending() {return this._ws == null ? 0 : this._ws.pending();}

    public String address() {
        try {
            return ((SSLSocket) this._ws.socket()).getInetAddress().getHostAddress();
        } catch (Exception e) {
            return "";
        }
    }

    public Object tag() {return this._tag;}

    public void setTag(Object tag) {this._tag = tag;}

    // ---------------------------------------------------------------- //
    // Sending

    public void sendJson(Map<String, Object> o) {
        if (this._ws != null && this._phase != Phase.CLOSED) this._ws.sendText(Json.write(o));
    }

    public void sendBinary(byte[] data) {
        if (this._ws != null && this._phase == Phase.READY) this._ws.sendBinary(data);
    }

    public void close(String reason) {
        if (this._phase == Phase.CLOSED) return;
        this._closeReason = reason;
        this.sendJson(Json.obj("t", "bye", "reason", reason));
        if (this._ws != null) this._ws.close();
    }

    private void fail(String reason) {
        if (this._phase == Phase.CLOSED) return;
        this._closeReason = reason;
        if (this._ws != null) this._ws.close();
    }

    // The loop of the node calls it every second: pings, a dead link dropped
    public void tick(long now) {
        if (this._phase == Phase.CLOSED) return;
        if (now - this._lastSeen > TIMEOUT) {this.close("no answer for " + TIMEOUT / 1000 + " s"); return;}
        if (this._phase == Phase.READY && now - this._lastPing >= PING_INTERVAL) {
            this._lastPing = now;
            this.sendJson(Json.obj("t", "ping", "ts", Long.toString(now)));
        }
    }

    // ---------------------------------------------------------------- //
    // Websocket (reader thread -> the loop of the node)

    @Override
    public void onText(Ws ws, String text) {this._ctx.loop().execute(() -> this.text(text));}

    @Override
    public void onBinary(Ws ws, byte[] data) {
        this._ctx.loop().execute(() -> {
            this._lastSeen = System.currentTimeMillis();
            if (this._phase == Phase.READY && data.length > 0) this._listener.onBinary(this, data);
        });
    }

    @Override
    public void onClose(Ws ws, String reason) {
        this._ctx.loop().execute(() -> {
            if (this._phase == Phase.CLOSED) return;
            this._phase = Phase.CLOSED;
            this._listener.onClosed(this, this._closeReason.isEmpty() ? reason : this._closeReason);
        });
    }

    private void text(String text) {
        Map<String, Object> o;
        String type;
        this._lastSeen = System.currentTimeMillis();
        try {
            o = Json.object(text);
        } catch (Exception e) {
            this.fail("malformed message");
            return;
        }
        type = Json.str(o, "t", "");
        if (type.equals("bye")) {this._closeReason = Json.str(o, "reason", ""); if (this._ws != null) this._ws.close(); return;}
        if (this._phase != Phase.READY) {
            if (this._outgoing && this._phase == Phase.HELLO && type.equals("hello")) this.onHello(o);
            else if (!this._outgoing && this._phase == Phase.HELLO && type.equals("auth")) this.onAuth(o);
            else if (this._outgoing && this._phase == Phase.AUTH && type.equals("welcome")) this.onWelcome(o);
            else if (this._phase != Phase.APPROVAL) this.fail("unexpected " + type);
            return;
        }
        if (type.equals("ping")) {this.sendJson(Json.obj("t", "pong", "ts", o.get("ts"))); return;}
        if (type.equals("pong")) {this._rtt = (int) (System.currentTimeMillis() - Json.num(o, "ts", System.currentTimeMillis())); return;}
        this._listener.onMessage(this, o);
    }

    // ---------------------------------------------------------------- //
    // Handshake

    private byte[] transcript(String role, String id) {
        return Crypto.transcript(this._serverNonce, this._clientNonce, this._serverFingerprint, this._mode + "|" + role + "|" + id);
    }

    private void onHello(Map<String, Object> o) {
        byte[] proof;
        Network net = this._ctx.network();
        if (Json.num(o, "v", 0) != PROTOCOL_VERSION) {this.fail("protocol version " + Json.num(o, "v", 0) + ": update xmouse"); return;}
        this._device = Json.str(o, "id", "");
        this._name = Json.str(o, "name", "");
        this._os = Json.str(o, "os", "");
        this._remotePort = (int) Json.num(o, "port", this._remotePort);
        this._serverNonce = Crypto.unb64(Json.str(o, "nonce", ""));
        if (this._device.isEmpty() || this._serverNonce.length != 32) {this.fail("malformed hello"); return;}
        if (this._device.equals(net.self())) {this.fail("this is this device"); return;}
        this._clientNonce = Crypto.random(32);
        if (this._mode.equals("member")) {
            byte[] pinned = this._ctx.pinned(this._device);
            if (!net.joined()) {this.fail("not in a network"); return;}
            if (!Json.str(o, "group", "").equals(net.tag())) {this.fail("another network"); return;}
            if (pinned.length > 0 && !Crypto.equal(pinned, this._serverFingerprint)) {this.fail("the identity of " + this._name + " changed (certificate): refused"); return;}
            proof = net.memberProof(this.transcript("client", net.self()));
        } else {
            byte[] salt = Crypto.unb64(Json.str(o, "salt", ""));
            if (salt.length != 16) {this.fail("the device does not accept new members"); return;}
            this._proofKey = Argon2.argon2id(this._code.getBytes(StandardCharsets.UTF_8), salt, Crypto.ARGON2_PASSES, Crypto.ARGON2_MEMORY, Crypto.ARGON2_LANES, 32);
            proof = Crypto.hmac(this._proofKey, this.transcript("client", net.self()));
        }
        this._phase = Phase.AUTH;
        this.sendJson(Json.obj("t", "auth", "mode", this._mode, "id", net.self(), "name", this._ctx.name(), "os", this._ctx.os(), "port", this._ctx.port(),
            "kind", this._ctx.kind(), "epoch", net.epoch(), "version", this._ctx.version(), "nonce", Crypto.b64(this._clientNonce), "proof", Crypto.b64(proof)));
    }

    private void onAuth(Map<String, Object> o) {
        Network net = this._ctx.network();
        byte[] proof = Crypto.unb64(Json.str(o, "proof", ""));
        this._mode = Json.str(o, "mode", "").equals("join") ? "join" : "member";
        this._device = Json.str(o, "id", "");
        this._name = Json.str(o, "name", "");
        this._os = Json.str(o, "os", "");
        this._remotePort = (int) Json.num(o, "port", 0);
        this._clientNonce = Crypto.unb64(Json.str(o, "nonce", ""));
        if (this._device.length() < 8 || this._clientNonce.length != 32 || proof.length != 32) {this.fail("malformed auth"); return;}
        if (this._device.equals(net.self())) {this.fail("this is this device"); return;}
        if (this._mode.equals("member")) {
            byte[] transcript = this.transcript("client", this._device);
            String refusal = this._ctx.memberCheck(this._device);
            if (!refusal.isEmpty()) {this.fail(refusal); return;}
            if (!Crypto.equal(net.memberProof(transcript), proof)) {
                if (!Crypto.equal(net.previousProof(transcript), proof)) {this.fail("wrong member proof"); return;}
                this._stale = true;
            }
            this.sendWelcome();
            return;
        }
        if (this._ctx.codeSalt().length != 16 || !net.joined()) {this.fail("this device does not accept new members"); return;}
        if (!Crypto.equal(Crypto.hmac(this._ctx.codeKey(), this.transcript("client", this._device)), proof)) {this.fail("wrong or expired code"); return;}
        this._proofKey = Crypto.hmac(this._ctx.codeKey(), this.transcript("server", net.self())); // before the rotation
        this._ctx.codeUsed();
        this._phase = Phase.APPROVAL;
        this._listener.onJoinRequest(this);
    }

    // Server: the answer of the local person to a join
    public void approve(boolean accepted) {
        if (this._phase != Phase.APPROVAL) return;
        if (!accepted) {this.close("refused on the device"); return;}
        this.sendWelcome();
    }

    private void sendWelcome() {
        Network net = this._ctx.network();
        Map<String, Object> welcome = Json.obj("t", "welcome", "mode", this._mode);
        if (this._mode.equals("member")) {
            byte[] transcript = this.transcript("server", net.self());
            welcome.put("proof", Crypto.b64(this._stale ? net.previousProof(transcript) : net.memberProof(transcript)));
        } else {
            welcome.put("proof", Crypto.b64(this._proofKey));
            welcome.put("group", Json.obj("id", net.id(), "name", net.name(), "key", Crypto.b64(net.key()), "epoch", net.epoch()));
            welcome.put("state", this._ctx.stateJson());
            this._proofKey = new byte[0];
        }
        this.sendJson(welcome);
        if (this._stale) this.sendJson(Json.obj("t", "rekey", "key", Crypto.b64(net.key()), "epoch", net.epoch()));
        this._phase = Phase.READY;
        this._listener.onReady(this);
    }

    private void onWelcome(Map<String, Object> o) {
        Network net = this._ctx.network();
        byte[] proof = Crypto.unb64(Json.str(o, "proof", ""));
        byte[] transcript = this.transcript("server", this._device);
        if (this._mode.equals("member")) {
            if (!Crypto.equal(net.memberProof(transcript), proof) && !Crypto.equal(net.previousProof(transcript), proof)) {this.fail("the device could not prove it is a member"); return;}
            this._phase = Phase.READY;
            this._listener.onReady(this);
            return;
        }
        if (!Crypto.equal(Crypto.hmac(this._proofKey, transcript), proof)) {this.fail("the device could not prove it knows the code"); return;}
        this._proofKey = new byte[0];
        this._mode = "member";
        this._phase = Phase.READY;
        this._listener.onJoined(this, o);
        this._listener.onReady(this);
    }
}
