/*
 * The links of this device to the members (net::Mesh of the desktop): the TLS 1.3 server (the others dial it), the
 * dials of every known address of the members, one link per pair (the link opened by the smaller id wins on both
 * sides), the LAN announces (udp 47901). Every decision runs on the loop of the node; the sockets block on their threads.
*/
package io.github.tsukini22.xmouse.core;

import java.io.IOException;
import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetAddress;
import java.net.InetSocketAddress;
import java.net.InterfaceAddress;
import java.net.NetworkInterface;
import java.net.Socket;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import javax.net.ssl.SSLContext;
import javax.net.ssl.SSLServerSocket;
import javax.net.ssl.SSLSocket;

public final class Mesh implements Link.Listener {
    public interface Events {
        void onPeerReady(String id);
        void onPeerLost(String id, String reason);
        void onMessage(String id, Map<String, Object> o);
        void onBinary(String id, byte[] data);
        void onJoinRequest(Link link);
        void onJoined(Map<String, Object> welcome, String id);
        void onJoinFailed(String reason);
    }

    public static final int DISCOVERY_PORT = 47901;
    private static final long DIAL_RETRY = 10000; // ms before an address is dialed again
    private static final long DIAL_GRACE = 3000; // ms the bigger id waits (the smaller one dials first)
    private static final String PAGE = "<!doctype html><meta charset=utf-8><title>xmouse</title><p>A phone of an xmouse network. To join it, use its link in xmouse (Network, Connect to a network).</p>";

    private final Link.Context _ctx;
    private final Events _events;
    private final SSLContext _server;
    private final SSLContext _client;
    private final Map<String, Link> _links = new HashMap<>(); // ready links by member id
    private final List<Link> _pending = new ArrayList<>();
    private final Map<String, List<String>> _candidates = new HashMap<>(); // id -> "host:port"
    private final Map<String, Long> _lastDial = new HashMap<>();
    private final Map<String, Long> _missingSince = new HashMap<>();
    private SSLServerSocket _socket;
    private DatagramSocket _discovery;
    private int _port = 0;
    private volatile boolean _running = false;
    private int _ticks = 0; // seconds: the LAN announce every 3

    public Mesh(Link.Context ctx, Events events, SSLContext server, SSLContext client) {
        this._ctx = ctx;
        this._events = events;
        this._server = server;
        this._client = client;
    }

    // ---------------------------------------------------------------- //
    // Server

    public void start(int port) throws IOException {
        this._socket = (SSLServerSocket) this._server.getServerSocketFactory().createServerSocket();
        this._socket.setReuseAddress(true);
        this._socket.bind(new InetSocketAddress(port));
        this._socket.setEnabledProtocols(new String[]{"TLSv1.3"});
        this._port = this._socket.getLocalPort();
        this._running = true;
        Thread accept = new Thread(this::acceptLoop, "xmouse-accept");
        accept.setDaemon(true);
        accept.start();
        try {
            this._discovery = new DatagramSocket(null);
            this._discovery.setReuseAddress(true);
            this._discovery.setBroadcast(true);
            this._discovery.bind(new InetSocketAddress(DISCOVERY_PORT));
            Thread listen = new Thread(this::discoveryLoop, "xmouse-discovery");
            listen.setDaemon(true);
            listen.start();
        } catch (IOException e) {
            this._discovery = null; // another program holds the port: dials by address only
        }
    }

    public void stop() {
        this._running = false;
        try {if (this._socket != null) this._socket.close();} catch (IOException e) { /* closed */ }
        if (this._discovery != null) this._discovery.close();
        for (Link l: new ArrayList<>(this._links.values())) l.close("xmouse stopped");
        for (Link l: new ArrayList<>(this._pending)) l.close("xmouse stopped");
    }

    public int port() {return this._port;}

    private void acceptLoop() {
        while (this._running) {
            try {
                SSLSocket socket = (SSLSocket) this._socket.accept();
                Thread t = new Thread(() -> this.upgrade(socket), "xmouse-upgrade");
                t.setDaemon(true);
                t.start();
            } catch (IOException e) {
                if (!this._running) return;
            }
        }
    }

    private void upgrade(SSLSocket socket) {
        try {
            socket.setSoTimeout(15000);
            socket.setTcpNoDelay(true);
            socket.startHandshake();
            Ws ws = Ws.accept(socket, "/ws", PAGE);
            if (ws == null) return;
            socket.setSoTimeout(0);
            this._ctx.loop().execute(() -> {
                Link link = Link.accept(ws, this._ctx, this);
                this._pending.add(link);
            });
        } catch (Exception e) {
            try {socket.close();} catch (IOException ignored) { /* closed */ }
        }
    }

    // ---------------------------------------------------------------- //
    // Dials

    // Opens a TLS socket (a thread), the link created on the loop; code = join, expected = identity of the link
    private void dial(String host, int port, String code, byte[] expected, String target) {
        Thread t = new Thread(() -> {
            try {
                SSLSocket socket = (SSLSocket) this._client.getSocketFactory().createSocket();
                socket.connect(new InetSocketAddress(host, port), 8000);
                socket.setEnabledProtocols(new String[]{"TLSv1.3"});
                socket.setTcpNoDelay(true);
                socket.setSoTimeout(15000);
                socket.startHandshake();
                Ws ws = Ws.connect(socket, host, port, "/ws");
                socket.setSoTimeout(0);
                this._ctx.loop().execute(() -> {
                    Link link = Link.dialed(ws, host, port, this._ctx, this, code, expected);
                    link.setTag(target);
                    this._pending.add(link);
                });
            } catch (Exception e) {
                if (!code.isEmpty()) this._ctx.loop().execute(() -> this._events.onJoinFailed(host + ":" + port + ": " + (e.getMessage() == null ? e.toString() : e.getMessage())));
            }
        }, "xmouse-dial");
        t.setDaemon(true);
        t.start();
    }

    public void join(String host, int port, String code, byte[] expected) {this.dial(host, port, code, expected, "");}

    public void setCandidates(String id, List<String> addresses) {
        if (addresses.isEmpty()) this._candidates.remove(id);
        else this._candidates.put(id, new ArrayList<>(addresses));
    }

    // Every second: the members without a link are dialed (the smaller id first), the links pinged
    public void tick(long now) {
        String self = this._ctx.network().self();
        for (Link l: new ArrayList<>(this._links.values())) l.tick(now);
        for (Link l: new ArrayList<>(this._pending)) l.tick(now);
        if (!this._ctx.network().joined()) return;
        for (Map.Entry<String, List<String>> it: this._candidates.entrySet()) {
            String id = it.getKey();
            boolean dialing = false;
            if (this._links.containsKey(id) || id.equals(self)) continue;
            for (Link p: this._pending) if (p.outgoing() && id.equals(p.tag())) dialing = true;
            if (dialing) continue;
            this._missingSince.putIfAbsent(id, now);
            if (self.compareTo(id) > 0 && now - this._missingSince.get(id) < DIAL_GRACE) continue;
            for (String address: it.getValue()) {
                String key = id + "|" + address;
                if (now - this._lastDial.getOrDefault(key, 0L) < DIAL_RETRY) continue;
                this._lastDial.put(key, now);
                int colon = address.lastIndexOf(':');
                if (colon <= 0) break;
                try {
                    this.dial(address.substring(0, colon), Integer.parseInt(address.substring(colon + 1)), "", new byte[0], id);
                } catch (NumberFormatException e) {
                    continue;
                }
                break;
            }
        }
        if (this._discovery != null && ++this._ticks % 3 == 0) this.announce();
    }

    // ---------------------------------------------------------------- //
    // Links

    public Link link(String id) {return this._links.get(id);}

    public List<String> ids() {
        List<String> out = new ArrayList<>(this._links.keySet());
        Collections.sort(out);
        return out;
    }

    public boolean online(String id) {return this._links.containsKey(id);}

    public void send(String id, Map<String, Object> o) {
        Link l = this._links.get(id);
        if (l != null) l.sendJson(o);
    }

    public void sendBinary(String id, byte[] data) {
        Link l = this._links.get(id);
        if (l != null) l.sendBinary(data);
    }

    public void broadcast(Map<String, Object> o, String except) {
        for (Map.Entry<String, Link> it: this._links.entrySet())
            if (!it.getKey().equals(except)) it.getValue().sendJson(o);
    }

    public void drop(String id, String reason) {
        Link l = this._links.get(id);
        if (l != null) l.close(reason);
    }

    public void dropAll(String reason) {
        for (Link l: new ArrayList<>(this._links.values())) l.close(reason);
    }

    private boolean keepNew(Link current, Link candidate) {
        String self = this._ctx.network().self();
        String smaller = self.compareTo(candidate.id()) < 0 ? self : candidate.id();
        String byCurrent = current.outgoing() ? self : current.id();
        String byCandidate = candidate.outgoing() ? self : candidate.id();
        if (byCurrent.equals(smaller)) return false;
        return byCandidate.equals(smaller);
    }

    @Override
    public void onReady(Link link) {
        Link current = this._links.get(link.id());
        this._pending.remove(link);
        if (current != null && current != link) {
            if (!this.keepNew(current, link)) {link.setTag("duplicate"); link.close("duplicate link"); return;}
            current.setTag("duplicate");
            this._links.remove(link.id());
            current.close("duplicate link");
        }
        this._links.put(link.id(), link);
        this._missingSince.remove(link.id());
        if (current != null && current != link) return;
        this._events.onPeerReady(link.id());
    }

    @Override
    public void onJoinRequest(Link link) {this._events.onJoinRequest(link);}

    @Override
    public void onJoined(Link link, Map<String, Object> welcome) {this._events.onJoined(welcome, link.id());}

    @Override
    public void onMessage(Link link, Map<String, Object> o) {
        if (this._links.get(link.id()) == link) this._events.onMessage(link.id(), o);
    }

    @Override
    public void onBinary(Link link, byte[] data) {
        if (this._links.get(link.id()) == link) this._events.onBinary(link.id(), data);
    }

    @Override
    public void onClosed(Link link, String reason) {
        this._pending.remove(link);
        if (this._links.get(link.id()) == link) {
            this._links.remove(link.id());
            this._events.onPeerLost(link.id(), reason);
        } else if (link.mode().equals("join") && link.outgoing() && !"duplicate".equals(link.tag())) this._events.onJoinFailed(reason);
    }

    // ---------------------------------------------------------------- //
    // LAN announces

    private void announce() {
        Network net = this._ctx.network();
        byte[] data = Json.write(Json.obj("xmouse", Link.PROTOCOL_VERSION, "id", net.self(), "name", this._ctx.name(), "port", this._port, "group", net.tag(), "network", net.joined() ? net.name() : "")).getBytes(StandardCharsets.UTF_8);
        List<InetAddress> targets = new ArrayList<>();
        try {
            targets.add(InetAddress.getByName("255.255.255.255"));
            for (NetworkInterface iface: Collections.list(NetworkInterface.getNetworkInterfaces())) {
                if (!iface.isUp() || iface.isLoopback()) continue;
                for (InterfaceAddress a: iface.getInterfaceAddresses()) if (a.getBroadcast() != null) targets.add(a.getBroadcast());
            }
            for (InetAddress target: targets) this._discovery.send(new DatagramPacket(data, data.length, target, DISCOVERY_PORT));
        } catch (Exception e) {
            // no network now
        }
    }

    private void discoveryLoop() {
        byte[] buffer = new byte[4096];
        while (this._running && this._discovery != null) {
            try {
                DatagramPacket packet = new DatagramPacket(buffer, buffer.length);
                this._discovery.receive(packet);
                Map<String, Object> o = Json.object(new String(packet.getData(), 0, packet.getLength(), StandardCharsets.UTF_8));
                String host = packet.getAddress().getHostAddress();
                this._ctx.loop().execute(() -> this.onAnnounce(o, host));
            } catch (Exception e) {
                if (!this._running) return;
            }
        }
    }

    // A member heard on the LAN: its address first, dialed at once when it has no link
    private void onAnnounce(Map<String, Object> o, String host) {
        Network net = this._ctx.network();
        String id = Json.str(o, "id", "");
        int port = (int) Json.num(o, "port", 0);
        if (Json.num(o, "xmouse", 0) != Link.PROTOCOL_VERSION || id.isEmpty() || id.equals(net.self()) || port == 0) return;
        if (!net.joined() || !Json.str(o, "group", "").equals(net.tag()) || this._links.containsKey(id)) return;
        List<String> addresses = this._candidates.computeIfAbsent(id, k -> new ArrayList<>());
        String address = host + ":" + port;
        if (!addresses.contains(address)) addresses.add(0, address);
        this._lastDial.remove(id + "|" + address);
    }

    // The LAN / VPN addresses of this device (published: the others dial them)
    public static List<String> localAddresses() {
        List<String> out = new ArrayList<>();
        try {
            for (NetworkInterface iface: Collections.list(NetworkInterface.getNetworkInterfaces())) {
                if (!iface.isUp() || iface.isLoopback()) continue;
                for (InterfaceAddress a: iface.getInterfaceAddresses()) {
                    InetAddress ip = a.getAddress();
                    if (ip.isLinkLocalAddress() || ip.getHostAddress().contains(":")) continue; // IPv4 only (the desktop dials host:port)
                    out.add(ip.getHostAddress());
                }
            }
        } catch (Exception e) {
            // no interface
        }
        return out;
    }
}
