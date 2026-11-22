/*
 * Replicated state of the network (the same as core::State of the desktop): last writer wins with Lamport clocks,
 * deltas {"<key>": {"v": value, "c": "clock", "a": author, "d": deleted}}, tombstones kept
*/
package io.github.tsukini22.xmouse.core;

import java.util.ArrayList;
import java.util.Collections;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Objects;

public final class StateStore {
    private static final class Entry {
        Object value;
        long clock;
        String author = "";
        boolean deleted;
    }

    private final Map<String, Entry> _entries = new LinkedHashMap<>();
    private long _clock = 0;
    private String _self = "";

    public void setSelf(String self) {this._self = self;}

    public void clear() {
        this._entries.clear();
        this._clock = 0;
    }

    private static Map<String, Object> toJson(Entry e) {
        Map<String, Object> o = Json.obj("c", Long.toUnsignedString(e.clock), "a", e.author);
        if (e.deleted) o.put("d", true);
        else o.put("v", e.value);
        return o;
    }

    private static Entry fromJson(Map<String, Object> o) {
        Entry e = new Entry();
        e.clock = Json.num(o, "c", 0);
        e.author = Json.str(o, "a", "");
        if (e.author.length() > 64) e.author = e.author.substring(0, 64);
        e.deleted = Json.bool(o, "d", false);
        e.value = e.deleted ? null : o.get("v");
        return e;
    }

    // a wins over b
    private static boolean newer(Entry a, Entry b) {
        if (a.clock != b.clock) return Long.compareUnsigned(a.clock, b.clock) > 0;
        if (!a.author.equals(b.author)) return a.author.compareTo(b.author) > 0;
        return a.deleted && !b.deleted;
    }

    public synchronized Map<String, Object> set(String key, Object value) {
        Entry current = this._entries.get(key);
        if (current != null && !current.deleted && Objects.equals(Json.write(current.value), Json.write(value))) return new LinkedHashMap<>();
        Entry e = new Entry();
        e.value = value;
        e.clock = ++this._clock;
        e.author = this._self;
        this._entries.put(key, e);
        return Json.obj(key, toJson(e));
    }

    public synchronized Map<String, Object> remove(String key) {
        Entry current = this._entries.get(key);
        if (current == null || current.deleted) return new LinkedHashMap<>();
        Entry e = new Entry();
        e.clock = ++this._clock;
        e.author = this._self;
        e.deleted = true;
        this._entries.put(key, e);
        return Json.obj(key, toJson(e));
    }

    // Remote writes: the keys whose visible value changed
    public synchronized List<String> merge(Map<String, Object> delta) {
        List<String> changed = new ArrayList<>();
        for (Map.Entry<String, Object> it: delta.entrySet()) {
            if (it.getKey().isEmpty() || it.getKey().length() > 256) continue;
            Entry e = fromJson(Json.map(it.getValue()));
            Entry current = this._entries.get(it.getKey());
            if (Long.compareUnsigned(e.clock, this._clock) > 0) this._clock = e.clock;
            if (current != null && !newer(e, current)) continue;
            boolean visible = current == null ? !e.deleted : (current.deleted != e.deleted || !Objects.equals(Json.write(current.value), Json.write(e.value)));
            this._entries.put(it.getKey(), e);
            if (visible) changed.add(it.getKey());
        }
        return changed;
    }

    public synchronized Object value(String key) {
        Entry e = this._entries.get(key);
        return e == null || e.deleted ? null : e.value;
    }

    public synchronized boolean contains(String key) {
        Entry e = this._entries.get(key);
        return e != null && !e.deleted;
    }

    public synchronized List<String> ids(String prefix) {
        List<String> out = new ArrayList<>();
        for (Map.Entry<String, Entry> it: this._entries.entrySet())
            if (!it.getValue().deleted && it.getKey().startsWith(prefix)) out.add(it.getKey().substring(prefix.length()));
        Collections.sort(out);
        return out;
    }

    public synchronized Map<String, Object> toJson() {
        Map<String, Object> o = new LinkedHashMap<>();
        for (Map.Entry<String, Entry> it: this._entries.entrySet()) o.put(it.getKey(), toJson(it.getValue()));
        return o;
    }
}
