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
##  @file MacCapture.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Attribute
#include <utils/utils.hpp>
#include "xmouse/platform/macos/MacInjector.hpp"
#include "xmouse/platform/macos/MacCapture.hpp"
#include <ApplicationServices/ApplicationServices.h>
#include <QMetaObject>
#include <unistd.h>
#include <cmath>

//----------------------------------------------------------------//
/* TOOLS */

// The tap of the session: it runs on the run loop of the main thread (the Qt event loop on macOS)
_hot static CGEventRef tap_callback(CGEventTapProxy, CGEventType type, CGEventRef event, void* data)
{
    xmouse::platform::MacCapture* capture = static_cast<xmouse::platform::MacCapture*>(data);

    if (type == kCGEventTapDisabledByTimeout || type == kCGEventTapDisabledByUserInput) {capture->reenable_(); return event;}
    return capture->onEvent_(static_cast<unsigned int>(type), event) ? nullptr : event;
}

// Modifier keys arrive as flagsChanged: the flag of the key says if it went down
_hot static quint64 flag_of(const QString& code)
{
    if (code.startsWith("Shift")) return kCGEventFlagMaskShift;
    if (code.startsWith("Control")) return kCGEventFlagMaskControl;
    if (code.startsWith("Alt")) return kCGEventFlagMaskAlternate;
    if (code.startsWith("Meta")) return kCGEventFlagMaskCommand;
    if (code == "CapsLock") return kCGEventFlagMaskAlphaShift;
    return 0;
}

//----------------------------------------------------------------//
/* CLASS */

xmouse::platform::MacCapture::~MacCapture()
{
    this->stop();
}

bool xmouse::platform::MacCapture::start(void)
{
    const CGEventMask mask = CGEventMaskBit(kCGEventMouseMoved) | CGEventMaskBit(kCGEventLeftMouseDragged) | CGEventMaskBit(kCGEventRightMouseDragged)
        | CGEventMaskBit(kCGEventOtherMouseDragged) | CGEventMaskBit(kCGEventLeftMouseDown) | CGEventMaskBit(kCGEventLeftMouseUp)
        | CGEventMaskBit(kCGEventRightMouseDown) | CGEventMaskBit(kCGEventRightMouseUp) | CGEventMaskBit(kCGEventOtherMouseDown)
        | CGEventMaskBit(kCGEventOtherMouseUp) | CGEventMaskBit(kCGEventScrollWheel) | CGEventMaskBit(kCGEventKeyDown)
        | CGEventMaskBit(kCGEventKeyUp) | CGEventMaskBit(kCGEventFlagsChanged);
    CFMachPortRef tap = CGEventTapCreate(kCGSessionEventTap, kCGHeadInsertEventTap, kCGEventTapOptionDefault, mask, &tap_callback, this);
    CFRunLoopSourceRef source = nullptr;
    CGEventRef now = CGEventCreate(nullptr);

    if (!tap) {
        this->_error = "no event tap: allow xmouse in System Settings > Privacy & Security > Accessibility (and Input Monitoring)";
        if (now) CFRelease(now);
        return false;
    }
    source = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, tap, 0);
    CFRunLoopAddSource(CFRunLoopGetMain(), source, kCFRunLoopCommonModes);
    CGEventTapEnable(tap, true);
    this->_tap = tap;
    this->_source = source;
    if (now) {
        const CGPoint p = CGEventGetLocation(now);
        this->_last = QPoint(static_cast<int>(p.x), static_cast<int>(p.y));
        CFRelease(now);
    }
    QMetaObject::invokeMethod(this, [this]() {emit this->ready();}, Qt::QueuedConnection);
    return true;
}

void xmouse::platform::MacCapture::stop(void)
{
    if (this->_capturing) this->release(this->_last);
    if (this->_source) {
        CFRunLoopRemoveSource(CFRunLoopGetMain(), static_cast<CFRunLoopSourceRef>(this->_source), kCFRunLoopCommonModes);
        CFRelease(static_cast<CFRunLoopSourceRef>(this->_source));
    }
    if (this->_tap) {
        CGEventTapEnable(static_cast<CFMachPortRef>(this->_tap), false);
        CFRelease(static_cast<CFMachPortRef>(this->_tap));
    }
    this->_source = nullptr;
    this->_tap = nullptr;
}

void xmouse::platform::MacCapture::reenable_(void)
{
    if (this->_tap) CGEventTapEnable(static_cast<CFMachPortRef>(this->_tap), true);
}

QPoint xmouse::platform::MacCapture::pointer(void) const
{
    CGEventRef now = CGEventCreate(nullptr);
    CGPoint p = CGPointMake(this->_last.x(), this->_last.y());

    if (now) {p = CGEventGetLocation(now); CFRelease(now);}
    return QPoint(static_cast<int>(p.x), static_cast<int>(p.y));
}

int xmouse::platform::MacCapture::heldButtons(void) const
{
    int bits = 0;

    if (CGEventSourceButtonState(kCGEventSourceStateCombinedSessionState, kCGMouseButtonLeft)) bits |= 1 << static_cast<int>(net::MouseButton::Left);
    if (CGEventSourceButtonState(kCGEventSourceStateCombinedSessionState, kCGMouseButtonCenter)) bits |= 1 << static_cast<int>(net::MouseButton::Middle);
    if (CGEventSourceButtonState(kCGEventSourceStateCombinedSessionState, kCGMouseButtonRight)) bits |= 1 << static_cast<int>(net::MouseButton::Right);
    return bits;
}

void xmouse::platform::MacCapture::release(const QPoint& local)
{
    this->_capturing = false;
    CGWarpMouseCursorPosition(CGPointMake(local.x(), local.y()));
    CGAssociateMouseAndMouseCursorPosition(true);
    CGDisplayShowCursor(kCGDirectMainDisplay);
    this->_last = local;
}

bool xmouse::platform::MacCapture::onEvent_(const unsigned int raw, void* data)
{
    const CGEventType type = static_cast<CGEventType>(raw);
    CGEventRef event = static_cast<CGEventRef>(data);
    const bool injected = CGEventGetIntegerValueField(event, kCGEventSourceUnixProcessID) == static_cast<int64_t>(getpid()); // posted by xmouse (a foreign cursor)
    const bool move = type == kCGEventMouseMoved || type == kCGEventLeftMouseDragged || type == kCGEventRightMouseDragged || type == kCGEventOtherMouseDragged;

    // Idle: the openings (the events posted by xmouse for the foreign cursors never cross)
    if (!this->_capturing) {
        CGPoint at;
        QPoint p;
        QPoint last;

        if (!move || injected) return false;
        at = CGEventGetLocation(event);
        p = QPoint(static_cast<int>(at.x), static_cast<int>(at.y));
        last = this->_last;
        this->_last = p;
        for (const core::Edge& e: this->_edges) {
            QPoint push;
            if (e.side == core::Side::Right && p.x() >= e.line - 1 && p.x() >= last.x() && p.y() >= e.from && p.y() <= e.to) push = QPoint(1, 0);
            else if (e.side == core::Side::Left && p.x() <= e.line && p.x() <= last.x() && p.y() >= e.from && p.y() <= e.to) push = QPoint(-1, 0);
            else if (e.side == core::Side::Bottom && p.y() >= e.line - 1 && p.y() >= last.y() && p.x() >= e.from && p.x() <= e.to) push = QPoint(0, 1);
            else if (e.side == core::Side::Top && p.y() <= e.line && p.y() <= last.y() && p.x() >= e.from && p.x() <= e.to) push = QPoint(0, -1);
            else continue;
            if (type != kCGEventMouseMoved) return false; // a drag (a window, a selection): the usual border
            this->_capturing = true;
            CGAssociateMouseAndMouseCursorPosition(false); // the pointer stays, the deltas still come
            CGDisplayHideCursor(kCGDirectMainDisplay);
            emit this->crossed(p, push);
            return false;
        }
        return false;
    }

    // Captured: everything becomes a signal, nothing reaches the apps of the Mac
    switch (type) {
        case kCGEventMouseMoved: case kCGEventLeftMouseDragged: case kCGEventRightMouseDragged: case kCGEventOtherMouseDragged: {
            const double dx = static_cast<double>(CGEventGetIntegerValueField(event, kCGMouseEventDeltaX));
            const double dy = static_cast<double>(CGEventGetIntegerValueField(event, kCGMouseEventDeltaY));
            if (dx != 0 || dy != 0) emit this->motion(dx, dy);
            break;
        }
        case kCGEventLeftMouseDown: case kCGEventLeftMouseUp:
            emit this->button(net::MouseButton::Left, type == kCGEventLeftMouseDown);
            break;
        case kCGEventRightMouseDown: case kCGEventRightMouseUp:
            emit this->button(net::MouseButton::Right, type == kCGEventRightMouseDown);
            break;
        case kCGEventOtherMouseDown: case kCGEventOtherMouseUp: {
            const qint64 number = CGEventGetIntegerValueField(event, kCGMouseEventButtonNumber);
            const net::MouseButton b = number == 3 ? net::MouseButton::Back : (number == 4 ? net::MouseButton::Forward : net::MouseButton::Middle);
            emit this->button(b, type == kCGEventOtherMouseDown);
            break;
        }
        case kCGEventScrollWheel: {
            const double dy = static_cast<double>(CGEventGetIntegerValueField(event, kCGScrollWheelEventPointDeltaAxis1));
            const double dx = static_cast<double>(CGEventGetIntegerValueField(event, kCGScrollWheelEventPointDeltaAxis2));
            const int wy = static_cast<int>(std::lround(dy * 3.0)); // pixels -> 1/120 of a notch (40 px a notch)
            const int wx = static_cast<int>(std::lround(dx * 3.0));
            if (wx != 0 || wy != 0) emit this->wheel(wx, wy);
            break;
        }
        case kCGEventKeyDown: case kCGEventKeyUp: {
            const QString code = code_from_mac(static_cast<int>(CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode)));
            if (!code.isEmpty() && CGEventGetIntegerValueField(event, kCGKeyboardEventAutorepeat) == 0) emit this->key(code, type == kCGEventKeyDown);
            break;
        }
        case kCGEventFlagsChanged: {
            const QString code = code_from_mac(static_cast<int>(CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode)));
            const quint64 flags = static_cast<quint64>(CGEventGetFlags(event));
            const quint64 flag = flag_of(code);
            if (!code.isEmpty() && flag != 0) emit this->key(code, (flags & flag) != 0);
            this->_flags = flags;
            break;
        }
        default:
            return false;
    }
    return true;
}
