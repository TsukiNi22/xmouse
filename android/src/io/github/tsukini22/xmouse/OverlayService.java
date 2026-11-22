/*
 * The screen of the phone in the layout without Accessibility (phantom): a foreground service draws the cursors of the
 * members over the apps with their name ("Display over other apps"), nothing is touched. Started by the app once the
 * person lends the screen; the Accessibility service (ScreenService) replaces it when it is turned on.
*/
package io.github.tsukini22.xmouse;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ServiceInfo;
import android.content.res.Configuration;
import android.os.Build;
import android.os.IBinder;
import android.provider.Settings;
import android.view.WindowManager;

public class OverlayService extends Service {
    private static final String CHANNEL = "screen";
    private static final int NOTIFICATION = 1;
    private static OverlayService _running;

    private ScreenLink _link;

    static boolean running() {return _running != null;}

    static void stopIfRunning() {
        if (_running != null) _running.stopSelf();
    }

    // Lends the screen (phantom) when the overlay is allowed and the Accessibility service is not already lending it
    static boolean start(Context context) {
        if (ScreenService.running() || _running != null) return true;
        if (!Settings.canDrawOverlays(context)) return false;
        Intent intent = new Intent(context, OverlayService.class);
        if (Build.VERSION.SDK_INT >= 26) context.startForegroundService(intent);
        else context.startService(intent);
        return true;
    }

    static void stop(Context context) {
        context.stopService(new Intent(context, OverlayService.class));
    }

    @Override
    public IBinder onBind(Intent intent) {return null;}

    @Override
    public int onStartCommand(Intent intent, int flags, int id) {
        if (_running != null) return START_STICKY;
        if (ScreenService.running() || !Settings.canDrawOverlays(this)) {this.stopSelf(); return START_NOT_STICKY;}
        _running = this;
        this.foreground();
        this._link = new ScreenLink(this, null, WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY);
        this._link.start();
        return START_STICKY;
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

    // The notification of a foreground service: the screen is lent (a tap opens the app)
    @SuppressWarnings("deprecation")
    private void foreground() {
        Notification.Builder builder;
        PendingIntent open = PendingIntent.getActivity(this, 0, new Intent(this, MainActivity.class), PendingIntent.FLAG_IMMUTABLE);
        if (Build.VERSION.SDK_INT >= 26) {
            NotificationManager manager = (NotificationManager) this.getSystemService(Context.NOTIFICATION_SERVICE);
            manager.createNotificationChannel(new NotificationChannel(CHANNEL, "Screen lent to the network", NotificationManager.IMPORTANCE_LOW));
            builder = new Notification.Builder(this, CHANNEL);
        } else builder = new Notification.Builder(this);
        Notification notification = builder
            .setSmallIcon(android.R.drawable.ic_menu_mylocation)
            .setContentTitle("xmouse: this screen is in the layout")
            .setContentText("The cursors of the network are shown over the apps")
            .setContentIntent(open)
            .setOngoing(true)
            .build();
        if (Build.VERSION.SDK_INT >= 34) this.startForeground(NOTIFICATION, notification, ServiceInfo.FOREGROUND_SERVICE_TYPE_SPECIAL_USE);
        else this.startForeground(NOTIFICATION, notification);
    }
}
