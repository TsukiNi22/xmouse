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

//----------------------------------------------------------------//
/* CROSSING */

void xmouse::core::Hub::onCrossed_(const QPoint& local, const QPoint& push)
{
    const QString self = this->_group.self();
    const QPoint global = this->_layout.toGlobal(self, local);
    const std::optional<Hit> hit = this->_layout.hit(global + push);

    // Nothing there (a layout changed meanwhile) or the device cannot be reached: the pointer stays here
    if (!hit || hit->device == self || !this->reachable(hit->device)) {
        this->_capture->release(local - push * 2);
        return;
    }
    this->_awayByPad = false;
    this->leaveTo_(global + push, hit->device, hit->local);
}

void xmouse::core::Hub::leaveTo_(const QPoint& global, const QString& device, const QPoint& local)
{
    this->_away = true;
    this->_target = device;
    this->_global = QPointF(global);
    this->_targetLocal = local;
    this->_keysDown.clear();
    this->_buttonsDown = 0;
    this->sendTo_(this->_target, QJsonObject{{"t", "enter"}, {"x", local.x()}, {"y", local.y()}, {"name", this->_config.deviceName()}});
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
    this->sendTo_(this->_target, QJsonObject{{"t", "enter"}, {"x", this->_targetLocal.x()}, {"y", this->_targetLocal.y()}, {"name", this->_config.deviceName()}});
    emit this->awayChanged();
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
    if (this->_awayByPad) {
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

    if (!this->_presenceDirty || !this->_group.joined()) return;
    this->_presenceDirty = false;
    if (this->_away) {
        on = this->_target;
        global = this->_global.toPoint();
        if (this->_buttonsDown) flags |= XMOUSE_PRESENCE_DOWN;
    } else {
        on = this->_group.self();
        global = this->_layout.toGlobal(on, this->localPointer_());
        flags |= XMOUSE_PRESENCE_HIDDEN;
    }
    if (now_ms() < this->_pingUntil) flags |= XMOUSE_PRESENCE_PING;
    if (this->_laser) flags |= XMOUSE_PRESENCE_LASER;
    mine = Presence{this->_group.self(), on, global, flags, now_ms(), ++this->_presenceSeq};
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

void xmouse::core::Hub::padMotion_(const double dx, const double dy)
{
    const QString self = this->_group.self();
    QPoint from;
    QPoint to;
    std::optional<Hit> hit;

    if (this->_away) {this->onMotion_(dx, dy); return;}
    from = this->_layout.toGlobal(self, this->localPointer_());
    to = this->_layout.move(from, QPoint(static_cast<int>(std::lround(dx)), static_cast<int>(std::lround(dy))));
    hit = this->_layout.hit(to);
    if (hit && hit->device != self && this->reachable(hit->device)) {
        // Through the border of this device: the cursor goes on the other one (nothing to release here)
        this->_awayByPad = true;
        this->leaveTo_(to, hit->device, hit->local);
        return;
    }
    if (!hit || hit->device != self) to = this->_layout.clampTo(self, to); // a wall
    hit = this->_layout.hit(to);
    if (!hit) return;
    this->_padPointer = hit->local;
    if (this->_injector) this->_injector->mouseMove(this->injectPoint_(hit->local).x(), this->injectPoint_(hit->local).y());
    this->_presenceDirty = true;
}

void xmouse::core::Hub::padButton_(const net::MouseButton button, const bool down)
{
    if (this->_away) {this->onButton_(button, down); return;}
    if (this->_injector) this->_injector->mouseButton(button, down);
}

void xmouse::core::Hub::padWheel_(const int dx, const int dy)
{
    if (this->_away) {this->onWheel_(dx, dy); return;}
    if (this->_injector) this->_injector->mouseWheel(dx, dy);
}

void xmouse::core::Hub::padKey_(const QString& code, const bool down)
{
    if (this->_away) {this->onKey_(code, down); return;}
    if (this->_injector) this->_injector->key(code, down);
}

void xmouse::core::Hub::padText_(const QString& text)
{
    if (text.isEmpty()) return;
    if (this->_away) {this->sendBinaryTo_(this->_target, net::ByteWriter(net::Binary::Text).bytes(text.left(4096).toUtf8()).data()); return;}
    if (this->_injector) this->_injector->text(text.left(4096));
}
