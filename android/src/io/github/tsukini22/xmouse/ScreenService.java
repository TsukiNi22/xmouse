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
