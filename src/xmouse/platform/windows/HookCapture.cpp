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
##  @file HookCapture.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Attribute
#include <utils/utils.hpp>
#include "xmouse/platform/windows/HookCapture.hpp"
#include "xmouse/platform/Keymap.hpp"
#include <QMetaObject>
#include <windows.h>

//----------------------------------------------------------------//
/* DATA */

static xmouse::platform::HookCapture* g_capture = nullptr; // the hooks have no user data: one capture per process

//----------------------------------------------------------------//
/* CLASS */

xmouse::platform::HookCapture::~HookCapture()
{
    this->stop();
}

bool xmouse::platform::HookCapture::start(void)
{
    HMODULE module = GetModuleHandleW(nullptr);

    if (g_capture && g_capture != this) {this->_error = "another capture is running"; return false;}
    g_capture = this;
    this->_mouse = SetWindowsHookExW(WH_MOUSE_LL, &HookCapture::mouseProc_, module, 0);
    this->_keyboard = SetWindowsHookExW(WH_KEYBOARD_LL, &HookCapture::keyboardProc_, module, 0);
    if (!this->_mouse || !this->_keyboard) {
        this->_error = QString("SetWindowsHookEx failed (%1)").arg(GetLastError());
        this->stop();
        return false;
    }
    GetCursorPos(&this->_last);
    this->_moveHook = SetWinEventHook(EVENT_SYSTEM_MOVESIZESTART, EVENT_SYSTEM_MOVESIZEEND, nullptr, &HookCapture::windowProc_, 0, 0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
    QMetaObject::invokeMethod(this, [this]() {emit this->ready();}, Qt::QueuedConnection);
    return true;
}

void xmouse::platform::HookCapture::stop(void)
{
    if (this->_mouse) UnhookWindowsHookEx(this->_mouse);
    if (this->_keyboard) UnhookWindowsHookEx(this->_keyboard);
    if (this->_moveHook) UnhookWinEvent(this->_moveHook);
    this->_moveHook = nullptr;
    this->_moving = nullptr;
    this->_mouse = nullptr;
    this->_keyboard = nullptr;
    this->_capturing = false;
    if (g_capture == this) g_capture = nullptr;
}

QPoint xmouse::platform::HookCapture::pointer(void) const
{
    POINT p = {0, 0};

    GetCursorPos(&p);
    return QPoint(p.x, p.y);
}

int xmouse::platform::HookCapture::heldButtons(void) const
{
    int bits = 0;

    if (GetAsyncKeyState(VK_LBUTTON) & 0x8000) bits |= 1 << static_cast<int>(net::MouseButton::Left);
    if (GetAsyncKeyState(VK_MBUTTON) & 0x8000) bits |= 1 << static_cast<int>(net::MouseButton::Middle);
    if (GetAsyncKeyState(VK_RBUTTON) & 0x8000) bits |= 1 << static_cast<int>(net::MouseButton::Right);
    return bits;
}

void CALLBACK xmouse::platform::HookCapture::windowProc_(HWINEVENTHOOK, DWORD event, HWND window, LONG object, LONG, DWORD, DWORD)
{
    if (!g_capture || object != OBJID_WINDOW) return;
    if (event == EVENT_SYSTEM_MOVESIZESTART) g_capture->_moving = window;
    else if (event == EVENT_SYSTEM_MOVESIZEEND) {g_capture->_moving = nullptr; g_capture->_sent = nullptr;}
}

void xmouse::platform::HookCapture::release(const QPoint& local)
{
    this->_capturing = false;
    SetCursorPos(local.x(), local.y());
    this->_last = {local.x(), local.y()};
}

LRESULT CALLBACK xmouse::platform::HookCapture::mouseProc_(int code, WPARAM wParam, LPARAM lParam)
{
    if (code == HC_ACTION && g_capture && g_capture->onMouse_(wParam, reinterpret_cast<const MSLLHOOKSTRUCT*>(lParam))) return 1;
    return CallNextHookEx(nullptr, code, wParam, lParam);
}

LRESULT CALLBACK xmouse::platform::HookCapture::keyboardProc_(int code, WPARAM wParam, LPARAM lParam)
{
    if (code == HC_ACTION && g_capture && g_capture->onKey_(wParam, reinterpret_cast<const KBDLLHOOKSTRUCT*>(lParam))) return 1;
    return CallNextHookEx(nullptr, code, wParam, lParam);
}

bool xmouse::platform::HookCapture::onMouse_(const WPARAM message, const MSLLHOOKSTRUCT* data)
{
    const POINT p = data->pt;
    const POINT last = this->_last;

    // Injected (the foreign cursors on this PC, xmouse itself): never captured
    if (data->flags & (LLMHF_INJECTED | LLMHF_LOWER_IL_INJECTED)) return false;

    // Idle: the openings (signals queued: a hook must answer at once)
    if (!this->_capturing) {
        if (message != WM_MOUSEMOVE) return false;
        this->_last = p;
        for (const core::Edge& e: this->_edges) {
            QPoint push;
            if (e.side == core::Side::Right && p.x >= e.line - 1 && p.x > last.x && p.y >= e.from && p.y <= e.to) push = QPoint(1, 0);
            else if (e.side == core::Side::Left && p.x <= e.line && p.x < last.x && p.y >= e.from && p.y <= e.to) push = QPoint(-1, 0);
            else if (e.side == core::Side::Bottom && p.y >= e.line - 1 && p.y > last.y && p.x >= e.from && p.x <= e.to) push = QPoint(0, 1);
            else if (e.side == core::Side::Top && p.y <= e.line && p.y < last.y && p.x >= e.from && p.x <= e.to) push = QPoint(0, -1);
            else continue;
            // A window dragged on the edge: given to the other device (switch on), or the usual border (off)
            if (this->_moving) {
                if (this->_windowMove && this->_sent != this->_moving) {
                    const quint64 window = reinterpret_cast<quint64>(this->_moving);
                    this->_sent = this->_moving;
                    QMetaObject::invokeMethod(this, [this, window, p, push]() {emit this->windowCrossed(window, QPoint(p.x, p.y), push);}, Qt::QueuedConnection);
                }
                return false;
            }
            if (GetAsyncKeyState(VK_LBUTTON) & 0x8000) return false; // a drag (a file, a selection): no crossing, the drop shelf of xmouse shows here
            this->_capturing = true;
            this->_frozen = p;
            QMetaObject::invokeMethod(this, [this, p, push]() {emit this->crossed(QPoint(p.x, p.y), push);}, Qt::QueuedConnection);
            return false; // this move lands on the edge pixel, the next ones are held
        }
        return false;
    }

    // Captured: everything becomes a signal, nothing reaches Windows
    switch (message) {
        case WM_MOUSEMOVE: {
            const int dx = p.x - this->_frozen.x;
            const int dy = p.y - this->_frozen.y;
            if (dx != 0 || dy != 0) QMetaObject::invokeMethod(this, [this, dx, dy]() {emit this->motion(dx, dy);}, Qt::QueuedConnection);
            break;
        }
        case WM_LBUTTONDOWN: case WM_LBUTTONUP:
            QMetaObject::invokeMethod(this, [this, message]() {emit this->button(net::MouseButton::Left, message == WM_LBUTTONDOWN);}, Qt::QueuedConnection);
            break;
        case WM_RBUTTONDOWN: case WM_RBUTTONUP:
            QMetaObject::invokeMethod(this, [this, message]() {emit this->button(net::MouseButton::Right, message == WM_RBUTTONDOWN);}, Qt::QueuedConnection);
            break;
        case WM_MBUTTONDOWN: case WM_MBUTTONUP:
            QMetaObject::invokeMethod(this, [this, message]() {emit this->button(net::MouseButton::Middle, message == WM_MBUTTONDOWN);}, Qt::QueuedConnection);
            break;
        case WM_XBUTTONDOWN: case WM_XBUTTONUP: {
            const net::MouseButton b = HIWORD(data->mouseData) == XBUTTON1 ? net::MouseButton::Back : net::MouseButton::Forward;
            QMetaObject::invokeMethod(this, [this, b, message]() {emit this->button(b, message == WM_XBUTTONDOWN);}, Qt::QueuedConnection);
            break;
        }
        case WM_MOUSEWHEEL: {
            const int delta = static_cast<short>(HIWORD(data->mouseData));
            QMetaObject::invokeMethod(this, [this, delta]() {emit this->wheel(0, delta);}, Qt::QueuedConnection);
            break;
        }
        case WM_MOUSEHWHEEL: {
            const int delta = static_cast<short>(HIWORD(data->mouseData));
            QMetaObject::invokeMethod(this, [this, delta]() {emit this->wheel(-delta, 0);}, Qt::QueuedConnection); // > 0 = right, xmouse: dx > 0 = left
            break;
        }
        default:
            break;
    }
    return true;
}

bool xmouse::platform::HookCapture::onKey_(const WPARAM message, const KBDLLHOOKSTRUCT* data)
{
    QString code;
    bool down = false;

    if (!this->_capturing || (data->flags & (LLKHF_INJECTED | LLKHF_LOWER_IL_INJECTED))) return false;
    code = code_from_scancode(static_cast<unsigned short>(data->scanCode), (data->flags & LLKHF_EXTENDED) != 0);
    down = message == WM_KEYDOWN || message == WM_SYSKEYDOWN;
    if (!code.isEmpty()) QMetaObject::invokeMethod(this, [this, code, down]() {emit this->key(code, down);}, Qt::QueuedConnection);
    return true;
}
