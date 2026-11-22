/*
 * The screen of the phone in the layout, with touches (Accessibility service): the cursors of the members drawn over
 * the apps (ScreenLink) and their clicks, drags, scrolls and text turned into gestures / the focused field.
 * Enabled by the person in the Accessibility settings, off at any time; it replaces the phantom OverlayService.
*/
package io.github.tsukini22.xmouse;

import android.accessibilityservice.AccessibilityService;
import android.accessibilityservice.GestureDescription;
import android.content.res.Configuration;
import android.graphics.Path;
import android.os.Build;
import android.os.Bundle;
import android.view.WindowManager;
import android.view.accessibility.AccessibilityEvent;
import android.view.accessibility.AccessibilityNodeInfo;

public class ScreenService extends AccessibilityService implements ScreenLink.Actions {
    private static ScreenService _running; // the activity asks if the screen is lent

    private ScreenLink _link;

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
        super.onDestroy();
    }

    // ---------------------------------------------------------------- //
    // Actions

    @Override
    public void gesture(Path path, long duration) {
        try {
            GestureDescription.StrokeDescription stroke = new GestureDescription.StrokeDescription(path, 0, Math.max(1, duration));
            this.dispatchGesture(new GestureDescription.Builder().addStroke(stroke).build(), null, null);
        } catch (Exception e) {
            // a stroke out of the screen: ignored
        }
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

    @Override
    public void back() {this.performGlobalAction(GLOBAL_ACTION_BACK);}

    @Override
    public void home() {this.performGlobalAction(GLOBAL_ACTION_HOME);}

    // Text typed by a member: inserted in the focused field at its cursor
    @Override
    public void insert(String text) {
        AccessibilityNodeInfo node = this.findFocus(AccessibilityNodeInfo.FOCUS_INPUT);
        if (node == null || !node.isEditable()) return;
        CharSequence current = node.getText() == null || node.isShowingHintText() ? "" : node.getText();
        int start = Math.max(0, Math.min(node.getTextSelectionStart(), current.length()));
        int end = Math.max(start, Math.min(node.getTextSelectionEnd(), current.length()));
        if (node.getTextSelectionStart() < 0) {start = current.length(); end = current.length();}
        String next = current.subSequence(0, start) + text + current.subSequence(end, current.length());
        this.setText(node, next, start + text.length());
    }

    @Override
    public void backspace() {
        AccessibilityNodeInfo node = this.findFocus(AccessibilityNodeInfo.FOCUS_INPUT);
        if (node == null || !node.isEditable() || node.getText() == null || node.isShowingHintText()) return;
        CharSequence current = node.getText();
        int start = Math.max(0, Math.min(node.getTextSelectionStart(), current.length()));
        int end = Math.max(start, Math.min(node.getTextSelectionEnd(), current.length()));
        if (node.getTextSelectionStart() < 0) {start = current.length(); end = current.length();}
        if (start == end && start > 0) start -= 1;
        this.setText(node, current.subSequence(0, start).toString() + current.subSequence(end, current.length()), start);
    }

    @Override
    public void paste() {this.focused(AccessibilityNodeInfo.ACTION_PASTE);}

    @Override
    public void copy() {this.focused(AccessibilityNodeInfo.ACTION_COPY);}

    @Override
    public void cut() {this.focused(AccessibilityNodeInfo.ACTION_CUT);}

    @Override
    public void selectAll() {
        AccessibilityNodeInfo node = this.findFocus(AccessibilityNodeInfo.FOCUS_INPUT);
        Bundle selection = new Bundle();
        if (node == null || node.getText() == null) return;
        selection.putInt(AccessibilityNodeInfo.ACTION_ARGUMENT_SELECTION_START_INT, 0);
        selection.putInt(AccessibilityNodeInfo.ACTION_ARGUMENT_SELECTION_END_INT, node.getText().length());
        node.performAction(AccessibilityNodeInfo.ACTION_SET_SELECTION, selection);
    }

    @Override
    public void enter(boolean newLine) {
        AccessibilityNodeInfo node = this.findFocus(AccessibilityNodeInfo.FOCUS_INPUT);
        if (node == null) {this.global("center"); return;} // no text field: the d-pad center (Android 13+) clicks what has the focus
        if (newLine && node.isEditable()) {this.insert("\n"); return;} // Shift + Enter: a new line, never the send of a chat
        // The action of the keyboard of the field (Android 11+): send, search, go, next; else a new line where it fits
        if (Build.VERSION.SDK_INT >= 30 && node.performAction(AccessibilityNodeInfo.AccessibilityAction.ACTION_IME_ENTER.getId())) return;
        if (node.isMultiLine()) this.insert("\n");
    }

    @Override
    public void deleteForward() {
        AccessibilityNodeInfo node = this.findFocus(AccessibilityNodeInfo.FOCUS_INPUT);
        if (node == null || !node.isEditable() || node.getText() == null || node.isShowingHintText()) return;
        CharSequence current = node.getText();
        int start = Math.max(0, Math.min(node.getTextSelectionStart(), current.length()));
        int end = Math.max(start, Math.min(node.getTextSelectionEnd(), current.length()));
        if (node.getTextSelectionStart() < 0) {start = current.length(); end = current.length();}
        if (start == end && end < current.length()) end += 1;
        this.setText(node, current.subSequence(0, start).toString() + current.subSequence(end, current.length()), start);
    }

    @Override
    public void nextField(boolean back) {
        AccessibilityNodeInfo node = this.findFocus(AccessibilityNodeInfo.FOCUS_INPUT);
        AccessibilityNodeInfo next = node == null ? null : node.focusSearch(back ? android.view.View.FOCUS_BACKWARD : android.view.View.FOCUS_FORWARD);
        if (next != null) next.performAction(AccessibilityNodeInfo.ACTION_FOCUS);
    }

    @Override
    public void moveCaret(int delta) {
        AccessibilityNodeInfo node = this.findFocus(AccessibilityNodeInfo.FOCUS_INPUT);
        if (node == null || !node.isEditable()) {this.global(delta < 0 ? "left" : "right"); return;} // no text field: the d-pad
        if (node.getText() == null || node.isShowingHintText()) return;
        int length = node.getText().length();
        int at = node.getTextSelectionStart() < 0 ? length : node.getTextSelectionStart();
        this.caretTo(node, Math.max(0, Math.min(length, at + delta)));
    }

    @Override
    public void caretEdge(boolean end) {
        AccessibilityNodeInfo node = this.findFocus(AccessibilityNodeInfo.FOCUS_INPUT);
        if (node == null || node.getText() == null || node.isShowingHintText()) return;
        this.caretTo(node, end ? node.getText().length() : 0);
    }

    @Override
    public void select(int delta) {
        AccessibilityNodeInfo node = this.findFocus(AccessibilityNodeInfo.FOCUS_INPUT);
        if (node == null || node.getText() == null || node.isShowingHintText()) return;
        int length = node.getText().length();
        int start = node.getTextSelectionStart() < 0 ? length : node.getTextSelectionStart();
        int end = node.getTextSelectionEnd() < 0 ? start : node.getTextSelectionEnd();
        this.selectRange(node, start, Math.max(0, Math.min(length, end + delta)));
    }

    @Override
    public void selectEdge(boolean end) {
        AccessibilityNodeInfo node = this.findFocus(AccessibilityNodeInfo.FOCUS_INPUT);
        if (node == null || node.getText() == null || node.isShowingHintText()) return;
        int length = node.getText().length();
        int start = node.getTextSelectionStart() < 0 ? length : node.getTextSelectionStart();
        this.selectRange(node, start, end ? length : 0);
    }

    // The actions of the whole phone a key may give: the d-pad (Android 13+), the recent apps, the notifications,
    // a screenshot (Android 9+), play / pause of the media (Android 12+)
    @Override
    public void global(String name) {
        int action;
        switch (name) {
            case "recents": action = GLOBAL_ACTION_RECENTS; break;
            case "notifications": action = GLOBAL_ACTION_NOTIFICATIONS; break;
            case "screenshot": if (Build.VERSION.SDK_INT < 28) return; action = GLOBAL_ACTION_TAKE_SCREENSHOT; break;
            case "playPause": if (Build.VERSION.SDK_INT < 31) return; action = GLOBAL_ACTION_KEYCODE_HEADSETHOOK; break;
            case "up": if (Build.VERSION.SDK_INT < 33) return; action = GLOBAL_ACTION_DPAD_UP; break;
            case "down": if (Build.VERSION.SDK_INT < 33) return; action = GLOBAL_ACTION_DPAD_DOWN; break;
            case "left": if (Build.VERSION.SDK_INT < 33) return; action = GLOBAL_ACTION_DPAD_LEFT; break;
            case "right": if (Build.VERSION.SDK_INT < 33) return; action = GLOBAL_ACTION_DPAD_RIGHT; break;
            case "center": if (Build.VERSION.SDK_INT < 33) return; action = GLOBAL_ACTION_DPAD_CENTER; break;
            default: return;
        }
        this.performGlobalAction(action);
    }

    private void selectRange(AccessibilityNodeInfo node, int start, int end) {
        Bundle selection = new Bundle();
        selection.putInt(AccessibilityNodeInfo.ACTION_ARGUMENT_SELECTION_START_INT, start);
        selection.putInt(AccessibilityNodeInfo.ACTION_ARGUMENT_SELECTION_END_INT, end);
        node.performAction(AccessibilityNodeInfo.ACTION_SET_SELECTION, selection);
    }

    private void caretTo(AccessibilityNodeInfo node, int at) {
        Bundle selection = new Bundle();
        selection.putInt(AccessibilityNodeInfo.ACTION_ARGUMENT_SELECTION_START_INT, at);
        selection.putInt(AccessibilityNodeInfo.ACTION_ARGUMENT_SELECTION_END_INT, at);
        node.performAction(AccessibilityNodeInfo.ACTION_SET_SELECTION, selection);
    }

    // An action on the focused field (paste, copy, cut)
    private void focused(int action) {
        AccessibilityNodeInfo node = this.findFocus(AccessibilityNodeInfo.FOCUS_INPUT);
        if (node != null) node.performAction(action);
    }

    private void setText(AccessibilityNodeInfo node, String text, int cursor) {
        Bundle args = new Bundle();
        args.putCharSequence(AccessibilityNodeInfo.ACTION_ARGUMENT_SET_TEXT_CHARSEQUENCE, text);
        node.performAction(AccessibilityNodeInfo.ACTION_SET_TEXT, args);
        Bundle selection = new Bundle();
        selection.putInt(AccessibilityNodeInfo.ACTION_ARGUMENT_SELECTION_START_INT, cursor);
        selection.putInt(AccessibilityNodeInfo.ACTION_ARGUMENT_SELECTION_END_INT, cursor);
        node.performAction(AccessibilityNodeInfo.ACTION_SET_SELECTION, selection);
    }
}
