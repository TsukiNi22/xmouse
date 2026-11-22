/*
 * The screen of the phone in the layout of the network: the Accessibility service links to the device of the pad
 * (role "screen"), tells the size of the screen, draws the cursors of the members that come on it (overlay over the
 * apps) and turns their clicks, drags, scrolls and text into touches / the focused field. Enabled by the person in
 * the Accessibility settings, off at any time.
*/
package io.github.tsukini22.xmouse;

import android.accessibilityservice.AccessibilityService;
import android.accessibilityservice.GestureDescription;
import android.content.Context;
import android.content.SharedPreferences;
import android.content.res.Configuration;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.PixelFormat;
import android.graphics.RectF;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;
import android.util.DisplayMetrics;
import android.view.View;
import android.view.WindowManager;
import android.view.accessibility.AccessibilityEvent;
import android.view.accessibility.AccessibilityNodeInfo;
import android.widget.Toast;

import org.json.JSONObject;

import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

public class ScreenService extends AccessibilityService {
    private static final long TAP_TIME = 300; // ms: a shorter press is a tap, a longer one a long press
    private static final float TAP_MOVE = 12; // px a tap may move
    private static final float WHEEL_PIXELS = 0.8f; // px of swipe per 1/120 of a notch
    private static final long[] RECONNECT = {1000, 2000, 4000, 8000, 15000}; // ms

    private static ScreenService _running; // the activity asks if the screen is lent

    private final Handler _ui = new Handler(Looper.getMainLooper());
    private final Map<String, Cursor> _cursors = new LinkedHashMap<>();
    private WsClient _ws;
    private CursorView _view;
    private float _density = 1;
    private int _tries = 0;
    private boolean _stopped = false;

    // A foreign cursor on this screen
    private static final class Cursor {
        String name = "";
        int color = Color.rgb(61, 220, 151);
        float x;
        float y;
        boolean down = false;
        int button = 0;
        long downAt = 0;
        final List<float[]> path = new ArrayList<>(); // its moves while a button is held (drag)
    }

    static boolean running() {return _running != null;}

    // ---------------------------------------------------------------- //
    // Life cycle

    @Override
    protected void onServiceConnected() {
        _running = this;
        this._stopped = false;
        this._density = this.getResources().getDisplayMetrics().density;
        this.addOverlay();
        this.connect();
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
        this._density = this.getResources().getDisplayMetrics().density;
        this.sendScreen(); // rotated: the layout gets the new size
    }

    @Override
    public void onDestroy() {
        this._stopped = true;
        _running = null;
        if (this._ws != null) this._ws.close();
        if (this._view != null) ((WindowManager) this.getSystemService(Context.WINDOW_SERVICE)).removeView(this._view);
        super.onDestroy();
    }

    // ---------------------------------------------------------------- //
    // Link

    private SharedPreferences prefs() {return this.getSharedPreferences(MainActivity.PREFS, Context.MODE_PRIVATE);}

    private void connect() {
        final String origin = this.prefs().getString("host", "");
        final String fp = this.prefs().getString("fp", "");
        final String token = this.prefs().getString("token", "");
        if (origin.isEmpty() || token.isEmpty()) {
            this._ui.post(() -> Toast.makeText(this, "xmouse: pair the app with a device first", Toast.LENGTH_LONG).show());
            return;
        }
        android.net.Uri uri = android.net.Uri.parse(origin);
        this._ws = new WsClient(uri.getHost(), uri.getPort() > 0 ? uri.getPort() : 47900, "/padws", fp, new WsClient.Listener() {
            @Override public void onOpen() {
                ScreenService.this._tries = 0;
                ScreenService.this.send(json("t", "hello", "token", token, "role", "screen"));
            }

            @Override public void onText(String text) {
                ScreenService.this._ui.post(() -> ScreenService.this.onMessage(text));
            }

            @Override public void onClose(String reason) {
                ScreenService.this._ui.post(() -> {
                    ScreenService.this._cursors.clear();
                    if (ScreenService.this._view != null) ScreenService.this._view.invalidate();
                    if (ScreenService.this._stopped) return;
                    long wait = RECONNECT[Math.min(ScreenService.this._tries++, RECONNECT.length - 1)];
                    ScreenService.this._ui.postDelayed(ScreenService.this::connect, wait);
                });
            }
        });
        this._ws.start();
    }

    private void send(JSONObject o) {
        if (this._ws != null) this._ws.send(o.toString());
    }

    private static JSONObject json(Object... pairs) {
        JSONObject o = new JSONObject();
        try {
            for (int i = 0; i + 1 < pairs.length; i += 2) o.put((String) pairs[i], pairs[i + 1]);
        } catch (Exception e) {
            // keys are strings
        }
        return o;
    }

    // Logical size of the screen (dp): the cursors of the members move at the same speed as on a desktop
    private void sendScreen() {
        DisplayMetrics m = new DisplayMetrics();
        ((WindowManager) this.getSystemService(Context.WINDOW_SERVICE)).getDefaultDisplay().getRealMetrics(m);
        this.send(json("t", "screen", "w", Math.round(m.widthPixels / m.density), "h", Math.round(m.heightPixels / m.density), "scale", (double) m.density, "on", true));
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
            case "welcome":
                this.sendScreen();
                break;
            case "error":
                Toast.makeText(this, "xmouse: " + o.optString("text"), Toast.LENGTH_LONG).show();
                break;
            case "enter": {
                Cursor c = new Cursor();
                c.name = o.optString("name");
                try {c.color = Color.parseColor(o.optString("color", "#3ddc97"));} catch (Exception e) { /* default */ }
                c.x = o.optInt("x") * this._density;
                c.y = o.optInt("y") * this._density;
                this._cursors.put(id, c);
                break;
            }
            case "leave":
                this._cursors.remove(id);
                break;
            case "pointer": {
                Cursor c = this._cursors.get(id);
                if (c == null) break;
                c.x = o.optInt("x") * this._density;
                c.y = o.optInt("y") * this._density;
                if (c.down) c.path.add(new float[]{c.x, c.y});
                break;
            }
            case "button":
                this.onButton(id, o);
                break;
            case "wheel":
                this.onWheel(id, o.optInt("dx"), o.optInt("dy"));
                break;
            case "key":
                if (o.optBoolean("down")) this.onKey(o.optString("code"));
                break;
            case "text":
                this.insert(o.optString("text"));
                break;
            default:
                break;
        }
        if (this._view != null) this._view.invalidate();
    }

    // ---------------------------------------------------------------- //
    // Input of the foreign cursors

    private void onButton(String id, JSONObject o) {
        Cursor c = this._cursors.get(id);
        int b = o.optInt("b");
        boolean down = o.optBoolean("down");
        if (c == null) return;
        if (o.has("x")) {c.x = o.optInt("x") * this._density; c.y = o.optInt("y") * this._density;}
        if (b == 3) {if (down) this.performGlobalAction(GLOBAL_ACTION_BACK); return;} // back button of the mouse
        if (b == 1) return;
        if (down) {
            c.down = true;
            c.button = b;
            c.downAt = SystemClock.uptimeMillis();
            c.path.clear();
            c.path.add(new float[]{c.x, c.y});
            return;
        }
        if (!c.down) return;
        c.down = false;
        long held = Math.max(1, SystemClock.uptimeMillis() - c.downAt);
        float[] first = c.path.get(0);
        boolean moved = Math.hypot(c.x - first[0], c.y - first[1]) > TAP_MOVE;
        Path path = new Path();
        path.moveTo(first[0], first[1]);
        if (moved) {
            for (float[] p: c.path) path.lineTo(p[0], p[1]);
            path.lineTo(c.x, c.y);
        }
        // Right button: a long press (the context menu of Android); a quick left click: a tap; held / moved: a press / a drag
        long duration = c.button == 2 ? 650 : (moved ? Math.min(held, 5000) : (held < TAP_TIME ? 40 : Math.min(held, 5000)));
        this.gesture(path, duration);
    }

    private void onWheel(String id, int dx, int dy) {
        Cursor c = this._cursors.get(id);
        if (c == null) return;
        Path path = new Path();
        path.moveTo(c.x, c.y);
        // Wheel up (dy > 0) shows what is above: the finger goes down
        path.lineTo(c.x + dx * WHEEL_PIXELS * this._density, c.y + dy * WHEEL_PIXELS * this._density);
        this.gesture(path, 120);
    }

    private void onKey(String code) {
        switch (code) {
            case "Escape": case "BrowserBack": this.performGlobalAction(GLOBAL_ACTION_BACK); break;
            case "MetaLeft": case "MetaRight": case "BrowserHome": this.performGlobalAction(GLOBAL_ACTION_HOME); break;
            case "Backspace": this.backspace(); break;
            case "Enter": case "NumpadEnter": this.insert("\n"); break;
            case "Tab": this.insert("\t"); break;
            default: break;
        }
    }

    private void gesture(Path path, long duration) {
        try {
            GestureDescription.StrokeDescription stroke = new GestureDescription.StrokeDescription(path, 0, Math.max(1, duration));
            this.dispatchGesture(new GestureDescription.Builder().addStroke(stroke).build(), null, null);
        } catch (Exception e) {
            // a stroke out of the screen: ignored
        }
    }

    // Text typed by a member: inserted in the focused field at its cursor
    private void insert(String text) {
        AccessibilityNodeInfo node = this.findFocus(AccessibilityNodeInfo.FOCUS_INPUT);
        if (node == null || !node.isEditable()) return;
        CharSequence current = node.getText() == null || node.isShowingHintText() ? "" : node.getText();
        int start = Math.max(0, Math.min(node.getTextSelectionStart(), current.length()));
        int end = Math.max(start, Math.min(node.getTextSelectionEnd(), current.length()));
        if (node.getTextSelectionStart() < 0) {start = current.length(); end = current.length();}
        String next = current.subSequence(0, start) + text + current.subSequence(end, current.length());
        Bundle args = new Bundle();
        args.putCharSequence(AccessibilityNodeInfo.ACTION_ARGUMENT_SET_TEXT_CHARSEQUENCE, next);
        node.performAction(AccessibilityNodeInfo.ACTION_SET_TEXT, args);
        Bundle selection = new Bundle();
        selection.putInt(AccessibilityNodeInfo.ACTION_ARGUMENT_SELECTION_START_INT, start + text.length());
        selection.putInt(AccessibilityNodeInfo.ACTION_ARGUMENT_SELECTION_END_INT, start + text.length());
        node.performAction(AccessibilityNodeInfo.ACTION_SET_SELECTION, selection);
    }

    private void backspace() {
        AccessibilityNodeInfo node = this.findFocus(AccessibilityNodeInfo.FOCUS_INPUT);
        if (node == null || !node.isEditable() || node.getText() == null || node.isShowingHintText()) return;
        CharSequence current = node.getText();
        int start = Math.max(0, Math.min(node.getTextSelectionStart(), current.length()));
        int end = Math.max(start, Math.min(node.getTextSelectionEnd(), current.length()));
        if (node.getTextSelectionStart() < 0) {start = current.length(); end = current.length();}
        if (start == end && start > 0) start -= 1;
        String next = current.subSequence(0, start).toString() + current.subSequence(end, current.length());
        Bundle args = new Bundle();
        args.putCharSequence(AccessibilityNodeInfo.ACTION_ARGUMENT_SET_TEXT_CHARSEQUENCE, next);
        node.performAction(AccessibilityNodeInfo.ACTION_SET_TEXT, args);
        Bundle selection = new Bundle();
        selection.putInt(AccessibilityNodeInfo.ACTION_ARGUMENT_SELECTION_START_INT, start);
        selection.putInt(AccessibilityNodeInfo.ACTION_ARGUMENT_SELECTION_END_INT, start);
        node.performAction(AccessibilityNodeInfo.ACTION_SET_SELECTION, selection);
    }

    // ---------------------------------------------------------------- //
    // Overlay

    private void addOverlay() {
        WindowManager.LayoutParams params = new WindowManager.LayoutParams(
            WindowManager.LayoutParams.MATCH_PARENT, WindowManager.LayoutParams.MATCH_PARENT,
            WindowManager.LayoutParams.TYPE_ACCESSIBILITY_OVERLAY,
            WindowManager.LayoutParams.FLAG_NOT_TOUCHABLE | WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE | WindowManager.LayoutParams.FLAG_LAYOUT_IN_SCREEN | WindowManager.LayoutParams.FLAG_LAYOUT_NO_LIMITS,
            PixelFormat.TRANSLUCENT);
        this._view = new CursorView(this);
        ((WindowManager) this.getSystemService(Context.WINDOW_SERVICE)).addView(this._view, params);
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
            float s = ScreenService.this._density;
            int[] location = new int[2];
            this.getLocationOnScreen(location); // the overlay may start below the status bar: cursors are in screen pixels
            this._border.setStrokeWidth(1.5f * s);
            this._text.setTextSize(12 * s);
            for (Cursor c: ScreenService.this._cursors.values()) {
                float x = c.x - location[0];
                float y = c.y - location[1];
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
                canvas.drawPath(arrow, this._fill);
                canvas.drawPath(arrow, this._border);
                if (c.down) canvas.drawCircle(x, y, 10 * s, this._border);
                float w = this._text.measureText(c.name) + 14 * s;
                RectF pill = new RectF(x + 14 * s, y + 20 * s, x + 14 * s + w, y + 38 * s);
                canvas.drawRoundRect(pill, 9 * s, 9 * s, this._fill);
                canvas.drawText(c.name, pill.left + 7 * s, pill.bottom - 5 * s, this._text);
            }
        }
    }
}
