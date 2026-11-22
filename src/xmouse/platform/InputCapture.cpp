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
##  @file InputCapture.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#ifdef _WIN32
    #include <windows.h>
#endif
#define _Attribute
#include <utils/utils.hpp>
#include "xmouse/platform/IInputCapture.hpp"
#include "xmouse/platform/GrabCapture.hpp"
#ifdef _WIN32
    #include "xmouse/platform/windows/HookCapture.hpp"
#elif defined(XMOUSE_HAS_DBUS)
    #include "xmouse/platform/linux/PortalCapture.hpp"
    #include "xmouse/platform/linux/ShellCapture.hpp"
#endif
#include <QtGlobal>

std::unique_ptr<xmouse::platform::IInputCapture> xmouse::platform::create_input_capture(QObject* parent)
{
    #ifdef _WIN32
        return std::make_unique<HookCapture>(parent);
    #else
        // Wayland: only the compositor knows the pointer, the portal InputCapture is the way (the session type, not the Qt platform: xmouse may run on Xwayland)
        const bool wayland = qEnvironmentVariable("XDG_SESSION_TYPE") == "wayland" || qEnvironmentVariableIsSet("WAYLAND_DISPLAY");
        #if defined(XMOUSE_HAS_DBUS)
            // GNOME: the xmouse extension when it is enabled (no dialog), else the portal (a dialog per run on GNOME 50)
            if (wayland && ShellCapture::available()) return std::make_unique<ShellCapture>(parent);
            if (wayland) return std::make_unique<PortalCapture>(parent);
        #else
            (void)wayland;
        #endif
        return std::make_unique<GrabCapture>(parent);
    #endif
}
