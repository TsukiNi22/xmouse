/**************************************************************\

 ██╗  ██╗ █████╗ ██████╗ ████████╗ █████╗ ███╗   ██╗██╗ █████╗
 ╚██╗██╔╝██╔══██╗██╔══██╗╚══██╔══╝██╔══██╗████╗  ██║██║██╔══██╗
  ╚███╔╝ ███████║██████╔╝   ██║   ███████║██╔██╗ ██║██║███████║
  ██╔██╗ ██╔══██║██╔══██╗   ██║   ██╔══██║██║╚██╗██║██║██╔══██║
 ██╔╝ ██╗██║  ██║██║  ██║   ██║   ██║  ██║██║ ╚████║██║██║  ██║
 ╚═╝  ╚═╝╚═╝  ╚═╝╚═╝  ╚═╝   ╚═╝   ╚═╝  ╚═╝╚═╝  ╚═══╝╚═╝╚═╝  ╚═╝

Edition:
##  @date 22/11/2026 by @author Tsukini

File Name:
##  @file Hub-route.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Attribute
#include <utils/utils.hpp>
#include "xmouse/net/Protocol.hpp"
#include "xmouse/core/Tools.hpp"
#include "xmouse/core/Hub.hpp"
#include <QJsonArray>

//----------------------------------------------------------------//
/* TOOLS */

// An IPv4 peer seen through a dual stack socket (::ffff:a.b.c.d): the udp socket is IPv4
_cold static QHostAddress plain_address(const QHostAddress& address)
{
    bool ok = false;
    const quint32 v4 = address.toIPv4Address(&ok);

    return ok ? QHostAddress(v4) : address;
}

_hot static QByteArray relay_binary(const QString& to, const QString& from, const int hops, const QByteArray& inner)
{
    xmouse::net::ByteWriter w(xmouse::net::Binary::Relay, static_cast<std::size_t>(inner.size()) + 64);

    w.text8(to).text8(from).u8(static_cast<std::uint8_t>(hops));
    w.bytes(inner);
    return w.data();
}

_hot static QJsonObject relay_json(const QString& to, const QString& from, const int hops, const QJsonObject& inner)
{
    return QJsonObject{{"t", "relay"}, {"to", to}, {"from", from}, {"hops", hops}, {"m", inner}};
}

//----------------------------------------------------------------//
/* ROUTE */

xmouse::core::Route xmouse::core::Hub::routeTo_(const QString& id) const
{
    const QStringList ids = this->_mesh.ids();
    const QSet<QString> online(ids.begin(), ids.end());
    QSet<QString> pads;

    for (const Pad& p: this->_pads)
        if (p.display && p.screenOn) pads.insert(p.id);
    return route_to(this->_state, online, pads, this->_group.self(), id);
}

bool xmouse::core::Hub::reachable(const QString& id) const
{
    return this->routeTo_(id).kind != RouteKind::None;
}

QStringList xmouse::core::Hub::links_(void) const
{
    QStringList out = this->_mesh.ids();

    for (const Pad& p: this->_pads)
        if (p.display && p.screenOn) out.append(p.id);
    out.sort();
    return out;
}

qint64 xmouse::core::Hub::pendingTo_(const QString& id) const
{
    const Route route = this->routeTo_(id);
    const net::Peer* peer = nullptr;

    switch (route.kind) {
        case RouteKind::Direct:
        case RouteKind::Relay:
            peer = this->_mesh.peer(route.hop);
            return peer ? peer->pending() : 0;
        case RouteKind::Pad: {
            const Pad pad = this->_pads.value(id);
            return pad.display ? pad.display->pendingBytes() : 0;
        }
        default:
            return 0;
    }
}

void xmouse::core::Hub::sendTo_(const QString& id, const QJsonObject& o, const QString& from)
{
    const QString source = from.isEmpty() ? this->_group.self() : from;
    const Route route = this->routeTo_(id);

    switch (route.kind) {
        case RouteKind::Direct:
            if (source == this->_group.self()) this->_mesh.send(id, o);
            else this->_mesh.send(id, relay_json(id, source, 0, o)); // for a pad of this device: the member sees who it comes from
            break;
        case RouteKind::Pad:
            this->deliverToPad_(id, source, o);
            break;
        case RouteKind::Relay:
            this->_mesh.send(route.hop, relay_json(id, source, 0, o));
            break;
        default:
            break;
    }
}

void xmouse::core::Hub::sendBinaryTo_(const QString& id, const QByteArray& data, const QString& from)
{
    const QString source = from.isEmpty() ? this->_group.self() : from;
    const Route route = this->routeTo_(id);

    switch (route.kind) {
        case RouteKind::Direct:
            if (source == this->_group.self()) this->_mesh.sendBinary(id, data);
            else this->_mesh.sendBinary(id, relay_binary(id, source, 0, data));
            break;
        case RouteKind::Pad:
            this->deliverBinaryToPad_(id, source, data);
            break;
        case RouteKind::Relay:
            this->_mesh.sendBinary(route.hop, relay_binary(id, source, 0, data));
            break;
        default:
            break;
    }
}

void xmouse::core::Hub::sendHot_(const QString& id, const QByteArray& data)
{
    // A direct member answering the udp probes: the move goes by udp (a lost one is replaced by the next)
    if (this->_udp.isBound() && this->_mesh.isOnline(id) && this->udpUp(id) && this->_udpPath.contains(id)) {
        const QPair<QHostAddress, quint16> path = this->_udpPath.value(id);
        if (this->_udp.send(path.first, path.second, data)) return;
    }
    this->sendBinaryTo_(id, data);
}

//----------------------------------------------------------------//
/* RELAY */

void xmouse::core::Hub::onRelay_(const QString& id, const QJsonObject& o)
{
    const QString to = o.value("to").toString();
    const QString from = o.value("from").toString();
    const int hops = o.value("hops").toInt(XMOUSE_RELAY_HOPS);
    const QJsonObject inner = o.value("m").toObject();
    Route route;

    if (!this->_mesh.isOnline(id) || to.isEmpty() || from.isEmpty() || inner.isEmpty()) return; // members relay, never a pad
    if (!this->_state.contains("device/" + from) || this->_state.contains("removed/" + from)) return;
    if (to == this->_group.self()) {this->onMessage_(from, inner); return;}
    if (this->_pads.contains(to)) {this->deliverToPad_(to, from, inner); return;}
    if (!this->_config.settings().relay || hops + 1 >= XMOUSE_RELAY_HOPS) return;
    route = this->routeTo_(to);
    if (route.kind == RouteKind::Direct) this->_mesh.send(to, relay_json(to, from, hops + 1, inner));
    else if (route.kind == RouteKind::Relay && route.hop != id) this->_mesh.send(route.hop, relay_json(to, from, hops + 1, inner));
}

void xmouse::core::Hub::onRelayBinary_(const QString& id, const QByteArray& data)
{
    net::ByteReader r(data);
    const QString to = r.text8();
    const QString from = r.text8();
    const int hops = r.u8();
    const QByteArray inner = r.rest();
    Route route;

    if (!r.ok() || inner.isEmpty() || !this->_mesh.isOnline(id) || to.isEmpty() || from.isEmpty()) return;
    if (!this->_state.contains("device/" + from) || this->_state.contains("removed/" + from)) return;
    if (static_cast<std::uint8_t>(inner[0]) == static_cast<std::uint8_t>(net::Binary::Relay)) return; // never nested
    if (to == this->_group.self()) {this->onBinary_(from, inner); return;}
    if (this->_pads.contains(to)) {this->deliverBinaryToPad_(to, from, inner); return;}
    if (!this->_config.settings().relay || hops + 1 >= XMOUSE_RELAY_HOPS) return;
    route = this->routeTo_(to);
    if (route.kind == RouteKind::Direct) this->_mesh.sendBinary(to, relay_binary(to, from, hops + 1, inner));
    else if (route.kind == RouteKind::Relay && route.hop != id) this->_mesh.sendBinary(route.hop, relay_binary(to, from, hops + 1, inner));
}

//----------------------------------------------------------------//
/* UDP */

void xmouse::core::Hub::probeUdp_(void)
{
    const QByteArray ping = net::ByteWriter(net::Binary::UdpProbe).u8(0).data();

    if (!this->_udp.isBound() || !this->_udp.hasKey()) return;
    for (const QString& id: this->_mesh.ids()) {
        const net::Peer* peer = this->_mesh.peer(id);
        const quint16 port = static_cast<quint16>(this->_state.value("device/" + id).toObject().value("port").toInt(XMOUSE_DEFAULT_PORT));

        if (!peer || !this->_state.value("device/" + id).toObject().value("udp").toBool(false)) continue;
        // The path its udp came from (a nat), else the address of its link on its port
        if (this->_udpPath.contains(id)) (void)this->_udp.send(this->_udpPath.value(id).first, this->_udpPath.value(id).second, ping);
        else (void)this->_udp.send(plain_address(peer->address()), port, ping);
    }
}

void xmouse::core::Hub::onDatagram_(const QString& sender, const QHostAddress& from, const quint16 port, const QByteArray& inner)
{
    const std::uint8_t type = static_cast<std::uint8_t>(inner[0]);

    if (!this->_mesh.isOnline(sender)) return; // udp only between linked members (the link proved who they are)
    if (type == static_cast<std::uint8_t>(net::Binary::UdpProbe)) {
        net::ByteReader r(inner);
        const bool pong = r.u8() == 1;
        const bool was = this->udpUp(sender);

        this->_udpPath[sender] = {plain_address(from), port};
        if (!pong) {(void)this->_udp.send(plain_address(from), port, net::ByteWriter(net::Binary::UdpProbe).u8(1).data()); return;}
        this->_udpUp[sender] = now_ms();
        if (!was) {
            this->_log.info("network", sender, "udp path up (" + plain_address(from).toString() + ":" + QString::number(port) + ")");
            emit this->membersChanged();
        }
        return;
    }
    // The hot path only (moves, presence): everything else stays on the ordered link
    if (type == static_cast<std::uint8_t>(net::Binary::Pointer) || type == static_cast<std::uint8_t>(net::Binary::Presence)) this->onBinary_(sender, inner);
}
