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
##  @file Hub-pads.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

/*
 * Pad protocol (websocket /padws, json text frames):
 *  pad  -> {"t": "pair", "code", "name", "kind": "phone" | "web"}   -> {"t": "waiting"} then {"t": "paired", "id", "token", ...welcome}
 *  pad  -> {"t": "hello", "token", "role": "pad" | "screen"}        -> {"t": "welcome", "id", "device", "network", "version"}
 *  pad  -> move {dx, dy}, button {b, down}, wheel {dx, dy}, key {code, down}, text {text}, home, ping, laser {on}, link {device, url}
 *  screen -> {"t": "screen", "w", "h", "scale"} (logical pixels), deny {id, reason}
 *  host -> pad: {"t": "map", devices, cursors, me}, {"t": "link", "url", "from"}, {"t": "error", "text"}
 *  host -> screen: enter / pointer / button / wheel / key / text / leave of the foreign cursors {id, name, color, x, y ...}
*/

#define _Attribute
#include <utils/utils.hpp>
#include "xmouse/security/Crypto.hpp"
#include "xmouse/net/Protocol.hpp"
#include "xmouse/core/Tools.hpp"
#include "xmouse/core/Hub.hpp"
#include <QGuiApplication>
#include <QJsonDocument>
#include <QClipboard>
#include <QJsonArray>
#include <QSaveFile>
#include <algorithm>

//----------------------------------------------------------------//
/* DEFINE */

#define PAD_AUTH_TIMEOUT 30000 // ms to send a hello / pair
#define PAD_APPROVAL_TIMEOUT 30000 // ms the local person has to accept a new pad
#define PAD_MAX_TEXT (64 * 1024) // biggest json message of a pad
#define PAD_MAX_PADS 32

//----------------------------------------------------------------//
/* TOOLS */

_hot static void send_json(xmouse::net::WebSocket* ws, const QJsonObject& o)
{
    if (ws && ws->isOpen()) ws->sendText(QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact)));
}

//----------------------------------------------------------------//
/* STORE */

void xmouse::core::Hub::loadPads_(void)
{
    QFile file(Config::dataDir() + "/pads.json");
    QJsonArray list;

    if (!file.open(QIODevice::ReadOnly)) return;
    list = QJsonDocument::fromJson(file.readAll()).object().value("pads").toArray();
    for (const QJsonValue& v: list) {
        const QJsonObject o = v.toObject();
        Pad pad;
        pad.id = o.value("id").toString();
        pad.name = o.value("name").toString().left(64);
        pad.kind = o.value("kind").toString() == "phone" ? "phone" : "web";
        pad.hash = QByteArray::fromHex(o.value("hash").toString().toLatin1());
        pad.created = o.value("created").toString().toLongLong();
        pad.seen = o.value("seen").toString().toLongLong();
        pad.screen = QSize(o.value("w").toInt(), o.value("h").toInt());
        pad.scale = o.value("scale").toDouble(1.0);
        if (!pad.id.isEmpty() && pad.hash.size() == 32) this->_pads.insert(pad.id, pad);
    }
}

void xmouse::core::Hub::savePads_(void) const
{
    QSaveFile file(Config::dataDir() + "/pads.json");
    QJsonArray list;

    for (const Pad& p: this->_pads)
        list.append(QJsonObject{{"id", p.id}, {"name", p.name}, {"kind", p.kind}, {"hash", QString::fromLatin1(p.hash.toHex())}, {"created", QString::number(p.created)}, {"seen", QString::number(p.seen)}, {"w", p.screen.width()}, {"h", p.screen.height()}, {"scale", p.scale}});
    if (!file.open(QIODevice::WriteOnly)) return;
    file.write(QJsonDocument(QJsonObject{{"pads", list}}).toJson(QJsonDocument::Indented));
    file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    (void)file.commit();
}

QList<xmouse::core::Pad> xmouse::core::Hub::pads(void) const
{
    QList<Pad> out = this->_pads.values();

    std::sort(out.begin(), out.end(), [](const Pad& a, const Pad& b) {return a.name.toLower() < b.name.toLower();});
    return out;
}

QString xmouse::core::Hub::padOf_(const net::WebSocket* ws) const
{
    return ws ? ws->property("xmouse_pad").toString() : QString();
}

//----------------------------------------------------------------//
/* SOCKETS */

void xmouse::core::Hub::onPadSocket_(net::WebSocket* ws, const QString& agent)
{
    if (!this->_config.settings().pads || this->_mesh.isBanned(ws->peerAddress())) {ws->close(1008, "phones and web pads are refused here"); ws->deleteLater(); return;}
    ws->setParent(this);
    ws->setProperty("xmouse_agent", agent.left(200));
    QObject::connect(ws, &net::WebSocket::textReceived, this, [this, ws](const QString& text) {this->onPadText_(ws, text);});
    QObject::connect(ws, &net::WebSocket::closed, this, [this, ws]() {this->onPadClosed_(ws);});
    QTimer::singleShot(PAD_AUTH_TIMEOUT, ws, [this, ws]() {
        if (this->padOf_(ws).isEmpty() && !ws->property("xmouse_waiting").toBool()) ws->close(1008, "authentication timeout");
    });
}

void xmouse::core::Hub::onPadClosed_(net::WebSocket* ws)
{
    const QString id = this->padOf_(ws);
    auto it = this->_pads.find(id);

    for (auto p = this->_padPending.begin(); p != this->_padPending.end();) {
        if (p.value() == ws) p = this->_padPending.erase(p);
        else ++p;
    }
    if (it != this->_pads.end()) {
        if (it->control == ws && this->_foreign.contains(id)) this->foreignLeave_(id); // its own cursor goes with it
        if (it->control == ws) this->padAwayEnd_(id); // there too
        if (it->control == ws) it->control = nullptr;
        if (it->display == ws) {
            it->display = nullptr;
            this->_log.info("pads", id, it->name + ": screen gone");
            if (this->_away && this->_target == id) this->comeBack(); // my cursor was on it: back here, never held on a screen gone
            for (const QString& other: this->_padAway.keys()) // the own cursors of the pads on it too
                if (this->_padAway.value(other).target == id) this->padAwayEnd_(other);
            this->_publishSoon.start(); // links: the others stop reaching its screen
            this->updateEdges_();
        }
        if (!it->control && !it->display) this->publishPad_(id); // offline in the members
        emit this->padsChanged();
    }
    ws->deleteLater();
}

void xmouse::core::Hub::onPadText_(net::WebSocket* ws, const QString& text)
{
    const QJsonObject o = text.size() <= PAD_MAX_TEXT ? QJsonDocument::fromJson(text.toUtf8()).object() : QJsonObject();
    const QString type = o.value("t").toString();
    const QString id = this->padOf_(ws);

    if (o.isEmpty()) return;
    if (!id.isEmpty()) {this->padCommand_(id, o); return;}

    // A paired pad comes back with its token
    if (type == "hello") {
        const QByteArray hash = security::sha256(o.value("token").toString().toUtf8());
        const QString role = o.value("role").toString() == "screen" ? "screen" : "pad";
        for (Pad& pad: this->_pads) {
            if (!security::constant_equal(pad.hash, hash)) continue;
            ws->setProperty("xmouse_pad", pad.id);
            ws->setProperty("xmouse_role", role);
            if (role == "screen") {
                if (pad.display && pad.display != ws) pad.display->close(1000, "replaced");
                pad.display = ws;
            } else {
                if (pad.control && pad.control != ws) pad.control->close(1000, "replaced");
                pad.control = ws;
            }
            pad.seen = now_ms();
            this->savePads_();
            this->publishPad_(pad.id); // in the members (and its screen in the layout once lent) as soon as it is there
            send_json(ws, QJsonObject{{"t", "welcome"}, {"id", pad.id}, {"device", this->_config.deviceName()}, {"network", this->_group.joined() ? this->_group.name() : QString()}, {"version", XMOUSE_VERSION}, {"role", role}});
            this->_log.info("pads", pad.id, pad.name + " connected (" + role + ", " + ws->peerAddress().toString() + ")");
            emit this->padsChanged();
            return;
        }
        send_json(ws, QJsonObject{{"t", "error"}, {"text", "unknown pad: pair it again with the code"}, {"unpaired", true}});
        ws->close(1008, "unknown token");
        return;
    }

    // A new pad: the one-time code of this device, then the local person accepts it
    if (type == "pair") {
        static int failures = 0;
        const QString name = o.value("name").toString().trimmed().left(64);
        if (this->_pads.size() >= PAD_MAX_PADS) {send_json(ws, QJsonObject{{"t", "error"}, {"text", "too many pads on this device"}}); ws->close(1008, "full"); return;}
        if (!this->_code.checkPlain(o.value("code").toString())) {
            this->_log.warn("pads", "", "wrong code from a pad (" + ws->peerAddress().toString() + ")");
            if (++failures >= this->_config.settings().maxAuthFailures) {this->_code.rotate(); failures = 0;}
            send_json(ws, QJsonObject{{"t", "error"}, {"text", "wrong or expired code"}});
            ws->close(1008, "wrong code");
            return;
        }
        failures = 0;
        this->_code.rotate(); // used once
        const QString pending = security::random_id(6);
        ws->setProperty("xmouse_waiting", true);
        ws->setProperty("xmouse_name", name.isEmpty() ? QString(o.value("kind").toString() == "phone" ? "Phone" : "Browser") : name);
        ws->setProperty("xmouse_kind", o.value("kind").toString() == "phone" ? "phone" : "web");
        this->_padPending[pending] = ws;
        send_json(ws, QJsonObject{{"t", "waiting"}, {"device", this->_config.deviceName()}});
        if (!this->_config.settings().joinApproval) {this->approvePad(pending, true); return;}
        QTimer::singleShot(PAD_APPROVAL_TIMEOUT, this, [this, pending]() {this->approvePad(pending, false);}); // nobody answered: refused
        emit this->padApproval(pending, ws->property("xmouse_name").toString(), ws->property("xmouse_kind").toString());
    }
}

void xmouse::core::Hub::approvePad(const QString& pending, const bool accepted)
{
    QPointer<net::WebSocket> ws = this->_padPending.take(pending);
    QString token;
    Pad pad;

    if (!ws) return;
    ws->setProperty("xmouse_waiting", false);
    if (!accepted) {
        send_json(ws, QJsonObject{{"t", "error"}, {"text", "refused on " + this->_config.deviceName()}});
        ws->close(1008, "refused");
        return;
    }
    token = security::random_id(32);
    pad.id = security::random_id(12);
    pad.name = ws->property("xmouse_name").toString();
    pad.kind = ws->property("xmouse_kind").toString();
    pad.hash = security::sha256(token.toUtf8());
    pad.created = now_ms();
    pad.seen = pad.created;
    pad.control = ws;
    ws->setProperty("xmouse_pad", pad.id);
    ws->setProperty("xmouse_role", "pad");
    this->_pads.insert(pad.id, pad);
    this->savePads_();
    send_json(ws, QJsonObject{{"t", "paired"}, {"id", pad.id}, {"token", token}, {"device", this->_config.deviceName()}, {"network", this->_group.joined() ? this->_group.name() : QString()}, {"version", XMOUSE_VERSION}, {"fp", QString::fromLatin1(this->_certificate.fingerprint().toHex())}});
    this->_log.info("pads", pad.id, pad.name + " paired (" + pad.kind + ")");
    this->publishPad_(pad.id);
    emit this->notice(pad.name + " can now drive this device");
    emit this->padsChanged();
}

void xmouse::core::Hub::removePad(const QString& id)
{
    auto it = this->_pads.find(id);

    if (it == this->_pads.end()) return;
    if (it->control) it->control->close(1008, "removed");
    if (it->display) it->display->close(1008, "removed");
    this->_log.info("pads", id, it->name + " removed");
    this->_pads.erase(it);
    this->savePads_();
    if (this->_group.joined() && this->_state.contains("device/" + id)) {
        this->broadcastDelta_(this->_state.remove("device/" + id));
        this->broadcastDelta_(this->_state.remove("layout/" + id));
    }
    this->_publishSoon.start();
    this->rebuildLayout_();
    emit this->padsChanged();
}

//----------------------------------------------------------------//
/* COMMANDS */

void xmouse::core::Hub::padCommand_(const QString& pad, const QJsonObject& o)
{
    const QString type = o.value("t").toString();
    auto it = this->_pads.find(pad);

    if (it == this->_pads.end()) return;

    if (type == "move") {this->padMotion_(pad, std::clamp(o.value("dx").toDouble(), -4000.0, 4000.0), std::clamp(o.value("dy").toDouble(), -4000.0, 4000.0)); return;}
    if (type == "button") {
        const int b = o.value("b").toInt(-1);
        if (b >= 0 && b <= static_cast<int>(net::MouseButton::Forward)) this->padButton_(pad, static_cast<net::MouseButton>(b), o.value("down").toBool());
        return;
    }
    if (type == "zoom") { // two fingers spreading / closing: Ctrl + wheel where the cursor is (a pinch on a phone screen)
        const int d = std::clamp(o.value("d").toInt(), -1200, 1200);
        if (d == 0) return;
        this->padKey_(pad, "ControlLeft", true);
        this->padWheel_(pad, 0, d);
        this->padKey_(pad, "ControlLeft", false);
        return;
    }
    if (type == "wheel") {this->padWheel_(pad, std::clamp(o.value("dx").toInt(), -32000, 32000), std::clamp(o.value("dy").toInt(), -32000, 32000)); return;}
    if (type == "key") {
        QString code = o.value("code").toString().left(32);
        const QString character = o.value("char").toString().left(2);
        // A shortcut of the pad (Ctrl + a): the key that types that letter on the layout of this device (azerty: KeyQ)
        if (!character.isEmpty() && this->_injector && !this->_injector->codeOf(character).isEmpty()) code = this->_injector->codeOf(character);
        if (!code.isEmpty()) this->padKey_(pad, code, o.value("down").toBool());
        return;
    }
    if (type == "text") {this->padText_(pad, o.value("text").toString().left(4096)); return;}
    if (type == "home") {
        if (this->_padAway.contains(pad)) {this->padAwayEnd_(pad); (void)this->padOwnCursor_(pad); return;} // its own cursor back here
        this->comeBack();
        return;
    }
    if (type == "ping") {this->ping(); return;}
    if (type == "laser") {this->setLaser(o.value("on").toBool()); return;}
    if (type == "follow") {this->follow(o.value("id").toString()); return;}
    if (type == "link") {this->sendLink(o.value("device").toString(), o.value("url").toString()); return;}

    // The screen of the android app: in the layout, the foreign cursors drawn and clicked there
    if (type == "screen") {
        const int w = std::clamp(o.value("w").toInt(), 0, 20000);
        const int h = std::clamp(o.value("h").toInt(), 0, 20000);
        const bool touch = o.value("touch").toBool();
        if (it->touch != touch || !it->screenOn) this->_log.info("pads", pad, it->name + " lends its screen (" + (touch ? "touch: the cursors click, scroll and type there" : "cursors shown only: their clicks do nothing there, 'Let them touch' in the app turns the Accessibility service on") + ")");
        if (it->touch != touch) this->_toldShowOnly.remove(pad);
        it->touch = touch;
        it->screenOn = w >= 100 && h >= 100 && o.value("on").toBool(true);
        it->screen = QSize(w, h);
        it->scale = std::clamp(o.value("scale").toDouble(1.0), 0.5, 8.0);
        it->kind = "phone";
        this->savePads_();
        this->publishPad_(pad);
        this->_publishSoon.start();
        this->updateEdges_();
        emit this->padsChanged();
        return;
    }
    if (type == "deny") {this->sendTo_(o.value("id").toString(), QJsonObject{{"t", "deny"}, {"reason", o.value("reason").toString().left(120)}}, pad); return;}
    if (type == "access") {
        // About my own cursor on its screen (why a click does nothing there): told here
        if (o.value("id").toString() == this->_group.self()) {emit this->notice(it->name + ": " + o.value("text").toString().left(200)); return;}
        this->sendTo_(o.value("id").toString(), QJsonObject{{"t", "access"}, {"text", o.value("text").toString().left(200)}}, pad);
        return;
    }
}

void xmouse::core::Hub::clipToPads_(const QString& only)
{
    QString text;
    QString hash;

    // The phone screens are not members: the text of the clipboard goes to them (to paste there), once per change
    if (!this->_config.settings().clipboard) return;
    if (this->_clipboard.backend() == "qt") text = is_gui() && QGuiApplication::clipboard() ? QGuiApplication::clipboard()->text() : QString();
    else text = this->_clipboard.text();
    if (text.isEmpty() || text.toUtf8().size() > XMOUSE_MAX_CLIPBOARD) return;
    hash = QString::fromLatin1(security::sha256(text.toUtf8()).toHex());
    for (const Pad& p: this->_pads) {
        if (!p.display || (!only.isEmpty() && p.id != only) || this->_padClip.value(p.id) == hash) continue;
        this->_padClip[p.id] = hash;
        send_json(p.display, QJsonObject{{"t", "clip"}, {"text", text}});
    }
}

void xmouse::core::Hub::tellShowOnly_(const QString& device)
{
    const auto it = this->_pads.constFind(device);

    if (it == this->_pads.constEnd() || !it->screenOn || it->touch || this->_toldShowOnly.contains(device)) return;
    this->_toldShowOnly.insert(device);
    emit this->notice(it->name + " only shows the cursors: clicks, scroll and keys do nothing there. On the phone: This screen in the layout > Let them touch (its Accessibility service)");
}

void xmouse::core::Hub::publishPad_(const QString& pad)
{
    const Pad p = this->_pads.value(pad);
    QJsonArray screens;
    QJsonObject entry;

    if (!this->_group.joined() || p.id.isEmpty()) return;
    if (p.screenOn) screens.append(QJsonObject{{"name", "screen"}, {"x", 0}, {"y", 0}, {"w", p.screen.width()}, {"h", p.screen.height()}, {"scale", p.scale}});
    entry["name"] = p.name;
    entry["os"] = p.kind == "phone" ? "android" : "web";
    entry["kind"] = p.kind;
    entry["version"] = XMOUSE_VERSION;
    entry["via"] = this->_group.self(); // reached through this device only
    entry["screens"] = screens;
    entry["addresses"] = QJsonArray();
    entry["port"] = 0;
    entry["joined"] = QString::number(p.created);
    entry["online"] = p.control != nullptr || p.display != nullptr; // the pad is connected now (touchpad or screen)
    this->broadcastDelta_(this->_state.set("device/" + pad, entry));
    this->rebuildLayout_();
}

//----------------------------------------------------------------//
/* DELIVERY TO THE SCREEN OF A PHONE */

void xmouse::core::Hub::deliverToPad_(const QString& pad, const QString& from, const QJsonObject& o)
{
    const Pad p = this->_pads.value(pad);
    const QString type = o.value("t").toString();
    const QString name = this->_layout.device(from).name.isEmpty() ? o.value("name").toString().left(64) : this->_layout.device(from).name;

    if (type == "link") {send_json(p.control, QJsonObject{{"t", "link"}, {"url", o.value("url").toString()}, {"from", name}}); return;}
    if (type != "enter" && type != "leave") return; // a phone has no clipboard / files / windows through this path
    if (!p.display || !p.screenOn) {
        if (type == "enter") this->sendTo_(from, QJsonObject{{"t", "kick"}}, pad); // its screen is gone: the cursor goes home
        return;
    }
    if (type == "enter") send_json(p.display, QJsonObject{{"t", "enter"}, {"id", from}, {"name", name}, {"color", colorOf(from).name()}, {"x", o.value("x").toInt()}, {"y", o.value("y").toInt()}});
    else send_json(p.display, QJsonObject{{"t", "leave"}, {"id", from}});
}

void xmouse::core::Hub::deliverBinaryToPad_(const QString& pad, const QString& from, const QByteArray& data)
{
    const Pad p = this->_pads.value(pad);
    net::ByteReader r(data);

    if (!p.display || data.isEmpty()) return;
    switch (static_cast<net::Binary>(static_cast<std::uint8_t>(data[0]))) {
        case net::Binary::Pointer: {
            (void)r.u16();
            const int x = r.i32();
            const int y = r.i32();
            if (r.ok()) send_json(p.display, QJsonObject{{"t", "pointer"}, {"id", from}, {"x", x}, {"y", y}});
            break;
        }
        case net::Binary::Button: {
            const int b = r.u8();
            const bool down = r.u8() != 0;
            QJsonObject o{{"t", "button"}, {"id", from}, {"b", b}, {"down", down}};
            if (r.remaining() >= 8) {
                const int x = r.i32();
                const int y = r.i32();
                o["x"] = x;
                o["y"] = y;
            }
            if (r.ok()) send_json(p.display, o);
            break;
        }
        case net::Binary::Wheel: {
            const int dx = r.i16();
            const int dy = r.i16();
            if (r.ok()) send_json(p.display, QJsonObject{{"t", "wheel"}, {"id", from}, {"dx", dx}, {"dy", dy}});
            break;
        }
        case net::Binary::Key: {
            const bool down = r.u8() != 0;
            const QString code = r.text8();
            QString character = r.remaining() > 0 ? r.text8() : QString(); // the letter on the layout of the sender
            if (character.isEmpty() && this->_injector) character = this->_injector->characterOf(code, false, false); // Ctrl + the letter of the layout (azerty Ctrl+A is KeyQ)
            if (r.ok()) send_json(p.display, QJsonObject{{"t", "key"}, {"id", from}, {"code", code}, {"down", down}, {"char", character}});
            break;
        }
        case net::Binary::Text:
            send_json(p.display, QJsonObject{{"t", "text"}, {"id", from}, {"text", QString::fromUtf8(r.rest()).left(4096)}});
            break;
        default:
            break;
    }
}

//----------------------------------------------------------------//
/* MAP FOR THE PADS */

void xmouse::core::Hub::sendPadMaps_(void)
{
    QJsonArray devices;
    QJsonArray cursors;
    QJsonObject map;
    bool any = false;

    for (const Pad& p: this->_pads)
        if (p.control) any = true;
    if (!any) return;
    for (const Device& d: this->_layout.devices()) {
        QJsonArray rects;
        for (int i = 0; i < d.screens.size(); ++i) {
            const QRect r = this->_layout.globalRect(d.id, i);
            rects.append(QJsonArray{r.x(), r.y(), r.width(), r.height()});
        }
        devices.append(QJsonObject{{"id", d.id}, {"name", d.name}, {"self", d.id == this->_group.self()}, {"online", d.id == this->_group.self() || this->reachable(d.id)}, {"kind", this->_state.value("device/" + d.id).toObject().value("kind").toString("desktop")}, {"rects", rects}});
    }
    for (const Presence& p: this->_presence) {
        const QPoint at = this->presenceGlobal(p);
        cursors.append(QJsonObject{{"id", p.id}, {"name", this->_layout.device(p.id).name}, {"color", colorOf(p.id).name()}, {"x", at.x()}, {"y", at.y()}, {"on", p.on}, {"flags", p.flags}});
    }
    map = QJsonObject{{"t", "map"}, {"devices", devices}, {"cursors", cursors}, {"me", QJsonObject{{"id", this->_group.self()}, {"away", this->_away}, {"target", this->_layout.device(this->_target).name}, {"laser", this->_laser}, {"x", this->pointerGlobal().x()}, {"y", this->pointerGlobal().y()}}}, {"network", this->_group.joined() ? this->_group.name() : QString()}, {"device", this->_config.deviceName()}, {"private", this->_config.settings().privateMode}};
    for (const Pad& p: this->_pads)
        send_json(p.control, map);
}
