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
##  @file Overlay.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Attribute
#define _Trigo
#include <utils/utils.hpp>
#include "xmouse/ui/gui/Overlay.hpp"
#include "xmouse/net/Protocol.hpp"
#include <QGuiApplication>
#include <QPainterPath>
#include <QFontMetrics>
#include <functional>
#include <QDateTime>
#include <QPainter>
#include <QWindow>
#include <cmath>

//----------------------------------------------------------------//
/* DEFINE */

#define ANIMATION_INTERVAL 30 // ms between two frames of the waves / trails
#define TRAIL_LIFETIME 700 // ms a point of a laser trail stays
#define PING_PERIOD 900 // ms of one wave

//----------------------------------------------------------------//
/* TOOLS */

_hot static qint64 now_ms(void) {return QDateTime::currentMSecsSinceEpoch();}

// Arrow cursor (12 x 19 at scale 1), tip at (0, 0)
_hot static QPainterPath arrow_path(const double s)
{
    QPainterPath path;

    path.moveTo(0, 0);
    path.lineTo(0, 17 * s);
    path.lineTo(4.4 * s, 13 * s);
    path.lineTo(7.6 * s, 19.4 * s);
    path.lineTo(10.2 * s, 18.2 * s);
    path.lineTo(7.1 * s, 12 * s);
    path.lineTo(12.6 * s, 12 * s);
    path.closeSubpath();
    return path;
}

_cold static QString state_text(const xmouse::core::Foreign& f, const int delay)
{
    if (f.passing) return "passing";
    switch (f.state) {
        case xmouse::core::ForeignState::Paused: return "locked: click again to ask";
        case xmouse::core::ForeignState::Asking: {
            const qint64 left = delay - (QDateTime::currentMSecsSinceEpoch() - f.askedAt) / 1000;
            return "access in " + QString::number(std::max<qint64>(0, left)) + " s";
        }
        case xmouse::core::ForeignState::Granted: return "panel access";
        default: return "";
    }
}

// Three rings growing out of a point ("look here")
_hot static void paint_wave(QPainter& p, const QPoint& at, const QColor& color, const double s)
{
    const double phase = static_cast<double>(now_ms() % PING_PERIOD) / PING_PERIOD;

    p.save();
    p.setBrush(Qt::NoBrush);
    for (int i = 0; i < 3; ++i) {
        const double t = std::fmod(phase + i / 3.0, 1.0);
        QColor ring = color;
        ring.setAlphaF(std::max(0.0, 0.9 * (1.0 - t)));
        p.setPen(QPen(ring, 3.0 * s));
        p.drawEllipse(QPointF(at), (10 + 60 * t) * s, (10 + 60 * t) * s);
    }
    p.restore();
}

//----------------------------------------------------------------//
/* WINDOW */

xmouse::ui::gui::OverlayWindow::OverlayWindow(core::Hub& hub, Overlay& owner, QScreen* screen)
: QWidget(nullptr, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool | Qt::WindowTransparentForInput | Qt::WindowDoesNotAcceptFocus | Qt::X11BypassWindowManagerHint), _hub{hub}, _owner{owner}, _screen{screen}
{
    this->setAttribute(Qt::WA_TranslucentBackground);
    this->setAttribute(Qt::WA_TransparentForMouseEvents);
    this->setAttribute(Qt::WA_ShowWithoutActivating);
    this->setAttribute(Qt::WA_NoSystemBackground);
    this->setFocusPolicy(Qt::NoFocus);
    this->setWindowTitle("xmouse cursors");
    this->setProperty("xmouse_overlay", true);
}

void xmouse::ui::gui::OverlayWindow::refresh(void)
{
    const qint64 now = now_ms();
    bool any = false;

    if (!this->_screen) {this->hide(); return;}
    for (const core::Foreign& f: this->_hub.foreign())
        if (this->_screen->geometry().contains(f.drawn(now))) any = true;
    any = any || this->_owner.animating();
    if (!any) {if (this->isVisible()) this->hide(); return;}
    if (this->geometry() != this->_screen->geometry()) this->setGeometry(this->_screen->geometry());
    if (!this->isVisible()) {
        this->show();
        if (this->windowHandle()) this->windowHandle()->setFlag(Qt::WindowTransparentForInput, true); // set again once the native window exists
    }
    this->raise();
    this->update();
}

void xmouse::ui::gui::OverlayWindow::paintFollow_(QPainter& p, const double s)
{
    const QString id = this->_hub.following();
    const core::Presence pr = this->_hub.presence().value(id);
    const QString self = this->_hub.group().self();
    const QRect screen = this->_screen->geometry();
    QPointF target;
    QPointF center;
    QPointF direction;
    QPointF edge;
    double length = 0;
    double k = 0;
    QColor color = core::Hub::colorOf(id);
    QString label;

    if (id.isEmpty() || pr.id.isEmpty()) return;
    // Only on the screen where my own cursor is (the primary one when it is away): one arrow, not one per screen
    QScreen* mine = QGuiApplication::screenAt(this->_hub.ownPointer());
    if (!mine) mine = QGuiApplication::primaryScreen();
    if (mine != this->_screen) return;
    target = QPointF(this->_owner.toLocal(pr.global));
    if (pr.on == self && !(pr.flags & XMOUSE_PRESENCE_HIDDEN) && screen.contains(target.toPoint())) return; // here: its cursor is drawn with a halo
    center = QPointF(screen.center());
    direction = target - center;
    length = std::hypot(direction.x(), direction.y());
    if (length < 1) return;
    k = std::min((screen.width() / 2.0 - 40 * s) / std::max(1e-6, std::abs(direction.x())), (screen.height() / 2.0 - 40 * s) / std::max(1e-6, std::abs(direction.y())));
    edge = center + direction * std::min(1.0, k) - QPointF(screen.topLeft());
    label = this->_hub.layout().device(id).name + " on " + this->_hub.layout().device(pr.on).name;

    // A triangle pointing at it, its name beside
    p.save();
    p.translate(edge);
    p.rotate(utils::math::trigo::rad_to_deg(std::atan2(direction.y(), direction.x())));
    p.setPen(QPen(Qt::white, 1.5 * s));
    p.setBrush(color);
    p.drawPolygon(QPolygonF({QPointF(16 * s, 0), QPointF(-8 * s, -11 * s), QPointF(-8 * s, 11 * s)}));
    p.restore();
    p.setPen(color.darker(130));
    p.drawText(QRectF(edge + QPointF(-120 * s, 16 * s), QSizeF(240 * s, 20 * s)), Qt::AlignCenter, label);
}

void xmouse::ui::gui::OverlayWindow::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    const QPoint origin = this->_screen ? this->_screen->geometry().topLeft() : QPoint();
    const double s = this->_screen ? std::clamp(this->_screen->logicalDotsPerInch() / 96.0, 1.0, 3.0) : 1.0;
    const qint64 now = now_ms();
    const QString self = this->_hub.group().self();
    const bool pointerMode = this->_hub.config().settings().foreignMode == "pointer";
    QFont font = this->font();

    p.setRenderHint(QPainter::Antialiasing);
    font.setPixelSize(static_cast<int>(12 * s));
    font.setBold(true);
    p.setFont(font);

    // Laser trails: fading lines where the cursors went
    for (auto it = this->_owner.trails().constBegin(); it != this->_owner.trails().constEnd(); ++it) {
        const QList<QPair<qint64, QPoint>>& points = it.value();
        for (qsizetype i = 1; i < points.size(); ++i) {
            const double age = static_cast<double>(now - points[i].first) / TRAIL_LIFETIME;
            QColor red(235, 40, 50);
            if (age > 1.0) continue;
            red.setAlphaF(std::max(0.0, 0.85 * (1.0 - age)));
            p.setPen(QPen(red, (5.0 - 3.0 * age) * s, Qt::SolidLine, Qt::RoundCap));
            p.drawLine(points[i - 1].second - origin, points[i].second - origin);
        }
    }

    // "Look here" waves of the cursors on this device (mine at home too)
    for (const core::Presence& pr: this->_hub.presence()) {
        if (!(pr.flags & XMOUSE_PRESENCE_PING) || pr.on != self) continue;
        paint_wave(p, this->_owner.toLocal(pr.global) - origin, core::Hub::colorOf(pr.id), s);
    }

    for (const core::Foreign& f: this->_hub.foreign()) {
        const QPoint at = (pointerMode && !f.passing ? f.local : f.drawn(now)) - origin; // pointer mode: where the real pointer was put
        const QString extra = state_text(f, this->_hub.config().settings().focusRequestDelay);
        const QString label = extra.isEmpty() ? f.name : f.name + "  -  " + extra;
        const QFontMetrics metrics(font);
        const QRect text = metrics.boundingRect(label).adjusted(-7 * s, -3 * s, 7 * s, 3 * s);
        QRect pill = text.translated(at + QPoint(static_cast<int>(14 * s), static_cast<int>(20 * s)) - text.topLeft());
        QColor color = f.color;

        if (!this->rect().contains(at)) continue;
        if (f.state == core::ForeignState::Paused || f.state == core::ForeignState::Asking || f.passing) color = color.darker(160);

        // Followed member: a halo around its cursor
        if (this->_hub.following() == f.id) {
            QColor halo = color;
            halo.setAlpha(90);
            p.setPen(Qt::NoPen);
            p.setBrush(halo);
            p.drawEllipse(QPointF(at), 22 * s, 22 * s);
        }

        // Arrow: white border, the color of the member, held buttons shown as a filled ring; pointer mode: the real
        // pointer is the cursor (its name only), unless it is held on the panel
        p.save();
        p.translate(at);
        if (!pointerMode || f.passing || f.state == core::ForeignState::Paused || f.state == core::ForeignState::Asking) {
            p.setPen(QPen(Qt::white, 1.6 * s));
            p.setBrush(color);
            p.drawPath(arrow_path(s));
        }
        if (f.buttons != 0) {
            p.setPen(QPen(color, 2 * s));
            p.setBrush(Qt::NoBrush);
            p.drawEllipse(QPointF(0, 0), 9 * s, 9 * s);
        }
        p.restore();

        // Name of the device (kept inside the screen)
        if (pill.right() > this->width()) pill.moveRight(at.x() - static_cast<int>(6 * s));
        if (pill.bottom() > this->height()) pill.moveBottom(at.y() - static_cast<int>(6 * s));
        p.setPen(Qt::NoPen);
        p.setBrush(color);
        p.drawRoundedRect(pill, pill.height() / 2.0, pill.height() / 2.0);
        p.setPen(Qt::white);
        p.drawText(pill, Qt::AlignCenter, label);
    }
    this->paintFollow_(p, s);
}

//----------------------------------------------------------------//
/* OVERLAY */

xmouse::ui::gui::Overlay::Overlay(core::Hub& hub, QObject* parent)
: QObject(parent), _hub{hub}
{
    this->rebuild_();
    this->_anim.setInterval(ANIMATION_INTERVAL);
    QObject::connect(&this->_anim, &QTimer::timeout, this, &Overlay::tick_);
    QObject::connect(&hub, &core::Hub::foreignChanged, this, &Overlay::refresh);
    QObject::connect(&hub, &core::Hub::presenceChanged, this, [this]() {if (this->animating() && !this->_anim.isActive()) this->_anim.start();});
    QObject::connect(&hub, &core::Hub::followChanged, this, [this]() {this->_anim.start(); this->refresh();});
    QObject::connect(qGuiApp, &QGuiApplication::screenAdded, this, &Overlay::rebuild_);
    QObject::connect(qGuiApp, &QGuiApplication::screenRemoved, this, &Overlay::rebuild_);
}

xmouse::ui::gui::Overlay::~Overlay()
{
    qDeleteAll(this->_windows);
}

void xmouse::ui::gui::Overlay::rebuild_(void)
{
    qDeleteAll(this->_windows);
    this->_windows.clear();
    for (QScreen* screen: QGuiApplication::screens())
        this->_windows.append(new OverlayWindow(this->_hub, *this, screen));
    this->refresh();
}

QPoint xmouse::ui::gui::Overlay::toLocal(const QPoint& global) const
{
    const QString self = this->_hub.group().self();

    return global - this->_hub.layout().device(self).position + this->_hub.layout().bounds(self).topLeft();
}

bool xmouse::ui::gui::Overlay::animating(void) const
{
    const QString self = this->_hub.group().self();

    if (!this->_trails.isEmpty() || !this->_hub.following().isEmpty()) return true;
    for (const core::Presence& pr: this->_hub.presence())
        if (pr.on == self && (pr.flags & (XMOUSE_PRESENCE_PING | XMOUSE_PRESENCE_LASER))) return true;
    return this->_hub.laser() && !this->_hub.isAway();
}

void xmouse::ui::gui::Overlay::tick_(void)
{
    const qint64 now = now_ms();
    const QString self = this->_hub.group().self();
    const std::function<void(const QString&, const QPoint&)> sample = [this, now](const QString& id, const QPoint& at) {
        QList<QPair<qint64, QPoint>>& points = this->_trails[id];
        if (points.isEmpty() || points.back().second != at) points.append({now, at});
    };

    // Laser: my own cursor at home, the foreign cursors here whose presence says laser
    if (this->_hub.laser() && !this->_hub.isAway()) sample(self, this->_hub.ownPointer());
    for (const core::Foreign& f: this->_hub.foreign())
        if (this->_hub.presence().value(f.id).flags & XMOUSE_PRESENCE_LASER) sample(f.id, f.drawn(now));
    for (auto it = this->_trails.begin(); it != this->_trails.end();) {
        while (!it->isEmpty() && now - it->front().first > TRAIL_LIFETIME)
            it->removeFirst();
        if (it->isEmpty()) it = this->_trails.erase(it);
        else ++it;
    }
    this->refresh();
    if (!this->animating()) this->_anim.stop();
}

void xmouse::ui::gui::Overlay::refresh(void)
{
    if (this->animating() && !this->_anim.isActive()) this->_anim.start();
    for (OverlayWindow* w: this->_windows)
        w->refresh();
}

QList<QWidget*> xmouse::ui::gui::Overlay::windows(void) const
{
    QList<QWidget*> out;

    for (OverlayWindow* w: this->_windows)
        out.append(w);
    return out;
}
