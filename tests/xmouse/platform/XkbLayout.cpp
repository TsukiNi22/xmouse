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
##  @file XkbLayout.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#include "xmouse/platform/linux/XkbLayout.hpp"
#include <linux/input-event-codes.h>
#include <gtest/gtest.h>

//----------------------------------------------------------------//
/* CHARACTER OF A KEY */

// The inverse of find: what a key types on a layout (a phone screen gets the characters, not the keys)
TEST(XkbLayout, character_of_a_key_on_azerty)
{
    xmouse::platform::XkbLayout layout;

    if (!layout.load("fr", "")) GTEST_SKIP() << "libxkbcommon or the fr layout is missing";
    EXPECT_EQ(layout.character(KEY_Q, false, false).value_or(0), static_cast<uint>('a')); // the key A of a qwerty
    EXPECT_EQ(layout.character(KEY_Q, true, false).value_or(0), static_cast<uint>('A'));
    EXPECT_EQ(layout.character(KEY_1, false, false).value_or(0), static_cast<uint>('&'));
    EXPECT_EQ(layout.character(KEY_1, true, false).value_or(0), static_cast<uint>('1')); // also on the keypad: every key is known
    EXPECT_EQ(layout.character(KEY_2, false, false).value_or(0), 0xE9u); // é
    EXPECT_EQ(layout.character(KEY_E, false, true).value_or(0), 0x20ACu); // AltGr + e = euro
    EXPECT_FALSE(layout.character(KEY_F5, false, false).has_value());
}

TEST(XkbLayout, character_of_a_key_on_qwerty)
{
    xmouse::platform::XkbLayout layout;

    if (!layout.load("us", "")) GTEST_SKIP() << "libxkbcommon or the us layout is missing";
    EXPECT_EQ(layout.character(KEY_Q, false, false).value_or(0), static_cast<uint>('q'));
    EXPECT_EQ(layout.character(KEY_1, true, false).value_or(0), static_cast<uint>('!'));
    EXPECT_EQ(layout.character(KEY_SPACE, false, false).value_or(0), static_cast<uint>(' '));
}
