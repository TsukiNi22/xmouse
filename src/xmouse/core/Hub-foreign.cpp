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
##  @file Hub-foreign.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Attribute
#include <utils/utils.hpp>
#include "xmouse/net/Protocol.hpp"
#include "xmouse/core/Tools.hpp"
#include "xmouse/core/Hub.hpp"
#include <algorithm>
#include <cmath>

//----------------------------------------------------------------//
/* DEFINE */

#define RESTORE_DELAY 40 // ms after the last button up before the real pointer goes back
#define WHEEL_RESTORE_DELAY 350 // ms after the last wheel step
#define PREDICT_HORIZON 80 // ms a cursor is drawn ahead of its last packet at most
#define PREDICT_INTERVAL_GUARD 16 // ms more: the last predicted frame is drawn back on the real position

//----------------------------------------------------------------//
/* ENTER / LEAVE */

void xmouse::core::Hub::foreignEnter_(const QString& id, const QJsonObject& o)
{
    const Rights rights = this->_config.rightsOf(id);
    Foreign f;

    // Private mode / no mouse right: the cursor may cross this device to reach the others, it acts on nothing here
    f.passing = this->_config.settings().privateMode || !rights.mouse;
    f.id = id;
    f.name = this->_layout.device(id).name.isEmpty() ? o.value("name").toString().left(64) : this->_layout.device(id).name;
    f.color = colorOf(id);
    f.local = QPoint(o.value("x").toInt(), o.value("y").toInt());
    f.seen = now_ms();
    if (this->_foreign.contains(id)) {f.state = this->_foreign.value(id).state; f.told = this->_foreign.value(id).told;}
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
    if (this->_pads.contains(id) && this->_presence.remove(id) > 0) emit this->presenceChanged(); // the own cursor of a pad: off the map
    this->_foreign.erase(it);
    this->_restoreTimer.start(RESTORE_DELAY);
    emit this->foreignChanged();
}

//----------------------------------------------------------------//
/* PANEL PROTECTION */

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
        this->sendTo_(f.id, QJsonObject{{"t", "access"}, {"text", "the xmouse panel is protected: click again to ask the access"}});
    } else if (f.state == ForeignState::Paused && this->_config.settings().focusRequest) {
        f.state = ForeignState::Asking;
        f.askedAt = now_ms();
        this->_log.warn("access", f.id, f.name + " asks the access to the panel (" + QString::number(this->_config.settings().focusRequestDelay) + " s)");
        this->sendTo_(f.id, QJsonObject{{"t", "access"}, {"text", "access asked, granted in " + QString::number(this->_config.settings().focusRequestDelay) + " s unless refused"}});
        emit this->accessRequested(f.id, f.name, this->_config.settings().focusRequestDelay);
    }
    emit this->foreignChanged();
    return true;
}

//----------------------------------------------------------------//
/* INPUT */

void xmouse::core::Hub::foreignPointer_(const QString& id, const QPoint& local, const quint16 seq, const bool hasSeq)
{
    auto it = this->_foreign.find(id);
    const QRect desktop = this->_layout.bounds(this->_group.self());
    const qint64 now = now_ms();
    QPoint clamped;
    qint64 dt = 0;

    if (it == this->_foreign.end()) return;
    // A move older than the last one (reordered udp): dropped, the newest position only matters
    if (hasSeq && it->hasSeq && static_cast<qint16>(seq - it->seq) <= 0 && now - it->seen < 1000) return;
    clamped = QPoint(std::clamp(local.x(), desktop.left(), std::max(desktop.left(), desktop.right())), std::clamp(local.y(), desktop.top(), std::max(desktop.top(), desktop.bottom())));

    // Speed of the cursor (smoothed): it is drawn ahead along it while the next packet is late
    dt = now - it->seen;
    if (dt > 0 && dt <= PREDICT_HORIZON) it->velocity = it->velocity * 0.5 + QPointF(clamped - it->local) / static_cast<double>(dt) * 0.5;
    else it->velocity = QPointF();
    it->local = clamped;
    it->seen = now;
    if (hasSeq) {it->seq = seq; it->hasSeq = true;}
    if (it->state == ForeignState::Paused && !this->_panelFocused) {
        bool over = false;
        for (const QRect& r: this->_panelRects)
            if (r.contains(it->local)) over = true;
        if (!over) it->state = ForeignState::Free;
    }

    // Pointer mode: the real pointer is the foreign cursor; phantom mode: it follows only a drag (the button held); a
    // cursor passing over a private device never moves it
    if (!it->passing && (it->buttons != 0 || this->_config.settings().foreignMode == "pointer") && this->_injector && it->state != ForeignState::Paused && it->state != ForeignState::Asking) {
        this->_lastInject = now;
        this->_injector->mouseMove(this->injectPoint_(it->local).x(), this->injectPoint_(it->local).y());
    }
    if (!this->_predict.isActive()) this->_predict.start();
    emit this->foreignChanged();
}

void xmouse::core::Hub::predict_(void)
{
    const qint64 now = now_ms();
    bool drawing = false;
    bool moving = false;

    // Redrawn while a cursor is between its packets (drawn ahead), stopped once every cursor is still
    for (const Foreign& f: this->_foreign) {
        const qint64 age = now - f.seen;
        const bool ahead = age <= PREDICT_HORIZON + PREDICT_INTERVAL_GUARD && (std::abs(f.velocity.x()) > 0.01 || std::abs(f.velocity.y()) > 0.01);
        moving = moving || ahead;
        drawing = drawing || (ahead && age >= 12);
    }
    if (drawing) emit this->foreignChanged();
    if (!moving) this->_predict.stop();
}

void xmouse::core::Hub::foreignButton_(const QString& id, const net::MouseButton button, const bool down)
{
    auto it = this->_foreign.find(id);
    const int bit = 1 << static_cast<int>(button);
    bool anyHeld = false;
    QPoint at;

    if (it == this->_foreign.end()) return;
    if (it->passing) {
        if (down && !it->told) {
            it->told = true;
            this->sendTo_(id, QJsonObject{{"t", "access"}, {"text", this->_config.settings().privateMode ? "private mode: your cursor passes over this device without acting" : "no mouse right here: your cursor only passes over"}});
        }
        return;
    }
    if (!this->_injector || !this->_config.rightsOf(id).mouse) return;
    if (down && this->blocked_(*it, it->local)) return;
    if (!down && !(it->buttons & bit)) return; // the down was held back: its up too
    for (const Foreign& f: this->_foreign)
        if (f.buttons != 0) anyHeld = true;

    // Phantom mode, teleported click: the real pointer goes there, clicks, and comes back after the last button up (pointer mode: it is there already)
    if (down && !anyHeld && !this->_hasSavedPointer && this->_config.settings().foreignMode == "phantom" && this->_config.settings().restorePointer && this->_capture && this->_capture->knowsPointer() && !this->_away) {
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

    if (it == this->_foreign.end() || it->passing || !this->_injector || !this->_config.rightsOf(id).mouse) return;
    if (this->blocked_(*it, it->local)) return;
    if (!this->_hasSavedPointer && this->_config.settings().foreignMode == "phantom" && this->_config.settings().restorePointer && this->_capture && this->_capture->knowsPointer() && !this->_away) {
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

    if (it == this->_foreign.end() || it->passing || !this->_injector || !this->_config.rightsOf(id).keyboard) return;
    if (down && this->_panelFocused && it->state != ForeignState::Granted) {(void)this->blocked_(*it, it->local); return;}
    this->_keyboardOwner = id;
    this->_lastInject = now_ms();
    this->_injector->key(code, down);
}

void xmouse::core::Hub::foreignText_(const QString& id, const QString& text)
{
    auto it = this->_foreign.find(id);

    if (it == this->_foreign.end() || it->passing || !this->_injector || !this->_config.rightsOf(id).keyboard) return;
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

//----------------------------------------------------------------//
/* ACCESS */

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
    this->sendTo_(id, QJsonObject{{"t", "access"}, {"text", "access to the panel refused"}});
    emit this->accessEnded(id, false);
    emit this->foreignChanged();
}

void xmouse::core::Hub::kickCursor(const QString& id)
{
    this->sendTo_(id, QJsonObject{{"t", "kick"}});
    this->foreignLeave_(id);
}

void xmouse::core::Hub::setPrivateMode(const bool enabled)
{
    this->_config.settings().privateMode = enabled;
    this->_config.save();
    for (Foreign& f: this->_foreign) // the cursors here now only pass over (or act again)
        f.passing = enabled || !this->_config.rightsOf(f.id).mouse;
    emit this->foreignChanged();
    this->_log.info("settings", "", QString("private mode ") + (enabled ? "on" : "off"));
}

void xmouse::core::Hub::setRights(const QString& id, const Rights& rights)
{
    this->_config.settings().rights[id] = rights;
    this->_config.save();
    if (this->_foreign.contains(id)) {this->_foreign[id].passing = this->_config.settings().privateMode || !rights.mouse; emit this->foreignChanged();}
    this->_log.info("settings", id, "rights changed");
}
