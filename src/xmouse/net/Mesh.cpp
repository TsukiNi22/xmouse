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
##  @file Mesh.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Attribute
#define _Exception
#include <utils/utils.hpp>
#include "xmouse/security/PairingCode.hpp"
#include "xmouse/security/Group.hpp"
#include "xmouse/net/Protocol.hpp"
#include "xmouse/net/Mesh.hpp"
#include <QNetworkInterface>
#include <QNetworkDatagram>
#include <QJsonDocument>
#include <QDateTime>

//----------------------------------------------------------------//
/* DEFINE */

#define ANNOUNCE_INTERVAL 3000 // ms between two LAN announces
#define DIAL_INTERVAL 2000 // ms between two passes of the dial loop
#define DIAL_RETRY 6000 // ms before the same address is dialed again
#define DIAL_GRACE 5000 // ms the bigger id waits for the smaller one to dial
#define ANNOUNCE_TTL 10000 // ms an announce stays in the list
#define BAN_TIME (3600 * 1000) // ms of the ban of an address after too many wrong codes

//----------------------------------------------------------------//
/* TOOLS */

_hot static inline qint64 now_ms(void) {return QDateTime::currentMSecsSinceEpoch();}

// IPv4 mapped IPv6 (::ffff:a.b.c.d) shown and compared as IPv4
_cold static QString plain_address(const QHostAddress& address)
{
    bool ok = false;
    const quint32 v4 = address.toIPv4Address(&ok);

    if (ok) return QHostAddress(v4).toString();
    return address.toString();
}

//----------------------------------------------------------------//
/* CLASS */

xmouse::net::Mesh::Mesh(QObject* parent)
: QObject(parent)
{
    QObject::connect(&this->_server, &Server::newSocket, this, [this](WebSocket* ws, const QString&) {this->onSocket_(ws);});
    this->_server.setFilter([this](const QHostAddress& address) {return !this->isBanned(address);});
    this->_announce.setInterval(ANNOUNCE_INTERVAL);
    QObject::connect(&this->_announce, &QTimer::timeout, this, &Mesh::announce_);
    this->_dial.setInterval(DIAL_INTERVAL);
    QObject::connect(&this->_dial, &QTimer::timeout, this, &Mesh::dialAll_);
    QObject::connect(&this->_discovery, &QUdpSocket::readyRead, this, &Mesh::onDatagram_);

    // Wrong codes: counted per address, banned for an hour past the limit
    this->_ctx.codeAllowed = [this](const QHostAddress& address) {return !this->isBanned(address);};
    this->_ctx.codeFailed = [this](const QHostAddress& address) {
        const QString key = plain_address(address);
        if (++this->_codeFailures[key] < this->_maxFailures) return;
        this->_banned[key] = now_ms() + BAN_TIME;
        this->_codeFailures.remove(key);
        if (this->_ctx.code) this->_ctx.code->rotate();
    };
}

xmouse::net::Mesh::~Mesh()
{
    this->stop();
}

void xmouse::net::Mesh::start(const QString& bind, const quint16 port, const QSslConfiguration& tls, const bool discovery)
{
    this->_server.listen(bind, port, tls);
    this->_ctx.port = this->_server.port();
    this->_discover = discovery;
    if (discovery) {
        if (!this->_discovery.bind(QHostAddress::AnyIPv4, XMOUSE_DISCOVERY_PORT, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint))
            this->_discover = false; // another program holds the port: dials by address only
        this->_announce.start();
        this->announce_();
    }
    this->_dial.start();
}

void xmouse::net::Mesh::stop(void)
{
    this->_announce.stop();
    this->_dial.stop();
    this->_discovery.close();
    this->dropAll("device stopped");
    this->_server.stop();
}

bool xmouse::net::Mesh::isBanned(const QHostAddress& address) const
{
    return this->_banned.value(plain_address(address), 0) > now_ms();
}

void xmouse::net::Mesh::setCandidates(const QString& id, const QList<QPair<QString, quint16>>& addresses)
{
    if (addresses.isEmpty()) {this->_candidates.remove(id); this->_missingSince.remove(id); return;}
    this->_candidates[id] = addresses;
    if (!this->_peers.contains(id) && !this->_missingSince.contains(id)) this->_missingSince[id] = now_ms();
}

void xmouse::net::Mesh::dialNow(void)
{
    this->_lastDial.clear();
    this->dialAll_();
}

//----------------------------------------------------------------//
/* LINKS */

void xmouse::net::Mesh::onSocket_(WebSocket* ws)
{
    Peer* peer = Peer::accept(ws, this->_ctx, this);

    this->_pending.append(peer);
    this->watch_(peer);
    QObject::connect(peer, &Peer::joinRequest, this, [this, peer]() {emit this->joinRequest(peer);});

    // Authentication in 30 s (the approval of a join has its own timer in the panel)
    QTimer::singleShot(30000, peer, [peer]() {
        if (peer->state() == PeerState::Hello) peer->close("authentication timeout");
    });
}

void xmouse::net::Mesh::watch_(Peer* peer)
{
    QObject::connect(peer, &Peer::ready, this, [this, peer]() {this->onReady_(peer);});
    QObject::connect(peer, &Peer::closed, this, [this, peer](const QString& reason) {this->onClosed_(peer, reason);});
    QObject::connect(peer, &Peer::message, this, [this, peer](const QJsonObject& o) {
        if (this->_peers.value(peer->id()) == peer) emit this->message(peer->id(), o);
    });
    QObject::connect(peer, &Peer::binary, this, [this, peer](const QByteArray& data) {
        if (this->_peers.value(peer->id()) == peer) emit this->binary(peer->id(), data);
    });
}

bool xmouse::net::Mesh::keepNew_(const Peer* current, const Peer* candidate) const
{
    const QString self = this->_ctx.group->self();
    const QString smaller = std::min(self, candidate->id());
    const QString byCurrent = current->isOutgoing() ? self : current->id();
    const QString byCandidate = candidate->isOutgoing() ? self : candidate->id();

    // The link opened by the smaller id wins on both sides: the same choice everywhere, no flapping
    if (byCurrent == smaller) return false;
    return byCandidate == smaller;
}

void xmouse::net::Mesh::onReady_(Peer* peer)
{
    Peer* current = this->_peers.value(peer->id(), nullptr);

    this->_pending.removeAll(peer);
    if (current && current != peer) {
        if (!this->keepNew_(current, peer)) {peer->setProperty("xmouse_duplicate", true); peer->close("duplicate link"); return;}
        current->setProperty("xmouse_duplicate", true);
        this->_peers.remove(peer->id());
        current->close("duplicate link");
    }
    this->_peers[peer->id()] = peer;
    this->_missingSince.remove(peer->id());
    if (current && current != peer) return; // replaced: the member never went offline
    emit this->peerReady(peer->id());
}

void xmouse::net::Mesh::onClosed_(Peer* peer, const QString& reason)
{
    const bool wasReady = this->_peers.value(peer->id()) == peer;

    this->_pending.removeAll(peer);
    if (wasReady) {
        this->_peers.remove(peer->id());
        this->_missingSince[peer->id()] = now_ms();
        emit this->peerLost(peer->id(), reason);
    } else if (peer->isOutgoing() && peer->mode() == "join") {
        emit this->joinFailed(reason);
    } else if (peer->isOutgoing() && !peer->property("xmouse_duplicate").toBool()) {
        emit this->linkFailed(peer->property("xmouse_target").toString(), reason);
    }
    peer->deleteLater();
}

void xmouse::net::Mesh::dialAll_(void)
{
    const qint64 now = now_ms();
    const QString self = this->_ctx.group ? this->_ctx.group->self() : QString();

    if (!this->_ctx.group || !this->_ctx.group->joined()) return;
    for (auto it = this->_candidates.constBegin(); it != this->_candidates.constEnd(); ++it) {
        const QString& id = it.key();
        bool dialing = false;

        if (this->_peers.contains(id) || id == self) continue;
        for (const Peer* p: this->_pending)
            if (p->isOutgoing() && p->property("xmouse_target").toString() == id) dialing = true;
        if (dialing) continue;
        if (self > id && now - this->_missingSince.value(id, now) < DIAL_GRACE) continue; // the smaller id dials first

        // One address per pass, the oldest tried first
        for (const QPair<QString, quint16>& address: it.value()) {
            const QString key = id + "|" + address.first + ":" + QString::number(address.second);
            if (now - this->_lastDial.value(key, 0) < DIAL_RETRY) continue;
            this->_lastDial[key] = now;
            Peer* peer = Peer::dial(address.first, address.second, this->_ctx, "", this, this->_ctx.postQuantum && !this->_ctx.postQuantum(id));
            peer->setProperty("xmouse_target", id);
            this->_pending.append(peer);
            this->watch_(peer);
            break;
        }
    }
}

xmouse::net::Peer* xmouse::net::Mesh::join(const QString& host, const quint16 port, const QString& code, const QByteArray& expected)
{
    Peer* peer = Peer::dial(host, port, this->_ctx, code, this, true, expected); // the device joined may lack ML-KEM: hybrid first, classic accepted

    this->_pending.append(peer);
    this->watch_(peer);
    QObject::connect(peer, &Peer::joined, this, [this, peer](const QJsonObject& welcome) {emit this->joined(welcome, peer->id());});
    return peer;
}

void xmouse::net::Mesh::send(const QString& id, const QJsonObject& object)
{
    Peer* peer = this->_peers.value(id, nullptr);

    if (peer) peer->sendJson(object);
}

void xmouse::net::Mesh::sendBinary(const QString& id, const QByteArray& data)
{
    Peer* peer = this->_peers.value(id, nullptr);

    if (peer) peer->sendBinary(data);
}

void xmouse::net::Mesh::broadcast(const QJsonObject& object, const QString& except)
{
    for (auto it = this->_peers.constBegin(); it != this->_peers.constEnd(); ++it)
        if (it.key() != except) it.value()->sendJson(object);
}

void xmouse::net::Mesh::broadcastBinary(const QByteArray& data, const QString& except)
{
    for (auto it = this->_peers.constBegin(); it != this->_peers.constEnd(); ++it)
        if (it.key() != except) it.value()->sendBinary(data);
}

void xmouse::net::Mesh::drop(const QString& id, const QString& reason)
{
    Peer* peer = this->_peers.value(id, nullptr);

    this->_candidates.remove(id);
    if (peer) peer->close(reason);
}

void xmouse::net::Mesh::dropAll(const QString& reason)
{
    const QList<Peer*> peers = this->_peers.values();
    const QList<Peer*> pending = this->_pending;

    for (Peer* p: peers)
        p->close(reason);
    for (Peer* p: pending)
        p->close(reason);
}

QStringList xmouse::net::Mesh::addressesOf(const QString& id) const
{
    QStringList out;
    const Peer* peer = this->_peers.value(id, nullptr);

    if (peer && !peer->isOutgoing()) out.append(plain_address(peer->address()));
    if (peer && peer->isOutgoing()) out.append(peer->host());
    if (this->_announces.contains(id)) out.append(this->_announces.value(id).host);
    out.removeDuplicates();
    out.removeAll("");
    return out;
}

//----------------------------------------------------------------//
/* DISCOVERY */

void xmouse::net::Mesh::announce_(void)
{
    QByteArray datagram;
    QList<QHostAddress> targets = {QHostAddress::Broadcast};

    if (!this->_ctx.group) return;
    datagram = QJsonDocument(QJsonObject{
        {"xmouse", XMOUSE_PROTOCOL_VERSION},
        {"id", this->_ctx.group->self()},
        {"name", this->_ctx.name},
        {"port", this->_server.port()},
        {"group", this->_ctx.group->tag()},
        {"network", this->_ctx.group->joined() ? this->_networkName : QString()},
    }).toJson(QJsonDocument::Compact);

    // Every interface: 255.255.255.255 alone does not leave every network on every OS
    for (const QNetworkInterface& iface: QNetworkInterface::allInterfaces()) {
        if (!(iface.flags() & QNetworkInterface::IsUp) || (iface.flags() & QNetworkInterface::IsLoopBack)) continue;
        for (const QNetworkAddressEntry& entry: iface.addressEntries())
            if (!entry.broadcast().isNull()) targets.append(entry.broadcast());
    }
    for (const QHostAddress& target: targets)
        (void)this->_discovery.writeDatagram(datagram, target, XMOUSE_DISCOVERY_PORT);

    // Old announces
    for (auto it = this->_announces.begin(); it != this->_announces.end();) {
        if (now_ms() - it->seen > ANNOUNCE_TTL) {it = this->_announces.erase(it); emit this->announced();}
        else ++it;
    }
}

void xmouse::net::Mesh::onDatagram_(void)
{
    while (this->_discovery.hasPendingDatagrams()) {
        const QNetworkDatagram datagram = this->_discovery.receiveDatagram(4096);
        const QJsonObject o = QJsonDocument::fromJson(datagram.data()).object();
        Announce a;

        if (o.value("xmouse").toInt() != XMOUSE_PROTOCOL_VERSION || !this->_ctx.group) continue;
        a.id = o.value("id").toString().left(64);
        a.name = o.value("name").toString().left(64);
        a.port = static_cast<quint16>(o.value("port").toInt());
        a.group = o.value("group").toString().left(32);
        a.network = o.value("network").toString().left(64);
        a.host = plain_address(datagram.senderAddress());
        a.seen = now_ms();
        if (a.id.isEmpty() || a.id == this->_ctx.group->self() || a.port == 0) continue;
        const bool fresh = !this->_announces.contains(a.id) || this->_announces.value(a.id).host != a.host;
        this->_announces[a.id] = a;
        if (fresh) emit this->announced();

        // A member heard on the LAN: its address first in the list, dialed at once when it has no link
        if (!a.group.isEmpty() && a.group == this->_ctx.group->tag() && this->_candidates.contains(a.id) && !this->_peers.contains(a.id)) {
            QList<QPair<QString, quint16>>& addresses = this->_candidates[a.id];
            const QPair<QString, quint16> address(a.host, a.port);
            if (!addresses.contains(address)) {addresses.prepend(address); this->dialAll_();}
        }
    }
}

QList<xmouse::net::Announce> xmouse::net::Mesh::announces(void) const
{
    QList<Announce> out;

    for (const Announce& a: this->_announces)
        if (now_ms() - a.seen <= ANNOUNCE_TTL) out.append(a);
    return out;
}
