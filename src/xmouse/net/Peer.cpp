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
##  @file Peer.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Attribute
#include <utils/utils.hpp>
#include "xmouse/security/Certificate.hpp"
#include "xmouse/security/PairingCode.hpp"
#include "xmouse/security/Crypto.hpp"
#include "xmouse/security/Group.hpp"
#include "xmouse/net/Protocol.hpp"
#include "xmouse/net/Peer.hpp"
#include <QJsonDocument>
#include <QSslSocket>
#include <QDateTime>

//----------------------------------------------------------------//
/* TOOLS */

_hot static inline qint64 now_ms(void) {return QDateTime::currentMSecsSinceEpoch();}

_cold static QByteArray from_b64(const QJsonValue& v)
{
    return QByteArray::fromBase64(v.toString().toLatin1());
}

_cold static QString to_b64(const QByteArray& data)
{
    return QString::fromLatin1(data.toBase64());
}

//----------------------------------------------------------------//
/* CLASS */

xmouse::net::Peer::Peer(WebSocket* ws, const PeerContext& ctx, const bool outgoing, QObject* parent)
: QObject(parent), _ws{ws}, _ctx{ctx}, _outgoing{outgoing}
{
    ws->setParent(this);
    this->_lastSeen = now_ms();
    QObject::connect(ws, &WebSocket::textReceived, this, &Peer::onText_);
    QObject::connect(ws, &WebSocket::binaryReceived, this, &Peer::onBinary_);
    QObject::connect(ws, &WebSocket::closed, this, [this]() {
        if (this->_state == PeerState::Closed) return;
        this->_state = PeerState::Closed;
        this->_ping.stop();
        emit this->closed(this->_closeReason.isEmpty() ? "connection closed" : this->_closeReason);
    });

    // Pings: the latency, and a dead link (a laptop closed, a wifi gone) noticed in XMOUSE_LINK_TIMEOUT
    this->_ping.setInterval(XMOUSE_PING_INTERVAL);
    QObject::connect(&this->_ping, &QTimer::timeout, this, [this]() {
        if (now_ms() - this->_lastSeen > XMOUSE_LINK_TIMEOUT) {this->close("no answer for " + QString::number(XMOUSE_LINK_TIMEOUT / 1000) + " s"); return;}
        if (this->_state == PeerState::Ready) this->sendJson(QJsonObject{{"t", "ping"}, {"ts", QString::number(now_ms())}});
    });
    this->_ping.start();
}

xmouse::net::Peer::~Peer()
{
    this->_ping.stop();
    if (this->_ws) this->_ws->disconnect(this);
}

xmouse::net::Peer* xmouse::net::Peer::accept(WebSocket* ws, const PeerContext& ctx, QObject* parent)
{
    Peer* peer = new Peer(ws, ctx, false, parent);

    peer->sendHello_();
    return peer;
}

xmouse::net::Peer* xmouse::net::Peer::dial(const QString& host, const quint16 port, const PeerContext& ctx, const QString& code, QObject* parent, const bool compat, const QByteArray& expected)
{
    WebSocket* ws = WebSocket::connectTo(host, port, "/ws", compat ? ctx.tlsCompat : ctx.tls);
    Peer* peer = new Peer(ws, ctx, true, parent);

    peer->_host = host;
    peer->_remotePort = port;
    peer->_code = security::normalize_code(code);
    peer->_mode = code.isEmpty() ? "member" : "join";
    peer->_expected = expected;
    QObject::connect(ws, &WebSocket::opened, peer, &Peer::onOpened_);
    QObject::connect(ws, &WebSocket::failed, peer, [peer](const QString& error) {peer->fail_(error);});
    return peer;
}

void xmouse::net::Peer::close(const QString& reason)
{
    if (this->_state == PeerState::Closed) return;
    this->_closeReason = reason;
    if (this->_ws) {
        this->sendJson(QJsonObject{{"t", "bye"}, {"reason", reason}});
        this->_ws->close(1000, reason.left(100));
        QTimer::singleShot(1500, this->_ws, [ws = this->_ws]() {if (ws) ws->abort();});
    }
}

void xmouse::net::Peer::fail_(const QString& reason)
{
    if (this->_state == PeerState::Closed) return;
    this->_closeReason = reason;
    if (this->_ws) this->_ws->abort();
    if (this->_state != PeerState::Closed) {
        this->_state = PeerState::Closed;
        this->_ping.stop();
        emit this->closed(reason);
    }
}

//----------------------------------------------------------------//
/* HANDSHAKE */

void xmouse::net::Peer::sendHello_(void)
{
    this->_serverNonce = security::random_bytes(32);
    this->_serverFingerprint = this->_ctx.fingerprint;
    this->_state = PeerState::Hello;
    this->sendJson(QJsonObject{
        {"t", "hello"},
        {"v", XMOUSE_PROTOCOL_VERSION},
        {"app", "xmouse"},
        {"version", XMOUSE_VERSION},
        {"id", this->_ctx.group->self()},
        {"name", this->_ctx.name},
        {"os", this->_ctx.os},
        {"port", this->_ctx.port},
        {"group", this->_ctx.group->tag()},
        {"nonce", to_b64(this->_serverNonce)},
        {"salt", this->_ctx.code ? to_b64(this->_ctx.code->salt()) : QString()},
    });
}

void xmouse::net::Peer::onOpened_(void)
{
    QSslSocket* socket = this->_ws ? this->_ws->socket() : nullptr;

    if (!socket) {this->fail_("no socket"); return;}
    this->_serverFingerprint = security::Certificate::fingerprintOf(socket->peerCertificate());
    if (this->_serverFingerprint.isEmpty()) {this->fail_("the device has no certificate"); return;}
    if (!this->_expected.isEmpty() && !security::constant_equal(this->_expected, this->_serverFingerprint)) {
        this->fail_("this is not the device of the link (another identity answered): nothing sent");
        return;
    }
    this->_state = PeerState::Hello;
}

void xmouse::net::Peer::onHello_(const QJsonObject& o)
{
    QByteArray proof;
    const QByteArray pinned = this->_ctx.pinned ? this->_ctx.pinned(o.value("id").toString()) : QByteArray();

    if (o.value("v").toInt() != XMOUSE_PROTOCOL_VERSION) {this->fail_("protocol version " + QString::number(o.value("v").toInt()) + " (this device speaks " + QString::number(XMOUSE_PROTOCOL_VERSION) + "): update xmouse"); return;}
    this->_device = o.value("id").toString().left(64);
    this->_name = o.value("name").toString().left(64);
    this->_os = o.value("os").toString().left(16);
    this->_remotePort = static_cast<quint16>(o.value("port").toInt(this->_remotePort));
    this->_serverNonce = from_b64(o.value("nonce"));
    if (this->_device.isEmpty() || this->_serverNonce.size() != 32) {this->fail_("malformed hello"); return;}
    if (this->_device == this->_ctx.group->self()) {this->fail_("this is this device"); return;}
    this->_clientNonce = security::random_bytes(32);

    // A member: its identity was pinned when it joined, another certificate is another device
    if (this->_mode == "member") {
        if (!this->_ctx.group->joined()) {this->fail_("not in a network"); return;}
        if (o.value("group").toString() != this->_ctx.group->tag()) {this->fail_("another network"); return;}
        if (!pinned.isEmpty() && !security::constant_equal(pinned, this->_serverFingerprint)) {this->fail_("the identity of " + this->_name + " changed (certificate): refused"); return;}
        proof = this->_ctx.group->memberProof(this->transcript_("client", this->_ctx.group->self()));
    } else {
        const QByteArray salt = from_b64(o.value("salt"));
        if (salt.size() != 16) {this->fail_("the device does not accept new members"); return;}
        this->_proofKey = security::argon2id(this->_code.toUtf8(), salt);
        proof = security::hmac_sha256(this->_proofKey, this->transcript_("client", this->_ctx.group->self()));
    }
    this->_state = PeerState::Auth;
    this->sendJson(QJsonObject{
        {"t", "auth"},
        {"mode", this->_mode},
        {"id", this->_ctx.group->self()},
        {"name", this->_ctx.name},
        {"os", this->_ctx.os},
        {"port", this->_ctx.port},
        {"kind", device_kind_name(DeviceKind::Desktop)},
        {"epoch", this->_ctx.group->epoch()},
        {"version", XMOUSE_VERSION},
        {"nonce", to_b64(this->_clientNonce)},
        {"proof", to_b64(proof)},
    });
}

void xmouse::net::Peer::onAuth_(const QJsonObject& o)
{
    const QByteArray proof = from_b64(o.value("proof"));
    QString refusal;

    this->_mode = o.value("mode").toString() == "join" ? "join" : "member";
    this->_device = o.value("id").toString().left(64);
    this->_name = o.value("name").toString().left(64);
    this->_os = o.value("os").toString().left(16);
    this->_remotePort = static_cast<quint16>(o.value("port").toInt());
    this->_clientNonce = from_b64(o.value("nonce"));
    if (this->_device.size() < 8 || this->_clientNonce.size() != 32 || proof.size() != 32) {this->fail_("malformed auth"); return;}
    if (this->_device == this->_ctx.group->self()) {this->fail_("this is this device"); return;}

    // Member: the group key (or the previous one, for a member that missed a rotation)
    if (this->_mode == "member") {
        const QByteArray transcript = this->transcript_("client", this->_device);
        refusal = this->_ctx.memberCheck ? this->_ctx.memberCheck(this->_device) : "no network";
        if (!refusal.isEmpty()) {this->fail_(refusal); return;}
        if (!security::constant_equal(this->_ctx.group->memberProof(transcript), proof)) {
            if (!security::constant_equal(this->_ctx.group->previousProof(transcript), proof)) {this->fail_("wrong member proof"); return;}
            this->_stale = true;
        }
        this->sendWelcome_();
        return;
    }

    // Join: the one time code shown here, then the local approval when asked
    if (!this->_ctx.code || !this->_ctx.group->joined()) {this->fail_("this device does not accept new members"); return;}
    if (this->_ctx.codeAllowed && !this->_ctx.codeAllowed(this->address())) {this->fail_("too many wrong codes from this address"); return;}
    if (!this->_ctx.code->checkProof(this->transcript_("client", this->_device), proof)) {
        if (this->_ctx.codeFailed) this->_ctx.codeFailed(this->address());
        this->fail_("wrong or expired code");
        return;
    }
    this->_proofKey = this->_ctx.code->serverProof(this->transcript_("server", this->_ctx.group->self())); // computed before the rotation below
    this->_ctx.code->rotate(); // one use only
    this->_state = PeerState::Approval;
    emit this->joinRequest();
}

void xmouse::net::Peer::approve(const bool accepted)
{
    if (this->_state != PeerState::Approval) return;
    if (!accepted) {this->close("refused on the device"); return;}
    this->sendWelcome_();
}

void xmouse::net::Peer::sendWelcome_(void)
{
    QJsonObject welcome{{"t", "welcome"}, {"mode", this->_mode}};

    if (this->_mode == "member") {
        const QByteArray transcript = this->transcript_("server", this->_ctx.group->self());
        welcome["proof"] = to_b64(this->_stale ? this->_ctx.group->previousProof(transcript) : this->_ctx.group->memberProof(transcript));
    } else {
        // Inside TLS 1.3 (post-quantum), after the proof of the code on both sides: the group key goes to the new member
        welcome["proof"] = to_b64(this->_proofKey);
        welcome["group"] = QJsonObject{
            {"id", this->_ctx.group->id()},
            {"name", this->_ctx.group->name()},
            {"key", to_b64(this->_ctx.group->key())},
            {"epoch", this->_ctx.group->epoch()},
        };
        welcome["state"] = this->_ctx.state ? this->_ctx.state() : QJsonObject();
        this->_proofKey.fill('\0');
    }
    this->sendJson(welcome);
    if (this->_stale) this->sendJson(QJsonObject{{"t", "rekey"}, {"key", to_b64(this->_ctx.group->key())}, {"epoch", this->_ctx.group->epoch()}});
    this->_state = PeerState::Ready;
    emit this->ready();
}

void xmouse::net::Peer::onWelcome_(const QJsonObject& o)
{
    const QByteArray proof = from_b64(o.value("proof"));
    const QByteArray transcript = this->transcript_("server", this->_device);

    // Mutual: a fake device that does not know the group key / the code is caught here
    if (this->_mode == "member") {
        if (!security::constant_equal(this->_ctx.group->memberProof(transcript), proof) && !security::constant_equal(this->_ctx.group->previousProof(transcript), proof)) {
            this->fail_("the device could not prove it is a member");
            return;
        }
        this->_state = PeerState::Ready;
        emit this->ready();
        return;
    }
    if (!security::constant_equal(security::hmac_sha256(this->_proofKey, transcript), proof)) {this->fail_("the device could not prove it knows the code"); return;}
    this->_proofKey.fill('\0');
    this->_mode = "member";
    this->_state = PeerState::Ready;
    emit this->joined(o);
    emit this->ready();
}

//----------------------------------------------------------------//
/* RECEIVE */

void xmouse::net::Peer::onText_(const QString& text)
{
    QJsonParseError error;
    QJsonObject o;
    QString type;

    this->_lastSeen = now_ms();
    if (text.size() > XMOUSE_MAX_CONTROL * 4) {this->fail_("control message too large"); return;}
    o = QJsonDocument::fromJson(text.toUtf8(), &error).object();
    if (error.error != QJsonParseError::NoError) {this->fail_("malformed message"); return;}
    type = o.value("t").toString();

    // Handshake: the only messages accepted before the authentication
    if (type == "bye") {this->_closeReason = o.value("reason").toString().left(200); if (this->_ws) this->_ws->close(); return;}
    if (this->_state != PeerState::Ready) {
        if (this->_outgoing && this->_state == PeerState::Hello && type == "hello") this->onHello_(o);
        else if (!this->_outgoing && this->_state == PeerState::Hello && type == "auth") this->onAuth_(o);
        else if (this->_outgoing && this->_state == PeerState::Auth && type == "welcome") this->onWelcome_(o);
        else if (this->_state != PeerState::Approval) this->fail_("unexpected " + type.left(32));
        return;
    }

    // Link keeping
    if (type == "ping") {this->sendJson(QJsonObject{{"t", "pong"}, {"ts", o.value("ts")}}); return;}
    if (type == "pong") {this->_rtt = static_cast<int>(now_ms() - o.value("ts").toString().toLongLong()); return;}
    emit this->message(o);
}

void xmouse::net::Peer::onBinary_(const QByteArray& data)
{
    this->_lastSeen = now_ms();
    if (this->_state != PeerState::Ready || data.isEmpty()) return;
    emit this->binary(data);
}
