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
#include "xmouse/core/Routing.hpp"
#include "xmouse/core/Tools.hpp"
#include "xmouse/core/Hub.hpp"
#include <QStandardPaths>
#include <QJsonDocument>
#include <QClipboard>
#include <QJsonArray>
#include <functional>
#include <QFileInfo>
#include <QDateTime>
#include <QUrlQuery>
#include <algorithm>
#include <QSysInfo>
#include <QSslKey>
#include <QScreen>
#include <cstdio>
#include <QFile>
#include <QUrl>

//----------------------------------------------------------------//
/* DEFINE */

#define FLUSH_INTERVAL 4 // ms between two moves sent (250 Hz at most, the latest position only)
#define PREDICT_INTERVAL 8 // ms between two predicted positions of a foreign cursor
#define PRESENCE_TTL 10000 // ms a presence stays on the map without news
#define JOIN_TIMEOUT 30000 // ms the local person has to accept a new device
#define PUBLISH_DELAY 300 // ms after a link change before the device entry is written
#define PAD_MAP_INTERVAL 100 // ms between two maps sent to the pads
#define STALLED_TRANSFER 45000 // ms without a chunk before a transfer is dropped
#define OFFER_LIFETIME (60 * 60 * 1000) // ms an offer of files can be fetched
#define STREAM_TIMEOUT 10000 // ms without a frame before a viewer closes

//----------------------------------------------------------------//
/* TOOLS */

_cold static QList<xmouse::core::Screen> local_screens(void)
{
    QList<xmouse::core::Screen> screens;

    if (!xmouse::core::is_gui()) return screens;
    for (QScreen* s: QGuiApplication::screens())
        screens.append(xmouse::core::Screen{s->name(), s->geometry(), s->devicePixelRatio()});
    return screens;
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
    this->_predict.setInterval(PREDICT_INTERVAL);
    this->_predict.setTimerType(Qt::PreciseTimer);
    QObject::connect(&this->_predict, &QTimer::timeout, this, &Hub::predict_);
    this->_udpProbe.setInterval(XMOUSE_UDP_PROBE_INTERVAL);
    QObject::connect(&this->_udpProbe, &QTimer::timeout, this, &Hub::probeUdp_);
    QObject::connect(&this->_udp, &net::Datagrams::received, this, &Hub::onDatagram_);
    this->_publishSoon.setSingleShot(true);
    this->_publishSoon.setInterval(PUBLISH_DELAY);
    QObject::connect(&this->_publishSoon, &QTimer::timeout, this, &Hub::publishSelf_);
    this->_transferPump.setInterval(2);
    QObject::connect(&this->_transferPump, &QTimer::timeout, this, &Hub::pumpTransfers_);
    QObject::connect(&this->_streamTimer, &QTimer::timeout, this, &Hub::pumpStreams_);
    this->_padTimer.setInterval(PAD_MAP_INTERVAL);
    QObject::connect(&this->_padTimer, &QTimer::timeout, this, &Hub::sendPadMaps_);
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
    this->loadPads_();

    // Links
    ctx.group = &this->_group;
    ctx.code = &this->_code;
    ctx.fingerprint = this->_certificate.fingerprint();
    ctx.name = this->_config.deviceName();
    ctx.os = osName();
    security::configure_tls(client, true); // devices: post-quantum only
    client.setSslOption(QSsl::SslOptionDisableServerNameIndication, true); // no SNI: the pinned certificate, even on the tailnet name (the browsers asking it get the tailscale one)
    ctx.tls = client;
    ctx.tlsCompat = QSslConfiguration::defaultConfiguration();
    security::configure_tls(ctx.tlsCompat, false);
    ctx.tlsCompat.setSslOption(QSsl::SslOptionDisableServerNameIndication, true);
    ctx.postQuantum = [this](const QString& id) {return this->_state.value("device/" + id).toObject().value("pq").toBool(true);};
    ctx.memberCheck = [this](const QString& id) -> QString {
        if (!this->_group.joined()) return "not in a network";
        if (this->_state.contains("removed/" + id)) return "removed from the network";
        if (!this->_state.contains("device/" + id)) return "unknown member (not in the network yet)";
        if (!this->_state.value("device/" + id).toObject().value("via").toString().isEmpty()) return "a phone / pad links through its device only";
        return "";
    };
    ctx.pinned = [this](const QString& id) {
        return QByteArray::fromHex(this->_state.value("device/" + id).toObject().value("fp").toString().toLatin1());
    };
    ctx.state = [this]() {return this->_state.toJson();};
    server.setLocalCertificate(this->_certificate.certificate());
    server.setPrivateKey(this->_certificate.key());
    security::configure_tls(server, false); // browsers (the web pad) may not speak ML-KEM yet
    this->_serverTls = server;
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
    QObject::connect(&this->_mesh.server(), &net::Server::newPad, this, &Hub::onPadSocket_);
    this->_mesh.start(s.bind, s.port, server, s.discovery);

    // Encrypted udp of the moves on the same port number (tcp only when it is taken or disabled)
    this->_udp.setSelf(this->_group.self());
    this->_udp.setKeys(this->_group.key(), this->_group.epoch(), this->_group.previous());
    if (s.udp && !this->_udp.start(s.bind, this->_mesh.port())) this->_log.warn("network", "", "udp port " + QString::number(this->_mesh.port()) + " taken: the moves go by tcp");

    // https://<address>:<port>/ shows what this device is and how to join (its /api/info: nothing secret)
    this->_mesh.server().setInfo([this]() {
        QFile apk(":/android/version.txt"); // the android app embedded in this build ("" = none: the page says so)
        if (!apk.exists()) apk.setFileName(Config::dataDir() + "/version.txt");
        return QJsonObject{
            {"apk", apk.open(QIODevice::ReadOnly) ? QString::fromUtf8(apk.readAll()).trimmed().left(32) : QString()},
            {"app", "xmouse"},
            {"version", XMOUSE_VERSION},
            {"name", this->_config.deviceName()},
            {"network", this->_group.joined() ? this->_state.value("net/name").toString(this->_group.name()) : QString()},
            {"identity", security::fingerprint_short(this->_certificate.fingerprint())},
            {"pads", this->_config.settings().pads},
        };
    });

    // How the others reach this device (the LAN announce of the mesh replaces the one of the exposure)
    exposure.discovery = false;
    this->_exposure.start(exposure, this->_mesh.port(), ctx.name, security::fingerprint_short(ctx.fingerprint));
    QObject::connect(&this->_exposure, &net::Exposure::changed, this, &Hub::publishSelf_);

    // The browsers asking the tailnet name (HSTS preloaded: a self-signed certificate is refused without exception) get the tailscale certificate
    QObject::connect(&this->_exposure, &net::Exposure::certificateChanged, this, [this]() {
        const QString name = this->_exposure.certificateName();
        QSslConfiguration named = this->_serverTls;
        named.setLocalCertificateChain(this->_exposure.certificateChain());
        named.setPrivateKey(this->_exposure.certificateKey());
        this->_mesh.server().setNamedCertificate(name, name.isEmpty() ? QSslConfiguration() : named);
        if (!name.isEmpty()) this->_log.info("server", "", "Tailscale certificate for " + name + ": the browsers can open the page on this name");
    });

    // Clipboard: Qt sees text, images and files, the X11 bridge (native Wayland) only text
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
        if (QGuiApplication::platformName() != "offscreen") this->_windows = platform::create_windows();
    }

    this->publishSelf_();
    this->rebuildLayout_();
    this->startInjector_();
    this->startCapture_();
    this->_housekeeping.start();
    this->_presenceTimer.start();
    this->_udpProbe.start();
    this->_padTimer.start();
    this->_streamTimer.setInterval(1000 / std::max(1, s.windowFps));
    this->_log.info("device", "", "started " + ctx.name + " on port " + QString::number(this->_mesh.port()) + " (" + security::tls_summary() + (this->_udp.isBound() ? ", udp" : "") + ")");
}

void xmouse::core::Hub::stop(void)
{
    if (this->_away) this->comeBack();
    for (const QString& id: this->_streams.keys())
        this->stopStream(id);
    for (const QString& id: this->_transfers.keys())
        this->failTransfer_(id, "xmouse stopped", true);
    for (Pad& pad: this->_pads) {
        if (pad.control) pad.control->close(1001, "xmouse stopped");
        if (pad.display) pad.display->close(1001, "xmouse stopped");
    }
    if (this->_capture) this->_capture->stop();
    if (this->_injector) this->_injector->releaseAll();
    this->_housekeeping.stop();
    this->_presenceTimer.stop();
    this->_flush.stop();
    this->_predict.stop();
    this->_udpProbe.stop();
    this->_padTimer.stop();
    this->_streamTimer.stop();
    this->_transferPump.stop();
    this->_udp.stop();
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
    this->_edgesKnown = false; // a new capture: its edges sent at the first update
    QObject::connect(this->_capture.get(), &platform::IInputCapture::crossed, this, &Hub::onCrossed_);
    QObject::connect(this->_capture.get(), &platform::IInputCapture::motion, this, &Hub::onMotion_);
    QObject::connect(this->_capture.get(), &platform::IInputCapture::button, this, &Hub::onButton_);
    QObject::connect(this->_capture.get(), &platform::IInputCapture::wheel, this, &Hub::onWheel_);
    QObject::connect(this->_capture.get(), &platform::IInputCapture::key, this, &Hub::onKey_);
    QObject::connect(this->_capture.get(), &platform::IInputCapture::windowCrossed, this, &Hub::onWindowCrossed_);
    QObject::connect(this->_capture.get(), &platform::IInputCapture::ready, this, [this]() {
        this->_captureError.clear();
        this->updateEdges_();
        this->_capture->setWindowMove(this->windowMoveEnabled() && this->_windows != nullptr);
        if (this->_capture->name() == "portal" && qEnvironmentVariable("XDG_CURRENT_DESKTOP").contains("GNOME", Qt::CaseInsensitive)) {
            // Installed but not running: GNOME loads a new extension only at the next login (Wayland)
            const QString uuid = "/gnome-shell/extensions/xmouse@tsukini22.github.io/extension.js";
            const bool installed = QFileInfo::exists(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + uuid) || QFileInfo::exists("/usr/share" + uuid);
            if (installed) {
                this->setStatus_("edges: portal (the xmouse GNOME extension is installed but not loaded yet: log out and back in once, then no dialog)");
                this->_log.warn("input", "", "the xmouse GNOME extension is installed but GNOME has not loaded it: log out and back in once (the portal asks at every start until then)");
                emit this->notice("The xmouse GNOME extension is installed but not loaded yet: log out and back in once, the input capture dialog then never comes back");
            } else this->setStatus_("edges: portal (GNOME asks once per run: scripts/install.sh --gnome removes the dialog)");
        } else this->setStatus_("edges: " + this->_capture->name());
    });
    QObject::connect(this->_capture.get(), &platform::IInputCapture::failed, this, [this](const QString& error) {
        this->_captureError = error;
        this->_log.warn("input", "", "edge capture: " + error);
        emit this->notice("Edge capture: " + error);
        this->setStatus_("edges unavailable: " + error);
    });
    QObject::connect(this->_capture.get(), &platform::IInputCapture::ended, this, [this]() {
        if (!this->_away || this->_awayByPad) return;
        this->_log.info("input", this->_target, "the compositor ended the capture of the pointer (" + this->_capture->name() + "): the cursor comes back");
        this->goHome_(this->_layout.clampTo(this->_group.self(), this->_global.toPoint()));
    });
    if (!this->_capture->start()) {
        this->_captureError = this->_capture->error();
        this->_log.warn("input", "", "no edge capture: " + this->_captureError + " (the cursor of this device cannot leave it)");
        this->setStatus_("edges unavailable: " + this->_captureError);
        QTimer::singleShot(1500, this, [this]() {emit this->notice("Edges: " + this->_captureError);}); // once the panel is up
    }
}

void xmouse::core::Hub::updateEdges_(void)
{
    QList<Edge> edges;

    if (!this->_capture) return;
    for (const Edge& e: this->_layout.exits(this->_group.self()))
        if (this->reachable(e.target)) edges.append(e); // an offline device is a wall
    // The same edges (a pad that sends its screen again, a state applied, every 30 s): the capture keeps its barriers,
    // the portal of GNOME ended the capture under the cursor when they were set again (the cursor came back by itself)
    if (this->_edgesKnown && edges == this->_edgesSent) return;
    this->_edgesSent = edges;
    this->_edgesKnown = true;
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
    // Every path of the exposure ("host" on the port of this device, "host:port" through a tunnel), then the LAN / tailnet ones
    for (const net::Endpoint& e: this->_exposure.endpoints()) {
        if (e.label == "This PC" || e.host.isEmpty()) continue;
        addresses.append(e.port == this->_mesh.port() ? e.host : e.host + ":" + QString::number(e.port));
        if (!e.ip.isEmpty()) addresses.append(e.port == this->_mesh.port() ? e.ip : e.ip + ":" + QString::number(e.port));
    }
    for (const QString& a: net::Exposure::lanAddresses())
        addresses.append(a);
    if (!net::Exposure::tailscaleAddress().isEmpty()) addresses.append(net::Exposure::tailscaleAddress());
    if (!this->_exposure.tailnet().dns.isEmpty()) addresses.append(this->_exposure.tailnet().dns);
    if (!this->_config.settings().publicHost.isEmpty()) addresses.append(this->_config.settings().publicHost);
    {
        QStringList unique;
        for (const QJsonValue& a: addresses)
            if (!unique.contains(a.toString())) unique.append(a.toString());
        addresses = QJsonArray::fromStringList(unique);
    }
    entry["name"] = this->_config.deviceName();
    entry["os"] = osName();
    entry["kind"] = net::device_kind_name(net::DeviceKind::Desktop);
    entry["version"] = XMOUSE_VERSION;
    entry["fp"] = QString::fromLatin1(this->_certificate.fingerprint().toHex());
    entry["pq"] = security::pq_available(); // the others dial this device post-quantum only when it can
    entry["port"] = this->_mesh.port();
    entry["addresses"] = addresses;
    entry["screens"] = screens_to_json(local_screens());
    entry["links"] = QJsonArray::fromStringList(this->links_()); // the others relay through this device to them
    entry["relay"] = this->_config.settings().relay;
    entry["udp"] = this->_udp.isBound();
    entry["windows"] = this->_windows != nullptr;
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

    // This device has no place yet: the default one is written (only by itself, the others keep a guess); its pads too
    if (this->_group.joined()) {
        for (const QString& id: this->_layout.devices().keys()) {
            const bool mine = id == self || this->_pads.contains(id);
            if (!mine || this->_state.contains("layout/" + id) || this->_layout.bounds(id).isEmpty()) continue;
            const QPoint p = this->_layout.device(id).position;
            this->broadcastDelta_(this->_state.set("layout/" + id, QJsonObject{{"x", p.x()}, {"y", p.y()}}));
        }
    }
    this->refreshCandidates_();
    this->updateEdges_();
    if (this->_away) this->keepAway_(); // the device of my cursor changed under it
    emit this->layoutChanged();
    emit this->membersChanged();
    emit this->presenceChanged(); // the cursors follow their devices
}

void xmouse::core::Hub::refreshCandidates_(void)
{
    for (const QString& id: this->_state.ids("device/")) {
        const QJsonObject device = this->_state.value("device/" + id).toObject();
        const quint16 port = static_cast<quint16>(device.value("port").toInt(XMOUSE_DEFAULT_PORT));
        QList<QPair<QString, quint16>> addresses;

        if (id == this->_group.self()) continue;
        if (this->_state.contains("removed/" + id) || !device.value("via").toString().isEmpty()) {this->_mesh.setCandidates(id, {}); continue;} // a pad is reached through its device
        for (const QString& a: this->_mesh.addressesOf(id))
            addresses.append({a, port});
        for (const QJsonValue& a: device.value("addresses").toArray()) {
            // "host" on the port of the member, "host:port" through a tunnel (bore, ngrok, UPnP)
            const QString text = a.toString();
            const QPair<QString, quint16> address = text.count(':') == 1 ? QPair<QString, quint16>(text.section(':', 0, 0), static_cast<quint16>(text.section(':', 1).toUInt())) : QPair<QString, quint16>(text, port);
            if (!address.first.isEmpty() && address.second != 0 && !addresses.contains(address)) addresses.append(address);
        }
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
    if (changed.contains("net/windowMove")) {
        if (this->_capture) this->_capture->setWindowMove(this->windowMoveEnabled() && this->_windows != nullptr);
        if (!this->windowMoveEnabled())
            for (const QString& id: this->_streams.keys())
                this->stopStream(id);
        emit this->networkChanged();
    }
    for (const QString& key: changed)
        if (key.startsWith("profile/")) {emit this->layoutChanged(); break;}
    if (layout) this->rebuildLayout_();
}

//----------------------------------------------------------------//
/* NETWORK */

void xmouse::core::Hub::createNetwork(const QString& name)
{
    const QString label = name.trimmed().isEmpty() ? this->_config.deviceName() + " network" : name.trimmed().left(64);

    if (this->_group.joined()) this->leave();
    this->_group.create(label);
    this->_udp.setKeys(this->_group.key(), this->_group.epoch(), QByteArray());
    this->_state.clear();
    this->_state.setSelf(this->_group.self());
    (void)this->_state.set("net/name", label);
    (void)this->_state.set("net/windowMove", true);
    this->_mesh.setNetworkName(label);
    this->_log.info("network", "", "network created: " + label);
    this->publishSelf_();
    for (const QString& id: this->_pads.keys())
        this->publishPad_(id);
    this->scheduleSave_();
    emit this->networkChanged();
}

void xmouse::core::Hub::join(const QString& link, const QString& code)
{
    QString host = link.trimmed();
    QString pin = code;
    QByteArray expected;
    quint16 port = XMOUSE_DEFAULT_PORT;

    // https://host:port/?code=..&fp=..&net=.. (QR code, copied link), the old xmouse:// links, host[:port]
    if (host.startsWith("https://") || host.startsWith("xmouse://")) {
        const QUrl url(host);
        const QUrlQuery query(url);
        host = url.host();
        if (url.port() > 0) port = static_cast<quint16>(url.port());
        if (pin.isEmpty()) pin = query.queryItemValue("code");
        expected = QByteArray::fromHex(query.queryItemValue("fp").toLatin1()); // the identity of the device that gave the link
    } else if (host.count(':') == 1) {
        port = static_cast<quint16>(host.section(':', 1).toUInt());
        host = host.section(':', 0, 0);
    }
    if (host.isEmpty() || port == 0) {emit this->joinResult(false, "invalid address"); return;}
    if (security::normalize_code(pin).size() < 6) {emit this->joinResult(false, "the code shown on the other device is needed"); return;}
    if (this->_group.joined()) this->leave();
    this->_log.info("network", "", "joining through " + host + ":" + QString::number(port));
    (void)this->_mesh.join(host, port, pin, expected.size() == 32 ? expected : QByteArray());
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
    this->_udp.setKeys(this->_group.key(), this->_group.epoch(), QByteArray());
    this->_state.clear();
    this->_state.setSelf(this->_group.self());
    (void)this->_state.merge(welcome.value("state").toObject());
    this->_mesh.setNetworkName(this->_group.name());
    this->_log.info("network", through, "joined the network " + this->_group.name());
    this->publishSelf_();
    for (const QString& id: this->_pads.keys())
        this->publishPad_(id);
    this->scheduleSave_();
    emit this->networkChanged();
    emit this->joinResult(true, "Joined " + this->_group.name());
}

void xmouse::core::Hub::leave(void)
{
    if (!this->_group.joined()) return;
    if (this->_away) this->comeBack();
    for (const QString& id: this->_streams.keys())
        this->stopStream(id);
    for (const QString& id: this->_transfers.keys())
        this->failTransfer_(id, "left the network", true);
    this->broadcastDelta_(this->_state.set("removed/" + this->_group.self(), true));
    this->_mesh.dropAll("left the network");
    this->_log.info("network", "", "left the network " + this->_group.name());
    for (const QString& id: this->_foreign.keys())
        this->foreignLeave_(id);
    this->_group.leave();
    this->_udp.setKeys(QByteArray(), 0, QByteArray());
    this->_udpUp.clear();
    this->_udpPath.clear();
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
    if (this->_pads.contains(id)) {this->removePad(id); return;}
    if (!this->_group.joined() || !this->_state.contains("device/" + id)) return;
    this->broadcastDelta_(this->_state.set("removed/" + id, true));
    this->broadcastDelta_(this->_state.remove("layout/" + id));

    // New key for the others only: the removed device keeps an old key that opens nothing
    key = security::random_bytes(XMOUSE_GROUP_KEY_SIZE);
    this->_group.rekey(key, this->_group.epoch() + 1);
    this->_udp.setKeys(this->_group.key(), this->_group.epoch(), this->_group.previous());
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

void xmouse::core::Hub::setExposure(const Exposure exposure)
{
    Settings settings = this->_config.settings();

    if (exposure == settings.exposure) return;
    this->_config.settings().exposure = exposure;
    this->_config.save();
    settings.exposure = exposure;
    settings.discovery = false; // the LAN announce is the one of the mesh
    this->_exposure.start(settings, this->_mesh.port(), this->_config.deviceName(), security::fingerprint_short(this->_certificate.fingerprint()));
    this->_log.info("settings", "", "reachable through " + exposure_name(exposure));
}

void xmouse::core::Hub::renameDevice(const QString& name)
{
    this->_config.settings().name = name.trimmed().left(64);
    this->_config.save();
    this->_mesh.context().name = this->_config.deviceName();
    this->publishSelf_();
}

QString xmouse::core::Hub::joinLink(const QString& host, const quint16 port) const
{
    QString target = host;
    quint16 targetPort = port;
    QUrl url;
    QUrlQuery query;

    // The first address another device can use (the public / tailnet ones come first, "This PC" last)
    for (const net::Endpoint& e: this->_exposure.endpoints()) {
        if (!target.isEmpty() || e.host.isEmpty() || e.label == "This PC") continue;
        target = QUrl(e.url).host().isEmpty() ? e.host : QUrl(e.url).host(); // a browser opens it: the tailnet name only with its certificate
        targetPort = e.port;
    }
    if (target.isEmpty() && !net::Exposure::lanAddresses().isEmpty()) target = net::Exposure::lanAddresses().front();
    if (target.isEmpty()) target = "127.0.0.1";
    url.setScheme("https"); // a browser opens the page of the device (how to join), xmouse joins with it
    url.setHost(target);
    url.setPort(targetPort == 0 ? this->_mesh.port() : targetPort);
    url.setPath("/");
    query.addQueryItem("code", this->_code.display());
    query.addQueryItem("fp", QString::fromLatin1(this->_certificate.fingerprint().toHex()));
    query.addQueryItem("net", this->_group.name());
    url.setQuery(query);
    return url.toString();
}

QString xmouse::core::Hub::padLink(const QString& host, const quint16 port) const
{
    QUrl url(this->joinLink(host, port));
    QUrlQuery query(url);

    url.setPath("/pad");
    query.removeQueryItem("net");
    url.setQuery(query);
    return url.toString();
}

QList<xmouse::core::Member> xmouse::core::Hub::members(void) const
{
    QList<Member> out;

    for (const QString& id: this->_state.ids("device/")) {
        const QJsonObject d = this->_state.value("device/" + id).toObject();
        const net::Peer* peer = this->_mesh.peer(id);
        const Route route = this->routeTo_(id);
        Member m;

        if (this->_state.contains("removed/" + id)) continue;
        m.id = id;
        m.name = d.value("name").toString();
        m.os = d.value("os").toString();
        m.kind = d.value("kind").toString("desktop");
        m.version = d.value("version").toString();
        m.via = d.value("via").toString();
        m.self = id == this->_group.self();
        m.online = m.self || route.kind != RouteKind::None;
        if (!m.via.isEmpty()) m.online = d.value("online").toBool(false) && (m.via == this->_group.self() || this->reachable(m.via)); // a pad: connected to its device now
        m.relayed = route.kind == RouteKind::Relay && m.via.isEmpty();
        m.udp = this->udpUp(id);
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

//----------------------------------------------------------------//
/* LAYOUT */

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

QStringList xmouse::core::Hub::profiles(void) const
{
    QStringList names = this->_state.ids("profile/");

    names.sort(Qt::CaseInsensitive);
    return names;
}

void xmouse::core::Hub::saveProfile(const QString& name)
{
    const QString label = name.trimmed().left(48).replace('/', '-');
    QJsonObject layout;

    if (!this->_group.joined() || label.isEmpty()) return;
    for (const Device& d: this->_layout.devices())
        layout[d.id] = QJsonObject{{"x", d.position.x()}, {"y", d.position.y()}};
    this->broadcastDelta_(this->_state.set("profile/" + label, QJsonObject{{"layout", layout}, {"saved", QString::number(now_ms())}, {"by", this->_config.deviceName()}}));
    this->_log.info("layout", "", "layout profile saved: " + label);
    emit this->layoutChanged();
}

void xmouse::core::Hub::applyProfile(const QString& name)
{
    const QHash<QString, QPoint> positions = profile_positions(this->_state, name);

    if (!this->_group.joined() || positions.isEmpty()) return;
    for (auto it = positions.constBegin(); it != positions.constEnd(); ++it)
        if (this->_layout.contains(it.key())) this->broadcastDelta_(this->_state.set("layout/" + it.key(), QJsonObject{{"x", it.value().x()}, {"y", it.value().y()}}));
    this->_log.info("layout", "", "layout profile applied: " + name);
    this->rebuildLayout_();
}

void xmouse::core::Hub::deleteProfile(const QString& name)
{
    if (!this->_group.joined()) return;
    this->broadcastDelta_(this->_state.remove("profile/" + name));
    emit this->layoutChanged();
}

void xmouse::core::Hub::setNetworkSetting(const QString& name, const QJsonValue& value)
{
    if (!this->_group.joined()) return;
    this->broadcastDelta_(this->_state.set("net/" + name, value));
    if (name == "windowMove") {
        if (this->_capture) this->_capture->setWindowMove(value.toBool() && this->_windows != nullptr);
        if (!value.toBool())
            for (const QString& id: this->_streams.keys())
                this->stopStream(id);
    }
    emit this->networkChanged();
}

bool xmouse::core::Hub::windowMoveEnabled(void) const
{
    return this->_group.joined() && this->_state.value("net/windowMove").toBool(true);
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
    this->_publishSoon.start(); // links of this device (the others may relay through it)
    this->probeUdp_();
    this->updateEdges_();
    emit this->membersChanged();
}

void xmouse::core::Hub::onPeerLost_(const QString& id, const QString& reason)
{
    this->_log.info("network", id, "link lost: " + reason);
    this->_udpUp.remove(id);
    this->_udpPath.remove(id);
    this->_udp.forget(id);
    this->_publishSoon.start();

    // Still reachable through another member: the cursors and the transfers go on through it
    if (!this->reachable(id)) {
        if (this->_away && this->_target == id) this->goHome_(this->_layout.clampTo(this->_group.self(), this->_global.toPoint()));
        this->foreignLeave_(id);
        this->_presence.remove(id);
    }
    this->updateEdges_();
    emit this->membersChanged();
    emit this->presenceChanged();
}

void xmouse::core::Hub::onMessage_(const QString& id, const QJsonObject& o)
{
    const QString type = o.value("t").toString();

    if (type == "relay") {this->onRelay_(id, o); return;}
    if (type == "state") {
        if (this->_mesh.isOnline(id)) this->applyDelta_(o.value("d").toObject(), id); // members only (never a relayed / pad sender)
        return;
    }
    if (type == "rekey") {
        if (!this->_mesh.isOnline(id)) return;
        this->_group.rekey(QByteArray::fromBase64(o.value("key").toString().toLatin1()), o.value("epoch").toInt());
        this->_udp.setKeys(this->_group.key(), this->_group.epoch(), this->_group.previous());
        return;
    }
    if (type == "clip") {this->onRemoteClipboard_(id, o); return;}
    if (type == "files") {this->onFilesOffer_(id, o); return;}
    if (type == "fget") {this->onFileGet_(id, o); return;}
    if (type == "fend") {this->onFileEnd_(id, o); return;}
    if (type == "link") {this->onLink_(id, o); return;}
    if (type.startsWith("w")) {this->onStreamMessage_(id, o); return;} // window streams: wopen, wclose, winput, wdoc

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

    if (data.isEmpty()) return;
    switch (static_cast<net::Binary>(static_cast<std::uint8_t>(data[0]))) {
        case net::Binary::Pointer: {
            const quint16 seq = r.u16();
            const int x = r.i32();
            const int y = r.i32();
            if (r.ok()) this->foreignPointer_(id, QPoint(x, y), seq, true);
            break;
        }
        case net::Binary::Button: {
            const std::uint8_t b = r.u8();
            const bool down = r.u8() != 0;
            if (!r.ok() || b > static_cast<std::uint8_t>(net::MouseButton::Forward)) break;
            if (r.remaining() >= 8) { // where the click is (the moves may come late by udp)
                const int x = r.i32();
                const int y = r.i32();
                if (r.ok()) this->foreignPointer_(id, QPoint(x, y), 0, false);
            }
            this->foreignButton_(id, static_cast<net::MouseButton>(b), down);
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
            QString code = r.text8();
            const QString character = r.remaining() > 0 ? r.text8() : QString(); // a shortcut of a phone: its letter
            if (!character.isEmpty() && this->_injector && !this->_injector->codeOf(character).isEmpty()) code = this->_injector->codeOf(character);
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
            p.seq = r.u16();
            p.id = id;
            p.on = r.text8();
            const int x = r.i32(); // one read per statement: the order of the arguments of a call is not defined
            const int y = r.i32();
            p.global = QPoint(x, y);
            p.local = this->_layout.toLocal(p.on, p.global);
            p.flags = r.u8();
            p.seen = now_ms();
            if (!r.ok()) break;
            if (this->_presence.contains(id) && static_cast<qint16>(p.seq - this->_presence.value(id).seq) <= 0 && now_ms() - this->_presence.value(id).seen < 2000) break; // late (reordered udp)
            this->_presence[id] = p;
            emit this->presenceChanged();
            break;
        }
        case net::Binary::Relay:
            this->onRelayBinary_(id, data);
            break;
        case net::Binary::FileChunk:
            this->onFileChunk_(id, data);
            break;
        case net::Binary::WindowFrame:
            this->onWindowFrame_(id, data);
            break;
        default:
            break;
    }
}

//----------------------------------------------------------------//
/* HOUSEKEEPING */

void xmouse::core::Hub::housekeeping_(void)
{
    const qint64 now = now_ms();
    bool presence = false;
    static int ticks = 0;

    // Access requests at the end of their delay
    for (Foreign& f: this->_foreign) {
        if (f.state != ForeignState::Asking || now - f.askedAt < static_cast<qint64>(this->_config.settings().focusRequestDelay) * 1000) continue;
        f.state = ForeignState::Granted;
        this->_granted.insert(f.id);
        this->_log.warn("access", f.id, "access to the panel granted to " + f.name + " (not refused in time)");
        this->sendTo_(f.id, QJsonObject{{"t", "access"}, {"text", "access to the panel granted"}});
        emit this->accessEnded(f.id, true);
        emit this->foreignChanged();
    }
    for (const QString& id: this->_foreign.keys())
        if (!this->_pads.contains(id) && !this->reachable(id)) this->foreignLeave_(id); // the own cursor of a pad goes with its socket
    for (const QString& pad: this->_padAway.keys()) // the own cursor of a pad on a device gone
        if (!this->_layout.contains(this->_padAway.value(pad).target) || !this->reachable(this->_padAway.value(pad).target)) this->padAwayEnd_(pad);
    for (auto it = this->_presence.begin(); it != this->_presence.end();) {
        const bool padHere = this->_pads.contains(it.key()) && this->_foreign.contains(it.key()); // still, but still here
        if (it.key() != this->_group.self() && !padHere && now - it->seen > PRESENCE_TTL) {it = this->_presence.erase(it); presence = true;}
        else ++it;
    }
    if (presence) emit this->presenceChanged();
    if (!this->_away) this->_presenceDirty = this->_capture && this->_capture->knowsPointer();

    // Files: stalled transfers dropped, old offers forgotten
    for (const QString& id: this->_transfers.keys())
        if (now - this->_transfers.value(id).touched > STALLED_TRANSFER) this->failTransfer_(id, "stalled", true);
    for (auto it = this->_offers.begin(); it != this->_offers.end();) {
        if (now - it->created > OFFER_LIFETIME) it = this->_offers.erase(it);
        else ++it;
    }

    // Windows: a viewer without frames for a while is closed
    for (const QString& id: this->_streams.keys()) {
        const Stream s = this->_streams.value(id);
        if (!s.outgoing && s.lastFrame != 0 && now - s.lastFrame > STREAM_TIMEOUT) this->stopStream(id);
        else if (!this->reachable(s.peer)) this->stopStream(id);
    }
    if (++ticks % 30 == 0) this->publishSelf_(); // addresses (dhcp, tailscale up / down)
    emit this->membersChanged(); // latencies
}
