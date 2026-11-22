/*
 * The device and its network (security::Group of the desktop): the id of this device, the id / name / 256 bits key of
 * the network, its epoch and the key of the epoch before; the member proof HMAC(key, transcript); the public tag of
 * the network (LAN announces). Saved by the Store given (the private folder of the app)
*/
package io.github.tsukini22.xmouse.core;

import java.nio.charset.StandardCharsets;
import java.util.Map;

public final class Network {
    // Where the network is kept (a file of the app, a map in the checks)
    public interface Store {
        String load(); // "" = nothing yet
        void save(String json);
    }

    public static final int KEY_SIZE = 32;

    private final Store _store;
    private String _self = "";
    private String _id = "";
    private String _name = "";
    private byte[] _key = new byte[0];
    private byte[] _previous = new byte[0];
    private int _epoch = 0;

    public Network(Store store) {this._store = store;}

    public synchronized void load() {
        String text = this._store.load();
        Map<String, Object> o = text.isEmpty() ? Json.obj() : Json.object(text);
        this._self = Json.str(o, "self", "");
        if (this._self.isEmpty()) this._self = Crypto.randomId(12);
        this._id = Json.str(o, "id", "");
        this._name = Json.str(o, "name", "");
        this._key = Crypto.unb64(Json.str(o, "key", ""));
        this._previous = Crypto.unb64(Json.str(o, "previous", ""));
        this._epoch = (int) Json.num(o, "epoch", 0);
        if (this._key.length != KEY_SIZE) {this._id = ""; this._key = new byte[0];}
        this.save();
    }

    private void save() {
        this._store.save(Json.write(Json.obj("self", this._self, "id", this._id, "name", this._name, "key", Crypto.b64(this._key), "previous", Crypto.b64(this._previous), "epoch", this._epoch)));
    }

    public synchronized void create(String name) {
        this._id = Crypto.randomId(12);
        this._name = name;
        this._key = Crypto.random(KEY_SIZE);
        this._previous = new byte[0];
        this._epoch = 1;
        this.save();
    }

    public synchronized void adopt(String id, String name, byte[] key, int epoch) {
        if (id.isEmpty() || key.length != KEY_SIZE) throw new IllegalArgumentException("invalid network");
        this._id = id;
        this._name = name;
        this._key = key;
        this._previous = new byte[0];
        this._epoch = epoch;
        this.save();
    }

    public synchronized void rekey(byte[] key, int epoch) {
        if (key.length != KEY_SIZE || epoch <= this._epoch) return;
        this._previous = this._key;
        this._key = key;
        this._epoch = epoch;
        this.save();
    }

    public synchronized void leave() {
        this._id = "";
        this._name = "";
        this._key = new byte[0];
        this._previous = new byte[0];
        this._epoch = 0;
        this.save();
    }

    public synchronized byte[] memberProof(byte[] transcript) {return Crypto.hmac(this._key, transcript);}

    public synchronized byte[] previousProof(byte[] transcript) {return this._previous.length == KEY_SIZE ? Crypto.hmac(this._previous, transcript) : new byte[0];}

    public synchronized String tag() {
        if (this._id.isEmpty()) return "";
        byte[] h = Crypto.sha256(("xmouse-group|" + this._id).getBytes(StandardCharsets.UTF_8));
        byte[] first = new byte[8];
        System.arraycopy(h, 0, first, 0, 8);
        return Crypto.hex(first);
    }

    public synchronized boolean joined() {return !this._id.isEmpty();}

    public synchronized String self() {return this._self;}

    public synchronized String id() {return this._id;}

    public synchronized String name() {return this._name;}

    public synchronized byte[] key() {return this._key.clone();}

    public synchronized int epoch() {return this._epoch;}
}
