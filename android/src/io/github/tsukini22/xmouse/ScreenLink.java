/*
 * The screen of the phone in the layout of the network, shared by the two services that lend it:
 *  - OverlayService (phantom): the cursors of the members drawn over the apps with their name, nothing touched
 *    ("Display over other apps" only: no Accessibility, nothing blocked by Play Protect)
 *  - ScreenService (Accessibility): the same, and their clicks / drags / scrolls / text become touches (Actions)
 * It links to the device of the pad (role "screen"), or takes the events of the node of the phone when the phone is a
 * member itself (NodeService), tells the size of the screen, keeps the cursors and draws them.
*/
package io.github.tsukini22.xmouse;

import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.PixelFormat;
import android.graphics.RectF;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;
import android.util.DisplayMetrics;
import android.view.KeyEvent;
import android.view.View;
import android.view.WindowManager;
import android.widget.Toast;

import io.github.tsukini22.xmouse.core.Json;
import io.github.tsukini22.xmouse.core.Node;

import org.json.JSONObject;

import java.util.Arrays;
import java.util.HashMap;
import java.util.HashSet;
import java.util.LinkedHashMap;
import java.util.Map;
import java.util.Set;

final class ScreenLink {
    // What the Accessibility service does with the input of the cursors (null: phantom, nothing touched)
    interface Actions {
        int TOUCH_DOWN = 0; // a finger put on the screen
        int TOUCH_MOVE = 1; // it moved (sent as it comes, never replayed at the end)
        int TOUCH_UP = 2; // lifted
        int BY_CHARACTER = 0; // units of the moves of the caret
        int BY_WORD = 1;
        int BY_LINE = 2;

        void gesture(Path path, long duration);
        void touch(int kind, float x, float y); // a live finger: a tap, a drag, a long press of their real length
        void swipe(float x, float y, float dx, float dy, long duration); // still at its end: the content moves that much, never flung
        void longPress(float x, float y); // the context menu of Android
        void pinch(float x, float y, float from, float to, long duration); // two fingers on a line through (x, y), their half distance from -> to
        void scroll(float x, float y, int direction, boolean toEdge); // the list under (x, y): a page, or up to its top / bottom
        void refresh(float x, float y); // reload: a pull down at the cursor
        boolean key(String code, String character, boolean down, int meta); // the real key event in a focused text field (Android 13+), false: none
        boolean editing(); // a text field has the focus
        void back();
        void home();
        void insert(String text);
        void backspace(boolean word); // Ctrl: the word before
        void deleteForward(boolean word); // Ctrl: the word after
        void paste(); // the clipboard of the phone (the one of the computer, sent when its cursor comes) in the focused field
        void copy();
        void cut();
        void selectAll();
        void undo(); // the edits made from the computers
        void redo();
        void enter(boolean newLine); // the action of the keyboard of the field (send, search, next); Shift: a new line
        void nextField(boolean back); // Tab, Shift + Tab
        void caret(int unit, int delta, boolean select); // the arrows: a character, a word (Ctrl), a line (Up / Down); Shift selects
        void caretEdge(boolean end, boolean whole, boolean select); // Home / End: the line, Ctrl: the text; Shift selects
        void global(String name); // home, back, recents, notifications, quickSettings, power, lock, screenshot, allApps, split, the d-pad
        void media(int keyCode); // KeyEvent.KEYCODE_MEDIA_*: to the player of the phone
        void volume(int direction); // 1 up, -1 down, 0 mute / unmute
        void launch(String what); // "settings", "search", a category of app (Intent.CATEGORY_APP_*)
    }

    private static final long WHEEL_LIFT = 140; // ms without a notch before the finger of the wheel lifts (still: no fling)
    private static final float WHEEL_MARGIN = 24; // dp: the finger of the wheel is put again at the cursor before a border
    private static final Set<String> MODIFIERS = new HashSet<>(Arrays.asList("ShiftLeft", "ShiftRight", "ControlLeft", "ControlRight", "AltLeft", "AltRight", "MetaLeft", "MetaRight", "OSLeft", "OSRight"));
    // Keys of the whole phone, never sent to an app as they are (Android acts on them: back, the media, the volume...)
    private static final Set<String> SYSTEM_KEYS = new HashSet<>(Arrays.asList("Escape", "PrintScreen", "ContextMenu", "AudioVolumeUp", "AudioVolumeDown", "AudioVolumeMute",
        "MediaPlayPause", "MediaPlay", "MediaPause", "MediaStop", "MediaTrackNext", "MediaTrackPrevious", "MediaFastForward", "MediaRewind", "MediaRecord", "Eject",
        "BrowserBack", "BrowserHome", "BrowserSearch", "LaunchMail", "LaunchApp1", "LaunchApp2", "MediaSelect", "Power", "Sleep", "WakeUp", "BrightnessUp", "BrightnessDown"));
    // The numpad without Num Lock (its digits come as text with it): its arrows, Home, End, PageUp, PageDown, Insert, Delete
    private static final Map<String, String> NUMPAD_NAVIGATION = new HashMap<>();

    static {
        String[][] pairs = {{"Numpad8", "ArrowUp"}, {"Numpad2", "ArrowDown"}, {"Numpad4", "ArrowLeft"}, {"Numpad6", "ArrowRight"}, {"Numpad7", "Home"},
            {"Numpad1", "End"}, {"Numpad9", "PageUp"}, {"Numpad3", "PageDown"}, {"Numpad0", "Insert"}, {"NumpadDecimal", "Delete"}};
        for (String[] pair: pairs) NUMPAD_NAVIGATION.put(pair[0], pair[1]);
    }
    private static final float WHEEL_PIXELS = 0.8f; // px of swipe per 1/120 of a notch
    private static final long[] RECONNECT = {1000, 2000, 4000, 8000, 15000}; // ms
    private static final long IDLE_HIDE = 3000; // ms a cursor stays drawn without moving
    private static final long IDLE_FADE = 600; // ms of its fade out (a click alone never brings it back)
    private static final long KEEP_ON = 300000; // ms the screen stays on after the last input of a cursor (else Android turns it off and locks it under the cursor)

    private final Context _context;
    private final Actions _actions;
    private final int _windowType;
    private final Handler _ui = new Handler(Looper.getMainLooper());
    private final Map<String, Cursor> _cursors = new LinkedHashMap<>();
    private final Set<String> _told = new HashSet<>(); // members told that this phone only shows their cursor
    private WsClient _ws;
    private CursorView _view;
    private WindowManager.LayoutParams _params; // of the overlay (keep the screen on)
    private boolean _keepOn = false;
    private float _density = 1;
    private int _tries = 0;
    private boolean _stopped = false;
    private boolean _local = false; // the phone is a member: the events come from its node
    private final Set<String> _held = new HashSet<>(); // the modifiers held by the cursors on this screen (Ctrl, Shift, Alt, Meta)
    private final Set<String> _native = new HashSet<>(); // keys pressed as real key events (released the same way)
    private boolean _metaAlone = false; // Meta pressed, no other key yet: alone it opens all the apps (the start menu)
    private String _wheelBy = null; // the cursor whose wheel holds a finger on the screen
    private float _wheelX;
    private float _wheelY;
    private final Runnable _wheelLift = this::wheelUp;
    private static volatile ScreenLink _active; // the screen lent now (the node gives it its events)

    // A foreign cursor on this screen
    private static final class Cursor {
        String name = "";
        int color = Color.rgb(61, 220, 151);
        float x;
        float y;
        boolean down = false; // its left button held: a finger on the screen
        long moved = SystemClock.uptimeMillis(); // last move: a still cursor fades out
    }

    ScreenLink(Context context, Actions actions, int windowType) {
        this._context = context;
        this._actions = actions;
        this._windowType = windowType;
    }

    // ---------------------------------------------------------------- //
    // Life cycle

    static ScreenLink active() {return _active;}

    void start() {
        this._stopped = false;
        this._density = this._context.getResources().getDisplayMetrics().density;
        this.addOverlay();
        _active = this;
        Node node = NodeService.node();
        this._local = node != null && node.joined(); // a member: its own network; else the computer its touchpad is paired with
        if (this._local) this.sendScreen(); // a member: its node publishes the screen
        else this.connect();
    }

    // An event of the node of the phone (a member itself): the same json as the screen of a pad
    void onLocalEvent(Map<String, Object> event) {
        if (this._local && !this._stopped) this.onMessage(Json.write(event));
    }

    void stop() {
        Node node = NodeService.node();
        if (this._local && node != null) node.setScreen(0, 0, 1);
        this.wheelUp();
        if (this._actions != null) for (Cursor c: this._cursors.values()) if (c.down) this._actions.touch(Actions.TOUCH_UP, c.x, c.y);
        if (_active == this) _active = null;
        this._stopped = true;
        this._ui.removeCallbacks(this._keepOff);
        if (this._ws != null) this._ws.close();
        this._ws = null;
        if (this._view != null) {
            try {
                ((WindowManager) this._context.getSystemService(Context.WINDOW_SERVICE)).removeView(this._view);
            } catch (Exception e) {
                // already gone
            }
        }
        this._view = null;
    }

    // Rotated / resized: the layout gets the new size
    void screenChanged() {
        this._density = this._context.getResources().getDisplayMetrics().density;
        this.sendScreen();
    }

    // ---------------------------------------------------------------- //
    // Link

    private SharedPreferences prefs() {return this._context.getSharedPreferences(MainActivity.PREFS, Context.MODE_PRIVATE);}

    // Private mode (setting of the app): the cursors of the others pass over this screen, nothing is touched
    private boolean privateMode() {return this.prefs().getBoolean("private", false);}

    private Actions actions() {return this.privateMode() ? null : this._actions;}

    private void connect() {
        final String origin = this.prefs().getString("host", "");
        final String fp = this.prefs().getString("fp", "");
        final String token = this.prefs().getString("token", "");
        if (this._stopped || this._local) return;
        if (origin.isEmpty() || token.isEmpty()) {
            this._ui.post(() -> Toast.makeText(this._context, "xmouse: pair the app with a device first", Toast.LENGTH_LONG).show());
            return;
        }
        android.net.Uri uri = android.net.Uri.parse(origin);
        this._ws = new WsClient(uri.getHost(), uri.getPort() > 0 ? uri.getPort() : 47900, "/padws", fp, new WsClient.Listener() {
            @Override public void onOpen() {
                ScreenLink.this._tries = 0;
                ScreenLink.this.send(json("t", "hello", "token", token, "role", "screen"));
            }

            @Override public void onText(String text) {
                ScreenLink.this._ui.post(() -> ScreenLink.this.onMessage(text));
            }

            @Override public void onClose(String reason) {
                ScreenLink.this._ui.post(() -> {
                    ScreenLink.this._cursors.clear();
                    if (ScreenLink.this._view != null) ScreenLink.this._view.postInvalidateOnAnimation();
                    if (ScreenLink.this._stopped) return;
                    // The touch link of this phone took the screen: the phantom one gives way (never two links taking turns)
                    if ("replaced".equals(reason) && ScreenLink.this._actions == null && ScreenLink.this._context instanceof android.app.Service) {
                        ((android.app.Service) ScreenLink.this._context).stopSelf();
                        return;
                    }
                    long wait = RECONNECT[Math.min(ScreenLink.this._tries++, RECONNECT.length - 1)];
                    ScreenLink.this._ui.postDelayed(ScreenLink.this::connect, wait);
                });
            }
        });
        this._ws.start();
    }

    private void send(JSONObject o) {
        Node node = NodeService.node();
        if (this._local && node != null) {
            if (o.optString("t").equals("screen")) node.setScreen(o.optInt("w"), o.optInt("h"), o.optDouble("scale", 1));
            else if (o.optString("t").equals("access")) node.tell(o.optString("id"), o.optString("text"));
            return;
        }
        if (this._ws != null) this._ws.send(o.toString());
    }

    static JSONObject json(Object... pairs) {
        JSONObject o = new JSONObject();
        try {
            for (int i = 0; i + 1 < pairs.length; i += 2) o.put((String) pairs[i], pairs[i + 1]);
        } catch (Exception e) {
            // keys are strings
        }
        return o;
    }

    // Logical size of the screen (dp): the cursors of the members move at the same speed as on a desktop
    @SuppressWarnings("deprecation")
    private void sendScreen() {
        DisplayMetrics m = new DisplayMetrics();
        ((WindowManager) this._context.getSystemService(Context.WINDOW_SERVICE)).getDefaultDisplay().getRealMetrics(m);
        this.send(json("t", "screen", "w", Math.round(m.widthPixels / m.density), "h", Math.round(m.heightPixels / m.density), "scale", (double) m.density, "on", true, "touch", this._actions != null));
    }

    private void onMessage(String text) {
        JSONObject o;
        try {
            o = new JSONObject(text);
        } catch (Exception e) {
            return;
        }
        String type = o.optString("t");
        String id = o.optString("id");
        switch (type) {
            case "enter": case "pointer": case "button": case "wheel": case "key": case "text": this.used(); break;
            default: break;
        }
        switch (type) {
            case "welcome":
                this.sendScreen();
                break;
            case "error":
                Toast.makeText(this._context, "xmouse: " + o.optString("text"), Toast.LENGTH_LONG).show();
                break;
            case "enter": {
                Cursor c = new Cursor();
                c.name = o.optString("name");
                try {c.color = Color.parseColor(o.optString("color", "#3ddc97"));} catch (Exception e) { /* default */ }
                c.x = o.optInt("x") * this._density;
                c.y = o.optInt("y") * this._density;
                this._cursors.put(id, c);
                this.wakeFade();
                break;
            }
            case "leave": {
                Cursor gone = this._cursors.remove(id);
                if (gone != null && gone.down && this.actions() != null) this._actions.touch(Actions.TOUCH_UP, gone.x, gone.y); // its finger lifted where it left
                if (id.equals(this._wheelBy)) this.wheelUp();
                if (this._cursors.isEmpty()) {
                    this._ui.removeCallbacks(this._keepOff);
                    this.keepOn(false); // no cursor: the timeout of Android
                    this._held.clear();
                    this._native.clear();
                }
                break;
            }
            case "pointer": {
                Cursor c = this._cursors.get(id);
                if (c == null) break;
                c.x = o.optInt("x") * this._density;
                c.y = o.optInt("y") * this._density;
                c.moved = SystemClock.uptimeMillis();
                if (c.down && this.actions() != null) this._actions.touch(Actions.TOUCH_MOVE, c.x, c.y); // the finger follows the cursor as it moves
                this.wakeFade();
                break;
            }
            case "button":
                this.onButton(id, o);
                break;
            case "wheel":
                this.onWheel(id, o.optInt("dx"), o.optInt("dy"));
                break;
            case "key":
                this.onKeyEvent(id, o.optString("code"), o.optString("char"), o.optBoolean("down"));
                break;
            case "clip": // the clipboard of the computer, to paste here
                try {
                    android.content.ClipboardManager clipboard = (android.content.ClipboardManager) this._context.getSystemService(Context.CLIPBOARD_SERVICE);
                    if (clipboard != null) clipboard.setPrimaryClip(android.content.ClipData.newPlainText("xmouse", o.optString("text")));
                } catch (Exception e) {
                    // the clipboard refused (a work profile): nothing to paste
                }
                break;
            case "text":
                if (this.actions() != null) this.actions().insert(o.optString("text"));
                break;
            default:
                break;
        }
        if (this._view != null) this._view.postInvalidateOnAnimation();
    }

    // ---------------------------------------------------------------- //
    // Input of the foreign cursors

    private void onButton(String id, JSONObject o) {
        Cursor c = this._cursors.get(id);
        int b = o.optInt("b");
        boolean down = o.optBoolean("down");
        if (c == null) return;
        if (o.has("x")) {c.x = o.optInt("x") * this._density; c.y = o.optInt("y") * this._density;}
        if (this.actions() == null) {
            // Phantom / private mode: the click is only shown; the member is told once why nothing happens
            c.down = down;
            if (down && this._told.add(id)) {
                this.send(json("t", "access", "id", id, "text", this.privateMode() ? "private mode: your cursor passes over this phone without acting"
                    : "this phone only shows your cursor (turn on its Accessibility service \"xmouse screen\" to let it touch)"));
                if (!this.privateMode()) Toast.makeText(this._context, "xmouse: " + (java.util.Locale.getDefault().getLanguage().equals("fr")
                    ? "les clics ne font rien ici : xmouse > Cet écran dans la disposition > Les laisser toucher"
                    : "the clicks do nothing here: xmouse > This screen in the layout > Let them touch"), Toast.LENGTH_LONG).show();
            }
            return;
        }
        if (b == 3) {if (down) this._actions.back(); return;} // back button of the mouse
        if (b == 1 || b == 4) return; // middle, forward: Android has none (never a touch by mistake)
        if (b == 2) {if (down) this._actions.longPress(c.x, c.y); return;} // right button: a long press at once (the context menu of Android)
        // Left button: a finger, put and lifted with the button, its moves sent as they come: a drag follows the cursor,
        // a long press lasts what the button is held, a quick click is a tap
        if (down) {
            this.wheelUp(); // the finger of the wheel lifted first: one finger at a time
            c.down = true;
            this._actions.touch(Actions.TOUCH_DOWN, c.x, c.y);
            return;
        }
        if (!c.down) return;
        c.down = false;
        this._actions.touch(Actions.TOUCH_UP, c.x, c.y);
    }

    // The view is drawn again when the next still cursor starts to fade (nothing else would redraw it)
    private final Runnable _fadeWake = () -> {if (this._view != null) this._view.postInvalidateOnAnimation();};

    private void wakeFade() {
        this._ui.removeCallbacks(this._fadeWake);
        this._ui.postDelayed(this._fadeWake, IDLE_HIDE + 16);
    }

    private static float idleAlpha(Cursor c, long now) {
        long idle = now - c.moved;
        if (c.down || idle < IDLE_HIDE) return 1f;
        return Math.max(0f, 1f - (float) (idle - IDLE_HIDE) / IDLE_FADE);
    }

    // Page Up / Down: a swipe of most of the screen at the cursor (or the middle of the screen)
    // A zoom where the cursor is (Ctrl + wheel, Ctrl + plus / minus): two fingers apart (in) or together (out), both
    // inside the screen (a finger off the screen is no finger): narrower near a border, centred inward at the edges
    private void zoom(float x, float y, int direction, int amount) {
        DisplayMetrics m = this._context.getResources().getDisplayMetrics();
        float margin = 8 * this._density;
        float base = 32 * this._density;
        float far = Math.min(base + Math.min(220 * this._density, Math.max(40, amount) * 0.5f * this._density), m.widthPixels / 2f - margin);
        float cx = Math.max(far + margin, Math.min(m.widthPixels - far - margin, x));
        if (direction > 0) this._actions.pinch(cx, y, base, far, 220);
        else this._actions.pinch(cx, y, far, base, 220);
    }

    // The wheel: a finger put at the cursor that follows the notches as they come, lifted still WHEEL_LIFT after the last
    // one (the content stops there, never flung), put again at the cursor before it leaves the screen
    private void onWheel(String id, int dx, int dy) {
        Cursor c = this._cursors.get(id);
        DisplayMetrics m = this._context.getResources().getDisplayMetrics();
        float margin = WHEEL_MARGIN * this._density;
        float mx = dx * WHEEL_PIXELS * this._density;
        float my = dy * WHEEL_PIXELS * this._density; // wheel up (dy > 0) shows what is above: the finger goes down
        float x0;
        float y0;

        if (c == null || this.actions() == null) return;
        if (this.ctrl() && dy != 0) {this.wheelUp(); this.zoom(c.x, c.y, dy > 0 ? 1 : -1, Math.abs(dy)); return;}
        if (c.down) return; // a button holds the finger: never a second one
        if (!id.equals(this._wheelBy) || outside(this._wheelX + mx, this._wheelY + my, m, margin)) {
            this.wheelUp();
            // At the cursor, moved back from the border it goes to: the content under the cursor follows it
            x0 = mx > 0 ? Math.min(c.x, m.widthPixels * 0.75f) : (mx < 0 ? Math.max(c.x, m.widthPixels * 0.25f) : c.x);
            y0 = my > 0 ? Math.min(c.y, m.heightPixels * 0.75f) : (my < 0 ? Math.max(c.y, m.heightPixels * 0.25f) : c.y);
            this._wheelBy = id;
            this._wheelX = x0;
            this._wheelY = y0;
            this._actions.touch(Actions.TOUCH_DOWN, x0, y0);
        }
        this._wheelX = Math.max(0, Math.min(m.widthPixels - 1, this._wheelX + mx));
        this._wheelY = Math.max(0, Math.min(m.heightPixels - 1, this._wheelY + my));
        this._actions.touch(Actions.TOUCH_MOVE, this._wheelX, this._wheelY);
        this._ui.removeCallbacks(this._wheelLift);
        this._ui.postDelayed(this._wheelLift, WHEEL_LIFT);
    }

    private static boolean outside(float x, float y, DisplayMetrics m, float margin) {
        return x < margin || y < margin || x > m.widthPixels - margin || y > m.heightPixels - margin;
    }

    private void wheelUp() {
        this._ui.removeCallbacks(this._wheelLift);
        if (this._wheelBy == null) return;
        this._wheelBy = null;
        if (this._actions != null) this._actions.touch(Actions.TOUCH_UP, this._wheelX, this._wheelY);
    }

    // ---------------------------------------------------------------- //
    // Keys of the foreign cursors

    private boolean ctrl() {return this._held.contains("ControlLeft") || this._held.contains("ControlRight");}

    private boolean shift() {return this._held.contains("ShiftLeft") || this._held.contains("ShiftRight");}

    private boolean alt() {return this._held.contains("AltLeft") || this._held.contains("AltRight");}

    private boolean meta() {return this._held.contains("MetaLeft") || this._held.contains("MetaRight");}

    // The modifiers held, as the meta state of a key event of Android
    private int metaState() {
        int meta = 0;
        if (this._held.contains("ShiftLeft")) meta |= KeyEvent.META_SHIFT_ON | KeyEvent.META_SHIFT_LEFT_ON;
        if (this._held.contains("ShiftRight")) meta |= KeyEvent.META_SHIFT_ON | KeyEvent.META_SHIFT_RIGHT_ON;
        if (this._held.contains("ControlLeft")) meta |= KeyEvent.META_CTRL_ON | KeyEvent.META_CTRL_LEFT_ON;
        if (this._held.contains("ControlRight")) meta |= KeyEvent.META_CTRL_ON | KeyEvent.META_CTRL_RIGHT_ON;
        if (this._held.contains("AltLeft")) meta |= KeyEvent.META_ALT_ON | KeyEvent.META_ALT_LEFT_ON;
        if (this._held.contains("AltRight")) meta |= KeyEvent.META_ALT_ON | KeyEvent.META_ALT_RIGHT_ON;
        if (this._held.contains("MetaLeft")) meta |= KeyEvent.META_META_ON | KeyEvent.META_META_LEFT_ON;
        if (this._held.contains("MetaRight")) meta |= KeyEvent.META_META_ON | KeyEvent.META_META_RIGHT_ON;
        return meta;
    }

    // A key of a cursor: a modifier is kept; in a focused text field the real key event (every shortcut of the app),
    // except the keys of the whole phone; else the action of the phone that does what the key does on a computer
    private void onKeyEvent(String id, String code, String character, boolean down) {
        String key = NUMPAD_NAVIGATION.containsKey(code) ? NUMPAD_NAVIGATION.get(code) : code;

        if (key.startsWith("OS")) key = "Meta" + key.substring(2);
        if (MODIFIERS.contains(key)) {
            this.onModifier(key, down);
            // A text field sees Shift, Ctrl and Alt themselves (it selects with Shift + arrows only then); Meta never
            if (!key.startsWith("Meta") && this.actions() != null) this._actions.key(key, "", down, this.metaState());
            return;
        }
        if (this.actions() == null) return;
        if (!down) {
            if (this._native.remove(key)) this._actions.key(key, character, false, this.metaState());
            return;
        }
        this._metaAlone = false;
        if (!this.systemKey(key) && this._actions.key(key, character, true, this.metaState())) {this._native.add(key); return;}
        this.onKey(id, key, character);
    }

    private void onModifier(String key, boolean down) {
        boolean meta = key.startsWith("Meta");

        if (down) {
            if (meta && !this.meta()) this._metaAlone = true; // the first Meta
            else if (!meta) this._metaAlone = false; // Meta + Shift + S...: not alone
            this._held.add(key);
            return;
        }
        this._held.remove(key);
        if (meta && this._metaAlone && !this.meta()) {
            this._metaAlone = false;
            if (this.actions() != null) this._actions.global("allApps"); // Meta alone: all the apps (the start menu), like Android 14
        }
    }

    // The keys the phone does itself whatever has the focus: Meta + any key, Alt + Tab, the media...
    private boolean systemKey(String key) {
        boolean ctrl = this.ctrl();
        boolean shift = this.shift();
        boolean alt = this.alt();

        if (SYSTEM_KEYS.contains(key) || this.meta()) return true;
        if (alt && !ctrl && (key.equals("Tab") || key.equals("F4") || key.equals("ArrowLeft") || key.equals("ArrowRight") || key.equals("Home"))) return true;
        if (ctrl && alt && key.equals("Delete")) return true;
        if (ctrl && (key.equals("Equal") || key.equals("Minus") || key.equals("NumpadAdd") || key.equals("NumpadSubtract"))) return true; // a zoom where the cursor is
        if ((ctrl || shift) && key.equals("Insert")) return true;
        return shift && !ctrl && (key.equals("Delete") || key.equals("F10"));
    }

    // A key that is not given to an app as it is: the action of the phone that does what it does on a computer
    private void onKey(String id, String key, String character) {
        Cursor c = this._cursors.get(id);
        DisplayMetrics m = this._context.getResources().getDisplayMetrics();
        float x = c != null ? c.x : m.widthPixels / 2f;
        float y = c != null ? c.y : m.heightPixels / 2f;
        // The letter of the layout of the computer (azerty Ctrl + A is the key KeyQ), else the one of the code
        String letter = (character.isEmpty() && key.startsWith("Key") ? key.substring(3) : character).toLowerCase(java.util.Locale.ROOT);
        boolean ctrl = this.ctrl();
        boolean shift = this.shift();
        boolean alt = this.alt();
        boolean editing;

        if (this.meta()) {this.onMetaKey(key, letter, shift, ctrl); return;}
        if (ctrl && alt && key.equals("Delete")) {this._actions.global("power"); return;} // Ctrl + Alt + Del: the power menu
        if (ctrl && key.equals("Escape")) {this._actions.global(shift ? "recents" : "allApps"); return;} // Ctrl + Shift + Esc: the apps running; Ctrl + Esc: the start menu
        if (alt && !ctrl) {
            switch (key) {
                case "Tab": this._actions.global("recents"); return;
                case "F4": case "ArrowLeft": this._actions.back(); return; // close, the page before
                case "Home": this._actions.home(); return;
                case "ArrowRight": return; // forward: Android has none
                default: break;
            }
        }
        if (ctrl) {
            switch (letter) {
                case "a": this._actions.selectAll(); return;
                case "c": this._actions.copy(); return;
                case "x": this._actions.cut(); return;
                case "v": this._actions.paste(); return; // Ctrl + Shift + V too: the text only
                case "z": if (shift) this._actions.redo(); else this._actions.undo(); return;
                case "y": this._actions.redo(); return;
                case "r": this._actions.refresh(x, y); return; // Ctrl + R, Ctrl + Shift + R: reload
                default: break;
            }
            switch (key) {
                case "Equal": case "NumpadAdd": this.zoom(x, y, 1, 120); return;
                case "Minus": case "NumpadSubtract": this.zoom(x, y, -1, 120); return;
                case "Insert": this._actions.copy(); return;
                case "F5": this._actions.refresh(x, y); return;
                default: break;
            }
        }
        if (shift && !ctrl) {
            switch (key) {
                case "Insert": this._actions.paste(); return;
                case "Delete": this._actions.cut(); return;
                case "F10": this._actions.longPress(x, y); return; // the context menu
                default: break;
            }
        }
        editing = this._actions.editing();
        switch (key) {
            case "Escape": case "BrowserBack": this._actions.back(); break;
            case "BrowserHome": this._actions.home(); break;
            case "F5": case "BrowserRefresh": this._actions.refresh(x, y); break;
            case "Backspace": this._actions.backspace(ctrl); break;
            case "Delete": this._actions.deleteForward(ctrl); break;
            case "Enter": case "NumpadEnter": this._actions.enter(shift && !ctrl); break;
            case "Tab": this._actions.nextField(shift); break;
            case "ArrowLeft": this._actions.caret(ctrl ? Actions.BY_WORD : Actions.BY_CHARACTER, -1, shift); break;
            case "ArrowRight": this._actions.caret(ctrl ? Actions.BY_WORD : Actions.BY_CHARACTER, 1, shift); break;
            case "ArrowUp": this._actions.caret(Actions.BY_LINE, -1, shift); break;
            case "ArrowDown": this._actions.caret(Actions.BY_LINE, 1, shift); break;
            case "Home": if (editing) this._actions.caretEdge(false, ctrl, shift); else this._actions.scroll(x, y, -1, true); break;
            case "End": if (editing) this._actions.caretEdge(true, ctrl, shift); else this._actions.scroll(x, y, 1, true); break;
            case "PageUp": this._actions.scroll(x, y, -1, false); break;
            case "PageDown": this._actions.scroll(x, y, 1, false); break;
            case "PrintScreen": this._actions.global("screenshot"); break;
            case "ContextMenu": this._actions.longPress(x, y); break;
            case "AudioVolumeUp": this._actions.volume(1); break;
            case "AudioVolumeDown": this._actions.volume(-1); break;
            case "AudioVolumeMute": this._actions.volume(0); break;
            case "MediaPlayPause": this._actions.media(KeyEvent.KEYCODE_MEDIA_PLAY_PAUSE); break;
            case "MediaPlay": this._actions.media(KeyEvent.KEYCODE_MEDIA_PLAY); break;
            case "MediaPause": this._actions.media(KeyEvent.KEYCODE_MEDIA_PAUSE); break;
            case "MediaStop": this._actions.media(KeyEvent.KEYCODE_MEDIA_STOP); break;
            case "MediaTrackNext": this._actions.media(KeyEvent.KEYCODE_MEDIA_NEXT); break;
            case "MediaTrackPrevious": this._actions.media(KeyEvent.KEYCODE_MEDIA_PREVIOUS); break;
            case "MediaFastForward": this._actions.media(KeyEvent.KEYCODE_MEDIA_FAST_FORWARD); break;
            case "MediaRewind": this._actions.media(KeyEvent.KEYCODE_MEDIA_REWIND); break;
            case "MediaRecord": this._actions.media(KeyEvent.KEYCODE_MEDIA_RECORD); break;
            case "Eject": this._actions.media(KeyEvent.KEYCODE_MEDIA_EJECT); break;
            case "BrowserSearch": this._actions.launch("search"); break;
            case "LaunchMail": this._actions.launch(Intent.CATEGORY_APP_EMAIL); break;
            case "LaunchApp1": this._actions.launch(Build.VERSION.SDK_INT >= 29 ? Intent.CATEGORY_APP_FILES : Intent.CATEGORY_APP_GALLERY); break;
            case "LaunchApp2": this._actions.launch(Intent.CATEGORY_APP_CALCULATOR); break;
            case "MediaSelect": this._actions.launch(Intent.CATEGORY_APP_MUSIC); break;
            case "Power": case "Sleep": this._actions.global("lock"); break;
            default: break; // F1-F4, F6-F24, CapsLock...: Android has no action for them outside a text field
        }
    }

    // Meta (Super, Windows) + a key: the actions of the whole phone, by the letter of the layout of the computer
    private void onMetaKey(String key, String letter, boolean shift, boolean ctrl) {
        switch (key) {
            case "Tab": this._actions.global("recents"); return;
            case "Backspace": case "ArrowLeft": case "Backquote": case "Escape": this._actions.back(); return;
            case "Enter": this._actions.home(); return;
            case "PrintScreen": this._actions.global("screenshot"); return;
            case "ArrowUp": this._actions.global("split"); return;
            default: break;
        }
        if (ctrl && letter.equals("s")) {this._actions.global("screenshot"); return;} // Meta + Ctrl + S, like Android
        switch (letter) {
            case "d": case "h": this._actions.home(); return; // the desktop
            case "n": this._actions.global("notifications"); return;
            case "a": this._actions.global("quickSettings"); return;
            case "l": this._actions.global("lock"); return;
            case "x": this._actions.global("power"); return;
            case "i": this._actions.launch("settings"); return;
            case "s": if (shift) this._actions.global("screenshot"); else this._actions.launch(Intent.CATEGORY_APP_MESSAGING); return;
            case "b": this._actions.launch(Intent.CATEGORY_APP_BROWSER); return;
            case "c": this._actions.launch(Intent.CATEGORY_APP_CONTACTS); return;
            case "e": this._actions.launch(Build.VERSION.SDK_INT >= 29 ? Intent.CATEGORY_APP_FILES : Intent.CATEGORY_APP_EMAIL); return;
            case "k": this._actions.launch(Intent.CATEGORY_APP_CALENDAR); return;
            case "m": this._actions.launch(Intent.CATEGORY_APP_MAPS); return;
            case "p": this._actions.launch(Intent.CATEGORY_APP_MUSIC); return;
            case "u": this._actions.launch(Intent.CATEGORY_APP_CALCULATOR); return;
            default: break;
        }
    }

    // ---------------------------------------------------------------- //
    // Overlay

    private void addOverlay() {
        WindowManager.LayoutParams params = new WindowManager.LayoutParams(
            WindowManager.LayoutParams.MATCH_PARENT, WindowManager.LayoutParams.MATCH_PARENT,
            this._windowType,
            WindowManager.LayoutParams.FLAG_NOT_TOUCHABLE | WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE | WindowManager.LayoutParams.FLAG_LAYOUT_IN_SCREEN | WindowManager.LayoutParams.FLAG_LAYOUT_NO_LIMITS,
            PixelFormat.TRANSLUCENT);
        this._view = new CursorView(this._context);
        this._params = params;
        ((WindowManager) this._context.getSystemService(Context.WINDOW_SERVICE)).addView(this._view, params);
    }

    // A cursor drives this screen from a computer: the screen stays on (no touch on the phone resets its timeout),
    // until KEEP_ON after its last input or its leave, then the timeout of Android again
    private final Runnable _keepOff = () -> this.keepOn(false);

    private void used() {
        this._ui.removeCallbacks(this._keepOff);
        this._ui.postDelayed(this._keepOff, KEEP_ON);
        this.keepOn(true);
    }

    private void keepOn(boolean on) {
        if (on == this._keepOn || this._view == null || this._params == null) return;
        if (on) this._params.flags |= WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON;
        else this._params.flags &= ~WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON;
        try {
            ((WindowManager) this._context.getSystemService(Context.WINDOW_SERVICE)).updateViewLayout(this._view, this._params);
            this._keepOn = on;
        } catch (Exception e) {
            // the overlay is gone
        }
    }

    // The cursors of the members: an arrow in their color and their name
    private final class CursorView extends View {
        private final Paint _fill = new Paint(Paint.ANTI_ALIAS_FLAG);
        private final Paint _border = new Paint(Paint.ANTI_ALIAS_FLAG);
        private final Paint _text = new Paint(Paint.ANTI_ALIAS_FLAG);

        CursorView(Context context) {
            super(context);
            this._border.setStyle(Paint.Style.STROKE);
            this._border.setColor(Color.WHITE);
            this._text.setColor(Color.WHITE);
            this._text.setFakeBoldText(true);
        }

        @Override
        protected void onDraw(Canvas canvas) {
            float s = ScreenLink.this._density;
            int[] location = new int[2];
            this.getLocationOnScreen(location); // the overlay may start below the status bar: cursors are in screen pixels
            this._border.setStrokeWidth(1.5f * s);
            this._text.setTextSize(12 * s);
            long now = SystemClock.uptimeMillis();
            boolean fading = false;
            for (Cursor c: ScreenLink.this._cursors.values()) {
                float alpha = idleAlpha(c, now);
                float x = c.x - location[0];
                float y = c.y - location[1];
                if (alpha <= 0f) continue;
                fading = fading || alpha < 1f;
                Path arrow = new Path();
                arrow.moveTo(x, y);
                arrow.lineTo(x, y + 17 * s);
                arrow.lineTo(x + 4.4f * s, y + 13 * s);
                arrow.lineTo(x + 7.6f * s, y + 19.4f * s);
                arrow.lineTo(x + 10.2f * s, y + 18.2f * s);
                arrow.lineTo(x + 7.1f * s, y + 12 * s);
                arrow.lineTo(x + 12.6f * s, y + 12 * s);
                arrow.close();
                this._fill.setColor(c.color);
                this._fill.setAlpha(Math.round(255 * alpha));
                this._border.setAlpha(Math.round(255 * alpha));
                this._text.setAlpha(Math.round(255 * alpha));
                canvas.drawPath(arrow, this._fill);
                canvas.drawPath(arrow, this._border);
                if (c.down) canvas.drawCircle(x, y, 10 * s, this._border);
                float w = this._text.measureText(c.name) + 14 * s;
                RectF pill = new RectF(x + 14 * s, y + 20 * s, x + 14 * s + w, y + 38 * s);
                canvas.drawRoundRect(pill, 9 * s, 9 * s, this._fill);
                canvas.drawText(c.name, pill.left + 7 * s, pill.bottom - 5 * s, this._text);
            }
            if (fading) this.postInvalidateOnAnimation(); // the next frame of the fade
        }
    }
}
