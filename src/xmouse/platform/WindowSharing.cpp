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
##  @file WindowSharing.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Attribute
#include <utils/utils.hpp>
#include "xmouse/platform/IWindows.hpp"
#ifdef _WIN32
    #include "xmouse/platform/windows/WinWindows.hpp"
#elif !defined(__APPLE__)
    #if defined(XMOUSE_HAS_DBUS)
        #include "xmouse/platform/linux/ShellCapture.hpp"
        #include "xmouse/platform/linux/ShellWindows.hpp"
    #endif
    #if defined(XMOUSE_HAS_XTEST)
        #include "xmouse/platform/linux/X11Windows.hpp"
    #endif
#endif
#include <QStringList>
#include <QFileInfo>
#include <QFile>
#include <QDir>

//----------------------------------------------------------------//
/* FACTORY */

std::unique_ptr<xmouse::platform::IWindows> xmouse::platform::create_windows(void)
{
    #ifdef _WIN32
        return std::make_unique<WinWindows>();
    #elif defined(__APPLE__)
        return nullptr; // macOS: the windows stay on their Mac (the cursor, keyboard and clipboard cross)
    #else
        const bool wayland = qEnvironmentVariable("XDG_SESSION_TYPE") == "wayland" || qEnvironmentVariableIsSet("WAYLAND_DISPLAY");
        #if defined(XMOUSE_HAS_DBUS)
            if (wayland && ShellCapture::version() >= 2) return std::make_unique<ShellWindows>(); // GNOME: every window, through the extension
        #endif
        #if defined(XMOUSE_HAS_XTEST)
            if (!wayland && qEnvironmentVariableIsSet("DISPLAY")) return std::make_unique<X11Windows>();
        #endif
        (void)wayland;
        return nullptr;
    #endif
}

//----------------------------------------------------------------//
/* DOCUMENT */

QString xmouse::platform::document_of_process(const qint64 pid)
{
    #if defined(_WIN32) || defined(__APPLE__)
        (void)pid;
        return "";
    #else
        QFile cmdline("/proc/" + QString::number(pid) + "/cmdline");
        QString cwd;
        QList<QByteArray> args;

        // The last argument that is a file (an editor / viewer opened on it): the handoff sends it
        if (pid <= 0 || !cmdline.open(QIODevice::ReadOnly)) return "";
        args = cmdline.readAll().split('\0');
        cwd = QFileInfo("/proc/" + QString::number(pid) + "/cwd").symLinkTarget();
        for (qsizetype i = args.size() - 1; i >= 1; --i) {
            const QString arg = QString::fromLocal8Bit(args[i]);
            QFileInfo info;
            if (arg.isEmpty() || arg.startsWith('-')) continue;
            info = QFileInfo(QDir::isAbsolutePath(arg) || cwd.isEmpty() ? arg : QDir(cwd).filePath(arg));
            if (info.isFile() && info.isReadable() && !info.isExecutable()) return info.absoluteFilePath();
        }
        return "";
    #endif
}
