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
##  @file GrabCapture.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Attribute
#include <utils/utils.hpp>
#include "xmouse/platform/GrabCapture.hpp"
#include "xmouse/platform/Keymap.hpp"
#include <QCoreApplication>
#include <QGuiApplication>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QCursor>
#include <QScreen>

//----------------------------------------------------------------//
/* DEFINE */

#define POLL_INTERVAL 8 // ms

//----------------------------------------------------------------//
/* CLASS */

// Fullscreen window almost transparent: it holds the grab and turns every event into a signal of the capture
class GrabWindow final: public QWidget {
    private:
        xmouse::platform::GrabCapture* _owner = nullptr;
        QPoint _center;

        _hot static std::optional<xmouse::net::MouseButton> button_(const Qt::MouseButton button)
        {
            switch (button) {
                case Qt::LeftButton:    return xmouse::net::MouseButton::Left;
                case Qt::RightButton:   return xmouse::net::MouseButton::Right;
                case Qt::MiddleButton:  return xmouse::net::MouseButton::Middle;
                case Qt::BackButton:    return xmouse::net::MouseButton::Back;
                case Qt::ForwardButton: return xmouse::net::MouseButton::Forward;
                default:                return std::nullopt;
            }
        };

        _hot void keyEvent_(QKeyEvent* event, const bool down)
        {
            QString code;

            // X11 key code = evdev + 8, macOS: the virtual key is not mapped yet (planned with the macOS backend)
            #if defined(__linux__)
                code = xmouse::platform::code_from_evdev(static_cast<int>(event->nativeScanCode()) - 8);
            #endif
            if (!code.isEmpty() && !event->isAutoRepeat()) emit this->_owner->key(code, down);
            event->accept();
        };

    protected:
        void mouseMoveEvent(QMouseEvent* event) override {this->_owner->moved_(event->globalPosition().toPoint());};
        void mousePressEvent(QMouseEvent* event) override
        {
            const std::optional<xmouse::net::MouseButton> b = this->button_(event->button());
            if (b) emit this->_owner->button(*b, true);
        };
        void mouseReleaseEvent(QMouseEvent* event) override
        {
            const std::optional<xmouse::net::MouseButton> b = this->button_(event->button());
            if (b) emit this->_owner->button(*b, false);
        };
        void mouseDoubleClickEvent(QMouseEvent* event) override {this->mousePressEvent(event);};
        void wheelEvent(QWheelEvent* event) override            {emit this->_owner->wheel(event->angleDelta().x(), event->angleDelta().y());};
        void keyPressEvent(QKeyEvent* event) override           {this->keyEvent_(event, true);};
        void keyReleaseEvent(QKeyEvent* event) override         {this->keyEvent_(event, false);};

    public:
        explicit GrabWindow(xmouse::platform::GrabCapture* owner): QWidget(nullptr, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool | Qt::X11BypassWindowManagerHint), _owner{owner}
        {
            this->setAttribute(Qt::WA_TranslucentBackground);
            this->setWindowOpacity(0.01);
            this->setMouseTracking(true);
            this->setCursor(Qt::BlankCursor);
        };
};

xmouse::platform::GrabCapture::GrabCapture(QObject* parent)
: IInputCapture(parent)
{
    this->_poll.setInterval(POLL_INTERVAL);
    this->_poll.setTimerType(Qt::PreciseTimer);
    QObject::connect(&this->_poll, &QTimer::timeout, this, &GrabCapture::onPoll_);
}

xmouse::platform::GrabCapture::~GrabCapture()
{
    this->stop();
}

bool xmouse::platform::GrabCapture::start(void)
{
    if (!qobject_cast<QGuiApplication*>(QCoreApplication::instance())) {this->_error = "no graphical session"; return false;}
    this->_last = QCursor::pos();
    this->_poll.start();
    QTimer::singleShot(0, this, [this]() {emit this->ready();});
    return true;
}

void xmouse::platform::GrabCapture::stop(void)
{
    this->_poll.stop();
    if (this->_capturing) this->release(this->_center);
    if (this->_grab) delete this->_grab.data();
}

void xmouse::platform::GrabCapture::onPoll_(void)
{
    const QPoint p = QCursor::pos();
    const QPoint last = this->_last;

    this->_last = p;
    if (this->_capturing || p == last) return;

    // On the last pixel of an opening, coming from inside: crossed
    for (const core::Edge& e: this->_edges) {
        switch (e.side) {
            case core::Side::Right:  if (p.x() >= e.line - 1 && last.x() < p.x() && p.y() >= e.from && p.y() <= e.to) {this->capture_(p, QPoint(1, 0)); return;}  break;
            case core::Side::Left:   if (p.x() <= e.line && last.x() > p.x() && p.y() >= e.from && p.y() <= e.to) {this->capture_(p, QPoint(-1, 0)); return;}     break;
            case core::Side::Bottom: if (p.y() >= e.line - 1 && last.y() < p.y() && p.x() >= e.from && p.x() <= e.to) {this->capture_(p, QPoint(0, 1)); return;}  break;
            case core::Side::Top:    if (p.y() <= e.line && last.y() > p.y() && p.x() >= e.from && p.x() <= e.to) {this->capture_(p, QPoint(0, -1)); return;}     break;
        }
    }
}

void xmouse::platform::GrabCapture::capture_(const QPoint& at, const QPoint& push)
{
    QScreen* screen = QGuiApplication::screenAt(at);

    if (!screen) screen = QGuiApplication::primaryScreen();
    if (!this->_grab) this->_grab = new GrabWindow(this);
    this->_grab->setGeometry(screen->geometry());
    this->_grab->show();
    this->_grab->raise();
    this->_grab->activateWindow();
    this->_grab->grabMouse(Qt::BlankCursor);
    this->_grab->grabKeyboard();
    this->_center = screen->geometry().center();
    QCursor::setPos(this->_center);
    this->_capturing = true;
    emit this->crossed(at, push);
}

void xmouse::platform::GrabCapture::moved_(const QPoint& position)
{
    const QPoint d = position - this->_center;

    if (!this->_capturing || d.isNull()) return;
    emit this->motion(d.x(), d.y());
    QCursor::setPos(this->_center); // held in the middle: the next move is a delta again
}

void xmouse::platform::GrabCapture::release(const QPoint& local)
{
    if (!this->_capturing) return;
    this->_capturing = false;
    if (this->_grab) {
        this->_grab->releaseKeyboard();
        this->_grab->releaseMouse();
        this->_grab->hide();
    }
    QCursor::setPos(local);
    this->_last = local;
}
