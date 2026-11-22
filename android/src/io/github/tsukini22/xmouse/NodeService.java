/*
 * The phone as a member of a network, kept alive by a foreground service: the node (core.Node) with its identity
 * (Android KeyStore), its files (network, state), the events of the node given to the app (the screen lent, the
 * touchpad page, the network screen, the approvals, the clipboard, the links)
*/
package io.github.tsukini22.xmouse;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ServiceInfo;
import android.net.Uri;
import android.os.Build;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.widget.Toast;

import io.github.tsukini22.xmouse.core.Network;
import io.github.tsukini22.xmouse.core.Node;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.nio.charset.StandardCharsets;
import java.util.Map;
import java.util.concurrent.CopyOnWriteArrayList;

public class NodeService extends Service implements Node.Listener {
    private static final String CHANNEL = "network";
    private static final int NOTIFICATION = 2;
    private static final int PORT = 47900;
    private static volatile Node _node;
    private static volatile NodeService _running;
    private static final CopyOnWriteArrayList<Node.Listener> _listeners = new CopyOnWriteArrayList<>(); // the screens of the app

    private final Handler _ui = new Handler(Looper.getMainLooper());

    static Node node() {return _node;}

    static boolean running() {return _running != null;}

    static void listen(Node.Listener listener) {_listeners.addIfAbsent(listener);}

    static void unlisten(Node.Listener listener) {_listeners.remove(listener);}

    static void start(Context context) {
        Intent intent = new Intent(context, NodeService.class);
        if (Build.VERSION.SDK_INT >= 26) context.startForegroundService(intent);
        else context.startService(intent);
    }

    static void stop(Context context) {context.stopService(new Intent(context, NodeService.class));}

    // A file of the private folder of the app (0600 by Android)
    private Network.Store store(String name) {
        File file = new File(this.getFilesDir(), name);
        return new Network.Store() {
            @Override public String load() {
                try (FileInputStream in = new FileInputStream(file)) {
                    byte[] data = new byte[(int) file.length()];
                    int n = in.read(data);
                    return new String(data, 0, Math.max(0, n), StandardCharsets.UTF_8);
                } catch (Exception e) {
                    return "";
                }
            }

            @Override public void save(String json) {
                File tmp = new File(file.getPath() + ".tmp");
                try (FileOutputStream out = new FileOutputStream(tmp)) {
                    out.write(json.getBytes(StandardCharsets.UTF_8));
                    out.getFD().sync();
                } catch (Exception e) {
                    return;
                }
                tmp.renameTo(file);
            }
        };
    }

    @Override
    public IBinder onBind(Intent intent) {return null;}

    @Override
    public int onStartCommand(Intent intent, int flags, int id) {
        if (_running != null) return START_STICKY;
        _running = this;
        this.foreground("starting...");
        try {
            Identity identity = Identity.load();
            String name = this.getSharedPreferences(MainActivity.PREFS, MODE_PRIVATE).getString("name", Build.MODEL);
            Node node = new Node(this.store("network.json"), this.store("state.json"), identity.server, identity.fingerprint, name, MainActivity.versionOf(this));
            node.start(PORT, this);
            _node = node;
        } catch (Exception e) {
            Toast.makeText(this, "xmouse cannot start its network: " + e.getMessage(), Toast.LENGTH_LONG).show();
            this.stopSelf();
            return START_NOT_STICKY;
        }
        return START_STICKY;
    }

    @Override
    public void onDestroy() {
        if (_node != null) _node.stop();
        _node = null;
        _running = null;
        super.onDestroy();
    }

    @SuppressWarnings("deprecation")
    private void foreground(String text) {
        Notification.Builder builder;
        PendingIntent open = PendingIntent.getActivity(this, 1, new Intent(this, MainActivity.class), PendingIntent.FLAG_IMMUTABLE);
        if (Build.VERSION.SDK_INT >= 26) {
            NotificationManager manager = (NotificationManager) this.getSystemService(Context.NOTIFICATION_SERVICE);
            manager.createNotificationChannel(new NotificationChannel(CHANNEL, "Network of the phone", NotificationManager.IMPORTANCE_LOW));
            builder = new Notification.Builder(this, CHANNEL);
        } else builder = new Notification.Builder(this);
        Notification notification = builder
            .setSmallIcon(android.R.drawable.stat_sys_data_bluetooth)
            .setContentTitle("xmouse: this phone is a member")
            .setContentText(text)
            .setContentIntent(open)
            .setOngoing(true)
            .build();
        if (Build.VERSION.SDK_INT >= 34) this.startForeground(NOTIFICATION, notification, ServiceInfo.FOREGROUND_SERVICE_TYPE_SPECIAL_USE);
        else this.startForeground(NOTIFICATION, notification);
    }

    // ---------------------------------------------------------------- //
    // Node.Listener (the loop of the node -> the main thread)

    @Override
    public void changed() {
        Node node = _node;
        String text = node == null || !node.joined() ? "in no network yet" : "network " + node.networkName();
        this._ui.post(() -> {
            this.foreground(text);
            for (Node.Listener l: _listeners) l.changed();
        });
    }

    // A device gave the right code: a notification with Accept / Refuse (the app may be in the background), and a
    // dialog when a screen of the app is open; refused by the node after 30 s
    @Override
    public void joinRequest(String pending, String name, String os) {
        this._ui.post(() -> {
            this.notify(pending.hashCode(), "A device wants to join", name + " (" + os + ") gave the right code", null,
                new String[]{"Accept", "Refuse"}, new PendingIntent[]{ApprovalReceiver.intent(this, pending, true), ApprovalReceiver.intent(this, pending, false)});
            for (Node.Listener l: _listeners) l.joinRequest(pending, name, os);
        });
    }

    @SuppressWarnings("deprecation")
    private void notify(int id, String title, String text, PendingIntent open, String[] actions, PendingIntent[] intents) {
        NotificationManager manager = (NotificationManager) this.getSystemService(Context.NOTIFICATION_SERVICE);
        Notification.Builder builder;
        if (Build.VERSION.SDK_INT >= 26) {
            manager.createNotificationChannel(new NotificationChannel("events", "Requests and links", NotificationManager.IMPORTANCE_HIGH));
            builder = new Notification.Builder(this, "events");
        } else builder = new Notification.Builder(this);
        builder.setSmallIcon(android.R.drawable.ic_dialog_info).setContentTitle(title).setContentText(text).setAutoCancel(true);
        if (open != null) builder.setContentIntent(open);
        for (int i = 0; i < actions.length; i++) builder.addAction(new Notification.Action.Builder(null, actions[i], intents[i]).build());
        manager.notify(id, builder.build());
    }

    static void cancel(Context context, String pending) {
        ((NotificationManager) context.getSystemService(Context.NOTIFICATION_SERVICE)).cancel(pending.hashCode());
    }

    @Override
    public void joinResult(boolean ok, String message) {
        this._ui.post(() -> {
            Toast.makeText(this, message, Toast.LENGTH_LONG).show();
            for (Node.Listener l: _listeners) l.joinResult(ok, message);
        });
    }

    @Override
    public void notice(String text) {this._ui.post(() -> Toast.makeText(this, text, Toast.LENGTH_SHORT).show());}

    @Override
    public void screen(Map<String, Object> event) {
        this._ui.post(() -> {
            ScreenLink link = ScreenLink.active();
            if (link != null) link.onLocalEvent(event);
        });
    }

    @Override
    public void map(Map<String, Object> map) {
        for (Node.Listener l: _listeners) l.map(map); // the touchpad page posts it to its WebView
    }

    @Override
    public void clipboard(String text) {
        this._ui.post(() -> {
            ClipboardManager clipboard = (ClipboardManager) this.getSystemService(Context.CLIPBOARD_SERVICE);
            if (clipboard != null) clipboard.setPrimaryClip(ClipData.newPlainText("xmouse", text));
        });
    }

    // A link of a member: a notification, opened when the person taps it (never by itself)
    @Override
    public void link(String from, String url) {
        this._ui.post(() -> {
            if (!url.startsWith("https://") && !url.startsWith("http://")) return;
            PendingIntent open = PendingIntent.getActivity(this, url.hashCode(), new Intent(Intent.ACTION_VIEW, Uri.parse(url)), PendingIntent.FLAG_IMMUTABLE);
            this.notify(url.hashCode(), from + " sent a link", url, open, new String[0], new PendingIntent[0]);
        });
    }
}
