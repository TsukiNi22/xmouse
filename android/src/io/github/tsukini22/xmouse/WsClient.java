/*
 * Minimal websocket client (RFC 6455, text frames) over the pinned TLS of the device: no library in the APK.
 * One thread reads; send() may be called from any thread.
*/
package io.github.tsukini22.xmouse;

import android.util.Base64;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.InetSocketAddress;
import java.nio.charset.StandardCharsets;
import java.security.SecureRandom;

import javax.net.ssl.SSLSocket;

final class WsClient {
    interface Listener {
        void onOpen();
        void onText(String text);
        void onClose(String reason);
    }

    private static final int MAX_MESSAGE = 1 << 20; // bytes
    private final String _host;
    private final int _port;
    private final String _path;
    private final String _fp;
    private final Listener _listener;
    private final SecureRandom _random = new SecureRandom();
    private SSLSocket _socket;
    private OutputStream _out;
    private volatile boolean _closed = false;

    WsClient(String host, int port, String path, String fp, Listener listener) {
        this._host = host;
        this._port = port;
        this._path = path;
        this._fp = fp;
        this._listener = listener;
    }

    void start() {
        new Thread(this::run, "xmouse-ws").start();
    }

    void close() {
        this._closed = true;
        try {
            if (this._socket != null) this._socket.close();
        } catch (IOException e) {
            // closed anyway
        }
    }

    synchronized void send(String text) {
        if (this._out == null || this._closed) return;
        try {
            this.writeFrame(0x1, text.getBytes(StandardCharsets.UTF_8));
        } catch (IOException e) {
            this.close();
        }
    }

    private void run() {
        String reason = "closed";
        try {
            SSLSocket socket = (SSLSocket) Pinned.factory(this._fp).createSocket();
            socket.connect(new InetSocketAddress(this._host, this._port), Pinned.TIMEOUT);
            socket.setTcpNoDelay(true);
            socket.startHandshake();
            this._socket = socket;
            InputStream in = socket.getInputStream();
            this._out = socket.getOutputStream();

            // Upgrade
            byte[] nonce = new byte[16];
            this._random.nextBytes(nonce);
            String key = Base64.encodeToString(nonce, Base64.NO_WRAP);
            String request = "GET " + this._path + " HTTP/1.1\r\nHost: " + this._host + ":" + this._port + "\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
                + "Sec-WebSocket-Key: " + key + "\r\nSec-WebSocket-Version: 13\r\nUser-Agent: xmouse-android\r\n\r\n";
            synchronized (this) {
                this._out.write(request.getBytes(StandardCharsets.US_ASCII));
                this._out.flush();
            }
            String status = readHeaders(in);
            if (!status.startsWith("HTTP/1.1 101")) throw new IOException("upgrade refused: " + status);
            socket.setSoTimeout(0);
            this._listener.onOpen();

            // Frames
            ByteArrayOutputStream message = new ByteArrayOutputStream();
            while (!this._closed) {
                int b0 = in.read();
                int b1 = in.read();
                if (b0 < 0 || b1 < 0) break;
                boolean fin = (b0 & 0x80) != 0;
                int opcode = b0 & 0x0f;
                long length = b1 & 0x7f;
                if (length == 126) length = (readByte(in) << 8) | readByte(in);
                else if (length == 127) {
                    length = 0;
                    for (int i = 0; i < 8; ++i) length = (length << 8) | readByte(in);
                }
                if (length > MAX_MESSAGE) throw new IOException("message too big");
                byte[] payload = readFully(in, (int) length);
                if (opcode == 0x8) break; // close
                if (opcode == 0x9) {synchronized (this) {this.writeFrame(0xA, payload);} continue;} // ping -> pong
                if (opcode == 0xA) continue;
                message.write(payload);
                if (message.size() > MAX_MESSAGE) throw new IOException("message too big");
                if (fin) {
                    if (opcode == 0x1 || opcode == 0x0) this._listener.onText(new String(message.toByteArray(), StandardCharsets.UTF_8));
                    message.reset();
                }
            }
        } catch (Exception e) {
            reason = e.getMessage() == null ? e.toString() : e.getMessage();
        } finally {
            this.close();
            this._listener.onClose(reason);
        }
    }

    private void writeFrame(int opcode, byte[] payload) throws IOException {
        ByteArrayOutputStream frame = new ByteArrayOutputStream(payload.length + 14);
        byte[] mask = new byte[4];
        this._random.nextBytes(mask);
        frame.write(0x80 | opcode);
        if (payload.length < 126) frame.write(0x80 | payload.length);
        else if (payload.length < 65536) {
            frame.write(0x80 | 126);
            frame.write((payload.length >> 8) & 0xff);
            frame.write(payload.length & 0xff);
        } else {
            frame.write(0x80 | 127);
            for (int i = 7; i >= 0; --i) frame.write((int) (((long) payload.length >> (8 * i)) & 0xff));
        }
        frame.write(mask);
        for (int i = 0; i < payload.length; ++i) frame.write(payload[i] ^ mask[i % 4]);
        this._out.write(frame.toByteArray());
        this._out.flush();
    }

    private static long readByte(InputStream in) throws IOException {
        int b = in.read();
        if (b < 0) throw new IOException("closed");
        return b;
    }

    private static byte[] readFully(InputStream in, int length) throws IOException {
        byte[] out = new byte[length];
        int done = 0;
        while (done < length) {
            int n = in.read(out, done, length - done);
            if (n < 0) throw new IOException("closed");
            done += n;
        }
        return out;
    }

    // The status line of the answer (the headers are read up to the empty line)
    private static String readHeaders(InputStream in) throws IOException {
        StringBuilder all = new StringBuilder();
        int last4 = 0;
        while (all.length() < 16384) {
            int c = in.read();
            if (c < 0) throw new IOException("closed during the upgrade");
            all.append((char) c);
            last4 = (last4 << 8) | c;
            if (last4 == 0x0d0a0d0a) break;
        }
        int end = all.indexOf("\r\n");
        return end < 0 ? all.toString() : all.substring(0, end);
    }
}
