/*
 * Accept / Refuse of the notification of a device that wants to join the network of the phone
*/
package io.github.tsukini22.xmouse;

import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;

import io.github.tsukini22.xmouse.core.Node;

public class ApprovalReceiver extends BroadcastReceiver {
    static PendingIntent intent(Context context, String pending, boolean accepted) {
        Intent intent = new Intent(context, ApprovalReceiver.class).putExtra("pending", pending).putExtra("accepted", accepted);
        return PendingIntent.getBroadcast(context, (pending + accepted).hashCode(), intent, PendingIntent.FLAG_IMMUTABLE | PendingIntent.FLAG_UPDATE_CURRENT);
    }

    @Override
    public void onReceive(Context context, Intent intent) {
        Node node = NodeService.node();
        String pending = intent.getStringExtra("pending");
        if (node == null || pending == null) return;
        node.approve(pending, intent.getBooleanExtra("accepted", false));
        NodeService.cancel(context, pending);
    }
}
