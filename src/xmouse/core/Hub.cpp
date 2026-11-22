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
##  @file Hub.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Attribute
#define _Exception
#include <utils/utils.hpp>
#include "xmouse/security/Crypto.hpp"
#include "xmouse/net/Protocol.hpp"
#include "xmouse/core/Hub.hpp"
#include <QCoreApplication>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QClipboard>
#include <QJsonArray>
#include <functional>
#include <QMimeData>
#include <QDateTime>
#include <QUrlQuery>
#include <algorithm>
#include <QSysInfo>
#include <QSslKey>
#include <QBuffer>
#include <QScreen>
#include <QImage>
#include <cstdio>
#include <cmath>
#include <QUrl>

//----------------------------------------------------------------//
/* DEFINE */

#define FLUSH_INTERVAL 4 // ms between two moves sent (250 Hz at most, the latest position only)
#define BACKPRESSURE_LIMIT (256 * 1024) // bytes waiting on a link before the moves are skipped (the next one carries the position)
#define RESTORE_DELAY 40 // ms after the last button up before the real pointer goes back
#define WHEEL_RESTORE_DELAY 350 // ms after the last wheel step
#define PRESENCE_TTL 10000 // ms a presence stays on the map without news
#define JOIN_TIMEOUT 30000 // ms the local person has to accept a new device

//----------------------------------------------------------------//
/* TOOLS */

_hot static inline qint64 now_ms(void) {return QDateTime::currentMSecsSinceEpoch();}

// A graphical application (screens, clipboard, overlays): not the terminal panel of a machine without display
_cold static bool is_gui(void)
{
    return qobject_cast<QGuiApplication*>(QCoreApplication::instance()) != nullptr;
}

_cold static QList<xmouse::core::Screen> local_screens(void)
{
    QList<xmouse::core::Screen> screens;

    if (!is_gui()) return screens;
    for (QScreen* s: QGuiApplication::screens())
        screens.append(xmouse::core::Screen{s->name(), s->geometry(), s->devicePixelRatio()});
    return screens;
}

_cold static QString content_hash(const QString& mime, const QByteArray& data)
{
    return QString::fromLatin1(xmouse::security::sha256(mime.toUtf8() + '|' + data).toHex());
}

//----------------------------------------------------------------//
/* CLASS */

xmouse::core::Hub::Hub(Config& config, QObject* parent)
: QObject(parent), _config{config}, _log{Config::logDir()}, _certificate{Config::dataDir()}, _group{Config::dataDir()}
{
    this->_saveTimer.setSingleShot(true);
    this->_saveTimer.setInterval(1000);
    QObject::connect(&this->_saveTimer, &QTimer::timeout, this, [this]() {
        try {
            if (this->_group.joined()) this->_state.save(Config::dataDir() + "/state.json");
        } catch (const std::exception& e) {
            this->_log.warn("state", "", QString("cannot save the state: ") + e.what());
        }
    });
    this->_flush.setInterval(FLUSH_INTERVAL);
    this->_flush.setTimerType(Qt::PreciseTimer);
    QObject::connect(&this->_flush, &QTimer::timeout, this, &Hub::flushMove_);
    this->_presenceTimer.setInterval(XMOUSE_PRESENCE_INTERVAL);
    QObject::connect(&this->_presenceTimer, &QTimer::timeout, this, &Hub::sendPresence_);
    this->_housekeeping.setInterval(1000);
    QObject::connect(&this->_housekeeping, &QTimer::timeout, this, &Hub::housekeeping_);
    this->_restoreTimer.setSingleShot(true);
    QObject::connect(&this->_restoreTimer, &QTimer::timeout, this, &Hub::restorePointer_);
}

xmouse::core::Hub::~Hub()
{
    this->stop();
}

QColor xmouse::core::Hub::colorOf(const QString& id)
{
    const QByteArray h = security::sha256(id.toUtf8());
    const int hue = (static_cast<unsigned char>(h[0]) * 256 + static_cast<unsigned char>(h[1])) % 360;

    return QColor::fromHsl(hue, 190, 120);
}

QString xmouse::core::Hub::osName(void)
{
    const QString type = QSysInfo::productType();

    if (type == "windows" || type == "macos" || type == "android") return type;
    if (type == "osx") return "macos";
    return "linux";
}

void xmouse::core::Hub::setStatus_(const QString& status)
{
    if (status == this->_status) return;
    this->_status = status;
    emit this->statusChanged(status);
}

//----------------------------------------------------------------//
/* START */

void xmouse::core::Hub::start(void)
{
    const Settings& s = this->_config.settings();
    QSslConfiguration server = QSslConfiguration::defaultConfiguration();
    QSslConfiguration client = QSslConfiguration::defaultConfiguration();
    net::PeerContext& ctx = this->_mesh.context();
    Settings exposure = s;

    // Identity: certificate (pinned by the others), device id, network
    this->_certificate.load();
    this->_group.load();
    this->_state.setSelf(this->_group.self());
    if (this->_group.joined()) this->_state.load(Config::dataDir() + "/state.json");
    this->_code.configure(s.codeLength, s.codeLifetime);

    // Links
    ctx.group = &this->_group;
    ctx.code = &this->_code;
    ctx.fingerprint = this->_certificate.fingerprint();
    ctx.name = this->_config.deviceName();
    ctx.os = osName();
    security::configure_tls(client, true); // devices: post-quantum only
    ctx.tls = client;
    ctx.memberCheck = [this](const QString& id) -> QString {
        if (!this->_group.joined()) return "not in a network";
        if (this->_state.contains("removed/" + id)) return "removed from the network";
        if (!this->_state.contains("device/" + id)) return "unknown member (not in the network yet)";
        return "";
    };
    ctx.pinned = [this](const QString& id) {
        return QByteArray::fromHex(this->_state.value("device/" + id).toObject().value("fp").toString().toLatin1());
    };
    ctx.state = [this]() {return this->_state.toJson();};
    server.setLocalCertificate(this->_certificate.certificate());
    server.setPrivateKey(this->_certificate.key());
    security::configure_tls(server, false); // browsers (the web pad) may not speak ML-KEM yet
    this->_mesh.setMaxFailures(s.maxAuthFailures);
    this->_mesh.setNetworkName(this->_group.name());
    QObject::connect(&this->_mesh, &net::Mesh::peerReady, this, &Hub::onPeerReady_);
    QObject::connect(&this->_mesh, &net::Mesh::peerLost, this, &Hub::onPeerLost_);
    QObject::connect(&this->_mesh, &net::Mesh::message, this, &Hub::onMessage_);
    QObject::connect(&this->_mesh, &net::Mesh::binary, this, &Hub::onBinary_);
    QObject::connect(&this->_mesh, &net::Mesh::joined, this, &Hub::onJoined_);
    QObject::connect(&this->_mesh, &net::Mesh::joinFailed, this, [this](const QString& reason) {
        this->_log.warn("network", "", "join failed: " + reason);
        emit this->joinResult(false, reason);
    });
    QObject::connect(&this->_mesh, &net::Mesh::announced, this, &Hub::membersChanged);
    QObject::connect(&this->_mesh, &net::Mesh::joinRequest, this, [this](net::Peer* peer) {
        this->_log.info("network", peer->id(), "join request of " + peer->name() + " (" + peer->address().toString() + "), code valid");
        if (!this->_config.settings().joinApproval) {peer->approve(true); return;}
        QTimer::singleShot(JOIN_TIMEOUT, peer, [peer]() {peer->approve(false);}); // nobody answered: refused
        emit this->joinApproval(peer);
    });
    this->_mesh.start(s.bind, s.port, server, s.discovery);

    // How the others reach this device (the LAN announce of the mesh replaces the one of the exposure)
    exposure.discovery = false;
    this->_exposure.start(exposure, this->_mesh.port(), ctx.name, security::fingerprint_short(ctx.fingerprint));
    QObject::connect(&this->_exposure, &net::Exposure::changed, this, &Hub::publishSelf_);

    // Clipboard: Qt sees text and images, the X11 bridge (native Wayland) only text
    if (this->_clipboard.backend() == "qt") QObject::connect(QGuiApplication::clipboard(), &QClipboard::dataChanged, this, &Hub::onLocalClipboard_);
    else QObject::connect(&this->_clipboard, &platform::Clipboard::changed, this, &Hub::onLocalClipboard_);

    // Screens plugged / moved / resized
    if (is_gui()) {
        const std::function<void(void)> screens = [this]() {this->publishSelf_(); this->startInjector_();};
        QObject::connect(qGuiApp, &QGuiApplication::screenAdded, this, [this, screens](QScreen* screen) {
            QObject::connect(screen, &QScreen::geometryChanged, this, screens);
            screens();
        });
        QObject::connect(qGuiApp, &QGuiApplication::screenRemoved, this, screens);
        for (QScreen* screen: QGuiApplication::screens())
            QObject::connect(screen, &QScreen::geometryChanged, this, screens);
    }

    this->publishSelf_();
    this->rebuildLayout_();
    this->startInjector_();
    this->startCapture_();
    this->_housekeeping.start();
    this->_presenceTimer.start();
    this->_log.info("device", "", "started " + ctx.name + " on port " + QString::number(this->_mesh.port()) + " (" + security::tls_summary() + ")");
}

void xmouse::core::Hub::stop(void)
{
    if (this->_away) this->comeBack();
    if (this->_capture) this->_capture->stop();
    if (this->_injector) this->_injector->releaseAll();
    this->_housekeeping.stop();
    this->_presenceTimer.stop();
    this->_flush.stop();
    this->_mesh.stop();
    this->_exposure.stop();
    if (this->_saveTimer.isActive()) {
        this->_saveTimer.stop();
        try {
            if (this->_group.joined()) this->_state.save(Config::dataDir() + "/state.json");
        } catch (const std::exception& e) {
            std::fprintf(stderr, "xmouse: cannot save the state: %s\n", e.what());
        }
    }
}

void xmouse::core::Hub::startInjector_(void)
{
    QRect desktop = this->_layout.bounds(this->_group.self());

    if (desktop.isEmpty() && is_gui() && QGuiApplication::primaryScreen()) desktop = QGuiApplication::primaryScreen()->virtualGeometry();
    if (desktop.isEmpty()) {this->_injectorError = "no screen"; return;}
    if (this->_injector) this->_injector->releaseAll();
    this->_injectorError.clear();
    this->_injector = platform::create_input_injector(desktop.size(), this->_injectorError);
    if (!this->_injector) this->_log.warn("input", "", "no input injection: " + this->_injectorError + " (the cursors of the others are shown, not clicked)");
}

void xmouse::core::Hub::startCapture_(void)
{
    if (!is_gui()) {this->_captureError = "no display"; return;} // a member without screen: links, clipboard, relay
    if (QGuiApplication::platformName() == "offscreen") {this->_captureError = "offscreen"; return;} // snapshots, tests: no portal dialog
    this->_capture = platform::create_input_capture(this);
    QObject::connect(this->_capture.get(), &platform::IInputCapture::crossed, this, &Hub::onCrossed_);
    QObject::connect(this->_capture.get(), &platform::IInputCapture::motion, this, &Hub::onMotion_);
    QObject::connect(this->_capture.get(), &platform::IInputCapture::button, this, &Hub::onButton_);
    QObject::connect(this->_capture.get(), &platform::IInputCapture::wheel, this, &Hub::onWheel_);
    QObject::connect(this->_capture.get(), &platform::IInputCapture::key, this, &Hub::onKey_);
    QObject::connect(this->_capture.get(), &platform::IInputCapture::ready, this, [this]() {
        this->_captureError.clear();
        this->updateEdges_();
        this->setStatus_("edges: " + this->_capture->name());
    });
    QObject::connect(this->_capture.get(), &platform::IInputCapture::failed, this, [this](const QString& error) {
        this->_captureError = error;
        this->_log.warn("input", "", "edge capture: " + error);
        emit this->notice("Edge capture: " + error);
        this->setStatus_("edges unavailable: " + error);
    });
    QObject::connect(this->_capture.get(), &platform::IInputCapture::ended, this, [this]() {
        if (this->_away) this->goHome_(this->_layout.clampTo(this->_group.self(), this->_global.toPoint()));
    });
    if (!this->_capture->start()) {
        this->_captureError = this->_capture->error();
        this->_log.warn("input", "", "no edge capture: " + this->_captureError + " (the cursor of this device cannot leave it)");
        this->setStatus_("edges unavailable: " + this->_captureError);
    }
}

void xmouse::core::Hub::updateEdges_(void)
{
    QList<Edge> edges;

    if (!this->_capture) return;
    for (const Edge& e: this->_layout.exits(this->_group.self()))
        if (this->_mesh.isOnline(e.target)) edges.append(e); // an offline device is a wall
    this->_capture->setEdges(edges);
}

//----------------------------------------------------------------//
/* STATE */

void xmouse::core::Hub::publishSelf_(void)
{
    const QString key = "device/" + this->_group.self();
    const QJsonObject old = this->_state.value(key).toObject();
    QJsonArray addresses;
    QJsonObject entry;

    if (!this->_group.joined()) return;
    for (const QString& a: net::Exposure::lanAddresses())
        addresses.append(a);
    if (!net::Exposure::tailscaleAddress().isEmpty()) addresses.append(net::Exposure::tailscaleAddress());
    if (!this->_exposure.tailnet().dns.isEmpty()) addresses.append(this->_exposure.tailnet().dns);
    if (!this->_config.settings().publicHost.isEmpty()) addresses.append(this->_config.settings().publicHost);
    entry["name"] = this->_config.deviceName();
    entry["os"] = osName();
    entry["kind"] = net::device_kind_name(net::DeviceKind::Desktop);
    entry["version"] = XMOUSE_VERSION;
    entry["fp"] = QString::fromLatin1(this->_certificate.fingerprint().toHex());
    entry["port"] = this->_mesh.port();
    entry["addresses"] = addresses;
    entry["screens"] = screens_to_json(local_screens());
    entry["joined"] = old.contains("joined") ? old.value("joined") : QJsonValue(QString::number(now_ms()));
    this->broadcastDelta_(this->_state.set(key, entry));
    this->rebuildLayout_();
}

void xmouse::core::Hub::rebuildLayout_(void)
{
    Layout layout;
    const QString self = this->_group.self();

    for (const QString& id: this->_state.ids("device/")) {
        const QJsonObject device = this->_state.value("device/" + id).toObject();
        const QJsonObject position = this->_state.value("layout/" + id).toObject();

        if (this->_state.contains("removed/" + id)) continue;
        layout.setDevice(id, device.value("name").toString(), screens_from_json(device.value("screens").toArray()));
        if (!position.isEmpty()) layout.setPosition(id, QPoint(position.value("x").toInt(), position.value("y").toInt()));
    }
    layout.placeUnplaced();
    this->_layout = layout;

    // This device has no place yet: the default one is written (only by itself, the others keep a guess)
    if (this->_group.joined() && this->_layout.contains(self) && !this->_state.contains("layout/" + self)) {
        const QPoint p = this->_layout.device(self).position;
        this->broadcastDelta_(this->_state.set("layout/" + self, QJsonObject{{"x", p.x()}, {"y", p.y()}}));
    }
    this->refreshCandidates_();
    this->updateEdges_();
    emit this->layoutChanged();
    emit this->membersChanged();
}

void xmouse::core::Hub::refreshCandidates_(void)
{
    for (const QString& id: this->_state.ids("device/")) {
        const QJsonObject device = this->_state.value("device/" + id).toObject();
        const quint16 port = static_cast<quint16>(device.value("port").toInt(XMOUSE_DEFAULT_PORT));
        QList<QPair<QString, quint16>> addresses;

        if (id == this->_group.self()) continue;
        if (this->_state.contains("removed/" + id)) {this->_mesh.setCandidates(id, {}); continue;}
        for (const QString& a: this->_mesh.addressesOf(id))
            addresses.append({a, port});
        for (const QJsonValue& a: device.value("addresses").toArray())
            if (!addresses.contains({a.toString(), port})) addresses.append({a.toString(), port});
        this->_mesh.setCandidates(id, addresses);
    }
}

void xmouse::core::Hub::broadcastDelta_(const QJsonObject& delta)
{
    if (delta.isEmpty()) return;
    this->_mesh.broadcast(QJsonObject{{"t", "state"}, {"d", delta}});
    this->scheduleSave_();
}

void xmouse::core::Hub::applyDelta_(const QJsonObject& delta, const QString& from)
{
    const QStringList changed = this->_state.merge(delta);
    QJsonObject forward;
    bool layout = false;

    if (changed.isEmpty()) return;
    for (const QString& key: changed) {
        forward[key] = delta.value(key);
        if (key.startsWith("device/") || key.startsWith("layout/") || key.startsWith("removed/")) layout = true;
    }

    // Gossip: the members not linked to the sender get it through this device
    this->_mesh.broadcast(QJsonObject{{"t", "state"}, {"d", forward}}, from);
    this->scheduleSave_();

    // Removed from the network by another member
    if (changed.contains("removed/" + this->_group.self())) {
        emit this->notice("This device was removed from the network");
        this->_log.alert("network", from, "this device was removed from the network");
        this->leave();
        return;
    }
    for (const QString& key: changed) {
        if (!key.startsWith("removed/")) continue;
        const QString id = key.mid(8);
        if (this->_state.contains(key)) {this->_mesh.drop(id, "removed from the network"); this->foreignLeave_(id);}
    }
    if (changed.contains("net/name")) {
        this->_mesh.setNetworkName(this->_state.value("net/name").toString());
        emit this->networkChanged();
    }
    if (layout) this->rebuildLayout_();
}

//----------------------------------------------------------------//
/* NETWORK */

void xmouse::core::Hub::createNetwork(const QString& name)
{
    const QString label = name.trimmed().isEmpty() ? this->_config.deviceName() + " network" : name.trimmed().left(64);

    if (this->_group.joined()) this->leave();
    this->_group.create(label);
    this->_state.clear();
    this->_state.setSelf(this->_group.self());
    (void)this->_state.set("net/name", label);
    (void)this->_state.set("net/windowMove", true);
    this->_mesh.setNetworkName(label);
    this->_log.info("network", "", "network created: " + label);
    this->publishSelf_();
    this->scheduleSave_();
    emit this->networkChanged();
}

void xmouse::core::Hub::join(const QString& link, const QString& code)
{
    QString host = link.trimmed();
    QString pin = code;
    quint16 port = XMOUSE_DEFAULT_PORT;

    // xmouse://host:port?fp=..&code=.. (QR code), https://host:port, host[:port]
    if (host.startsWith("xmouse://") || host.startsWith("https://")) {
        const QUrl url(host);
        const QUrlQuery query(url);
        host = url.host();
        if (url.port() > 0) port = static_cast<quint16>(url.port());
        if (pin.isEmpty()) pin = query.queryItemValue("code");
    } else if (host.count(':') == 1) {
        port = static_cast<quint16>(host.section(':', 1).toUInt());
        host = host.section(':', 0, 0);
    }
    if (host.isEmpty() || port == 0) {emit this->joinResult(false, "invalid address"); return;}
    if (security::normalize_code(pin).size() < 6) {emit this->joinResult(false, "the code shown on the other device is needed"); return;}
    if (this->_group.joined()) this->leave();
    this->_log.info("network", "", "joining through " + host + ":" + QString::number(port));
    (void)this->_mesh.join(host, port, pin);
}

void xmouse::core::Hub::onJoined_(const QJsonObject& welcome, const QString& through)
{
    const QJsonObject g = welcome.value("group").toObject();

    try {
        this->_group.adopt(g.value("id").toString(), g.value("name").toString(), QByteArray::fromBase64(g.value("key").toString().toLatin1()), g.value("epoch").toInt());
    } catch (const std::exception& e) {
        emit this->joinResult(false, e.what());
        return;
    }
    this->_state.clear();
    this->_state.setSelf(this->_group.self());
    (void)this->_state.merge(welcome.value("state").toObject());
    this->_mesh.setNetworkName(this->_group.name());
    this->_log.info("network", through, "joined the network " + this->_group.name());
    this->publishSelf_();
    this->scheduleSave_();
    emit this->networkChanged();
    emit this->joinResult(true, "Joined " + this->_group.name());
}

void xmouse::core::Hub::leave(void)
{
    if (!this->_group.joined()) return;
    if (this->_away) this->comeBack();
    this->broadcastDelta_(this->_state.set("removed/" + this->_group.self(), true));
    this->_mesh.dropAll("left the network");
    this->_log.info("network", "", "left the network " + this->_group.name());
    for (const QString& id: this->_foreign.keys())
        this->foreignLeave_(id);
    this->_group.leave();
    this->_state.clear();
    this->_presence.clear();
    (void)QFile::remove(Config::dataDir() + "/state.json");
    this->rebuildLayout_();
    emit this->networkChanged();
    emit this->presenceChanged();
}

void xmouse::core::Hub::removeMember(const QString& id)
{
    QByteArray key;

    if (id == this->_group.self()) {this->leave(); return;}
    if (!this->_group.joined() || !this->_state.contains("device/" + id)) return;
    this->broadcastDelta_(this->_state.set("removed/" + id, true));
    this->broadcastDelta_(this->_state.remove("layout/" + id));

    // New key for the others only: the removed device keeps an old key that opens nothing
    key = security::random_bytes(XMOUSE_GROUP_KEY_SIZE);
    this->_group.rekey(key, this->_group.epoch() + 1);
    for (const QString& peer: this->_mesh.ids())
        if (peer != id) this->_mesh.send(peer, QJsonObject{{"t", "rekey"}, {"key", QString::fromLatin1(key.toBase64())}, {"epoch", this->_group.epoch()}});
    this->_mesh.drop(id, "removed from the network");
    this->foreignLeave_(id);
    this->_log.alert("network", id, "member removed (key rotated, epoch " + QString::number(this->_group.epoch()) + ")");
    this->rebuildLayout_();
}

void xmouse::core::Hub::renameNetwork(const QString& name)
{
    if (!this->_group.joined() || name.trimmed().isEmpty()) return;
    this->broadcastDelta_(this->_state.set("net/name", name.trimmed().left(64)));
    this->_mesh.setNetworkName(name.trimmed().left(64));
    emit this->networkChanged();
}

void xmouse::core::Hub::renameDevice(const QString& name)
{
    this->_config.settings().name = name.trimmed().left(64);
    this->_config.save();
    this->_mesh.context().name = this->_config.deviceName();
    this->publishSelf_();
}

QString xmouse::core::Hub::joinLink(void) const
{
    QString host = "";
    QUrl url;
    QUrlQuery query;

    for (const net::Endpoint& e: this->_exposure.endpoints())
        if (!e.host.isEmpty()) {host = e.host; break;}
    if (host.isEmpty() && !net::Exposure::lanAddresses().isEmpty()) host = net::Exposure::lanAddresses().front();
    if (host.isEmpty()) host = "127.0.0.1";
    url.setScheme("xmouse");
    url.setHost(host);
    url.setPort(this->_mesh.port());
    query.addQueryItem("fp", QString::fromLatin1(this->_certificate.fingerprint().toHex()));
    query.addQueryItem("code", this->_code.display());
    query.addQueryItem("net", this->_group.name());
    url.setQuery(query);
    return url.toString();
}

QList<xmouse::core::Member> xmouse::core::Hub::members(void) const
{
    QList<Member> out;

    for (const QString& id: this->_state.ids("device/")) {
        const QJsonObject d = this->_state.value("device/" + id).toObject();
        const net::Peer* peer = this->_mesh.peer(id);
        Member m;

        if (this->_state.contains("removed/" + id)) continue;
        m.id = id;
        m.name = d.value("name").toString();
        m.os = d.value("os").toString();
        m.version = d.value("version").toString();
        m.self = id == this->_group.self();
        m.online = m.self || peer != nullptr;
        m.rtt = peer ? peer->rtt() : -1;
        m.port = static_cast<quint16>(d.value("port").toInt());
        m.screens = screens_from_json(d.value("screens").toArray());
        m.color = colorOf(id);
        for (const QJsonValue& a: d.value("addresses").toArray())
            m.addresses.append(a.toString());
        out.append(m);
    }
    std::sort(out.begin(), out.end(), [](const Member& a, const Member& b) {return a.self != b.self ? a.self : a.name.toLower() < b.name.toLower();});
    return out;
}

QList<xmouse::net::Announce> xmouse::core::Hub::nearby(void) const
{
    QList<net::Announce> out;

    for (const net::Announce& a: this->_mesh.announces())
        if (!this->_group.joined() || a.group != this->_group.tag()) out.append(a);
    return out;
}

void xmouse::core::Hub::placeDevice(const QString& id, const QPoint& position)
{
    if (!this->_group.joined() || !this->_layout.contains(id)) return;
    this->broadcastDelta_(this->_state.set("layout/" + id, QJsonObject{{"x", position.x()}, {"y", position.y()}}));
    this->rebuildLayout_();
}

void xmouse::core::Hub::resetLayout(void)
{
    QStringList ids = this->_layout.devices().keys();
    int x = 0;

    ids.sort();
    for (const QString& id: ids) {
        const QRect b = this->_layout.bounds(id);
        if (b.isEmpty()) continue;
        this->broadcastDelta_(this->_state.set("layout/" + id, QJsonObject{{"x", x}, {"y", 0}}));
        x += b.width();
    }
    this->rebuildLayout_();
}

void xmouse::core::Hub::setNetworkSetting(const QString& name, const QJsonValue& value)
{
    if (!this->_group.joined()) return;
    this->broadcastDelta_(this->_state.set("net/" + name, value));
    emit this->networkChanged();
}

//----------------------------------------------------------------//
/* LINKS */

void xmouse::core::Hub::onPeerReady_(const QString& id)
{
    const net::Peer* peer = this->_mesh.peer(id);

    // A new member (the code was right and it was accepted here): it may join again after a leave / a removal
    if (peer && !peer->isOutgoing() && peer->mode() == "join") {
        this->broadcastDelta_(this->_state.remove("removed/" + id));
        this->_log.info("network", id, peer->name() + " joined the network");
        emit this->notice(peer->name() + " joined the network");
    }
    this->_mesh.send(id, QJsonObject{{"t", "state"}, {"d", this->_state.toJson()}});
    this->_log.info("network", id, "linked to " + (peer ? peer->name() : id) + (peer ? " (" + peer->address().toString() + ")" : QString()));
    if (this->_away && this->_target == id) this->_mesh.send(id, QJsonObject{{"t", "enter"}, {"x", this->_targetLocal.x()}, {"y", this->_targetLocal.y()}, {"name", this->_config.deviceName()}});
    this->updateEdges_();
    emit this->membersChanged();
}

void xmouse::core::Hub::onPeerLost_(const QString& id, const QString& reason)
{
    this->_log.info("network", id, "link lost: " + reason);
    if (this->_away && this->_target == id) this->goHome_(this->_layout.clampTo(this->_group.self(), this->_global.toPoint()));
    this->foreignLeave_(id);
    this->_presence.remove(id);
    this->updateEdges_();
    emit this->membersChanged();
    emit this->presenceChanged();
}

void xmouse::core::Hub::onMessage_(const QString& id, const QJsonObject& o)
{
    const QString type = o.value("t").toString();

    if (type == "state") {this->applyDelta_(o.value("d").toObject(), id); return;}
    if (type == "rekey") {
        this->_group.rekey(QByteArray::fromBase64(o.value("key").toString().toLatin1()), o.value("epoch").toInt());
        return;
    }
    if (type == "clip") {this->onRemoteClipboard_(id, o); return;}

    // Foreign cursor (it is the owner)
    if (type == "enter") {this->foreignEnter_(id, o); return;}
    if (type == "leave") {this->foreignLeave_(id); return;}

    // My cursor on the device id (it is the target)
    if (type == "deny" && this->_away && this->_target == id) {
        emit this->notice(this->_layout.device(id).name + " refused the cursor: " + o.value("reason").toString());
        this->goHome_(this->_layout.clampTo(this->_group.self(), this->_global.toPoint()));
        return;
    }
    if (type == "kick" && this->_away && this->_target == id) {
        this->goHome_(this->_layout.clampTo(this->_group.self(), this->_global.toPoint()));
        return;
    }
    if (type == "access") {
        emit this->notice(this->_layout.device(id).name + ": " + o.value("text").toString());
        return;
    }
}

void xmouse::core::Hub::onBinary_(const QString& id, const QByteArray& data)
{
    net::ByteReader r(data);

    switch (static_cast<net::Binary>(static_cast<std::uint8_t>(data[0]))) {
        case net::Binary::Pointer: {
            (void)r.u16();
            const int x = r.i32();
            const int y = r.i32();
            if (r.ok()) this->foreignPointer_(id, QPoint(x, y));
            break;
        }
        case net::Binary::Button: {
            const std::uint8_t b = r.u8();
            const bool down = r.u8() != 0;
            if (r.ok() && b <= static_cast<std::uint8_t>(net::MouseButton::Forward)) this->foreignButton_(id, static_cast<net::MouseButton>(b), down);
            break;
        }
        case net::Binary::Wheel: {
            const int dx = r.i16();
            const int dy = r.i16();
            if (r.ok()) this->foreignWheel_(id, dx, dy);
            break;
        }
        case net::Binary::Key: {
            const bool down = r.u8() != 0;
            const QString code = r.text8();
            if (r.ok() && !code.isEmpty()) this->foreignKey_(id, code, down);
            break;
        }
        case net::Binary::Text: {
            const QString text = QString::fromUtf8(r.rest()).left(4096);
            if (r.ok()) this->foreignText_(id, text);
            break;
        }
        case net::Binary::Presence: {
            Presence p;
            (void)r.u16();
            p.id = id;
            p.on = r.text8();
            const int x = r.i32(); // one read per statement: the order of the arguments of a call is not defined
            const int y = r.i32();
            p.global = QPoint(x, y);
            p.flags = r.u8();
            p.seen = now_ms();
            if (!r.ok()) break;
            this->_presence[id] = p;
            emit this->presenceChanged();
            break;
        }
        default:
            break;
    }
}

//----------------------------------------------------------------//
/* MY CURSOR */

void xmouse::core::Hub::onCrossed_(const QPoint& local, const QPoint& push)
{
    const QString self = this->_group.self();
    const QPoint global = this->_layout.toGlobal(self, local);
    const std::optional<Hit> hit = this->_layout.hit(global + push);

    // Nothing there (a layout changed meanwhile) or the device is not linked: the pointer stays here
    if (!hit || hit->device == self || !this->_mesh.isOnline(hit->device)) {
        this->_capture->release(local - push * 2);
        return;
    }
    this->_away = true;
    this->_target = hit->device;
    this->_global = QPointF(global + push);
    this->_targetLocal = hit->local;
    this->_keysDown.clear();
    this->_buttonsDown = 0;
    this->_mesh.send(this->_target, QJsonObject{{"t", "enter"}, {"x", hit->local.x()}, {"y", hit->local.y()}, {"name", this->_config.deviceName()}});
    this->_moveDirty = true;
    this->_presenceDirty = true;
    this->_flush.start();
    emit this->awayChanged();
}

void xmouse::core::Hub::onMotion_(const double dx, const double dy)
{
    QPoint from;
    QPoint to;
    std::optional<Hit> hit;

    if (!this->_away) return;
    from = this->_global.toPoint();
    this->_global += QPointF(dx, dy);
    to = this->_layout.move(from, this->_global.toPoint() - from);
    if (to != this->_global.toPoint()) this->_global = QPointF(to); // stopped on a border: no hidden drift
    hit = this->_layout.hit(to);
    if (!hit) return;
    if (hit->device == this->_group.self()) {this->goHome_(to); return;}
    if (hit->device != this->_target) {
        if (!this->_mesh.isOnline(hit->device)) {this->_global = QPointF(from); return;} // an offline device is a wall
        this->switchTarget_(hit->device);
    }
    this->_targetLocal = hit->local;
    this->_moveDirty = true;
    this->_presenceDirty = true;
    if (!this->_flush.isActive()) this->_flush.start();
}

void xmouse::core::Hub::flushMove_(void)
{
    const net::Peer* peer = this->_mesh.peer(this->_target);

    if (!this->_away) {this->_flush.stop(); return;}
    if (!this->_moveDirty || !peer) return;
    if (peer->pending() > BACKPRESSURE_LIMIT) return; // a slow link: the next flush sends the latest position
    this->_mesh.sendBinary(this->_target, net::ByteWriter(net::Binary::Pointer).u16(++this->_seq).i32(this->_targetLocal.x()).i32(this->_targetLocal.y()).data());
    this->_moveDirty = false;
}

void xmouse::core::Hub::switchTarget_(const QString& device)
{
    // Nothing stays held on the device left (a drag across devices drops there)
    for (int b = 0; b <= static_cast<int>(net::MouseButton::Forward); ++b)
        if (this->_buttonsDown & (1 << b)) this->_mesh.sendBinary(this->_target, net::ByteWriter(net::Binary::Button).u8(static_cast<std::uint8_t>(b)).u8(0).data());
    for (const QString& code: this->_keysDown)
        this->_mesh.sendBinary(this->_target, net::ByteWriter(net::Binary::Key).u8(0).text8(code).data());
    this->_buttonsDown = 0;
    this->_mesh.send(this->_target, QJsonObject{{"t", "leave"}});
    this->_target = device;
    this->_mesh.send(this->_target, QJsonObject{{"t", "enter"}, {"x", this->_targetLocal.x()}, {"y", this->_targetLocal.y()}, {"name", this->_config.deviceName()}});
    emit this->awayChanged();
}

void xmouse::core::Hub::goHome_(const QPoint& global)
{
    const QString self = this->_group.self();
    const std::optional<Hit> hit = this->_layout.hit(global);
    const QPoint local = hit && hit->device == self ? hit->local : this->_layout.bounds(self).center();

    if (!this->_away) return;
    for (int b = 0; b <= static_cast<int>(net::MouseButton::Forward); ++b)
        if (this->_buttonsDown & (1 << b)) this->_mesh.sendBinary(this->_target, net::ByteWriter(net::Binary::Button).u8(static_cast<std::uint8_t>(b)).u8(0).data());
    for (const QString& code: this->_keysDown)
        this->_mesh.sendBinary(this->_target, net::ByteWriter(net::Binary::Key).u8(0).text8(code).data());
    this->_mesh.send(this->_target, QJsonObject{{"t", "leave"}});
    this->_away = false;
    this->_target.clear();
    this->_keysDown.clear();
    this->_buttonsDown = 0;
    this->_flush.stop();
    this->_global = QPointF(global);
    if (this->_capture) this->_capture->release(local);
    this->_presenceDirty = true;
    emit this->awayChanged();
}

void xmouse::core::Hub::comeBack(void)
{
    QScreen* primary = is_gui() ? QGuiApplication::primaryScreen() : nullptr;

    if (!this->_away) return;
    this->goHome_(this->_layout.toGlobal(this->_group.self(), primary ? primary->geometry().center() : this->_layout.bounds(this->_group.self()).center()));
}

void xmouse::core::Hub::onButton_(const net::MouseButton button, const bool down)
{
    const int bit = 1 << static_cast<int>(button);

    if (!this->_away) return;
    if (down) this->_buttonsDown |= bit;
    else this->_buttonsDown &= ~bit;
    this->flushMove_(); // the click lands where the cursor is drawn
    this->_mesh.sendBinary(this->_target, net::ByteWriter(net::Binary::Button).u8(static_cast<std::uint8_t>(button)).u8(down ? 1 : 0).data());
    this->_presenceDirty = true;
}

void xmouse::core::Hub::onWheel_(const int dx, const int dy)
{
    if (!this->_away || (dx == 0 && dy == 0)) return;
    this->flushMove_();
    this->_mesh.sendBinary(this->_target, net::ByteWriter(net::Binary::Wheel).i16(static_cast<std::int16_t>(std::clamp(dx, -32000, 32000))).i16(static_cast<std::int16_t>(std::clamp(dy, -32000, 32000))).data());
}

void xmouse::core::Hub::onKey_(const QString& code, const bool down)
{
    const std::function<bool(const QString&, const QString&)> held = [this](const QString& a, const QString& b) {return this->_keysDown.contains(a) || this->_keysDown.contains(b);};

    if (!this->_away) return;
    if (down) this->_keysDown.insert(code);
    else this->_keysDown.remove(code);

    // Ctrl + Alt + Shift: back on this device at once (the modifiers are released on the other side)
    if (down && held("ControlLeft", "ControlRight") && held("AltLeft", "AltRight") && held("ShiftLeft", "ShiftRight")) {
        this->comeBack();
        return;
    }
    this->_mesh.sendBinary(this->_target, net::ByteWriter(net::Binary::Key).u8(down ? 1 : 0).text8(code).data());
}

void xmouse::core::Hub::sendPresence_(void)
{
    Presence mine;
    QString on;
    QPoint global;
    int flags = 0;

    if (!this->_presenceDirty || !this->_group.joined()) return;
    this->_presenceDirty = false;
    if (this->_away) {
        on = this->_target;
        global = this->_global.toPoint();
        if (this->_buttonsDown) flags |= XMOUSE_PRESENCE_DOWN;
    } else {
        on = this->_group.self();
        global = this->_capture && this->_capture->knowsPointer() ? this->_layout.toGlobal(on, this->_capture->pointer()) : this->_global.toPoint();
        flags |= XMOUSE_PRESENCE_HIDDEN;
    }
    mine = Presence{this->_group.self(), on, global, flags, now_ms()};
    this->_presence[mine.id] = mine;
    this->_mesh.broadcastBinary(net::ByteWriter(net::Binary::Presence).u16(++this->_seq).text8(on).i32(global.x()).i32(global.y()).u8(static_cast<std::uint8_t>(flags)).data());
    emit this->presenceChanged();
}

//----------------------------------------------------------------//
/* FOREIGN CURSORS */

void xmouse::core::Hub::foreignEnter_(const QString& id, const QJsonObject& o)
{
    const Rights rights = this->_config.rightsOf(id);
    Foreign f;

    if (this->_config.settings().privateMode) {this->_mesh.send(id, QJsonObject{{"t", "deny"}, {"reason", "private mode"}}); return;}
    if (!rights.mouse) {this->_mesh.send(id, QJsonObject{{"t", "deny"}, {"reason", "no mouse right here"}}); return;}
    f.id = id;
    f.name = this->_layout.device(id).name.isEmpty() ? o.value("name").toString().left(64) : this->_layout.device(id).name;
    f.color = colorOf(id);
    f.local = QPoint(o.value("x").toInt(), o.value("y").toInt());
    f.seen = now_ms();
    if (this->_foreign.contains(id)) f.state = this->_foreign.value(id).state;
    this->_foreign[id] = f;
    this->_log.info("input", id, "cursor of " + f.name + " entered");
    emit this->foreignChanged();
}

void xmouse::core::Hub::foreignLeave_(const QString& id)
{
    auto it = this->_foreign.find(id);

    if (it == this->_foreign.end()) return;
    if (this->_injector) {
        for (int b = 0; b <= static_cast<int>(net::MouseButton::Forward); ++b)
            if (it->buttons & (1 << b)) this->_injector->mouseButton(static_cast<net::MouseButton>(b), false);
        if (this->_keyboardOwner == id) this->_injector->releaseAll(); // no modifier left held by its keyboard
    }
    if (this->_keyboardOwner == id) this->_keyboardOwner.clear();
    if (it->state == ForeignState::Asking) emit this->accessEnded(id, false);
    this->_foreign.erase(it);
    this->_restoreTimer.start(RESTORE_DELAY);
    emit this->foreignChanged();
}

bool xmouse::core::Hub::blocked_(Foreign& f, const QPoint& at)
{
    bool over = false;

    if (f.state == ForeignState::Granted) return false;
    for (const QRect& r: this->_panelRects)
        if (r.contains(at)) over = true;
    if (!this->_panelFocused && !over) {
        if (f.state == ForeignState::Paused) {f.state = ForeignState::Free; emit this->foreignChanged();}
        return false;
    }

    // First touch: held and told; a second one asks the access (granted after the delay unless refused)
    if (f.state == ForeignState::Free) {
        f.state = ForeignState::Paused;
        this->_mesh.send(f.id, QJsonObject{{"t", "access"}, {"text", "the xmouse panel is protected: click again to ask the access"}});
    } else if (f.state == ForeignState::Paused && this->_config.settings().focusRequest) {
        f.state = ForeignState::Asking;
        f.askedAt = now_ms();
        this->_log.warn("access", f.id, f.name + " asks the access to the panel (" + QString::number(this->_config.settings().focusRequestDelay) + " s)");
        this->_mesh.send(f.id, QJsonObject{{"t", "access"}, {"text", "access asked, granted in " + QString::number(this->_config.settings().focusRequestDelay) + " s unless refused"}});
        emit this->accessRequested(f.id, f.name, this->_config.settings().focusRequestDelay);
    }
    emit this->foreignChanged();
    return true;
}

void xmouse::core::Hub::foreignPointer_(const QString& id, const QPoint& local)
{
    auto it = this->_foreign.find(id);
    const QRect desktop = this->_layout.bounds(this->_group.self());

    if (it == this->_foreign.end()) return;
    it->local = QPoint(std::clamp(local.x(), desktop.left(), std::max(desktop.left(), desktop.right())), std::clamp(local.y(), desktop.top(), std::max(desktop.top(), desktop.bottom())));
    it->seen = now_ms();
    if (it->state == ForeignState::Paused && !this->_panelFocused) {
        bool over = false;
        for (const QRect& r: this->_panelRects)
            if (r.contains(it->local)) over = true;
        if (!over) it->state = ForeignState::Free;
    }

    // A drag: the real pointer follows the cursor that holds the button
    if (it->buttons != 0 && this->_injector && it->state != ForeignState::Paused && it->state != ForeignState::Asking) this->_injector->mouseMove(this->injectPoint_(it->local).x(), this->injectPoint_(it->local).y());
    emit this->foreignChanged();
}

void xmouse::core::Hub::foreignButton_(const QString& id, const net::MouseButton button, const bool down)
{
    auto it = this->_foreign.find(id);
    const int bit = 1 << static_cast<int>(button);
    bool anyHeld = false;
    QPoint at;

    if (it == this->_foreign.end() || !this->_injector || !this->_config.rightsOf(id).mouse) return;
    if (down && this->blocked_(*it, it->local)) return;
    if (!down && !(it->buttons & bit)) return; // the down was held back: its up too
    for (const Foreign& f: this->_foreign)
        if (f.buttons != 0) anyHeld = true;

    // Teleported click: the real pointer goes there, clicks, and comes back after the last button up
    if (down && !anyHeld && !this->_hasSavedPointer && this->_config.settings().restorePointer && this->_capture && this->_capture->knowsPointer() && !this->_away) {
        this->_savedPointer = this->_capture->pointer();
        this->_hasSavedPointer = true;
    }
    this->_restoreTimer.stop();
    at = this->injectPoint_(it->local);
    this->_lastInject = now_ms();
    this->_injector->mouseMove(at.x(), at.y());
    this->_injector->mouseButton(button, down);
    if (down) {it->buttons |= bit; this->_keyboardOwner = id;}
    else it->buttons &= ~bit;
    anyHeld = false;
    for (const Foreign& f: this->_foreign)
        if (f.buttons != 0) anyHeld = true;
    if (!anyHeld) this->_restoreTimer.start(RESTORE_DELAY);
    emit this->foreignChanged();
}

void xmouse::core::Hub::foreignWheel_(const QString& id, const int dx, const int dy)
{
    auto it = this->_foreign.find(id);
    QPoint at;

    if (it == this->_foreign.end() || !this->_injector || !this->_config.rightsOf(id).mouse) return;
    if (this->blocked_(*it, it->local)) return;
    if (!this->_hasSavedPointer && this->_config.settings().restorePointer && this->_capture && this->_capture->knowsPointer() && !this->_away) {
        this->_savedPointer = this->_capture->pointer();
        this->_hasSavedPointer = true;
    }
    at = this->injectPoint_(it->local);
    this->_lastInject = now_ms();
    this->_injector->mouseMove(at.x(), at.y()); // the wheel scrolls what is under the pointer
    this->_injector->mouseWheel(dx, dy);
    this->_restoreTimer.start(WHEEL_RESTORE_DELAY);
}

void xmouse::core::Hub::foreignKey_(const QString& id, const QString& code, const bool down)
{
    auto it = this->_foreign.find(id);

    if (it == this->_foreign.end() || !this->_injector || !this->_config.rightsOf(id).keyboard) return;
    if (down && this->_panelFocused && it->state != ForeignState::Granted) {(void)this->blocked_(*it, it->local); return;}
    this->_keyboardOwner = id;
    this->_lastInject = now_ms();
    this->_injector->key(code, down);
}

void xmouse::core::Hub::foreignText_(const QString& id, const QString& text)
{
    auto it = this->_foreign.find(id);

    if (it == this->_foreign.end() || !this->_injector || !this->_config.rightsOf(id).keyboard) return;
    if (this->_panelFocused && it->state != ForeignState::Granted) {(void)this->blocked_(*it, it->local); return;}
    this->_lastInject = now_ms();
    this->_injector->text(text);
}

void xmouse::core::Hub::restorePointer_(void)
{
    QPoint at;

    for (const Foreign& f: this->_foreign)
        if (f.buttons != 0) return;
    if (!this->_hasSavedPointer || !this->_injector) return;
    at = this->injectPoint_(this->_savedPointer);
    this->_injector->mouseMove(at.x(), at.y());
    this->_hasSavedPointer = false;
}

void xmouse::core::Hub::setPanelFocused(const bool focused)
{
    this->_panelFocused = focused;
    if (focused) return; // held at their next click / key (blocked_), told then
    for (Foreign& f: this->_foreign) {
        if (f.state == ForeignState::Paused) f.state = ForeignState::Free;
    }
    emit this->foreignChanged();
}

void xmouse::core::Hub::panelUsedLocally(void)
{
    bool changed = false;

    for (Foreign& f: this->_foreign) {
        if (f.state != ForeignState::Granted && f.state != ForeignState::Asking) continue;
        if (f.state == ForeignState::Asking) emit this->accessEnded(f.id, false);
        f.state = ForeignState::Paused;
        changed = true;
    }
    if (changed) emit this->foreignChanged();
}

void xmouse::core::Hub::refuseAccess(const QString& id)
{
    auto it = this->_foreign.find(id);

    if (it == this->_foreign.end() || it->state != ForeignState::Asking) return;
    it->state = ForeignState::Paused;
    this->_log.info("access", id, "access to the panel refused for " + it->name);
    this->_mesh.send(id, QJsonObject{{"t", "access"}, {"text", "access to the panel refused"}});
    emit this->accessEnded(id, false);
    emit this->foreignChanged();
}

void xmouse::core::Hub::kickCursor(const QString& id)
{
    this->_mesh.send(id, QJsonObject{{"t", "kick"}});
    this->foreignLeave_(id);
}

void xmouse::core::Hub::setPrivateMode(const bool enabled)
{
    this->_config.settings().privateMode = enabled;
    this->_config.save();
    if (enabled)
        for (const QString& id: this->_foreign.keys())
            this->kickCursor(id);
    this->_log.info("settings", "", QString("private mode ") + (enabled ? "on" : "off"));
}

void xmouse::core::Hub::setRights(const QString& id, const Rights& rights)
{
    this->_config.settings().rights[id] = rights;
    this->_config.save();
    if (!rights.mouse && this->_foreign.contains(id)) this->kickCursor(id);
    this->_log.info("settings", id, "rights changed");
}

void xmouse::core::Hub::housekeeping_(void)
{
    const qint64 now = now_ms();
    bool presence = false;
    static int ticks = 0;

    // Access requests at the end of their delay
    for (Foreign& f: this->_foreign) {
        if (f.state != ForeignState::Asking || now - f.askedAt < static_cast<qint64>(this->_config.settings().focusRequestDelay) * 1000) continue;
        f.state = ForeignState::Granted;
        this->_log.warn("access", f.id, "access to the panel granted to " + f.name + " (not refused in time)");
        this->_mesh.send(f.id, QJsonObject{{"t", "access"}, {"text", "access to the panel granted"}});
        emit this->accessEnded(f.id, true);
        emit this->foreignChanged();
    }
    for (const QString& id: this->_foreign.keys())
        if (!this->_mesh.isOnline(id)) this->foreignLeave_(id);
    for (auto it = this->_presence.begin(); it != this->_presence.end();) {
        if (it.key() != this->_group.self() && now - it->seen > PRESENCE_TTL) {it = this->_presence.erase(it); presence = true;}
        else ++it;
    }
    if (presence) emit this->presenceChanged();
    if (!this->_away) this->_presenceDirty = this->_capture && this->_capture->knowsPointer();
    if (++ticks % 30 == 0) this->publishSelf_(); // addresses (dhcp, tailscale up / down)
    emit this->membersChanged(); // latencies
}

//----------------------------------------------------------------//
/* CLIPBOARD */

void xmouse::core::Hub::onLocalClipboard_(void)
{
    QJsonObject message{{"t", "clip"}};
    QByteArray data;
    QString mime;

    if (!this->_config.settings().clipboard || !this->_group.joined() || this->_mesh.ids().isEmpty()) return;
    if (this->_clipboard.backend() == "qt") {
        const QMimeData* m = QGuiApplication::clipboard()->mimeData();
        if (!m) return;
        if (m->hasImage()) {
            const QImage image = qvariant_cast<QImage>(m->imageData());
            QBuffer buffer(&data);
            buffer.open(QIODevice::WriteOnly);
            if (image.isNull() || !image.save(&buffer, "PNG")) return;
            mime = "image/png";
        } else if (m->hasText()) {
            data = m->text().toUtf8();
            mime = "text/plain";
        } else return;
    } else {
        data = this->_clipboard.text().toUtf8();
        mime = "text/plain";
    }
    if (data.isEmpty() || data.size() > XMOUSE_MAX_CLIPBOARD) return;
    const QString hash = content_hash(mime, data);
    if (hash == this->_lastClipboardHash) return; // set here by a member: not sent back
    this->_lastClipboardHash = hash;
    message["mime"] = mime;
    message["data"] = QString::fromLatin1(data.toBase64());
    this->_mesh.broadcast(message);
}

void xmouse::core::Hub::onRemoteClipboard_(const QString& id, const QJsonObject& o)
{
    const QString mime = o.value("mime").toString();
    const QByteArray data = QByteArray::fromBase64(o.value("data").toString().toLatin1());

    if (!this->_config.rightsOf(id).clipboard || data.isEmpty() || data.size() > XMOUSE_MAX_CLIPBOARD) return;
    this->_lastClipboardHash = content_hash(mime, data);
    if (mime == "image/png") {
        QImage image;
        if (!image.loadFromData(data, "PNG") || this->_clipboard.backend() != "qt") return;
        QGuiApplication::clipboard()->setImage(image);
    } else if (mime == "text/plain") {
        if (this->_clipboard.backend() == "qt") QGuiApplication::clipboard()->setText(QString::fromUtf8(data));
        else this->_clipboard.setText(QString::fromUtf8(data));
    }
}
