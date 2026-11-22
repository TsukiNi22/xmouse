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
##  @file Hub-cursor.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Attribute
#include <utils/utils.hpp>
#include "xmouse/net/Protocol.hpp"
#include "xmouse/core/Tools.hpp"
#include "xmouse/core/Hub.hpp"
#include <QGuiApplication>
#include <functional>
#include <algorithm>
#include <QScreen>
#include <cmath>

//----------------------------------------------------------------//
/* DEFINE */

#define BACKPRESSURE_LIMIT (256 * 1024) // bytes waiting on a link before the moves are skipped (the next one carries the position)
#define PING_DURATION 1500 // ms a "look here" wave is told
#define SLOW_MOVE_INTERVAL 15 // ms between two moves sent to a phone (its screen draws at 60 Hz: more only loads it)
#define AWAY_CHECK_TICKS 4 // presence ticks (50 ms) between two reads of the real pointer while away (200 ms)
#define AWAY_PAD_MOVE 24 // px the real pointer may move while the cursor is away with a pad (the person took the mouse)
#define AWAY_LOST_MOVE 120 // px a held pointer may be found away from where it is held (twice in a row: the capture is lost)
#define PAD_PRESENCE_INTERVAL 33 // ms between two map updates for the own cursor of a pad

//----------------------------------------------------------------//
/* CROSSING */

void xmouse::core::Hub::onCrossed_(const QPoint& local, const QPoint& push)
{
    const QString self = this->_group.self();
    const QPoint global = this->_layout.toGlobal(self, local);
    const std::optional<Hit> hit = this->_layout.hit(global + push);

    // Nothing there (a layout changed meanwhile), the device cannot be reached, or the push came from a foreign cursor
    // driving the real pointer (pointer mode): the pointer stays here
    if (!hit || hit->device == self || !this->reachable(hit->device) || (this->injectingNow() && !this->_foreign.isEmpty())) {
        this->_capture->release(local - push * 2);
        return;
    }

    // Away with a pad, the real mouse pushes an edge now: it drives the cursor from where it is
    if (this->_away) {
        this->_awayByPad = false;
        this->_awayPointer = local;
        this->_awayWatch = this->_capture->knowsPointer();
        this->_awayLost = 0;
        return;
    }
    this->_awayByPad = false;
    this->leaveTo_(global + push, hit->device, hit->local);
}

void xmouse::core::Hub::leaveTo_(const QPoint& global, const QString& device, const QPoint& local)
{
    this->_away = true;
    this->_target = device;
    this->_slowTarget = this->_pads.contains(device) || this->_state.value("device/" + device).toObject().value("kind").toString() == "phone";
    this->_awayWatch = this->_capture && this->_capture->knowsPointer();
    this->_awayPointer = this->_awayWatch ? this->_capture->pointer() : QPoint();
    this->_awayLost = 0;
    this->_global = QPointF(global);
    this->_targetLocal = local;
    this->_keysDown.clear();
    this->_buttonsDown = 0;
    this->sendTo_(this->_target, QJsonObject{{"t", "enter"}, {"x", local.x()}, {"y", local.y()}, {"name", this->_config.deviceName()}});
    this->tellShowOnly_(device);
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
    if (!hit) {this->keepAway_(); return;} // between the screens: its device changed under it
    if (hit->device == this->_group.self()) {this->goHome_(to); return;}
    if (hit->device != this->_target) {
        if (!this->reachable(hit->device)) {this->_global = QPointF(from); return;} // an unreachable device is a wall
        this->switchTarget_(hit->device);
    }
    this->_targetLocal = hit->local;
    this->_moveDirty = true;
    this->_presenceDirty = true;
    if (!this->_flush.isActive()) this->_flush.start();
}

void xmouse::core::Hub::flushMove_(void)
{
    if (!this->_away) {this->_flush.stop(); return;}
    if (!this->_moveDirty) return;
    if (this->pendingTo_(this->_target) > BACKPRESSURE_LIMIT) return; // a slow link: the next flush sends the latest position
    if (this->_slowTarget && now_ms() - this->_lastSlowMove < SLOW_MOVE_INTERVAL) return; // a phone: the latest position, 60 times a second
    this->_lastSlowMove = now_ms();
    this->sendHot_(this->_target, net::ByteWriter(net::Binary::Pointer).u16(++this->_seq).i32(this->_targetLocal.x()).i32(this->_targetLocal.y()).data());
    this->_moveDirty = false;
}

void xmouse::core::Hub::switchTarget_(const QString& device)
{
    // Nothing stays held on the device left (a drag across devices drops there)
    for (int b = 0; b <= static_cast<int>(net::MouseButton::Forward); ++b)
        if (this->_buttonsDown & (1 << b)) this->sendBinaryTo_(this->_target, net::ByteWriter(net::Binary::Button).u8(static_cast<std::uint8_t>(b)).u8(0).data());
    for (const QString& code: this->_keysDown)
        this->sendBinaryTo_(this->_target, net::ByteWriter(net::Binary::Key).u8(0).text8(code).data());
    this->_buttonsDown = 0;
    this->sendTo_(this->_target, QJsonObject{{"t", "leave"}});
    this->_target = device;
    this->_slowTarget = this->_pads.contains(device) || this->_state.value("device/" + device).toObject().value("kind").toString() == "phone";
    this->sendTo_(this->_target, QJsonObject{{"t", "enter"}, {"x", this->_targetLocal.x()}, {"y", this->_targetLocal.y()}, {"name", this->_config.deviceName()}});
    this->tellShowOnly_(device);
    emit this->awayChanged();
}

void xmouse::core::Hub::keepAway_(void)
{
    const QPoint at = this->_global.toPoint();
    std::optional<Hit> hit;
    QPoint to;

    if (!this->_away) return;
    // Its device left the layout (a screen lent no more, removed): back here, never held on nothing
    if (!this->_layout.contains(this->_target) || this->_layout.bounds(this->_target).isEmpty()) {
        this->_log.info("input", this->_target, "the device of the cursor left the layout: the cursor comes back");
        this->comeBack();
        return;
    }

    // Its screens changed (a phone turned, a device moved): the cursor stays on it, at the closest point
    hit = this->_layout.hit(at);
    if (hit && hit->device == this->_target) return;
    to = this->_layout.clampTo(this->_target, at);
    hit = this->_layout.hit(to);
    if (!hit || hit->device != this->_target) {this->comeBack(); return;}
    this->_global = QPointF(to);
    this->_targetLocal = hit->local;
    this->_moveDirty = true;
    this->_presenceDirty = true;
    if (!this->_flush.isActive()) this->_flush.start();
}

void xmouse::core::Hub::checkAway_(void)
{
    QPoint now;
    int moved = 0;

    // The real pointer of this device, read 5 times a second while away: left with a pad, it moves = the person took
    // the mouse here; captured, it is found far from where it is held twice in a row = the capture was lost (the
    // compositor, an old extension): the cursor comes back where the pointer is, never stuck away
    if (!this->_capture || (++this->_awayTick % AWAY_CHECK_TICKS) != 0) return;

    // Away with the capture, but the capture holds nothing any more (ended without a word, any backend): back here
    if (!this->_awayByPad && !this->_capture->capturing()) {
        if (++this->_awayLost < 2) return;
        this->_log.info("input", this->_target, "the capture of the pointer ended without a word: the cursor comes back");
        this->goHome_(this->_layout.clampTo(this->_group.self(), this->_global.toPoint()));
        return;
    }
    if (!this->_awayWatch) return;
    now = this->_capture->pointer();
    if (this->injectingNow()) {this->_awayPointer = now; return;} // a foreign cursor moved it (pointer mode)
    moved = (now - this->_awayPointer).manhattanLength();
    if (moved < (this->_awayByPad ? AWAY_PAD_MOVE : AWAY_LOST_MOVE)) {this->_awayLost = 0; return;}
    if (!this->_awayByPad && ++this->_awayLost < 2) return;
    this->_log.info("input", this->_target, this->_awayByPad ? "the real mouse moved here: the cursor comes back" : "the capture of the pointer was lost: the cursor comes back");
    this->_homePad.clear(); // the person drives, not the pad
    this->goHome_(this->_layout.toGlobal(this->_group.self(), now));
}

void xmouse::core::Hub::goHome_(const QPoint& global)
{
    const QString self = this->_group.self();
    const std::optional<Hit> hit = this->_layout.hit(global);
    const QPoint local = hit && hit->device == self ? hit->local : this->_layout.bounds(self).center();

    if (!this->_away) return;
    for (int b = 0; b <= static_cast<int>(net::MouseButton::Forward); ++b)
        if (this->_buttonsDown & (1 << b)) this->sendBinaryTo_(this->_target, net::ByteWriter(net::Binary::Button).u8(static_cast<std::uint8_t>(b)).u8(0).data());
    for (const QString& code: this->_keysDown)
        this->sendBinaryTo_(this->_target, net::ByteWriter(net::Binary::Key).u8(0).text8(code).data());
    this->sendTo_(this->_target, QJsonObject{{"t", "leave"}});
    this->_away = false;
    this->_target.clear();
    this->_keysDown.clear();
    this->_buttonsDown = 0;
    this->_flush.stop();
    this->_global = QPointF(global);
    if (this->_awayByPad && this->padPhantom_() && this->_pads.contains(this->_homePad)) {
        // Left with a pad in phantom / private mode: its own cursor comes back here, the real pointer is not touched
        this->padPhantomAt_(this->_homePad, local);
    } else if (this->_awayByPad) {
        // Left with a pad: the local pointer was never held, it is put where the cursor comes back
        this->_padPointer = local;
        if (this->_injector) this->_injector->mouseMove(this->injectPoint_(local).x(), this->injectPoint_(local).y());
    } else if (this->_capture) this->_capture->release(local);
    this->_awayByPad = false;
    this->_presenceDirty = true;
    emit this->awayChanged();
}

void xmouse::core::Hub::comeBack(void)
{
    QScreen* primary = is_gui() ? QGuiApplication::primaryScreen() : nullptr;

    if (!this->_away) return;
    this->goHome_(this->_layout.toGlobal(this->_group.self(), primary ? primary->geometry().center() : this->_layout.bounds(this->_group.self()).center()));
}

//----------------------------------------------------------------//
/* INPUT WHILE AWAY */

void xmouse::core::Hub::onButton_(const net::MouseButton button, const bool down)
{
    const int bit = 1 << static_cast<int>(button);

    if (!this->_away) return;
    if (down) this->_buttonsDown |= bit;
    else this->_buttonsDown &= ~bit;
    this->flushMove_(); // the click lands where the cursor is drawn
    this->sendBinaryTo_(this->_target, net::ByteWriter(net::Binary::Button).u8(static_cast<std::uint8_t>(button)).u8(down ? 1 : 0).i32(this->_targetLocal.x()).i32(this->_targetLocal.y()).data());
    this->_presenceDirty = true;
}

void xmouse::core::Hub::onWheel_(const int dx, const int dy)
{
    if (!this->_away || (dx == 0 && dy == 0)) return;
    this->flushMove_();
    this->sendBinaryTo_(this->_target, net::ByteWriter(net::Binary::Wheel).i16(static_cast<std::int16_t>(std::clamp(dx, -32000, 32000))).i16(static_cast<std::int16_t>(std::clamp(dy, -32000, 32000))).data());
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
    this->sendBinaryTo_(this->_target, net::ByteWriter(net::Binary::Key).u8(down ? 1 : 0).text8(code).data());
}

//----------------------------------------------------------------//
/* PRESENCE */

void xmouse::core::Hub::sendPresence_(void)
{
    Presence mine;
    QString on;
    QPoint global;
    QByteArray data;
    int flags = 0;

    if (!this->_group.joined()) return;
    if (this->_away) this->checkAway_();
    // At home: the pointer the platform tells is followed live (10 Hz), not only once a second
    if (!this->_away && this->_capture && this->_capture->knowsPointer() && (++this->_presenceTick % 2) == 0) {
        const QPoint now = this->localPointer_();
        if (now != this->_lastHomePointer) {this->_lastHomePointer = now; this->_presenceDirty = true;}
    }
    if (!this->_presenceDirty) return;
    this->_presenceDirty = false;
    if (this->_away) {
        on = this->_target;
        global = this->_global.toPoint();
        if (this->_buttonsDown) flags |= XMOUSE_PRESENCE_DOWN;
    } else {
        on = this->_group.self();
        global = this->_layout.toGlobal(on, this->localPointer_());
        flags |= XMOUSE_PRESENCE_HIDDEN;
        if (!(this->_capture && this->_capture->knowsPointer()) && this->_padPointer.isNull()) flags |= XMOUSE_PRESENCE_UNKNOWN; // never a stale position on the maps
    }
    if (now_ms() < this->_pingUntil) flags |= XMOUSE_PRESENCE_PING;
    if (this->_laser) flags |= XMOUSE_PRESENCE_LASER;
    mine = Presence{this->_group.self(), on, global, this->_away ? this->_targetLocal : this->localPointer_(), flags, now_ms(), ++this->_presenceSeq};
    this->_presence[mine.id] = mine;
    data = net::ByteWriter(net::Binary::Presence).u16(mine.seq).text8(on).i32(global.x()).i32(global.y()).u8(static_cast<std::uint8_t>(flags)).data();

    // Every member, the relayed ones too (udp to the direct ones when it passes)
    for (const QString& id: this->_state.ids("device/")) {
        if (id == this->_group.self() || this->_pads.contains(id) || !this->_state.value("device/" + id).toObject().value("via").toString().isEmpty()) continue;
        if (this->reachable(id)) this->sendHot_(id, data);
    }
    emit this->presenceChanged();
}

void xmouse::core::Hub::ping(void)
{
    this->_pingUntil = now_ms() + PING_DURATION;
    this->_presenceDirty = true;
    this->sendPresence_();
    QTimer::singleShot(PING_DURATION + 100, this, [this]() {this->_presenceDirty = true;}); // the end of the wave told too
}

void xmouse::core::Hub::setLaser(const bool enabled)
{
    if (this->_laser == enabled) return;
    this->_laser = enabled;
    this->_presenceDirty = true;
    this->sendPresence_();
    emit this->presenceChanged();
}

void xmouse::core::Hub::follow(const QString& id)
{
    this->_following = id == this->_group.self() ? QString() : id;
    emit this->followChanged();
    emit this->presenceChanged();
}

QPoint xmouse::core::Hub::presenceGlobal(const Presence& p) const
{
    // Mine: where it is now; the others: on their device as it is placed now
    if (p.id == this->_group.self() && !this->_away) return this->pointerGlobal();
    if (!p.on.isEmpty() && this->_layout.contains(p.on)) return this->_layout.toGlobal(p.on, p.local);
    return p.global;
}

QPoint xmouse::core::Hub::pointerGlobal(void) const
{
    if (this->_away) return this->_global.toPoint();
    return this->_layout.toGlobal(this->_group.self(), this->localPointer_());
}

QPoint xmouse::core::Hub::localPointer_(void) const
{
    if (this->_capture && this->_capture->knowsPointer()) return this->_capture->pointer();
    if (!this->_padPointer.isNull()) return this->_padPointer;
    return this->_layout.bounds(this->_group.self()).center();
}

//----------------------------------------------------------------//
/* PAD: THE CURSOR OF THIS DEVICE DRIVEN BY A PHONE / BROWSER */

bool xmouse::core::Hub::padOwnCursor_(const QString& pad)
{
    // Pointer mode: the pad drives the real pointer (its own cursor dropped, the pointer goes on from there)
    if (!this->padPhantom_()) {
        if (this->_padAway.contains(pad)) this->padAwayEnd_(pad);
        if (this->_foreign.contains(pad)) {this->_padPointer = this->_foreign.value(pad).local; this->foreignLeave_(pad);}
        return false;
    }
    if (!this->_foreign.contains(pad) && !this->_padAway.contains(pad)) this->padPhantomAt_(pad, this->_padPointer.isNull() ? this->localPointer_() : this->_padPointer);
    return true;
}

void xmouse::core::Hub::padPhantomAt_(const QString& pad, const QPoint& local)
{
    const QString self = this->_group.self();

    // Its own cursor, drawn over the screens with its name like the cursor of a member: private mode, it passes over
    // and acts on nothing; phantom mode, the real pointer goes there only for its clicks
    if (!this->_foreign.contains(pad)) {
        Foreign f;
        f.passing = this->_config.settings().privateMode;
        f.id = pad;
        f.name = this->_pads.value(pad).name.isEmpty() ? QString("Phone") : this->_pads.value(pad).name;
        f.color = colorOf(pad);
        f.local = local;
        f.seen = now_ms();
        if (this->_granted.contains(pad)) f.state = ForeignState::Granted; // given the panel before (the pad came back / reconnected)
        this->_foreign[pad] = f;
        this->_log.info("input", pad, "cursor of " + f.name + " here (" + (f.passing ? "private mode: passing" : "phantom") + ")");
    }
    this->foreignPointer_(pad, local, 0, false);
    this->_presence[pad] = Presence{pad, self, this->_layout.toGlobal(self, this->_foreign.value(pad).local), this->_foreign.value(pad).local, 0, now_ms(), 0}; // on the map too
    this->padPresence_();
}

void xmouse::core::Hub::padPresence_(void)
{
    // The maps at 30 Hz at most (the overlay draws every move), the last position always told
    if (now_ms() - this->_padPresenceAt >= PAD_PRESENCE_INTERVAL) {
        this->_padPresenceAt = now_ms();
        emit this->presenceChanged();
    } else if (!this->_padPresencePending) {
        this->_padPresencePending = true;
        QTimer::singleShot(PAD_PRESENCE_INTERVAL, this, [this]() {
            this->_padPresencePending = false;
            this->_padPresenceAt = now_ms();
            emit this->presenceChanged();
        });
    }
}

void xmouse::core::Hub::padLeave_(const QString& pad, const QPoint& global, const Hit& hit)
{
    PadAway away;

    // Its cursor here ends, it enters the other device under its own name (the relay says it comes from the pad)
    if (this->_foreign.contains(pad)) this->foreignLeave_(pad);
    away.target = hit.device;
    away.global = QPointF(global);
    away.local = hit.local;
    this->_padAway[pad] = away;
    this->sendTo_(away.target, QJsonObject{{"t", "enter"}, {"x", hit.local.x()}, {"y", hit.local.y()}, {"name", this->_pads.value(pad).name}}, pad);
    this->tellShowOnly_(away.target);
    this->_presence[pad] = Presence{pad, away.target, global, hit.local, 0, now_ms(), 0};
    this->padPresence_();
}

void xmouse::core::Hub::padAwayMotion_(const QString& pad, const double dx, const double dy)
{
    PadAway& away = this->_padAway[pad];
    const QPoint from = away.global.toPoint();
    QPoint to;
    std::optional<Hit> hit;

    away.global += QPointF(dx, dy);
    to = this->_layout.move(from, away.global.toPoint() - from);
    if (to != away.global.toPoint()) away.global = QPointF(to); // stopped on a border: no hidden drift
    hit = this->_layout.hit(to);
    if (!hit) return;
    if (hit->device == this->_group.self()) { // back here: its own cursor again
        this->padAwayEnd_(pad);
        this->padPhantomAt_(pad, hit->local);
        return;
    }
    if (hit->device != away.target) {
        const QString left = away.target;
        if (!this->reachable(hit->device)) {away.global = QPointF(from); return;} // an unreachable device is a wall
        for (int b = 0; b <= static_cast<int>(net::MouseButton::Forward); ++b)
            if (away.buttons & (1 << b)) this->sendBinaryTo_(left, net::ByteWriter(net::Binary::Button).u8(static_cast<std::uint8_t>(b)).u8(0).data(), pad);
        away.buttons = 0;
        away.target = hit->device;
        this->sendTo_(left, QJsonObject{{"t", "leave"}}, pad);
        this->sendTo_(away.target, QJsonObject{{"t", "enter"}, {"x", hit->local.x()}, {"y", hit->local.y()}, {"name", this->_pads.value(pad).name}}, pad);
    }
    away.local = hit->local;
    this->sendBinaryTo_(away.target, net::ByteWriter(net::Binary::Pointer).u16(++away.seq).i32(away.local.x()).i32(away.local.y()).data(), pad);
    this->_presence[pad] = Presence{pad, away.target, to, away.local, 0, now_ms(), 0};
    this->padPresence_();
}

void xmouse::core::Hub::padAwayEnd_(const QString& pad)
{
    auto it = this->_padAway.find(pad);

    if (it == this->_padAway.end()) return;
    for (int b = 0; b <= static_cast<int>(net::MouseButton::Forward); ++b)
        if (it->buttons & (1 << b)) this->sendBinaryTo_(it->target, net::ByteWriter(net::Binary::Button).u8(static_cast<std::uint8_t>(b)).u8(0).data(), pad);
    this->sendTo_(it->target, QJsonObject{{"t", "leave"}}, pad);
    this->_padAway.erase(it);
    if (this->_presence.remove(pad) > 0) emit this->presenceChanged();
}

void xmouse::core::Hub::padAwayInput_(const QString& pad, const QByteArray& data)
{
    const PadAway away = this->_padAway.value(pad);

    this->sendBinaryTo_(away.target, data, pad);
}

void xmouse::core::Hub::padMotion_(const QString& pad, const double dx, const double dy)
{
    const QString self = this->_group.self();
    bool own = false;
    QPoint from;
    QPoint to;
    std::optional<Hit> hit;

    own = this->padOwnCursor_(pad);
    if (own && this->_padAway.contains(pad)) {this->padAwayMotion_(pad, dx, dy); return;} // its own cursor on another device
    if (!own && this->_away) {this->onMotion_(dx, dy); return;} // pointer mode: it drives the cursor of this device where it is
    this->_homePad = pad;
    from = this->_layout.toGlobal(self, own ? this->_foreign.value(pad).local : this->localPointer_());
    to = this->_layout.move(from, QPoint(static_cast<int>(std::lround(dx)), static_cast<int>(std::lround(dy))));
    hit = this->_layout.hit(to);
    if (hit && hit->device != self && this->reachable(hit->device)) {
        // Through the border of this device: phantom / private mode, its own cursor goes on the other one (the cursor of
        // this device stays with the person); pointer mode, the cursor of this device goes (nothing to release here)
        if (own) {this->padLeave_(pad, to, *hit); return;}
        this->_awayByPad = true;
        this->leaveTo_(to, hit->device, hit->local);
        return;
    }
    if (!hit || hit->device != self) to = this->_layout.clampTo(self, to); // a wall
    hit = this->_layout.hit(to);
    if (!hit) return;
    if (own) {this->padPhantomAt_(pad, hit->local); return;}
    this->_padPointer = hit->local;
    if (this->_injector) this->_injector->mouseMove(this->injectPoint_(hit->local).x(), this->injectPoint_(hit->local).y());
    this->_presenceDirty = true;
}

void xmouse::core::Hub::padButton_(const QString& pad, const net::MouseButton button, const bool down)
{
    const bool own = this->padOwnCursor_(pad);

    if (own && this->_padAway.contains(pad)) {
        PadAway& away = this->_padAway[pad];
        if (down) away.buttons |= 1 << static_cast<int>(button);
        else away.buttons &= ~(1 << static_cast<int>(button));
        this->padAwayInput_(pad, net::ByteWriter(net::Binary::Button).u8(static_cast<std::uint8_t>(button)).u8(down ? 1 : 0).i32(away.local.x()).i32(away.local.y()).data());
        return;
    }
    if (own) {this->foreignButton_(pad, button, down); return;} // at its own cursor here (private mode: nothing)
    if (this->_away) {this->onButton_(button, down); return;}
    if (this->_injector) this->_injector->mouseButton(button, down);
}

void xmouse::core::Hub::padWheel_(const QString& pad, const int dx, const int dy)
{
    const bool own = this->padOwnCursor_(pad);

    if (own && this->_padAway.contains(pad)) {this->padAwayInput_(pad, net::ByteWriter(net::Binary::Wheel).i16(static_cast<std::int16_t>(std::clamp(dx, -32000, 32000))).i16(static_cast<std::int16_t>(std::clamp(dy, -32000, 32000))).data()); return;}
    if (own) {this->foreignWheel_(pad, dx, dy); return;}
    if (this->_away) {this->onWheel_(dx, dy); return;}
    if (this->_injector) this->_injector->mouseWheel(dx, dy);
}

void xmouse::core::Hub::padKey_(const QString& pad, const QString& code, const bool down)
{
    const bool own = this->padOwnCursor_(pad);

    if (own && this->_padAway.contains(pad)) {this->padAwayInput_(pad, net::ByteWriter(net::Binary::Key).u8(down ? 1 : 0).text8(code).data()); return;}
    if (own) {this->foreignKey_(pad, code, down); return;}
    if (this->_away) {this->onKey_(code, down); return;}
    if (this->_injector) this->_injector->key(code, down);
}

void xmouse::core::Hub::padText_(const QString& pad, const QString& text)
{
    bool own = false;

    if (text.isEmpty()) return;
    own = this->padOwnCursor_(pad);
    if (own && this->_padAway.contains(pad)) {this->padAwayInput_(pad, net::ByteWriter(net::Binary::Text).bytes(text.left(4096).toUtf8()).data()); return;}
    if (own) {this->foreignText_(pad, text.left(4096)); return;} // private mode: the pad acts only on the other devices
    if (this->_away) {this->sendBinaryTo_(this->_target, net::ByteWriter(net::Binary::Text).bytes(text.left(4096).toUtf8()).data()); return;}
    if (this->_injector) this->_injector->text(text.left(4096));
}
