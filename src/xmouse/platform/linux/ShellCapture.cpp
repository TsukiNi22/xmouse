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
##  @file ShellCapture.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Attribute
#include <utils/utils.hpp>
#include "xmouse/platform/linux/ShellCapture.hpp"
#include "xmouse/platform/Keymap.hpp"
#include <QDBusPendingCallWatcher>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QTimer>

//----------------------------------------------------------------//
/* DEFINE */

#define POINTER_TIMEOUT 200 // ms for GetPointer (local bus: answered in well under 1 ms)

//----------------------------------------------------------------//
/* CLASS */

xmouse::platform::ShellCapture::~ShellCapture()
{
    this->stop();
}

bool xmouse::platform::ShellCapture::available(void)
{
    QDBusMessage call = QDBusMessage::createMethodCall(XMOUSE_SHELL_SERVICE, XMOUSE_SHELL_PATH, XMOUSE_SHELL_INTERFACE, "Version");
    const QDBusMessage reply = QDBusConnection::sessionBus().call(call, QDBus::Block, 1000);

    if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty()) return false;
    return reply.arguments().front().toUInt() == XMOUSE_SHELL_VERSION;
}

bool xmouse::platform::ShellCapture::start(void)
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    bool ok = true;

    if (!bus.isConnected()) {this->_error = "no session bus"; return false;}
    ok &= bus.connect(XMOUSE_SHELL_SERVICE, XMOUSE_SHELL_PATH, XMOUSE_SHELL_INTERFACE, "Crossed", this, SLOT(onCrossed_(int,int,int,int)));
    ok &= bus.connect(XMOUSE_SHELL_SERVICE, XMOUSE_SHELL_PATH, XMOUSE_SHELL_INTERFACE, "Motion", this, SLOT(onMotion_(double,double)));
    ok &= bus.connect(XMOUSE_SHELL_SERVICE, XMOUSE_SHELL_PATH, XMOUSE_SHELL_INTERFACE, "Button", this, SLOT(onButton_(uint,bool)));
    ok &= bus.connect(XMOUSE_SHELL_SERVICE, XMOUSE_SHELL_PATH, XMOUSE_SHELL_INTERFACE, "Wheel", this, SLOT(onWheel_(int,int)));
    ok &= bus.connect(XMOUSE_SHELL_SERVICE, XMOUSE_SHELL_PATH, XMOUSE_SHELL_INTERFACE, "Key", this, SLOT(onKey_(uint,bool)));
    ok &= bus.connect(XMOUSE_SHELL_SERVICE, XMOUSE_SHELL_PATH, XMOUSE_SHELL_INTERFACE, "Ended", this, SLOT(onEnded_()));
    if (!ok) {this->_error = "cannot listen to the GNOME Shell extension"; return false;}
    QTimer::singleShot(0, this, [this]() {emit this->ready();});
    return true;
}

void xmouse::platform::ShellCapture::stop(void)
{
    if (this->_capturing) this->release(this->_last);
    this->call_("SetBarriers", {QVariant::fromValue(QList<int>())});
}

void xmouse::platform::ShellCapture::call_(const QString& method, const QList<QVariant>& args)
{
    QDBusMessage call = QDBusMessage::createMethodCall(XMOUSE_SHELL_SERVICE, XMOUSE_SHELL_PATH, XMOUSE_SHELL_INTERFACE, method);
    QDBusPendingCallWatcher* watcher = nullptr;

    call.setArguments(args);
    watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(call, 5000), this);
    QObject::connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, method](QDBusPendingCallWatcher* w) {
        if (w->isError()) {
            this->_error = method + ": " + w->error().message();
            emit this->failed(this->_error);
        }
        w->deleteLater();
    });
}

void xmouse::platform::ShellCapture::setEdges(const QList<core::Edge>& edges)
{
    QList<int> flat;

    // 5 integers per edge (x1, y1, x2, y2, side), the barrier on the line just outside the screen
    for (const core::Edge& e: edges) {
        const bool vertical = e.side == core::Side::Left || e.side == core::Side::Right;
        if (vertical) flat << e.line << e.from << e.line << e.to + 1;
        else flat << e.from << e.line << e.to + 1 << e.line;
        flat << static_cast<int>(e.side); // Left 0, Right 1, Top 2, Bottom 3 (the same order in the extension)
    }
    this->call_("SetBarriers", {QVariant::fromValue(flat)});
}

void xmouse::platform::ShellCapture::release(const QPoint& local)
{
    if (!this->_capturing) return;
    this->_capturing = false;
    this->_last = local;
    this->call_("Release", {local.x(), local.y()});
}

QPoint xmouse::platform::ShellCapture::pointer(void) const
{
    QDBusMessage call = QDBusMessage::createMethodCall(XMOUSE_SHELL_SERVICE, XMOUSE_SHELL_PATH, XMOUSE_SHELL_INTERFACE, "GetPointer");
    const QDBusMessage reply = QDBusConnection::sessionBus().call(call, QDBus::Block, POINTER_TIMEOUT);

    if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().size() != 2) return this->_last;
    return QPoint(reply.arguments().at(0).toInt(), reply.arguments().at(1).toInt());
}

//----------------------------------------------------------------//
/* SIGNALS */

void xmouse::platform::ShellCapture::onCrossed_(const int x, const int y, const int dx, const int dy)
{
    this->_capturing = true;
    this->_last = QPoint(x, y);
    emit this->crossed(QPoint(x, y), QPoint(dx, dy));
}

void xmouse::platform::ShellCapture::onKey_(const uint evdev, const bool down)
{
    const QString code = code_from_evdev(static_cast<int>(evdev));

    if (this->_capturing && !code.isEmpty()) emit this->key(code, down);
}

void xmouse::platform::ShellCapture::onEnded_(void)
{
    if (!this->_capturing) return;
    this->_capturing = false;
    emit this->ended();
}
