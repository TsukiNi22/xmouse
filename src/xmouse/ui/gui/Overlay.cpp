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
#include <utils/utils.hpp>
#include "xmouse/ui/gui/Overlay.hpp"
#include <QGuiApplication>
#include <QPainterPath>
#include <QFontMetrics>
#include <QDateTime>
#include <QPainter>
#include <QWindow>

//----------------------------------------------------------------//
/* TOOLS */

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

//----------------------------------------------------------------//
/* WINDOW */

xmouse::ui::gui::OverlayWindow::OverlayWindow(core::Hub& hub, QScreen* screen)
: QWidget(nullptr, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool | Qt::WindowTransparentForInput | Qt::WindowDoesNotAcceptFocus | Qt::X11BypassWindowManagerHint), _hub{hub}, _screen{screen}
{
    this->setAttribute(Qt::WA_TranslucentBackground);
    this->setAttribute(Qt::WA_TransparentForMouseEvents);
    this->setAttribute(Qt::WA_ShowWithoutActivating);
    this->setAttribute(Qt::WA_NoSystemBackground);
    this->setFocusPolicy(Qt::NoFocus);
    this->setWindowTitle("xmouse cursors");
}

void xmouse::ui::gui::OverlayWindow::refresh(void)
{
    bool any = false;

    if (!this->_screen) {this->hide(); return;}
    for (const core::Foreign& f: this->_hub.foreign())
        if (this->_screen->geometry().contains(f.local)) any = true;
    if (!any) {if (this->isVisible()) this->hide(); return;}
    if (this->geometry() != this->_screen->geometry()) this->setGeometry(this->_screen->geometry());
    if (!this->isVisible()) {
        this->show();
        if (this->windowHandle()) this->windowHandle()->setFlag(Qt::WindowTransparentForInput, true); // set again once the native window exists
    }
    this->raise();
    this->update();
}

void xmouse::ui::gui::OverlayWindow::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    const QPoint origin = this->_screen ? this->_screen->geometry().topLeft() : QPoint();
    const double s = this->_screen ? std::clamp(this->_screen->logicalDotsPerInch() / 96.0, 1.0, 3.0) : 1.0;
    QFont font = this->font();

    p.setRenderHint(QPainter::Antialiasing);
    font.setPixelSize(static_cast<int>(12 * s));
    font.setBold(true);
    p.setFont(font);
    for (const core::Foreign& f: this->_hub.foreign()) {
        const QPoint at = f.local - origin;
        const QString extra = state_text(f, this->_hub.config().settings().focusRequestDelay);
        const QString label = extra.isEmpty() ? f.name : f.name + "  -  " + extra;
        const QFontMetrics metrics(font);
        const QRect text = metrics.boundingRect(label).adjusted(-7 * s, -3 * s, 7 * s, 3 * s);
        QRect pill = text.translated(at + QPoint(static_cast<int>(14 * s), static_cast<int>(20 * s)) - text.topLeft());
        QColor color = f.color;

        if (!this->rect().contains(at)) continue;
        if (f.state == core::ForeignState::Paused || f.state == core::ForeignState::Asking) color = color.darker(160);

        // Arrow: white border, the color of the member, held buttons shown as a filled ring
        p.save();
        p.translate(at);
        p.setPen(QPen(Qt::white, 1.6 * s));
        p.setBrush(color);
        p.drawPath(arrow_path(s));
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
}

//----------------------------------------------------------------//
/* OVERLAY */

xmouse::ui::gui::Overlay::Overlay(core::Hub& hub, QObject* parent)
: QObject(parent), _hub{hub}
{
    this->rebuild_();
    QObject::connect(&hub, &core::Hub::foreignChanged, this, &Overlay::refresh);
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
        this->_windows.append(new OverlayWindow(this->_hub, screen));
    this->refresh();
}

void xmouse::ui::gui::Overlay::refresh(void)
{
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
