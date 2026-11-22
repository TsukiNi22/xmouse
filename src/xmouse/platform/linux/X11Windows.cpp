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
##  @file X11Windows.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Attribute
#include <utils/utils.hpp>
#include "xmouse/platform/linux/X11Windows.hpp"
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <cstring>

// Xlib macros that clash with Qt / the code below
#undef Bool
#undef Status
#undef None

//----------------------------------------------------------------//
/* TOOLS */

_cold static Display* display_of(void* native)
{
    return static_cast<Display*>(native);
}

// A property of a window (bytes, items), {} when missing
_cold static QByteArray property_of(Display* d, const Window w, const char* name, const Atom type, unsigned long& items)
{
    const Atom atom = XInternAtom(d, name, False);
    Atom actual = 0;
    int format = 0;
    unsigned long after = 0;
    unsigned char* data = nullptr;
    QByteArray out;

    items = 0;
    if (XGetWindowProperty(d, w, atom, 0, 1 << 20, False, type, &actual, &format, &items, &after, &data) != Success || !data) return {};
    out = QByteArray(reinterpret_cast<const char*>(data), static_cast<qsizetype>(items * (format == 32 ? sizeof(long) : static_cast<unsigned long>(format / 8))));
    XFree(data);
    return out;
}

_cold static std::optional<xmouse::platform::WindowInfo> describe(Display* d, const Window w)
{
    xmouse::platform::WindowInfo info;
    XWindowAttributes attributes;
    XClassHint hint;
    Window child = 0;
    unsigned long items = 0;
    int x = 0;
    int y = 0;
    QByteArray title;
    QByteArray pid;

    if (!XGetWindowAttributes(d, w, &attributes) || attributes.map_state != IsViewable) return std::nullopt;
    title = property_of(d, w, "_NET_WM_NAME", XInternAtom(d, "UTF8_STRING", False), items);
    if (title.isEmpty()) title = property_of(d, w, "WM_NAME", XA_STRING, items);
    pid = property_of(d, w, "_NET_WM_PID", XA_CARDINAL, items);
    (void)XTranslateCoordinates(d, w, DefaultRootWindow(d), 0, 0, &x, &y, &child);
    info.id = static_cast<quint64>(w);
    info.title = QString::fromUtf8(title);
    info.pid = items == 1 && pid.size() >= static_cast<qsizetype>(sizeof(long)) ? static_cast<qint64>(*reinterpret_cast<const long*>(pid.constData())) : 0;
    info.rect = QRect(x, y, attributes.width, attributes.height);
    if (XGetClassHint(d, w, &hint)) {
        info.app = QString::fromLocal8Bit(hint.res_class ? hint.res_class : "");
        if (hint.res_name) XFree(hint.res_name);
        if (hint.res_class) XFree(hint.res_class);
    }
    info.document = xmouse::platform::document_of_process(info.pid);
    return info;
}

//----------------------------------------------------------------//
/* CLASS */

xmouse::platform::X11Windows::X11Windows(void)
{
    this->_native = XOpenDisplay(nullptr);
}

xmouse::platform::X11Windows::~X11Windows()
{
    if (this->_native) XCloseDisplay(display_of(this->_native));
}

QList<xmouse::platform::WindowInfo> xmouse::platform::X11Windows::list(void)
{
    Display* d = display_of(this->_native);
    QList<WindowInfo> out;
    unsigned long items = 0;
    QByteArray clients;

    if (!d) return out;
    clients = property_of(d, DefaultRootWindow(d), "_NET_CLIENT_LIST", XA_WINDOW, items);
    for (unsigned long i = 0; i < items && static_cast<qsizetype>((i + 1) * sizeof(long)) <= clients.size(); ++i) {
        const Window w = static_cast<Window>(reinterpret_cast<const long*>(clients.constData())[i]);
        const std::optional<WindowInfo> info = describe(d, w);
        if (info && !info->title.isEmpty()) out.append(*info);
    }
    return out;
}

std::optional<xmouse::platform::WindowInfo> xmouse::platform::X11Windows::info(const quint64 id)
{
    Display* d = display_of(this->_native);

    if (!d) return std::nullopt;
    return describe(d, static_cast<Window>(id));
}

QImage xmouse::platform::X11Windows::capture(const quint64 id)
{
    Display* d = display_of(this->_native);
    XWindowAttributes attributes;
    XImage* image = nullptr;
    QImage out;

    if (!d || !XGetWindowAttributes(d, static_cast<Window>(id), &attributes) || attributes.map_state != IsViewable) return {};
    if (attributes.width <= 0 || attributes.height <= 0 || attributes.width > 16384 || attributes.height > 16384) return {};
    image = XGetImage(d, static_cast<Window>(id), 0, 0, static_cast<unsigned int>(attributes.width), static_cast<unsigned int>(attributes.height), AllPlanes, ZPixmap);
    if (!image) return {};
    if (image->bits_per_pixel == 32) out = QImage(reinterpret_cast<const uchar*>(image->data), image->width, image->height, image->bytes_per_line, QImage::Format_RGB32).copy();
    XDestroyImage(image);
    return out;
}

void xmouse::platform::X11Windows::activate(const quint64 id)
{
    Display* d = display_of(this->_native);
    XEvent event;

    if (!d) return;
    std::memset(&event, 0, sizeof(event));
    event.xclient.type = ClientMessage;
    event.xclient.window = static_cast<Window>(id);
    event.xclient.message_type = XInternAtom(d, "_NET_ACTIVE_WINDOW", False);
    event.xclient.format = 32;
    event.xclient.data.l[0] = 2; // from a pager / tool: honoured
    event.xclient.data.l[1] = CurrentTime;
    XSendEvent(d, DefaultRootWindow(d), False, SubstructureRedirectMask | SubstructureNotifyMask, &event);
    XMapRaised(d, static_cast<Window>(id));
    XFlush(d);
}
