/*
 * Small JSON reader / writer of the member core (the same code on the phone and in the checks of the PC, where the JDK
 * has no JSON): objects are LinkedHashMap<String, Object>, arrays ArrayList<Object>, numbers Long / Double, strings,
 * booleans, null
*/
package io.github.tsukini22.xmouse.core;

import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

public final class Json {
    private final String _text;
    private int _pos = 0;

    private Json(String text) {this._text = text;}

    // ---------------------------------------------------------------- //
    // Reading

    public static Object parse(String text) {
        Json j = new Json(text);
        j.space();
        Object out = j.value(0);
        j.space();
        if (j._pos != j._text.length()) throw new IllegalArgumentException("json: trailing characters");
        return out;
    }

    @SuppressWarnings("unchecked")
    public static Map<String, Object> object(String text) {
        Object o = parse(text);
        if (!(o instanceof Map)) throw new IllegalArgumentException("json: not an object");
        return (Map<String, Object>) o;
    }

    private void space() {
        while (this._pos < this._text.length() && Character.isWhitespace(this._text.charAt(this._pos))) this._pos++;
    }

    private char peek() {
        if (this._pos >= this._text.length()) throw new IllegalArgumentException("json: unexpected end");
        return this._text.charAt(this._pos);
    }

    private Object value(int depth) {
        if (depth > 64) throw new IllegalArgumentException("json: too deep");
        char c = this.peek();
        if (c == '{') return this.object(depth);
        if (c == '[') return this.array(depth);
        if (c == '"') return this.string();
        if (this._text.startsWith("true", this._pos)) {this._pos += 4; return Boolean.TRUE;}
        if (this._text.startsWith("false", this._pos)) {this._pos += 5; return Boolean.FALSE;}
        if (this._text.startsWith("null", this._pos)) {this._pos += 4; return null;}
        return this.number();
    }

    private Map<String, Object> object(int depth) {
        Map<String, Object> out = new LinkedHashMap<>();
        this._pos++;
        this.space();
        if (this.peek() == '}') {this._pos++; return out;}
        while (true) {
            this.space();
            String key = this.string();
            this.space();
            if (this.peek() != ':') throw new IllegalArgumentException("json: ':' expected");
            this._pos++;
            this.space();
            out.put(key, this.value(depth + 1));
            this.space();
            char c = this.peek();
            this._pos++;
            if (c == '}') return out;
            if (c != ',') throw new IllegalArgumentException("json: ',' expected");
        }
    }

    private List<Object> array(int depth) {
        List<Object> out = new ArrayList<>();
        this._pos++;
        this.space();
        if (this.peek() == ']') {this._pos++; return out;}
        while (true) {
            this.space();
            out.add(this.value(depth + 1));
            this.space();
            char c = this.peek();
            this._pos++;
            if (c == ']') return out;
            if (c != ',') throw new IllegalArgumentException("json: ',' expected");
        }
    }

    private String string() {
        if (this.peek() != '"') throw new IllegalArgumentException("json: string expected");
        StringBuilder out = new StringBuilder();
        this._pos++;
        while (true) {
            char c = this.peek();
            this._pos++;
            if (c == '"') return out.toString();
            if (c != '\\') {out.append(c); continue;}
            char e = this.peek();
            this._pos++;
            switch (e) {
                case 'n': out.append('\n'); break;
                case 't': out.append('\t'); break;
                case 'r': out.append('\r'); break;
                case 'b': out.append('\b'); break;
                case 'f': out.append('\f'); break;
                case 'u':
                    out.append((char) Integer.parseInt(this._text.substring(this._pos, this._pos + 4), 16));
                    this._pos += 4;
                    break;
                default: out.append(e); break;
            }
        }
    }

    private Object number() {
        int start = this._pos;
        while (this._pos < this._text.length() && "+-0123456789.eE".indexOf(this._text.charAt(this._pos)) >= 0) this._pos++;
        String n = this._text.substring(start, this._pos);
        if (n.isEmpty()) throw new IllegalArgumentException("json: value expected");
        if (n.indexOf('.') < 0 && n.indexOf('e') < 0 && n.indexOf('E') < 0) {
            try {return Long.parseLong(n);} catch (NumberFormatException e) { /* too big: a double */ }
        }
        return Double.parseDouble(n);
    }

    // ---------------------------------------------------------------- //
    // Writing

    public static String write(Object value) {
        StringBuilder out = new StringBuilder();
        write(out, value);
        return out.toString();
    }

    @SuppressWarnings("unchecked")
    private static void write(StringBuilder out, Object value) {
        if (value == null) {out.append("null"); return;}
        if (value instanceof String) {quote(out, (String) value); return;}
        if (value instanceof Boolean) {out.append(value.toString()); return;}
        if (value instanceof Double || value instanceof Float) {
            double d = ((Number) value).doubleValue();
            if (d == Math.rint(d) && Math.abs(d) < 1e15) out.append((long) d);
            else out.append(d);
            return;
        }
        if (value instanceof Number) {out.append(((Number) value).longValue()); return;}
        if (value instanceof Map) {
            boolean first = true;
            out.append('{');
            for (Map.Entry<String, Object> e: ((Map<String, Object>) value).entrySet()) {
                if (!first) out.append(',');
                first = false;
                quote(out, e.getKey());
                out.append(':');
                write(out, e.getValue());
            }
            out.append('}');
            return;
        }
        if (value instanceof List) {
            boolean first = true;
            out.append('[');
            for (Object v: (List<Object>) value) {
                if (!first) out.append(',');
                first = false;
                write(out, v);
            }
            out.append(']');
            return;
        }
        quote(out, value.toString());
    }

    private static void quote(StringBuilder out, String s) {
        out.append('"');
        for (int i = 0; i < s.length(); i++) {
            char c = s.charAt(i);
            switch (c) {
                case '"': out.append("\\\""); break;
                case '\\': out.append("\\\\"); break;
                case '\n': out.append("\\n"); break;
                case '\r': out.append("\\r"); break;
                case '\t': out.append("\\t"); break;
                default:
                    if (c < 0x20) out.append(String.format("\\u%04x", (int) c));
                    else out.append(c);
            }
        }
        out.append('"');
    }

    // ---------------------------------------------------------------- //
    // Access

    public static Map<String, Object> obj(Object... pairs) {
        Map<String, Object> out = new LinkedHashMap<>();
        for (int i = 0; i + 1 < pairs.length; i += 2) out.put((String) pairs[i], pairs[i + 1]);
        return out;
    }

    @SuppressWarnings("unchecked")
    public static Map<String, Object> map(Object o) {return o instanceof Map ? (Map<String, Object>) o : new LinkedHashMap<>();}

    @SuppressWarnings("unchecked")
    public static List<Object> list(Object o) {return o instanceof List ? (List<Object>) o : new ArrayList<>();}

    public static String str(Map<String, Object> o, String key, String fallback) {
        Object v = o.get(key);
        return v instanceof String ? (String) v : fallback;
    }

    public static long num(Map<String, Object> o, String key, long fallback) {
        Object v = o.get(key);
        if (v instanceof Number) return ((Number) v).longValue();
        if (v instanceof String) {
            try {return Long.parseLong((String) v);} catch (NumberFormatException e) {return fallback;}
        }
        return fallback;
    }

    public static double dbl(Map<String, Object> o, String key, double fallback) {
        Object v = o.get(key);
        return v instanceof Number ? ((Number) v).doubleValue() : fallback;
    }

    public static boolean bool(Map<String, Object> o, String key, boolean fallback) {
        Object v = o.get(key);
        return v instanceof Boolean ? (Boolean) v : fallback;
    }
}
