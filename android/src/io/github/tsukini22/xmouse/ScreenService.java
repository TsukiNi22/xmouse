/*
 * The screen of the phone in the layout, with touches (Accessibility service): the cursors of the members drawn over
 * the apps (ScreenLink) and their clicks, drags, scrolls, keys and text turned into touches, key events and actions.
 * Enabled by the person in the Accessibility settings, off at any time; it replaces the phantom OverlayService.
 * A focused text field (Android 13+) gets the real key events through the input method of the service: every key and
 * shortcut of the app works there (Ctrl + arrows, Ctrl + Z, Ctrl + R, F5...); elsewhere the keys become actions.
*/
package io.github.tsukini22.xmouse;

import android.accessibilityservice.AccessibilityService;
import android.accessibilityservice.GestureDescription;
import android.content.Intent;
import android.content.res.Configuration;
import android.graphics.Path;
import android.graphics.Rect;
import android.media.AudioManager;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;
import android.text.InputType;
import android.util.DisplayMetrics;
import android.view.InputDevice;
import android.view.KeyCharacterMap;
import android.view.KeyEvent;
import android.view.WindowManager;
import android.view.accessibility.AccessibilityEvent;
import android.view.accessibility.AccessibilityNodeInfo;
import android.view.accessibility.AccessibilityWindowInfo;
import android.view.inputmethod.EditorInfo;

import java.util.ArrayDeque;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;

public class ScreenService extends AccessibilityService implements ScreenLink.Actions {
    private static final long TOUCH_PIECE = 16; // ms of a piece of the path of a live finger (a frame)
    private static final long SWIPE_HOLD = 120; // ms a swipe stays still at its end (the content stops there, never flung)
    private static final long LONG_PRESS = 650; // ms of a long press (the context menu of Android)
    private static final int EDGE_SCROLLS = 40; // pages scrolled at most to reach the top / bottom of a list
    private static final long EDGE_SCROLL_DELAY = 60; // ms between two of them (the list lays out its next page)
    private static final int UNDO_DEPTH = 100; // edits kept to undo
    private static final long TYPING_GROUP = 1500; // ms: the characters typed within it are undone together
    private static ScreenService _running; // the activity asks if the screen is lent

    private final Handler _ui = new Handler(Looper.getMainLooper());
    private final ArrayDeque<float[]> _touches = new ArrayDeque<>(); // {kind, x, y} of the live finger, not dispatched yet
    private final ArrayDeque<Edit> _undo = new ArrayDeque<>(); // the edits made from the computers (fields without key events)
    private final ArrayDeque<Edit> _redo = new ArrayDeque<>();
    private final Map<Integer, Long> _keysDown = new HashMap<>(); // key code -> time of its press (its release and its repeats say it)
    private final Set<Integer> _fieldModifiers = new HashSet<>(); // the modifier keys the text field of the keys saw pressed
    private String _field = ""; // that field (its app and its id): another one starts with no modifier seen
    private ScreenLink _link;
    private GestureDescription.StrokeDescription _finger; // the live finger down now (null: none)
    private float _fingerX;
    private float _fingerY;
    private boolean _dispatching = false; // a piece of the finger is on its way (the next one waits: it would cancel it)
    private int _anchor = -1; // the fixed end of the selection made here (an app may give the ends sorted)
    private int _focus = -1; // its moving end

    // An edit of a field made from a computer: its text before and after (undone / done again by Ctrl + Z / Y)
    private static final class Edit {
        AccessibilityNodeInfo field;
        String before = "";
        int beforeAt = 0;
        String after = "";
        int afterAt = 0;
        long at = 0;
        boolean typing = false; // characters typed: grouped with the next ones
    }

    static boolean running() {return _running != null;}

    // ---------------------------------------------------------------- //
    // Life cycle

    @Override
    protected void onServiceConnected() {
        _running = this;
        OverlayService.stopIfRunning(); // the phantom one gives its place: one link of the screen at a time
        this._link = new ScreenLink(this, this, WindowManager.LayoutParams.TYPE_ACCESSIBILITY_OVERLAY);
        this._link.start();
    }

    @Override
    public void onAccessibilityEvent(AccessibilityEvent event) {
        // Nothing read: the service only draws and touches
    }

    @Override
    public void onInterrupt() {
        // Nothing to stop
    }

    @Override
    public void onConfigurationChanged(Configuration configuration) {
        super.onConfigurationChanged(configuration);
        if (this._link != null) this._link.screenChanged();
    }

    @Override
    public void onDestroy() {
        _running = null;
        if (this._link != null) this._link.stop();
        this._touches.clear();
        this._finger = null;
        super.onDestroy();
    }

    // ---------------------------------------------------------------- //
    // Touches

    @Override
    public void gesture(Path path, long duration) {
        try {
            GestureDescription.StrokeDescription stroke = new GestureDescription.StrokeDescription(path, 0, Math.max(1, duration));
            this.dispatchGesture(new GestureDescription.Builder().addStroke(stroke).build(), null, null);
        } catch (Exception e) {
            // a stroke out of the screen: ignored
        }
    }

    // A live finger: each piece of its path dispatched as it comes (a continued stroke of a frame), the next one when the
    // last is done (a new gesture would cancel it); the moves queued meanwhile go in one piece (never behind the cursor)
    @Override
    public void touch(int kind, float x, float y) {
        this._touches.add(new float[]{kind, Math.max(0, x), Math.max(0, y)});
        if (!this._dispatching) this.nextPiece();
    }

    private void nextPiece() {
        float[] op = this._touches.peek();
        Path path = new Path();
        GestureDescription.StrokeDescription stroke;
        boolean held;

        if (op == null) {this._dispatching = false; return;}
        if (op[0] == TOUCH_DOWN && this._finger != null) op = new float[]{TOUCH_UP, this._fingerX, this._fingerY}; // one finger at a time: the last one lifted first
        else this._touches.poll();
        if (op[0] != TOUCH_DOWN && this._finger == null) {this.nextPiece(); return;} // the moves of a finger the screen took back: dropped
        if (op[0] == TOUCH_DOWN) {
            path.moveTo(op[1], op[2]);
            this._fingerX = op[1];
            this._fingerY = op[2];
            held = true;
        } else {
            held = op[0] != TOUCH_UP;
            path.moveTo(this._fingerX, this._fingerY);
            this.lineTo(path, op[1], op[2]);
            while (op[0] != TOUCH_STEP && held && this._touches.peek() != null && this._touches.peek()[0] != TOUCH_DOWN && this._touches.peek()[0] != TOUCH_STEP) {
                float[] next = this._touches.poll();
                this.lineTo(path, next[1], next[2]);
                held = next[0] != TOUCH_UP;
            }
        }
        try {
            stroke = op[0] == TOUCH_DOWN ? new GestureDescription.StrokeDescription(path, 0, TOUCH_PIECE, true) : this._finger.continueStroke(path, 0, TOUCH_PIECE, held);
        } catch (Exception e) {
            this.fingerLost();
            return;
        }
        this.dispatchPiece(stroke, held);
    }

    private void dispatchPiece(GestureDescription.StrokeDescription stroke, boolean held) {
        boolean sent;

        this._finger = stroke;
        this._dispatching = true;
        try {
            sent = this.dispatchGesture(new GestureDescription.Builder().addStroke(stroke).build(), new GestureResultCallback() {
                @Override
                public void onCompleted(GestureDescription gesture) {
                    if (!held) ScreenService.this._finger = null;
                    ScreenService.this.nextPiece();
                }

                @Override
                public void onCancelled(GestureDescription gesture) {
                    ScreenService.this.fingerLost();
                }
            }, null);
        } catch (Exception e) {
            sent = false;
        }
        if (!sent) this.fingerLost();
    }

    // Another gesture (a touch of the person, a pinch) took the screen: the finger is up, its next moves are dropped
    private void fingerLost() {
        this._finger = null;
        while (this._touches.peek() != null && this._touches.peek()[0] != TOUCH_DOWN) this._touches.poll();
        this._dispatching = false;
        if (!this._touches.isEmpty()) this.nextPiece();
    }

    private void lineTo(Path path, float x, float y) {
        if (x == this._fingerX && y == this._fingerY) return;
        path.lineTo(x, y);
        this._fingerX = x;
        this._fingerY = y;
    }

    // A finger drawn from (x, y) by (dx, dy), still at its end before it lifts: the content moves by that much, never flung
    @Override
    public void swipe(float x, float y, float dx, float dy, long duration) {
        Path move = new Path();
        Path still = new Path();
        float tx = Math.max(0, x + dx);
        float ty = Math.max(0, y + dy);

        move.moveTo(Math.max(0, x), Math.max(0, y));
        move.lineTo(tx, ty);
        still.moveTo(tx, ty);
        try {
            GestureDescription.StrokeDescription first = new GestureDescription.StrokeDescription(move, 0, Math.max(1, duration), true);
            GestureDescription.StrokeDescription end = first.continueStroke(still, 0, SWIPE_HOLD, false);
            this.dispatchGesture(new GestureDescription.Builder().addStroke(first).build(), new GestureResultCallback() {
                @Override
                public void onCompleted(GestureDescription gesture) {
                    ScreenService.this.dispatchGesture(new GestureDescription.Builder().addStroke(end).build(), null, null);
                }
            }, null);
        } catch (Exception e) {
            // a stroke out of the screen: ignored
        }
    }

    @Override
    public void longPress(float x, float y) {
        Path path = new Path();
        path.moveTo(Math.max(0, x), Math.max(0, y));
        this.gesture(path, LONG_PRESS);
    }

    @Override
    public void pinch(float x, float y, float from, float to, long duration) {
        try {
            Path left = new Path();
            Path right = new Path();
            left.moveTo(Math.max(0, x - from), y);
            left.lineTo(Math.max(0, x - to), y);
            right.moveTo(x + from, y);
            right.lineTo(x + to, y);
            this.dispatchGesture(new GestureDescription.Builder()
                .addStroke(new GestureDescription.StrokeDescription(left, 0, Math.max(1, duration)))
                .addStroke(new GestureDescription.StrokeDescription(right, 0, Math.max(1, duration)))
                .build(), null, null);
        } catch (Exception e) {
            // a stroke out of the screen: ignored
        }
    }

    // PageUp / PageDown: the list under (x, y) by a page (its own scroll), else a finger that pushes most of a screen;
    // to its edge (Home / End outside a field): page after page up to its top / bottom
    @Override
    public void scroll(float x, float y, int direction, boolean toEdge) {
        AccessibilityNodeInfo list = this.scrollableAt(Math.round(x), Math.round(y));
        int action = direction < 0 ? AccessibilityNodeInfo.ACTION_SCROLL_BACKWARD : AccessibilityNodeInfo.ACTION_SCROLL_FORWARD;
        DisplayMetrics m = this.getResources().getDisplayMetrics();
        float span = m.heightPixels * 0.6f;

        if (list != null && list.performAction(action)) {
            if (toEdge) this.scrollOn(list, action, EDGE_SCROLLS - 1);
            return;
        }
        this.swipe(x, m.heightPixels / 2f + direction * span / 2f, 0, -direction * span, 300);
    }

    private void scrollOn(AccessibilityNodeInfo list, int action, int left) {
        if (left <= 0) return;
        this._ui.postDelayed(() -> {if (list.refresh() && list.performAction(action)) this.scrollOn(list, action, left - 1);}, EDGE_SCROLL_DELAY);
    }

    // The root of the window a touch at (x, y) goes to: the top one there (the overlays of the cursors let it through)
    // The touchpad of this app (its page in a WebView) is the top window under (x, y)
    @Override
    public boolean ownTouchpadAt(float x, float y) {
        AccessibilityNodeInfo root = this.windowAt(Math.round(x), Math.round(y));

        return root != null && this.getPackageName().contentEquals(root.getPackageName() == null ? "" : root.getPackageName()) && hasWebView(root, 0);
    }

    private static boolean hasWebView(AccessibilityNodeInfo node, int depth) {
        AccessibilityNodeInfo child;

        if (node == null || depth > 6) return false;
        if ("android.webkit.WebView".contentEquals(node.getClassName() == null ? "" : node.getClassName())) return true;
        for (int i = 0; i < node.getChildCount(); i++) {
            child = node.getChild(i);
            if (hasWebView(child, depth + 1)) return true;
        }
        return false;
    }

    private AccessibilityNodeInfo windowAt(int x, int y) {
        Rect bounds = new Rect();

        try {
            for (AccessibilityWindowInfo window: this.getWindows()) {
                window.getBoundsInScreen(bounds);
                if (window.getType() == AccessibilityWindowInfo.TYPE_ACCESSIBILITY_OVERLAY || !bounds.contains(x, y)) continue;
                return window.getRoot();
            }
            return this.getRootInActiveWindow();
        } catch (Exception e) {
            return null; // a window gone meanwhile
        }
    }

    // The deepest list that scrolls under (x, y)
    private AccessibilityNodeInfo scrollableAt(int x, int y) {return scrollableIn(this.windowAt(x, y), x, y, 0);}

    private static AccessibilityNodeInfo scrollableIn(AccessibilityNodeInfo node, int x, int y, int depth) {
        Rect bounds = new Rect();
        AccessibilityNodeInfo found;

        if (node == null || depth > 48) return null;
        node.getBoundsInScreen(bounds);
        if (!bounds.contains(x, y)) return null;
        for (int i = node.getChildCount() - 1; i >= 0; i--) { // the last child is drawn on top
            found = scrollableIn(node.getChild(i), x, y, depth + 1);
            if (found != null) return found;
        }
        return node.isScrollable() ? node : null;
    }

    // Reload (F5, Ctrl + R outside a text field): the pull down of a finger at the cursor from the upper part of the
    // screen (the apps that reload by a pull, at the top of their content)
    @Override
    public void refresh(float x, float y) {
        DisplayMetrics m = this.getResources().getDisplayMetrics();
        this.swipe(x, m.heightPixels * 0.25f, 0, m.heightPixels * 0.4f, 350);
    }

    // ---------------------------------------------------------------- //
    // The whole phone

    @Override
    public void back() {this.performGlobalAction(GLOBAL_ACTION_BACK);}

    @Override
    public void home() {this.performGlobalAction(GLOBAL_ACTION_HOME);}

    // The actions of the whole phone a key may give: the d-pad (Android 13+), the recent apps, the notifications, the quick
    // settings, the power menu, the lock (Android 9+), a screenshot (Android 9+), all the apps (Android 12+, else home)
    @Override
    public void global(String name) {
        int action;
        switch (name) {
            case "home": action = GLOBAL_ACTION_HOME; break;
            case "back": action = GLOBAL_ACTION_BACK; break;
            case "recents": action = GLOBAL_ACTION_RECENTS; break;
            case "notifications": action = GLOBAL_ACTION_NOTIFICATIONS; break;
            case "quickSettings": action = GLOBAL_ACTION_QUICK_SETTINGS; break;
            case "power": action = GLOBAL_ACTION_POWER_DIALOG; break;
            case "split": action = GLOBAL_ACTION_TOGGLE_SPLIT_SCREEN; break;
            case "lock": if (Build.VERSION.SDK_INT < 28) return; action = GLOBAL_ACTION_LOCK_SCREEN; break;
            case "screenshot": if (Build.VERSION.SDK_INT < 28) return; action = GLOBAL_ACTION_TAKE_SCREENSHOT; break;
            case "allApps": action = Build.VERSION.SDK_INT >= 31 ? GLOBAL_ACTION_ACCESSIBILITY_ALL_APPS : GLOBAL_ACTION_HOME; break;
            case "up": if (Build.VERSION.SDK_INT < 33) return; action = GLOBAL_ACTION_DPAD_UP; break;
            case "down": if (Build.VERSION.SDK_INT < 33) return; action = GLOBAL_ACTION_DPAD_DOWN; break;
            case "left": if (Build.VERSION.SDK_INT < 33) return; action = GLOBAL_ACTION_DPAD_LEFT; break;
            case "right": if (Build.VERSION.SDK_INT < 33) return; action = GLOBAL_ACTION_DPAD_RIGHT; break;
            case "center": if (Build.VERSION.SDK_INT < 33) return; action = GLOBAL_ACTION_DPAD_CENTER; break;
            default: return;
        }
        this.performGlobalAction(action);
    }

    // A media key (KeyEvent.KEYCODE_MEDIA_*): to the player of the phone
    @Override
    public void media(int keyCode) {
        AudioManager audio = (AudioManager) this.getSystemService(AUDIO_SERVICE);
        if (audio == null) return;
        audio.dispatchMediaKeyEvent(new KeyEvent(KeyEvent.ACTION_DOWN, keyCode));
        audio.dispatchMediaKeyEvent(new KeyEvent(KeyEvent.ACTION_UP, keyCode));
    }

    // The volume: up (1), down (-1), mute / unmute (0), its bar shown
    @Override
    public void volume(int direction) {
        AudioManager audio = (AudioManager) this.getSystemService(AUDIO_SERVICE);
        int adjust = direction > 0 ? AudioManager.ADJUST_RAISE : (direction < 0 ? AudioManager.ADJUST_LOWER : AudioManager.ADJUST_TOGGLE_MUTE);
        try {
            if (audio != null) audio.adjustSuggestedStreamVolume(adjust, AudioManager.USE_DEFAULT_STREAM_TYPE, AudioManager.FLAG_SHOW_UI);
        } catch (Exception e) {
            // Do not disturb refuses the mute
        }
    }

    // An app of the phone: "settings", "search", or a category of app (Intent.CATEGORY_APP_BROWSER, ..._EMAIL...)
    @Override
    public void launch(String what) {
        Intent intent;
        if (what.equals("settings")) intent = new Intent(android.provider.Settings.ACTION_SETTINGS);
        else if (what.equals("search")) intent = new Intent(Intent.ACTION_WEB_SEARCH);
        else intent = Intent.makeMainSelectorActivity(Intent.ACTION_MAIN, what);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        try {
            this.startActivity(intent);
        } catch (Exception e) {
            // no app of that kind on the phone
        }
    }

    // ---------------------------------------------------------------- //
    // Keys: the real key events in a focused text field (Android 13+: the input method of the service)

    @Override
    public boolean key(String code, String character, boolean down, int meta) {
        return Build.VERSION.SDK_INT >= 33 && Native.key(this, code, character, down, meta);
    }

    @Override
    public boolean enterKey(boolean newLine) {
        return Build.VERSION.SDK_INT >= 33 && Native.enter(this, newLine);
    }

    // Android 13+ only (the input method of an Accessibility service): its classes loaded only there
    private static final class Native {
        private Native() {}

        static android.accessibilityservice.InputMethod.AccessibilityInputConnection input(ScreenService service) {
            android.accessibilityservice.InputMethod method = service.getInputMethod();
            return method == null ? null : method.getCurrentInputConnection();
        }

        // The modifier keys a text field is told about, with their bit in the meta state: a field of Android selects with
        // Shift + arrows (Home, End, Ctrl + arrows...) only when it saw the Shift key itself, its key listener keeps it
        private static final int[][] MODIFIERS = {
            {KeyEvent.KEYCODE_SHIFT_LEFT, KeyEvent.META_SHIFT_LEFT_ON}, {KeyEvent.KEYCODE_SHIFT_RIGHT, KeyEvent.META_SHIFT_RIGHT_ON},
            {KeyEvent.KEYCODE_CTRL_LEFT, KeyEvent.META_CTRL_LEFT_ON}, {KeyEvent.KEYCODE_CTRL_RIGHT, KeyEvent.META_CTRL_RIGHT_ON},
            {KeyEvent.KEYCODE_ALT_LEFT, KeyEvent.META_ALT_LEFT_ON}, {KeyEvent.KEYCODE_ALT_RIGHT, KeyEvent.META_ALT_RIGHT_ON},
        };

        static boolean key(ScreenService service, String code, String character, boolean down, int meta) {
            android.accessibilityservice.InputMethod.AccessibilityInputConnection input = input(service);
            int keyCode = KeyCodes.of(code, character);
            long now = SystemClock.uptimeMillis();
            Long pressed;

            if (keyCode == KeyEvent.KEYCODE_UNKNOWN) return false;
            if (input != null) sync(service, input, meta, now);
            if (KeyEvent.isModifierKey(keyCode)) return input != null; // its press / release went with the sync
            pressed = service._keysDown.get(keyCode);
            if (!down) {
                service._keysDown.remove(keyCode);
                if (input == null || pressed == null) return true; // the field is gone: nothing to release there
                send(input, keyCode, false, 0, meta, pressed, now);
                return true;
            }
            if (input == null) return false; // no text field: the keys are actions of the phone
            if (pressed == null) service._keysDown.put(keyCode, now);
            send(input, keyCode, true, pressed == null ? 0 : 1, meta, pressed == null ? now : pressed, now);
            return true;
        }

        // The field of the keys sees the modifiers held and only them: a Shift pressed before it had the focus is pressed
        // there, one released while another field had it is released; another field starts clean (no Shift left on it)
        private static void sync(ScreenService service, android.accessibilityservice.InputMethod.AccessibilityInputConnection input, int meta, long now) {
            android.accessibilityservice.InputMethod method = service.getInputMethod();
            EditorInfo info = method == null ? null : method.getCurrentInputEditorInfo();
            String field = info == null ? "" : info.packageName + "/" + info.fieldId;
            boolean held;

            if (!field.equals(service._field)) {
                service._field = field;
                service._fieldModifiers.clear();
                input.clearMetaKeyStates(KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON | KeyEvent.META_SYM_ON);
            }
            for (int[] modifier: MODIFIERS) {
                held = (meta & modifier[1]) != 0;
                if (held && service._fieldModifiers.add(modifier[0])) send(input, modifier[0], true, 0, meta, now, now);
                else if (!held && service._fieldModifiers.remove(modifier[0])) send(input, modifier[0], false, 0, meta, now, now);
            }
        }

        private static void send(android.accessibilityservice.InputMethod.AccessibilityInputConnection input, int keyCode, boolean down, int repeat, int meta, long downTime, long now) {
            input.sendKeyEvent(new KeyEvent(downTime, now, down ? KeyEvent.ACTION_DOWN : KeyEvent.ACTION_UP, keyCode, repeat, meta, KeyCharacterMap.VIRTUAL_KEYBOARD, 0, KeyEvent.FLAG_SOFT_KEYBOARD | KeyEvent.FLAG_KEEP_TOUCH_MODE, InputDevice.SOURCE_KEYBOARD));
        }

        // Enter in a field of several lines, as the keyboard of the phone: the action its Enter key shows (a field that asks
        // for one: send...), else a new line typed as text (an app that sends on the Enter key of a keyboard sent on a plain
        // Enter); newLine (Shift / Ctrl + Enter): a new line, never the action; false: no field of several lines
        static boolean enter(ScreenService service, boolean newLine) {
            android.accessibilityservice.InputMethod method = service.getInputMethod();
            android.accessibilityservice.InputMethod.AccessibilityInputConnection input = method == null ? null : method.getCurrentInputConnection();
            EditorInfo info = method == null ? null : method.getCurrentInputEditorInfo();
            int action;

            if (input == null || info == null) return false;
            if ((info.inputType & InputType.TYPE_MASK_CLASS) != InputType.TYPE_CLASS_TEXT || (info.inputType & InputType.TYPE_TEXT_FLAG_MULTI_LINE) == 0) return false;
            action = info.imeOptions & EditorInfo.IME_MASK_ACTION;
            if (!newLine && (info.imeOptions & EditorInfo.IME_FLAG_NO_ENTER_ACTION) == 0 && action != EditorInfo.IME_ACTION_UNSPECIFIED && action != EditorInfo.IME_ACTION_NONE) {
                input.performEditorAction(action);
                return true;
            }
            input.commitText("\n", 1, null);
            return true;
        }

        static boolean commit(ScreenService service, String text) {
            android.accessibilityservice.InputMethod.AccessibilityInputConnection input = input(service);
            if (input == null) return false;
            input.commitText(text, 1, null);
            return true;
        }
    }

    // ---------------------------------------------------------------- //
    // Text fields without key events (Android 12-, a field the input method does not reach): the actions on the field

    // The focused text field (null: none, or not editable)
    private AccessibilityNodeInfo field() {
        AccessibilityNodeInfo node = this.findFocus(AccessibilityNodeInfo.FOCUS_INPUT);
        return node != null && node.isEditable() ? node : null;
    }

    @Override
    public boolean editing() {return this.field() != null;}

    private static String textOf(AccessibilityNodeInfo node) {
        return node.getText() == null || node.isShowingHintText() ? "" : node.getText().toString();
    }

    // The ends of the selection {anchor, focus}: the ones set here while the field still has them (sorted by some apps)
    private int[] selection(AccessibilityNodeInfo node, int length) {
        int start = node.getTextSelectionStart();
        int end = node.getTextSelectionEnd();
        if (start < 0 || end < 0) {start = length; end = length;}
        start = Math.max(0, Math.min(start, length));
        end = Math.max(0, Math.min(end, length));
        if (this._anchor >= 0 && Math.min(this._anchor, this._focus) == Math.min(start, end) && Math.max(this._anchor, this._focus) == Math.max(start, end)) return new int[]{this._anchor, this._focus};
        return new int[]{start, end};
    }

    // The selection sorted {start, end}
    private int[] range(AccessibilityNodeInfo node, int length) {
        int[] s = this.selection(node, length);
        return new int[]{Math.min(s[0], s[1]), Math.max(s[0], s[1])};
    }

    private void select(AccessibilityNodeInfo node, int anchor, int focus) {
        Bundle selection = new Bundle();
        selection.putInt(AccessibilityNodeInfo.ACTION_ARGUMENT_SELECTION_START_INT, anchor);
        selection.putInt(AccessibilityNodeInfo.ACTION_ARGUMENT_SELECTION_END_INT, focus);
        node.performAction(AccessibilityNodeInfo.ACTION_SET_SELECTION, selection);
        this._anchor = anchor;
        this._focus = focus;
    }

    private void setText(AccessibilityNodeInfo node, String text, int cursor) {
        Bundle args = new Bundle();
        args.putCharSequence(AccessibilityNodeInfo.ACTION_ARGUMENT_SET_TEXT_CHARSEQUENCE, text);
        node.performAction(AccessibilityNodeInfo.ACTION_SET_TEXT, args);
        this.select(node, cursor, cursor);
    }

    // Text typed by a member: the real input of the field (Android 13+), else inserted at its cursor
    @Override
    public void insert(String text) {
        AccessibilityNodeInfo node;
        String current;
        int[] s;

        if (Build.VERSION.SDK_INT >= 33 && Native.commit(this, text)) return;
        if ((node = this.field()) == null) return;
        current = textOf(node);
        s = this.range(node, current.length());
        this.edit(node, current.substring(0, s[0]) + text + current.substring(s[1]), s[0] + text.length(), text.length() == 1 && !text.equals("\n"));
    }

    // Backspace (Ctrl: the word before), the selection when there is one
    @Override
    public void backspace(boolean word) {
        AccessibilityNodeInfo node = this.field();
        String current;
        int[] s;
        int start;

        if (node == null) return;
        current = textOf(node);
        s = this.range(node, current.length());
        start = s[0];
        if (s[0] == s[1]) start = word ? wordBefore(current, s[0]) : (s[0] > 0 ? current.offsetByCodePoints(s[0], -1) : 0);
        if (start == s[1]) return;
        this.edit(node, current.substring(0, start) + current.substring(s[1]), start, false);
    }

    // Delete (Ctrl: the word after), the selection when there is one
    @Override
    public void deleteForward(boolean word) {
        AccessibilityNodeInfo node = this.field();
        String current;
        int[] s;
        int end;

        if (node == null) return;
        current = textOf(node);
        s = this.range(node, current.length());
        end = s[1];
        if (s[0] == s[1]) end = word ? wordAfter(current, s[1]) : (s[1] < current.length() ? current.offsetByCodePoints(s[1], 1) : s[1]);
        if (end == s[0]) return;
        this.edit(node, current.substring(0, s[0]) + current.substring(end), s[0], false);
    }

    @Override
    public void paste() {this.recorded(AccessibilityNodeInfo.ACTION_PASTE);}

    @Override
    public void cut() {this.recorded(AccessibilityNodeInfo.ACTION_CUT);}

    @Override
    public void copy() {
        AccessibilityNodeInfo node = this.findFocus(AccessibilityNodeInfo.FOCUS_INPUT);
        if (node != null) node.performAction(AccessibilityNodeInfo.ACTION_COPY);
    }

    @Override
    public void selectAll() {
        AccessibilityNodeInfo node = this.field();
        if (node != null) this.select(node, 0, textOf(node).length());
    }

    @Override
    public void enter(boolean newLine) {
        AccessibilityNodeInfo node = this.findFocus(AccessibilityNodeInfo.FOCUS_INPUT);
        if (node == null || !node.isEditable()) {this.global("center"); return;} // no text field: the d-pad center (Android 13+) clicks what has the focus
        // As the keyboard of the phone: a new line in a field of several lines (Shift / Ctrl + Enter too, never the send of
        // a chat), the action of a field of one line (Android 11+): send, search, go, next
        if (node.isMultiLine()) {this.insert("\n"); return;}
        if (Build.VERSION.SDK_INT >= 30) node.performAction(AccessibilityNodeInfo.AccessibilityAction.ACTION_IME_ENTER.getId());
    }

    @Override
    public void nextField(boolean back) {
        AccessibilityNodeInfo node = this.findFocus(AccessibilityNodeInfo.FOCUS_INPUT);
        AccessibilityNodeInfo next = node == null ? null : node.focusSearch(back ? android.view.View.FOCUS_BACKWARD : android.view.View.FOCUS_FORWARD);
        if (next != null) next.performAction(AccessibilityNodeInfo.ACTION_FOCUS);
    }

    // The arrows: a character, a word (Ctrl), a line (Up / Down); Shift selects; no text field: the d-pad (Android 13+)
    @Override
    public void caret(int unit, int delta, boolean select) {
        AccessibilityNodeInfo node = this.field();
        String text;
        int[] s;
        int at;
        int to;

        if (node == null) {this.global(unit == BY_LINE ? (delta < 0 ? "up" : "down") : (delta < 0 ? "left" : "right")); return;}
        text = textOf(node);
        s = this.selection(node, text.length());
        at = s[1];
        if (!select && s[0] != s[1] && unit != BY_LINE) to = delta < 0 ? Math.min(s[0], s[1]) : Math.max(s[0], s[1]); // a selection: the caret to its side
        else if (unit == BY_WORD) to = delta < 0 ? wordBefore(text, at) : wordAfter(text, at);
        else if (unit == BY_LINE) to = lineMove(text, at, delta);
        else if (delta < 0) to = at > 0 ? text.offsetByCodePoints(at, -1) : 0;
        else to = at < text.length() ? text.offsetByCodePoints(at, 1) : at;
        this.select(node, select ? s[0] : to, to);
    }

    // Home / End: the line, Ctrl: the whole text; Shift selects
    @Override
    public void caretEdge(boolean end, boolean whole, boolean select) {
        AccessibilityNodeInfo node = this.field();
        String text;
        int[] s;
        int to;

        if (node == null) return;
        text = textOf(node);
        s = this.selection(node, text.length());
        if (whole) to = end ? text.length() : 0;
        else if (end) to = text.indexOf('\n', s[1]) < 0 ? text.length() : text.indexOf('\n', s[1]);
        else to = text.lastIndexOf('\n', s[1] - 1) + 1;
        this.select(node, select ? s[0] : to, to);
    }

    // Ctrl + Z: the last edit made from a computer, while the field still holds what it wrote (a change made on the phone
    // since ends the history: nothing undone blindly)
    @Override
    public void undo() {
        AccessibilityNodeInfo node = this.field();
        Edit e = this._undo.peekLast();

        if (node == null || e == null) return;
        if (!sameField(e.field, node) || !textOf(node).equals(e.after)) {this._undo.clear(); this._redo.clear(); return;}
        this._undo.removeLast();
        this.setText(node, e.before, e.beforeAt);
        this._redo.addLast(e);
    }

    // Ctrl + Y, Ctrl + Shift + Z: the edit undone, done again
    @Override
    public void redo() {
        AccessibilityNodeInfo node = this.field();
        Edit e = this._redo.peekLast();

        if (node == null || e == null) return;
        if (!sameField(e.field, node) || !textOf(node).equals(e.before)) {this._redo.clear(); return;}
        this._redo.removeLast();
        this.setText(node, e.after, e.afterAt);
        this._undo.addLast(e);
    }

    // The text of the field set, the edit kept to be undone
    private void edit(AccessibilityNodeInfo node, String next, int cursor, boolean typing) {
        String before = textOf(node);
        this.remember(node, before, this.range(node, before.length())[0], next, cursor, typing);
        this.setText(node, next, cursor);
    }

    // An action of the app on the field (paste, cut): its text read again after it, the edit kept to be undone
    private void recorded(int action) {
        AccessibilityNodeInfo node = this.findFocus(AccessibilityNodeInfo.FOCUS_INPUT);
        String before;
        String after;
        int at;

        if (node == null) return;
        before = textOf(node);
        at = this.range(node, before.length())[0];
        if (!node.performAction(action) || !node.isEditable() || !node.refresh()) return;
        after = textOf(node);
        if (!after.equals(before)) this.remember(node, before, at, after, this.range(node, after.length())[1], false);
    }

    private void remember(AccessibilityNodeInfo node, String before, int beforeAt, String after, int afterAt, boolean typing) {
        long now = SystemClock.uptimeMillis();
        Edit top = this._undo.peekLast();
        Edit e = new Edit();

        this._redo.clear();
        if (typing && top != null && top.typing && sameField(top.field, node) && now - top.at < TYPING_GROUP && before.equals(top.after)) {
            top.after = after;
            top.afterAt = afterAt;
            top.at = now;
            return;
        }
        e.field = node;
        e.before = before;
        e.beforeAt = beforeAt;
        e.after = after;
        e.afterAt = afterAt;
        e.at = now;
        e.typing = typing;
        this._undo.addLast(e);
        while (this._undo.size() > UNDO_DEPTH) this._undo.removeFirst();
    }

    private static boolean sameField(AccessibilityNodeInfo a, AccessibilityNodeInfo b) {
        if (a.equals(b)) return true;
        return a.getWindowId() == b.getWindowId() && a.getViewIdResourceName() != null && a.getViewIdResourceName().equals(b.getViewIdResourceName());
    }

    // ---------------------------------------------------------------- //
    // Words and lines of a text

    private static boolean wordCharacter(char c) {return Character.isLetterOrDigit(c) || c == '_';}

    // The start of the word before at (Ctrl + Left / Backspace): the spaces, then the letters or the other signs
    private static int wordBefore(String s, int at) {
        int i = at;
        boolean word;

        while (i > 0 && Character.isWhitespace(s.charAt(i - 1))) i--;
        word = i > 0 && wordCharacter(s.charAt(i - 1));
        while (i > 0 && !Character.isWhitespace(s.charAt(i - 1)) && wordCharacter(s.charAt(i - 1)) == word) i--;
        return i;
    }

    // The end of the word after at (Ctrl + Right / Delete)
    private static int wordAfter(String s, int at) {
        int i = at;
        boolean word;

        while (i < s.length() && Character.isWhitespace(s.charAt(i))) i++;
        word = i < s.length() && wordCharacter(s.charAt(i));
        while (i < s.length() && !Character.isWhitespace(s.charAt(i)) && wordCharacter(s.charAt(i)) == word) i++;
        return i;
    }

    // The same column on the line before / after (the lines of the text: a long one wrapped on the screen is one line)
    private static int lineMove(String text, int at, int delta) {
        int start = text.lastIndexOf('\n', at - 1) + 1;
        int column = at - start;
        int end;
        int nextEnd;

        if (delta < 0) {
            if (start == 0) return 0;
            return Math.min(text.lastIndexOf('\n', start - 2) + 1 + column, start - 1);
        }
        end = text.indexOf('\n', at);
        if (end < 0) return text.length();
        nextEnd = text.indexOf('\n', end + 1);
        return Math.min(end + 1 + column, nextEnd < 0 ? text.length() : nextEnd);
    }
}
