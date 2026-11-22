/*
 * Websocket (RFC 6455) over a TLS socket, both sides: the client upgrades /ws (masked frames), the server answers an
 * upgrade (unmasked frames). One thread reads; send() may be called from any thread.
*/
package io.github.tsukini22.xmouse.core;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.Socket;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.security.SecureRandom;
import java.util.Base64;
import java.util.LinkedHashMap;
import java.util.Map;

public final class Ws {
    public interface Listener {
        void onText(Ws ws, String text);
        void onBinary(Ws ws, byte[] data);
        void onClose(Ws ws, String reason);
    }

    public static final int MAX_MESSAGE = 24 * 1024 * 1024; // an image of the clipboard
    private static final String GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

    private final Socket _socket;
    private final boolean _client;
    private final InputStream _in;
    private final OutputStream _out;
    private final SecureRandom _random = new SecureRandom();
    private Listener _listener;
    private volatile boolean _closed = false;
    private volatile long _pending = 0; // bytes being written (back pressure)

    private Ws(Socket socket, boolean client) throws IOException {
        this._socket = socket;
        this._client = client;
        this._in = socket.getInputStream();
        this._out = socket.getOutputStream();
    }

    // ---------------------------------------------------------------- //
    // Opening

    public static Ws connect(Socket socket, String host, int port, String path) throws IOException {
        Ws ws = new Ws(socket, true);
        byte[] nonce = new byte[16];
        ws._random.nextBytes(nonce);
        String key = Base64.getEncoder().encodeToString(nonce);
        String request = "GET " + path + " HTTP/1.1\r\nHost: " + host + ":" + port + "\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
            + "Sec-WebSocket-Key: " + key + "\r\nSec-WebSocket-Version: 13\r\nUser-Agent: xmouse-android\r\n\r\n";
        ws._out.write(request.getBytes(StandardCharsets.US_ASCII));
        ws._out.flush();
        String status = readHead(ws._in).getOrDefault(":status", "");
        if (!status.startsWith("HTTP/1.1 101")) throw new IOException("upgrade refused: " + status);
        return ws;
    }

    // Server side: the request read; null = not a websocket upgrade of this path (answered and closed)
    public static Ws accept(Socket socket, String path, String page) throws IOException {
        Map<String, String> head = readHead(socket.getInputStream());
        String line = head.getOrDefault(":status", "");
        String key = head.getOrDefault("sec-websocket-key", "");
        OutputStream out = socket.getOutputStream();
        if (!line.startsWith("GET " + path + " ") || key.isEmpty() || !head.getOrDefault("upgrade", "").toLowerCase().contains("websocket")) {
            byte[] body = page.getBytes(StandardCharsets.UTF_8);
            out.write(("HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nContent-Length: " + body.length + "\r\nConnection: close\r\nCache-Control: no-store\r\n\r\n").getBytes(StandardCharsets.US_ASCII));
            out.write(body);
            out.flush();
            socket.close();
            return null;
        }
        String accept;
        try {
            accept = Base64.getEncoder().encodeToString(MessageDigest.getInstance("SHA-1").digest((key + GUID).getBytes(StandardCharsets.US_ASCII)));
        } catch (Exception e) {
            throw new IOException(e);
        }
        out.write(("HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: " + accept + "\r\n\r\n").getBytes(StandardCharsets.US_ASCII));
        out.flush();
        return new Ws(socket, false);
    }

    private static Map<String, String> readHead(InputStream in) throws IOException {
        StringBuilder all = new StringBuilder();
        int last4 = 0;
        while (all.length() < 16384) {
            int c = in.read();
            if (c < 0) throw new IOException("closed during the upgrade");
            all.append((char) c);
            last4 = (last4 << 8) | c;
            if (last4 == 0x0d0a0d0a) break;
        }
        Map<String, String> out = new LinkedHashMap<>();
        String[] lines = all.toString().split("\r\n");
        out.put(":status", lines.length > 0 ? lines[0] : "");
        for (int i = 1; i < lines.length; i++) {
            int colon = lines[i].indexOf(':');
            if (colon > 0) out.put(lines[i].substring(0, colon).trim().toLowerCase(), lines[i].substring(colon + 1).trim());
        }
        return out;
    }

    // ---------------------------------------------------------------- //
    // Running

    public void start(Listener listener) {
        this._listener = listener;
        Thread reader = new Thread(this::run, "xmouse-ws");
        reader.setDaemon(true);
        reader.start();
    }

    public Socket socket() {return this._socket;}

    public long pending() {return this._pending;}

    public boolean isOpen() {return !this._closed;}

    public void close() {
        if (this._closed) return;
        this._closed = true;
        try {
            this._socket.close();
        } catch (IOException e) {
            // closed anyway
        }
    }

    public void sendText(String text) {this.send(0x1, text.getBytes(StandardCharsets.UTF_8));}

    public void sendBinary(byte[] data) {this.send(0x2, data);}

    private void send(int opcode, byte[] payload) {
        if (this._closed) return;
        try {
            this.writeFrame(opcode, payload);
        } catch (IOException e) {
            this.close();
        }
    }

    private synchronized void writeFrame(int opcode, byte[] payload) throws IOException {
        ByteArrayOutputStream frame = new ByteArrayOutputStream(payload.length + 14);
        frame.write(0x80 | opcode);
        int mask = this._client ? 0x80 : 0;
        if (payload.length < 126) frame.write(mask | payload.length);
        else if (payload.length < 65536) {
            frame.write(mask | 126);
            frame.write((payload.length >> 8) & 0xff);
            frame.write(payload.length & 0xff);
        } else {
            frame.write(mask | 127);
            for (int i = 7; i >= 0; --i) frame.write((int) (((long) payload.length >> (8 * i)) & 0xff));
        }
        if (this._client) {
            byte[] key = new byte[4];
            this._random.nextBytes(key);
            frame.write(key);
            for (int i = 0; i < payload.length; ++i) frame.write(payload[i] ^ key[i % 4]);
        } else frame.write(payload, 0, payload.length);
        byte[] bytes = frame.toByteArray();
        this._pending += bytes.length;
        try {
            this._out.write(bytes);
            this._out.flush();
        } finally {
            this._pending -= bytes.length;
        }
    }

    private void run() {
        String reason = "connection closed";
        try {
            ByteArrayOutputStream message = new ByteArrayOutputStream();
            int messageType = 0;
            while (!this._closed) {
                int b0 = this._in.read();
                int b1 = this._in.read();
                if (b0 < 0 || b1 < 0) break;
                boolean fin = (b0 & 0x80) != 0;
                int opcode = b0 & 0x0f;
                boolean masked = (b1 & 0x80) != 0;
                long length = b1 & 0x7f;
                if (length == 126) length = (readByte() << 8) | readByte();
                else if (length == 127) {
                    length = 0;
                    for (int i = 0; i < 8; ++i) length = (length << 8) | readByte();
                }
                if (length > MAX_MESSAGE) throw new IOException("message too big");
                byte[] key = new byte[4];
                if (masked) for (int i = 0; i < 4; i++) key[i] = (byte) readByte();
                byte[] payload = readFully((int) length);
                if (masked) for (int i = 0; i < payload.length; i++) payload[i] ^= key[i % 4];
                if (opcode == 0x8) {reason = "closed by the other side"; break;}
                if (opcode == 0x9) {this.send(0xA, payload); continue;} // ping -> pong
                if (opcode == 0xA) continue;
                if (opcode != 0x0) messageType = opcode;
                message.write(payload);
                if (message.size() > MAX_MESSAGE) throw new IOException("message too big");
                if (!fin) continue;
                byte[] data = message.toByteArray();
                message.reset();
                if (messageType == 0x1) this._listener.onText(this, new String(data, StandardCharsets.UTF_8));
                else if (messageType == 0x2) this._listener.onBinary(this, data);
            }
        } catch (Exception e) {
            reason = e.getMessage() == null ? e.toString() : e.getMessage();
        } finally {
            this.close();
            this._listener.onClose(this, reason);
        }
    }

    private long readByte() throws IOException {
        int b = this._in.read();
        if (b < 0) throw new IOException("closed");
        return b;
    }

    private byte[] readFully(int length) throws IOException {
        byte[] out = new byte[length];
        int done = 0;
        while (done < length) {
            int n = this._in.read(out, done, length - done);
            if (n < 0) throw new IOException("closed");
            done += n;
        }
        return out;
    }
}
