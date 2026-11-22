/*
 * The process of the app: a crash is written down before Android ends the app (shown at the next launch with its
 * trace), and the lent screen and the network of the phone are turned off so that a crash cannot come back at every
 * launch (the person turns them on again from the home)
*/
package io.github.tsukini22.xmouse;

import android.app.Application;
import android.os.Build;

import java.io.File;
import java.io.FileOutputStream;
import java.io.PrintWriter;
import java.io.StringWriter;
import java.nio.charset.StandardCharsets;

public class App extends Application {
    static final String CRASH = "crash.txt";
    private static final int MAX_TRACE = 12000; // characters kept

    @Override
    public void onCreate() {
        super.onCreate();
        final Thread.UncaughtExceptionHandler previous = Thread.getDefaultUncaughtExceptionHandler();
        Thread.setDefaultUncaughtExceptionHandler((thread, error) -> {
            try {
                StringWriter trace = new StringWriter();
                error.printStackTrace(new PrintWriter(trace));
                String text = "xmouse " + MainActivity.versionOf(this) + ", Android " + Build.VERSION.RELEASE + " (API " + Build.VERSION.SDK_INT + "), "
                    + Build.MANUFACTURER + " " + Build.MODEL + ", thread " + thread.getName() + "\n\n" + trace;
                if (text.length() > MAX_TRACE) text = text.substring(0, MAX_TRACE);
                try (FileOutputStream out = new FileOutputStream(new File(this.getFilesDir(), CRASH))) {out.write(text.getBytes(StandardCharsets.UTF_8));}
                this.getSharedPreferences(MainActivity.PREFS, MODE_PRIVATE).edit().putBoolean("lend", false).putBoolean("online", false).commit();
            } catch (Throwable ignored) {
                // the crash goes on as usual
            }
            if (previous != null) previous.uncaughtException(thread, error);
            else System.exit(2);
        });
    }
}
