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
##  @file Protocol.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Attribute
#include <utils/utils.hpp>
#include "xmouse/net/Protocol.hpp"

QString xmouse::net::device_kind_name(const DeviceKind kind)
{
    switch (kind) {
        case DeviceKind::Desktop: return "desktop";
        case DeviceKind::Phone: return "phone";
        default: return "web";
    }
}

xmouse::net::DeviceKind xmouse::net::device_kind_from(const QString& name)
{
    if (name == "desktop") return DeviceKind::Desktop;
    if (name == "phone") return DeviceKind::Phone;
    return DeviceKind::Web;
}

QString xmouse::net::mouse_button_name(const MouseButton button)
{
    switch (button) {
        case MouseButton::Left: return "left";
        case MouseButton::Middle: return "middle";
        case MouseButton::Right: return "right";
        case MouseButton::Back: return "back";
        default: return "forward";
    }
}
