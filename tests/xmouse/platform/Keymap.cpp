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
##  @file Keymap.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#include "xmouse/platform/Keymap.hpp"
#include <gtest/gtest.h>

//----------------------------------------------------------------//

// Every key goes from the capture of a device to the injector of another one by its name: the keys of a full keyboard
// are known both ways (Linux evdev, Windows scancode), the function, browser, media and language ones included
TEST(Keymap, every_key_of_a_full_keyboard_is_known_both_ways)
{
    const char* const codes[] = {
        "KeyA", "KeyZ", "Digit0", "Digit9", "Minus", "Equal", "Backquote", "IntlBackslash", "Escape", "Backspace", "Tab", "Enter",
        "Space", "CapsLock", "Insert", "Delete", "Home", "End", "PageUp", "PageDown", "ArrowUp", "ArrowLeft", "PrintScreen",
        "ScrollLock", "Pause", "ContextMenu", "ShiftLeft", "ControlRight", "AltRight", "MetaLeft", "F1", "F12", "F13", "F24",
        "NumLock", "Numpad0", "NumpadEnter", "NumpadDecimal", "AudioVolumeMute", "MediaPlayPause", "MediaTrackNext", "MediaPlay",
        "MediaPause", "BrowserBack", "BrowserForward", "BrowserRefresh", "BrowserSearch", "BrowserHome", "LaunchMail", "LaunchApp2",
        "Power", "Sleep", "BrightnessUp", "Help", "Undo", "Copy", "Paste", "Convert", "Lang1",
    };

    for (const char* code: codes) {
        const std::optional<xmouse::platform::KeyInfo> info = xmouse::platform::key_info(code);
        ASSERT_TRUE(info.has_value()) << code;
        EXPECT_EQ(xmouse::platform::code_from_evdev(info->evdev), QString::fromLatin1(code)) << code; // one name per key
    }
}

// Windows: the reload, back and media keys come with their E0 scancode, F13 - F24 with theirs
TEST(Keymap, windows_scancodes_of_the_extra_keys)
{
    EXPECT_EQ(xmouse::platform::code_from_scancode(0x67, true), "BrowserRefresh");
    EXPECT_EQ(xmouse::platform::code_from_scancode(0x6A, true), "BrowserBack");
    EXPECT_EQ(xmouse::platform::code_from_scancode(0x22, true), "MediaPlayPause");
    EXPECT_EQ(xmouse::platform::code_from_scancode(0x76, false), "F24");
    EXPECT_EQ(xmouse::platform::code_from_scancode(0x3F, false), "F5");
    EXPECT_EQ(xmouse::platform::key_info("Lang1")->virtualKey, 0x15); // no scancode: its virtual key (VK_HANGUL)
}
