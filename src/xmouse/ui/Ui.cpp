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
##  @file Ui.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#ifdef _WIN32
    #include <windows.h>
#else
    #include <QSocketNotifier>
    #include <unistd.h>
    #include <csignal>
#endif
#define _Attribute
#include <utils/utils.hpp>
#include "xmouse/ui/gui/MainWindow.hpp"
#include "xmouse/ui/gui/Overlay.hpp"
#include "xmouse/ui/gui/Theme.hpp"
#include "xmouse/ui/tty/Tui.hpp"
#include "xmouse/core/Hub.hpp"
#include "xmouse/ui/Ui.hpp"
#include <QCoreApplication>
#include <QApplication>
#include <QMessageBox>
#include <QDialog>
#include <memory>
#include <QTimer>

#ifndef _WIN32
    static int g_stopPipe[2] = {-1, -1}; // signal handler -> event loop

    static void on_stop_signal(const int sig)
    {
        const char c = static_cast<char>(sig);
        (void)::write(g_stopPipe[1], &c, 1); // async-signal-safe: the rest runs in the event loop
    }
#endif

// SIGTERM / SIGINT / SIGHUP: a clean stop (the cursor of this device comes back, the members are told)
static void handle_stop_signals(QCoreApplication* app)
{
    #ifndef _WIN32
        struct sigaction action = {};
        QSocketNotifier* notifier = nullptr;

        if (::pipe(g_stopPipe) != 0) return;
        action.sa_handler = on_stop_signal;
        sigemptyset(&action.sa_mask);
        for (const int sig: {SIGTERM, SIGINT, SIGHUP})
            (void)sigaction(sig, &action, nullptr);
        notifier = new QSocketNotifier(g_stopPipe[0], QSocketNotifier::Read, app);
        QObject::connect(notifier, &QSocketNotifier::activated, app, [app, notifier]() {
            char c = 0;
            if (::read(g_stopPipe[0], &c, 1) != 1) return;
            notifier->setEnabled(false);
            app->quit();
        });
    #else
        (void)app;
    #endif
}

bool xmouse::ui::has_display(void)
{
    #if defined(_WIN32) || defined(__APPLE__)
        return true;
    #else
        return !qEnvironmentVariableIsEmpty("DISPLAY") || !qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY");
    #endif
}

void xmouse::ui::choose_platform(void)
{
    #if defined(__linux__)
        const QString forced = qEnvironmentVariable("XMOUSE_QPA");

        if (!forced.isEmpty()) {qputenv("QT_QPA_PLATFORM", forced.toUtf8()); return;}
        if (qEnvironmentVariableIsSet("QT_QPA_PLATFORM")) return;

        // Wayland: a client cannot stay on top of the others nor place its windows (no layer shell on GNOME):
        // the overlays of the cursors need Xwayland, where they are override-redirect windows above every app
        if (!qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY") && !qEnvironmentVariableIsEmpty("DISPLAY")) qputenv("QT_QPA_PLATFORM", "xcb");
    #endif
}

// XMOUSE_SNAPSHOT=<file.png> (+ XMOUSE_SNAPSHOT_PAGE=0..3): the window saved after 2 s, then quit (docs, ui checks)
static void snapshot_if_asked(xmouse::ui::gui::MainWindow* window)
{
    const QString path = qEnvironmentVariable("XMOUSE_SNAPSHOT");

    if (path.isEmpty()) return;
    window->showPage(qEnvironmentVariableIntValue("XMOUSE_SNAPSHOT_PAGE"));
    if (qEnvironmentVariableIsSet("XMOUSE_SNAPSHOT_QR")) QMetaObject::invokeMethod(window, "showQr_", Qt::QueuedConnection, Q_ARG(int, -1));
    QTimer::singleShot(2000, window, [window, path]() {
        QWidget* shown = window;
        for (QWidget* top: QApplication::topLevelWidgets())
            if (top != window && top->isVisible() && qobject_cast<QDialog*>(top)) shown = top; // a dialog opened for the snapshot
        shown->grab().save(path);
        QCoreApplication::quit();
    });
}

// Join asked on the command line (xmouse join <address> <code>)
static void join_if_asked(xmouse::core::Hub& hub, const QString& join, const QString& code)
{
    if (join.isEmpty()) return;
    QTimer::singleShot(0, &hub, [&hub, join, code]() {hub.join(join, code);});
}

int xmouse::ui::run_gui(int& argc, char** argv, core::Config& config, const QString& join, const QString& code)
{
    std::unique_ptr<QApplication> app;
    std::unique_ptr<core::Hub> hub;
    std::unique_ptr<gui::MainWindow> window;
    int status = 0;

    choose_platform();
    app = std::make_unique<QApplication>(argc, argv);
    QApplication::setApplicationName("xmouse");
    QApplication::setApplicationVersion(XMOUSE_VERSION);
    QApplication::setQuitOnLastWindowClosed(false);
    gui::Theme::apply(*app, config.settings().theme);
    gui::Theme::keyboard(*app);
    hub = std::make_unique<core::Hub>(config);
    try {
        hub->start();
    } catch (const std::exception& e) {
        QMessageBox::critical(nullptr, "xmouse", QString("xmouse cannot start:\n") + e.what());
        return 1;
    }
    window = std::make_unique<gui::MainWindow>(*hub);
    if (!config.settings().startMinimized) window->show();
    snapshot_if_asked(window.get());
    handle_stop_signals(app.get());
    join_if_asked(*hub, join, code);
    status = app->exec();
    window.reset();
    hub->stop();
    return status;
}

int xmouse::ui::run_tty(int& argc, char** argv, core::Config& config, const QString& join, const QString& code)
{
    std::unique_ptr<QCoreApplication> app;
    std::unique_ptr<core::Hub> hub;
    std::unique_ptr<gui::Overlay> overlay;
    std::unique_ptr<tty::Tui> tui;
    const bool display = has_display();
    int status = 0;

    // With a display: the foreign cursors are still drawn (and the edges captured), only the panel is in the terminal
    if (display) {
        choose_platform();
        app = std::make_unique<QApplication>(argc, argv);
        QApplication::setQuitOnLastWindowClosed(false);
    } else app = std::make_unique<QCoreApplication>(argc, argv);
    hub = std::make_unique<core::Hub>(config);
    hub->start();
    if (display) overlay = std::make_unique<gui::Overlay>(*hub);
    tui = std::make_unique<tty::Tui>(*hub);
    tui->start();
    handle_stop_signals(app.get());
    join_if_asked(*hub, join, code);

    // Without a person to answer: joins are refused after their delay, the code of the terminal panel is the way
    status = app->exec();
    tui.reset();
    overlay.reset();
    hub->stop();
    return status;
}
