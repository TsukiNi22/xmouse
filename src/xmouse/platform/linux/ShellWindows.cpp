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
##  @file ShellWindows.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Attribute
#include <utils/utils.hpp>
#include "xmouse/platform/linux/ShellWindows.hpp"
#include "xmouse/platform/linux/ShellCapture.hpp"
#include <QDBusConnection>
#include <QDBusArgument>
#include <QDBusMessage>
#include <QFile>

//----------------------------------------------------------------//
/* DEFINE */

#define CALL_TIMEOUT 1500 // ms (a picture of a big window takes a few ms)

//----------------------------------------------------------------//
/* TOOLS */

_cold static QDBusMessage call(const QString& method, const QList<QVariant>& args = {})
{
    QDBusMessage message = QDBusMessage::createMethodCall(XMOUSE_SHELL_SERVICE, XMOUSE_SHELL_PATH, XMOUSE_SHELL_INTERFACE, method);

    message.setArguments(args);
    return QDBusConnection::sessionBus().call(message, QDBus::Block, CALL_TIMEOUT);
}

//----------------------------------------------------------------//
/* CLASS */

xmouse::platform::ShellWindows::ShellWindows(void)
{
    /* Nothing */
}

xmouse::platform::ShellWindows::~ShellWindows()
{
    /* Nothing */
}

QList<xmouse::platform::WindowInfo> xmouse::platform::ShellWindows::list(void)
{
    const QDBusMessage reply = call("ListWindows");
    QList<WindowInfo> out;
    QDBusArgument array;

    // a(tssuiiii): id, title, class, pid, x, y, width, height
    if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty()) return out;
    array = reply.arguments().front().value<QDBusArgument>();
    array.beginArray();
    while (!array.atEnd()) {
        WindowInfo w;
        qulonglong id = 0;
        uint pid = 0;
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;
        array.beginStructure();
        array >> id >> w.title >> w.app >> pid >> x >> y >> width >> height;
        array.endStructure();
        w.id = id;
        w.pid = pid;
        w.rect = QRect(x, y, width, height);
        w.document = document_of_process(w.pid);
        out.append(w);
    }
    array.endArray();
    return out;
}

std::optional<xmouse::platform::WindowInfo> xmouse::platform::ShellWindows::info(const quint64 id)
{
    for (const WindowInfo& w: this->list())
        if (w.id == id) return w;
    return std::nullopt;
}

QImage xmouse::platform::ShellWindows::capture(const quint64 id)
{
    const QDBusMessage reply = call("CaptureWindow", {QVariant::fromValue(static_cast<qulonglong>(id))});
    QString path;
    QImage out;

    // The extension writes a png in the runtime folder of the session (0700), read then removed
    if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty()) return {};
    path = reply.arguments().front().toString();
    if (path.isEmpty() || !path.startsWith(qEnvironmentVariable("XDG_RUNTIME_DIR", "/run/user"))) return {};
    (void)out.load(path, "PNG");
    (void)QFile::remove(path);
    return out;
}

void xmouse::platform::ShellWindows::activate(const quint64 id)
{
    (void)call("ActivateWindow", {QVariant::fromValue(static_cast<qulonglong>(id))});
}
