/*
 * The keys of the computers (KeyboardEvent.code, the names on the wire) as the key codes of Android, for the real key
 * events sent to a focused text field; a letter goes by the character it types on the layout of the computer
*/
package io.github.tsukini22.xmouse;

import android.view.KeyEvent;

import java.util.HashMap;
import java.util.Map;

final class KeyCodes {
    private static final Map<String, Integer> CODES = new HashMap<>();

    static {
        for (char c = 'A'; c <= 'Z'; c++) CODES.put("Key" + c, KeyEvent.KEYCODE_A + (c - 'A'));
        for (char c = '0'; c <= '9'; c++) {
            CODES.put("Digit" + c, KeyEvent.KEYCODE_0 + (c - '0'));
            CODES.put("Numpad" + c, KeyEvent.KEYCODE_NUMPAD_0 + (c - '0'));
        }
        for (int f = 1; f <= 12; f++) CODES.put("F" + f, KeyEvent.KEYCODE_F1 + f - 1);
        /* editing */
        CODES.put("Escape", KeyEvent.KEYCODE_ESCAPE);
        CODES.put("Backspace", KeyEvent.KEYCODE_DEL);
        CODES.put("Delete", KeyEvent.KEYCODE_FORWARD_DEL);
        CODES.put("Tab", KeyEvent.KEYCODE_TAB);
        CODES.put("Enter", KeyEvent.KEYCODE_ENTER);
        CODES.put("Space", KeyEvent.KEYCODE_SPACE);
        CODES.put("Insert", KeyEvent.KEYCODE_INSERT);
        CODES.put("Home", KeyEvent.KEYCODE_MOVE_HOME);
        CODES.put("End", KeyEvent.KEYCODE_MOVE_END);
        CODES.put("PageUp", KeyEvent.KEYCODE_PAGE_UP);
        CODES.put("PageDown", KeyEvent.KEYCODE_PAGE_DOWN);
        CODES.put("ArrowUp", KeyEvent.KEYCODE_DPAD_UP);
        CODES.put("ArrowDown", KeyEvent.KEYCODE_DPAD_DOWN);
        CODES.put("ArrowLeft", KeyEvent.KEYCODE_DPAD_LEFT);
        CODES.put("ArrowRight", KeyEvent.KEYCODE_DPAD_RIGHT);
        CODES.put("CapsLock", KeyEvent.KEYCODE_CAPS_LOCK);
        CODES.put("ScrollLock", KeyEvent.KEYCODE_SCROLL_LOCK);
        CODES.put("NumLock", KeyEvent.KEYCODE_NUM_LOCK);
        CODES.put("PrintScreen", KeyEvent.KEYCODE_SYSRQ);
        CODES.put("Pause", KeyEvent.KEYCODE_BREAK);
        CODES.put("ContextMenu", KeyEvent.KEYCODE_MENU);
        /* modifiers (Meta never: the keys of the whole phone) */
        CODES.put("ShiftLeft", KeyEvent.KEYCODE_SHIFT_LEFT);
        CODES.put("ShiftRight", KeyEvent.KEYCODE_SHIFT_RIGHT);
        CODES.put("ControlLeft", KeyEvent.KEYCODE_CTRL_LEFT);
        CODES.put("ControlRight", KeyEvent.KEYCODE_CTRL_RIGHT);
        CODES.put("AltLeft", KeyEvent.KEYCODE_ALT_LEFT);
        CODES.put("AltRight", KeyEvent.KEYCODE_ALT_RIGHT);
        /* signs */
        CODES.put("Minus", KeyEvent.KEYCODE_MINUS);
        CODES.put("Equal", KeyEvent.KEYCODE_EQUALS);
        CODES.put("BracketLeft", KeyEvent.KEYCODE_LEFT_BRACKET);
        CODES.put("BracketRight", KeyEvent.KEYCODE_RIGHT_BRACKET);
        CODES.put("Backslash", KeyEvent.KEYCODE_BACKSLASH);
        CODES.put("Semicolon", KeyEvent.KEYCODE_SEMICOLON);
        CODES.put("Quote", KeyEvent.KEYCODE_APOSTROPHE);
        CODES.put("Backquote", KeyEvent.KEYCODE_GRAVE);
        CODES.put("Comma", KeyEvent.KEYCODE_COMMA);
        CODES.put("Period", KeyEvent.KEYCODE_PERIOD);
        CODES.put("Slash", KeyEvent.KEYCODE_SLASH);
        CODES.put("IntlRo", KeyEvent.KEYCODE_RO);
        CODES.put("IntlYen", KeyEvent.KEYCODE_YEN);
        /* numpad */
        CODES.put("NumpadDivide", KeyEvent.KEYCODE_NUMPAD_DIVIDE);
        CODES.put("NumpadMultiply", KeyEvent.KEYCODE_NUMPAD_MULTIPLY);
        CODES.put("NumpadSubtract", KeyEvent.KEYCODE_NUMPAD_SUBTRACT);
        CODES.put("NumpadAdd", KeyEvent.KEYCODE_NUMPAD_ADD);
        CODES.put("NumpadDecimal", KeyEvent.KEYCODE_NUMPAD_DOT);
        CODES.put("NumpadComma", KeyEvent.KEYCODE_NUMPAD_COMMA);
        CODES.put("NumpadEnter", KeyEvent.KEYCODE_NUMPAD_ENTER);
        CODES.put("NumpadEqual", KeyEvent.KEYCODE_NUMPAD_EQUALS);
        CODES.put("NumpadParenLeft", KeyEvent.KEYCODE_NUMPAD_LEFT_PAREN);
        CODES.put("NumpadParenRight", KeyEvent.KEYCODE_NUMPAD_RIGHT_PAREN);
        /* browser, edit and languages */
        CODES.put("BrowserRefresh", KeyEvent.KEYCODE_REFRESH);
        CODES.put("BrowserForward", KeyEvent.KEYCODE_FORWARD);
        CODES.put("BrowserFavorites", KeyEvent.KEYCODE_BOOKMARK);
        CODES.put("Help", KeyEvent.KEYCODE_HELP);
        CODES.put("Copy", KeyEvent.KEYCODE_COPY);
        CODES.put("Paste", KeyEvent.KEYCODE_PASTE);
        CODES.put("Cut", KeyEvent.KEYCODE_CUT);
        CODES.put("Convert", KeyEvent.KEYCODE_HENKAN);
        CODES.put("NonConvert", KeyEvent.KEYCODE_MUHENKAN);
        CODES.put("KanaMode", KeyEvent.KEYCODE_KATAKANA_HIRAGANA);
        CODES.put("Lang1", KeyEvent.KEYCODE_KANA);
        CODES.put("Lang2", KeyEvent.KEYCODE_EISU);
    }

    private KeyCodes() {}

    // The key code of Android for a key (KEYCODE_UNKNOWN: none); a letter by its character (azerty Ctrl + A: KeyQ, "a")
    static int of(String code, String character) {
        if (character.length() == 1) {
            char c = Character.toLowerCase(character.charAt(0));
            if (c >= 'a' && c <= 'z') return KeyEvent.KEYCODE_A + (c - 'a');
        }
        Integer key = CODES.get(code);
        return key == null ? KeyEvent.KEYCODE_UNKNOWN : key;
    }
}
