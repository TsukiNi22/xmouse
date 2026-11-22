/**************************************************************\

 ██╗  ██╗ █████╗ ██████╗ ████████╗ █████╗ ███╗   ██╗██╗ █████╗
 ╚██╗██╔╝██╔══██╗██╔══██╗╚══██╔══╝██╔══██╗████╗  ██║██║██╔══██╗
  ╚███╔╝ ███████║██████╔╝   ██║   ███████║██╔██╗ ██║██║███████║
  ██╔██╗ ██╔══██║██╔══██╗   ██║   ██╔══██║██║╚██╗██║██║██╔══██║
 ██╔╝ ██╗██║  ██║██║  ██║   ██║   ██║  ██║██║ ╚████║██║██║  ██║
 ╚═╝  ╚═╝╚═╝  ╚═╝╚═╝  ╚═╝   ╚═╝   ╚═╝  ╚═╝╚═╝  ╚═══╝╚═╝╚═╝  ╚═╝

Edition:
##  @date 05/11/2026 by @author Tsukini

File Name:
##  @file SendInputInjector.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#include <windows.h> // xstyle: ignore CPP-INCLUDE-ORDER (Windows headers before libutils: _unused clash)
#define _Attribute
#include <utils/utils.hpp>
#include "xmouse/platform/windows/SendInputInjector.hpp"
#include "xmouse/platform/Keymap.hpp"
#include <optional>

#ifndef MAPVK_VSC_TO_VK_EX
    #define MAPVK_VSC_TO_VK_EX 3 // a scancode (0xE0 prefix for the extended keys) to its virtual key
#endif
#ifndef MAPVK_VK_TO_VSC_EX
    #define MAPVK_VK_TO_VSC_EX 4 // a virtual key to its scancode (0xE0 prefix for the extended keys)
#endif
#define TO_UNICODE_KEEP_STATE 0x4 // ToUnicodeEx leaves the dead key state of the layout as it is (Windows 10 1607+)

bool xmouse::platform::SendInputInjector::start(const QSize& screen)
{
    this->_screen = screen;
    return true;
}

void xmouse::platform::SendInputInjector::mouseMove(const int x, const int y)
{
    INPUT input{};

    // Absolute coordinates are 0..65535 over the whole virtual desktop (every monitor, origin = its top left)
    input.type = INPUT_MOUSE;
    input.mi.dx = static_cast<LONG>((static_cast<long long>(x) * 65535) / std::max(1, this->_screen.width() - 1));
    input.mi.dy = static_cast<LONG>((static_cast<long long>(y) * 65535) / std::max(1, this->_screen.height() - 1));
    input.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK;
    SendInput(1, &input, sizeof(INPUT));
}

void xmouse::platform::SendInputInjector::mouseButton(const net::MouseButton button, const bool down)
{
    INPUT input{};

    input.type = INPUT_MOUSE;
    switch (button) {
        case net::MouseButton::Left:    input.mi.dwFlags = down ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP;     break;
        case net::MouseButton::Right:   input.mi.dwFlags = down ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP;   break;
        case net::MouseButton::Middle:  input.mi.dwFlags = down ? MOUSEEVENTF_MIDDLEDOWN : MOUSEEVENTF_MIDDLEUP; break;
        case net::MouseButton::Back:    input.mi.dwFlags = down ? MOUSEEVENTF_XDOWN : MOUSEEVENTF_XUP; input.mi.mouseData = XBUTTON1; break;
        case net::MouseButton::Forward: input.mi.dwFlags = down ? MOUSEEVENTF_XDOWN : MOUSEEVENTF_XUP; input.mi.mouseData = XBUTTON2; break;
    }
    if (down) this->_buttons.insert(static_cast<int>(button));
    else this->_buttons.remove(static_cast<int>(button));
    SendInput(1, &input, sizeof(INPUT));
}

void xmouse::platform::SendInputInjector::mouseWheel(const int dx, const int dy)
{
    INPUT inputs[2] = {};
    UINT count = 0;

    if (dy != 0) {
        inputs[count].type = INPUT_MOUSE;
        inputs[count].mi.dwFlags = MOUSEEVENTF_WHEEL;
        inputs[count++].mi.mouseData = static_cast<DWORD>(dy);
    }
    if (dx != 0) {
        inputs[count].type = INPUT_MOUSE;
        inputs[count].mi.dwFlags = MOUSEEVENTF_HWHEEL;
        inputs[count++].mi.mouseData = static_cast<DWORD>(-dx); // positive dx = left, MOUSEEVENTF_HWHEEL > 0 = right
    }
    if (count > 0) SendInput(count, inputs, sizeof(INPUT));
}

void xmouse::platform::SendInputInjector::key(const QString& code, const bool down)
{
    const std::optional<KeyInfo> info = key_info(code);
    INPUT input{};

    if (!info.has_value() || (info->scancode == 0 && info->virtualKey == 0)) return; // no such key on Windows (brightness, eject...)
    input.type = INPUT_KEYBOARD;

    // Scancodes: the layout of the host applies (like a physical keyboard)
    if (info->scancode != 0) {
        input.ki.wScan = info->scancode;
        input.ki.dwFlags = KEYEVENTF_SCANCODE | (info->extended ? KEYEVENTF_EXTENDEDKEY : 0) | (down ? 0 : KEYEVENTF_KEYUP);
    } else {
        input.ki.wVk = info->virtualKey;
        input.ki.dwFlags = down ? 0 : KEYEVENTF_KEYUP;
    }
    if (down) this->_keys.insert(code);
    else this->_keys.remove(code);
    SendInput(1, &input, sizeof(INPUT));
}

void xmouse::platform::SendInputInjector::text(const QString& text)
{
    // UTF-16 units sent as unicode key strokes (any character, whatever the layout)
    for (const QChar c: text) {
        INPUT inputs[2] = {};
        inputs[0].type = INPUT_KEYBOARD;
        inputs[0].ki.wScan = c.unicode();
        inputs[0].ki.dwFlags = KEYEVENTF_UNICODE;
        inputs[1] = inputs[0];
        inputs[1].ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
        SendInput(2, inputs, sizeof(INPUT));
    }
}

void xmouse::platform::SendInputInjector::releaseAll(void)
{
    const QSet<QString> keys = this->_keys;
    const QSet<int> buttons = this->_buttons;

    for (const QString& code: keys)
        this->key(code, false);
    for (const int button: buttons)
        this->mouseButton(static_cast<net::MouseButton>(button), false);
}

//----------------------------------------------------------------//
/* LAYOUT */

// The layout of the window in front (every thread of Windows has its own one)
static HKL foreground_layout(void)
{
    return GetKeyboardLayout(GetWindowThreadProcessId(GetForegroundWindow(), nullptr));
}

QString xmouse::platform::SendInputInjector::characterOf(const QString& code, const bool shift, const bool altgr) const
{
    const std::optional<KeyInfo> info = key_info(code);
    const HKL layout = foreground_layout();
    BYTE state[256] = {};
    wchar_t out[8] = {};
    UINT vk = 0;
    int count = 0;

    if (!info.has_value()) return "";
    vk = info->scancode != 0 ? MapVirtualKeyExW(info->scancode | (info->extended ? 0xE000u : 0u), MAPVK_VSC_TO_VK_EX, layout) : info->virtualKey;
    if (vk == 0) return "";
    if (shift) state[VK_SHIFT] = 0x80;
    if (altgr) {state[VK_CONTROL] = 0x80; state[VK_MENU] = 0x80;} // AltGr is Ctrl + Alt for a layout of Windows
    count = ToUnicodeEx(vk, info->scancode, state, out, 8, TO_UNICODE_KEEP_STATE, layout);
    if (count <= 0) return ""; // no character, or a dead key
    return QString::fromWCharArray(out, count);
}

QString xmouse::platform::SendInputInjector::codeOf(const QString& character) const
{
    const HKL layout = foreground_layout();
    SHORT found = 0;
    UINT scancode = 0;

    if (character.size() != 1) return "";
    found = VkKeyScanExW(static_cast<wchar_t>(character.toLower().at(0).unicode()), layout);
    if (found == -1) return "";
    scancode = MapVirtualKeyExW(static_cast<UINT>(found & 0xFF), MAPVK_VK_TO_VSC_EX, layout);
    if (scancode == 0) return "";
    return code_from_scancode(static_cast<unsigned short>(scancode & 0xFF), (scancode & 0xFF00) == 0xE000);
}

xmouse::platform::LockKeys xmouse::platform::SendInputInjector::lockKeys(void) const
{
    LockKeys locks;

    locks.caps = (GetKeyState(VK_CAPITAL) & 1) != 0;
    locks.num = (GetKeyState(VK_NUMLOCK) & 1) != 0;
    return locks;
}
