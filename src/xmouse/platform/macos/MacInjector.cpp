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
##  @file MacInjector.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Attribute
#include <utils/utils.hpp>
#include "xmouse/platform/macos/MacInjector.hpp"
#include <ApplicationServices/ApplicationServices.h>
#include <Carbon/Carbon.h>
#include <QGuiApplication>
#include <QDateTime>
#include <QScreen>
#include <cmath>

//----------------------------------------------------------------//
/* DEFINE */

#define DOUBLE_CLICK_MS 400 // a second click within this delay and 4 px counts as a double click
#define WHEEL_PIXELS 40.0 // pixels of one notch (1/120 units -> pixel scroll)

//----------------------------------------------------------------//
/* TOOLS */

_hot static CGMouseButton cg_button(const xmouse::net::MouseButton button)
{
    switch (button) {
        case xmouse::net::MouseButton::Left:  return kCGMouseButtonLeft;
        case xmouse::net::MouseButton::Right: return kCGMouseButtonRight;
        default:                              return kCGMouseButtonCenter;
    }
}

_hot static void post(CGEventRef event)
{
    if (!event) return;
    CGEventPost(kCGHIDEventTap, event);
    CFRelease(event);
}

//----------------------------------------------------------------//
/* CLASS */

bool xmouse::platform::MacInjector::start(const QSize& screen)
{
    const void* keys[] = {kAXTrustedCheckOptionPrompt};
    const void* values[] = {kCFBooleanTrue};
    CFDictionaryRef options = CFDictionaryCreate(kCFAllocatorDefault, keys, values, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    const bool trusted = AXIsProcessTrustedWithOptions(options); // asks the permission once (System Settings)

    CFRelease(options);
    (void)screen;
    if (!trusted) {this->_error = "allow xmouse in System Settings > Privacy & Security > Accessibility, then restart it"; return false;}
    if (QGuiApplication::primaryScreen()) this->_origin = QGuiApplication::primaryScreen()->virtualGeometry().topLeft();
    return true;
}

quint64 xmouse::platform::MacInjector::flags_(void) const
{
    quint64 flags = 0;

    if (this->_keys.contains("ShiftLeft") || this->_keys.contains("ShiftRight")) flags |= kCGEventFlagMaskShift;
    if (this->_keys.contains("ControlLeft") || this->_keys.contains("ControlRight")) flags |= kCGEventFlagMaskControl;
    if (this->_keys.contains("AltLeft") || this->_keys.contains("AltRight")) flags |= kCGEventFlagMaskAlternate;
    if (this->_keys.contains("MetaLeft") || this->_keys.contains("MetaRight")) flags |= kCGEventFlagMaskCommand;
    return flags;
}

void xmouse::platform::MacInjector::mouseMove(const int x, const int y)
{
    CGEventType type = kCGEventMouseMoved;
    CGMouseButton button = kCGMouseButtonLeft;
    CGEventRef event = nullptr;

    this->_at = QPoint(x, y) + this->_origin;
    if (this->_buttons & (1 << static_cast<int>(net::MouseButton::Left))) type = kCGEventLeftMouseDragged;
    else if (this->_buttons & (1 << static_cast<int>(net::MouseButton::Right))) {type = kCGEventRightMouseDragged; button = kCGMouseButtonRight;}
    else if (this->_buttons != 0) {type = kCGEventOtherMouseDragged; button = kCGMouseButtonCenter;}
    event = CGEventCreateMouseEvent(nullptr, type, CGPointMake(this->_at.x(), this->_at.y()), button);
    if (event) CGEventSetFlags(event, static_cast<CGEventFlags>(this->flags_()));
    post(event);
}

void xmouse::platform::MacInjector::postButton_(const net::MouseButton button, const bool down)
{
    const CGMouseButton cg = cg_button(button);
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    CGEventType type = kCGEventOtherMouseDown;
    CGEventRef event = nullptr;

    if (cg == kCGMouseButtonLeft) type = down ? kCGEventLeftMouseDown : kCGEventLeftMouseUp;
    else if (cg == kCGMouseButtonRight) type = down ? kCGEventRightMouseDown : kCGEventRightMouseUp;
    else type = down ? kCGEventOtherMouseDown : kCGEventOtherMouseUp;

    // Click count: the apps see double / triple clicks only through it
    if (down) {
        this->_clicks = static_cast<int>(button) == this->_lastButton && now - this->_lastClick < DOUBLE_CLICK_MS ? this->_clicks + 1 : 1;
        this->_lastButton = static_cast<int>(button);
        this->_lastClick = now;
    }
    event = CGEventCreateMouseEvent(nullptr, type, CGPointMake(this->_at.x(), this->_at.y()), cg);
    if (!event) return;
    CGEventSetIntegerValueField(event, kCGMouseEventClickState, std::max(1, this->_clicks));
    if (button == net::MouseButton::Back || button == net::MouseButton::Forward) CGEventSetIntegerValueField(event, kCGMouseEventButtonNumber, button == net::MouseButton::Back ? 3 : 4);
    CGEventSetFlags(event, static_cast<CGEventFlags>(this->flags_()));
    post(event);
}

void xmouse::platform::MacInjector::mouseButton(const net::MouseButton button, const bool down)
{
    const int bit = 1 << static_cast<int>(button);

    if (down) this->_buttons |= bit;
    else this->_buttons &= ~bit;
    this->postButton_(button, down);
}

void xmouse::platform::MacInjector::mouseWheel(const int dx, const int dy)
{
    int x = 0;
    int y = 0;

    this->_wheelX += dx * WHEEL_PIXELS / 120.0;
    this->_wheelY += dy * WHEEL_PIXELS / 120.0;
    x = static_cast<int>(this->_wheelX);
    y = static_cast<int>(this->_wheelY);
    this->_wheelX -= x;
    this->_wheelY -= y;
    if (x == 0 && y == 0) return;
    post(CGEventCreateScrollWheelEvent(nullptr, kCGScrollEventUnitPixel, 2, y, x));
}

void xmouse::platform::MacInjector::key(const QString& code, const bool down)
{
    const int keycode = mac_keycode(code);
    CGEventRef event = nullptr;

    if (keycode < 0) return;
    if (down) this->_keys.insert(code);
    else this->_keys.remove(code);
    event = CGEventCreateKeyboardEvent(nullptr, static_cast<CGKeyCode>(keycode), down);
    if (event) CGEventSetFlags(event, static_cast<CGEventFlags>(this->flags_()));
    post(event);
}

void xmouse::platform::MacInjector::text(const QString& text)
{
    // The unicode string of a key event: any character, whatever the layout of the Mac (20 UTF-16 units per event)
    for (qsizetype i = 0; i < text.size(); i += 20) {
        const QString part = text.mid(i, 20);
        CGEventRef down = CGEventCreateKeyboardEvent(nullptr, 0, true);
        CGEventRef up = CGEventCreateKeyboardEvent(nullptr, 0, false);
        if (down) CGEventKeyboardSetUnicodeString(down, static_cast<UniCharCount>(part.size()), reinterpret_cast<const UniChar*>(part.utf16()));
        if (up) CGEventKeyboardSetUnicodeString(up, static_cast<UniCharCount>(part.size()), reinterpret_cast<const UniChar*>(part.utf16()));
        post(down);
        post(up);
    }
}

void xmouse::platform::MacInjector::releaseAll(void)
{
    const QSet<QString> keys = this->_keys;

    for (int b = 0; b <= static_cast<int>(net::MouseButton::Forward); ++b)
        if (this->_buttons & (1 << b)) this->mouseButton(static_cast<net::MouseButton>(b), false);
    for (const QString& code: keys)
        this->key(code, false);
}

//----------------------------------------------------------------//
/* LAYOUT */

QString xmouse::platform::MacInjector::characterOf(const QString& code, const bool shift, const bool altgr) const
{
    const int keycode = mac_keycode(code);
    TISInputSourceRef source = nullptr;
    CFDataRef data = nullptr;
    UInt32 dead = 0;
    UniChar out[8] = {};
    UniCharCount length = 0;
    QString text;

    if (keycode < 0) return "";
    source = TISCopyCurrentKeyboardLayoutInputSource();
    if (source) data = static_cast<CFDataRef>(TISGetInputSourceProperty(source, kTISPropertyUnicodeKeyLayoutData));
    // Shift / Option in the high byte of the Carbon modifiers; the dead keys give no character
    if (data && UCKeyTranslate(reinterpret_cast<const UCKeyboardLayout*>(CFDataGetBytePtr(data)), static_cast<UInt16>(keycode), kUCKeyActionDown,
            ((shift ? shiftKey : 0) | (altgr ? optionKey : 0)) >> 8, LMGetKbdType(), kUCKeyTranslateNoDeadKeysMask, &dead, 8, &length, out) == noErr && length > 0)
        text = QString::fromUtf16(reinterpret_cast<const char16_t*>(out), static_cast<qsizetype>(length));
    if (source) CFRelease(source);
    return text;
}

QString xmouse::platform::MacInjector::codeOf(const QString& character) const
{
    const QString wanted = character.toLower();
    QString code;

    if (character.size() != 1) return "";
    for (int keycode = 0; keycode < 0x80; ++keycode) {
        code = code_from_mac(keycode);
        if (!code.isEmpty() && this->characterOf(code, false, false) == wanted) return code;
    }
    return "";
}

xmouse::platform::LockKeys xmouse::platform::MacInjector::lockKeys(void) const
{
    LockKeys locks;

    locks.caps = (CGEventSourceFlagsState(kCGEventSourceStateHIDSystemState) & kCGEventFlagMaskAlphaShift) != 0;
    return locks;
}
