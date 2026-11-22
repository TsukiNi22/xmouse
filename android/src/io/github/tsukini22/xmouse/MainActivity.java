/*
 * xmouse android application: the /pad page of a device of the network in a WebView, with
 *  - the device taken from its QR code / link (https://host:port/pad?code=..&fp=..): the fp pins its certificate, every
 *    TLS error is accepted ONLY for that exact certificate
 *  - a bridge for the page (XmouseAndroid): the token of the pad shared with the screen service, the screen lent
 *  - updates: /app/version of the device compared with this app, /app/xmouse.apk installed with the PackageInstaller
 *  - a QR code scanner (assets/scan.html on a local https origin, the camera given to it only)
*/
package io.github.tsukini22.xmouse;

import android.app.Activity;
import android.app.AlertDialog;
import android.app.PendingIntent;
import android.Manifest;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.PackageInfo;
import android.content.pm.PackageInstaller;
import android.content.pm.PackageManager;
import android.graphics.Color;
import android.graphics.Typeface;
import android.net.Uri;
import android.net.http.SslCertificate;
import android.net.http.SslError;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.provider.Settings;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.ViewGroup;
import android.webkit.JavascriptInterface;
import android.webkit.PermissionRequest;
import android.webkit.SslErrorHandler;
import android.webkit.WebChromeClient;
import android.webkit.WebResourceRequest;
import android.webkit.WebResourceResponse;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.widget.Button;
import android.widget.EditText;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.TextView;
import android.widget.Toast;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.security.MessageDigest;

import javax.net.ssl.HttpsURLConnection;

public class MainActivity extends Activity {
    // ---------------------------------------------------------------- //
    // Constants

    static final String PREFS = "xmouse";
    private static final int DEFAULT_PORT = 47900;
    private static final int CAMERA_REQUEST = 7;
    private static final String SCANNER = "https://appassets.androidplatform.net"; // local origin of the scanner page (assets)

    /* palette (dark tokens of the panel) */
    private static final int BG = Color.parseColor("#0f1214");
    private static final int TEXT = Color.parseColor("#e4e7e9");
    private static final int MUTED = Color.parseColor("#98a2a9");
    private static final int ACCENT = Color.parseColor("#3ddc97");
    private static final int WARN = Color.parseColor("#f0a742");

    // ---------------------------------------------------------------- //
    // State

    private final Handler _ui = new Handler(Looper.getMainLooper());
    private SharedPreferences _prefs;
    private FrameLayout _root;
    private WebView _web;
    private String _pendingCode = ""; // code of a link, given to the pad page once
    private long _lastUpdateCheck = 0; // ms: checked at every launch / return to the app (10 min at most between two)

    // ---------------------------------------------------------------- //
    // Life cycle

    @Override
    protected void onCreate(Bundle state) {
        super.onCreate(state);
        this._prefs = this.getSharedPreferences(PREFS, MODE_PRIVATE);
        this._root = new FrameLayout(this);
        this._root.setBackgroundColor(BG);
        this.setContentView(this._root);
        if (!this.handleIntent(this.getIntent())) this.startOrSetup();
    }

    @Override
    protected void onNewIntent(Intent intent) {
        super.onNewIntent(intent);
        this.handleIntent(intent);
    }

    @Override
    public void onBackPressed() {
        if (this._web != null && this._web.getUrl() != null && this._web.getUrl().startsWith(SCANNER)) {this.startOrSetup(); return;}
        this.moveTaskToBack(true); // the pad stays connected
    }

    private String host() {return this._prefs.getString("host", "");}

    private String pin() {return this._prefs.getString("fp", "");}

    private void startOrSetup() {
        if (this.host().isEmpty() || this.pin().isEmpty()) this.showSetup("");
        else this.openPad();
    }

    // A link shared to the app, or typed / scanned: https://host:port/pad?code=..&fp=..
    private boolean handleIntent(Intent intent) {
        if (intent == null || !Intent.ACTION_SEND.equals(intent.getAction())) return false;
        String text = intent.getStringExtra(Intent.EXTRA_TEXT);
        return text != null && this.useLink(text.trim());
    }

    private boolean useLink(String link) {
        Uri uri = Uri.parse(link);
        String fp = uri.getQueryParameter("fp");
        if (!"https".equalsIgnoreCase(uri.getScheme()) || uri.getHost() == null || fp == null || fp.length() != 64) {
            this.showSetup(this.tr("This is not the link of an xmouse device (its QR code holds its identity).", "Ce n'est pas le lien d'un appareil xmouse (son QR code contient son identité)."));
            return false;
        }
        String origin = "https://" + uri.getHost() + ":" + (uri.getPort() > 0 ? uri.getPort() : DEFAULT_PORT);
        if (!origin.equals(this.host()) || !fp.equalsIgnoreCase(this.pin())) this._prefs.edit().putString("token", "").apply(); // another device: pair again
        this._prefs.edit().putString("host", origin).putString("fp", fp.toLowerCase()).apply();
        String code = uri.getQueryParameter("code");
        this._pendingCode = code == null ? "" : code;
        this.openPad();
        return true;
    }

    // ---------------------------------------------------------------- //
    // Screens

    private int dp(int v) {return (int) TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, v, this.getResources().getDisplayMetrics());}

    private boolean french() {return java.util.Locale.getDefault().getLanguage().equals("fr");}

    private String tr(String en, String fr) {return this.french() ? fr : en;}

    private void clearRoot() {
        if (this._web != null) {
            this._root.removeView(this._web);
            this._web.destroy();
            this._web = null;
        }
        this._root.removeAllViews();
    }

    private TextView text(String value, int size, int color) {
        TextView t = new TextView(this);
        t.setText(value);
        t.setTextSize(size);
        t.setTextColor(color);
        t.setPadding(0, this.dp(4), 0, this.dp(4));
        return t;
    }

    private void showSetup(String error) {
        this.clearRoot();
        LinearLayout box = new LinearLayout(this);
        box.setOrientation(LinearLayout.VERTICAL);
        box.setPadding(this.dp(24), this.dp(48), this.dp(24), this.dp(24));
        TextView title = this.text("xmouse", 28, TEXT);
        title.setTypeface(Typeface.DEFAULT_BOLD);
        box.addView(title);
        box.addView(this.text(this.tr("A touchpad and a keyboard for the devices of your network, and this screen in their layout.", "Un touchpad et un clavier pour les appareils de votre réseau, et cet écran dans leur disposition."), 15, MUTED));
        box.addView(this.text(this.tr("On the computer: xmouse > Network > Pair a phone. Scan its QR code.", "Sur l'ordinateur : xmouse > Réseau > Associer un téléphone. Scannez son QR code."), 15, TEXT));
        Button scan = new Button(this);
        scan.setText(this.tr("Scan the QR code", "Scanner le QR code"));
        scan.setOnClickListener(v -> this.openScanner());
        box.addView(scan);
        final EditText link = new EditText(this);
        link.setHint("https://192.168.1.20:47900/pad?code=...&fp=...");
        link.setTextColor(TEXT);
        link.setHintTextColor(MUTED);
        link.setSingleLine(true);
        box.addView(link);
        Button use = new Button(this);
        use.setText(this.tr("Use this link", "Utiliser ce lien"));
        use.setOnClickListener(v -> this.useLink(link.getText().toString().trim()));
        box.addView(use);
        if (!error.isEmpty()) box.addView(this.text(error, 14, WARN));
        if (!this.host().isEmpty()) {
            Button back = new Button(this);
            back.setText(this.tr("Back to ", "Revenir à ") + Uri.parse(this.host()).getHost());
            back.setOnClickListener(v -> this.openPad());
            box.addView(back);
        }
        this._root.addView(box, new FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT, Gravity.CENTER_VERTICAL));
    }

    // ---------------------------------------------------------------- //
    // Pad

    private static String fingerprintOf(SslCertificate certificate) {
        try {
            if (Build.VERSION.SDK_INT >= 29 && certificate.getX509Certificate() != null) return Pinned.sha256(certificate.getX509Certificate().getEncoded());
            Bundle saved = SslCertificate.saveState(certificate);
            byte[] der = saved == null ? null : saved.getByteArray("x509-certificate");
            return der == null ? "" : Pinned.sha256(der);
        } catch (Exception e) {
            return "";
        }
    }

    private WebView newWeb() {
        WebView web = new WebView(this);
        web.setBackgroundColor(BG);
        WebSettings s = web.getSettings();
        s.setJavaScriptEnabled(true);
        s.setDomStorageEnabled(true);
        s.setAllowFileAccess(false);
        s.setAllowContentAccess(false);
        s.setSupportZoom(false);
        s.setUserAgentString(s.getUserAgentString() + " xmouse-app/" + this.version());
        return web;
    }

    private void openPad() {
        this.clearRoot();
        final String origin = this.host();
        final String fp = this.pin();
        WebView web = this.newWeb();
        this._web = web;
        web.addJavascriptInterface(new Bridge(), "XmouseAndroid");
        web.setWebViewClient(new WebViewClient() {
            @Override
            public void onReceivedSslError(WebView view, SslErrorHandler handler, SslError error) {
                // Self-signed device: accepted ONLY for the certificate of its QR code
                String seen = fingerprintOf(error.getCertificate());
                if (!seen.isEmpty() && MessageDigest.isEqual(seen.getBytes(), fp.getBytes())) {handler.proceed(); return;}
                handler.cancel();
                MainActivity.this._ui.post(() -> MainActivity.this.showSetup(MainActivity.this.tr("This device does not have the identity of its QR code: scan it again.", "Cet appareil n'a pas l'identité de son QR code : scannez-le à nouveau.")));
            }

            @Override
            public boolean shouldOverrideUrlLoading(WebView view, WebResourceRequest request) {
                Uri uri = request.getUrl();
                if (origin.equals("https://" + uri.getHost() + ":" + (uri.getPort() > 0 ? uri.getPort() : 443))) {
                    if (uri.getPath() != null && uri.getPath().startsWith("/app/xmouse.apk")) {MainActivity.this.checkUpdate(true); return true;}
                    return false;
                }
                try {MainActivity.this.startActivity(new Intent(Intent.ACTION_VIEW, uri));} catch (Exception e) { /* no browser */ }
                return true;
            }

            @Override
            public void onReceivedError(WebView view, WebResourceRequest request, android.webkit.WebResourceError error) {
                if (!request.isForMainFrame()) return;
                MainActivity.this._ui.post(() -> MainActivity.this.showSetup(MainActivity.this.tr("Device unreachable: ", "Appareil injoignable : ") + error.getDescription()));
            }
        });
        this._root.addView(web, new FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
        web.loadUrl(origin + "/pad" + (this._pendingCode.isEmpty() ? "" : "?code=" + Uri.encode(this._pendingCode)));
        this._pendingCode = "";
        this.autoUpdate();
        this.resumeScreen();
    }

    @Override
    protected void onResume() {
        super.onResume();
        if (this._web != null && !this.host().isEmpty()) {
            this.autoUpdate();
            this.resumeScreen(); // back from the overlay permission: the screen is lent at once
        }
    }

    // Every launch / return to the app: the device is asked for a newer app (a dialog only when there is one)
    private void autoUpdate() {
        long now = System.currentTimeMillis();
        if (now - this._lastUpdateCheck < 10 * 60 * 1000) return;
        this._lastUpdateCheck = now;
        this.checkUpdate(false);
    }

    // The screen stays lent (phantom) as long as the person wants it and the overlay is allowed
    private void resumeScreen() {
        if (this._prefs.getBoolean("lend", false) && !this._prefs.getString("token", "").isEmpty()) OverlayService.start(this);
    }

    private void lendPhantom() {
        this._prefs.edit().putBoolean("lend", true).apply();
        if (OverlayService.start(this)) {
            Toast.makeText(this, this.tr("This screen is in the layout of the network", "Cet écran est dans la disposition du réseau"), Toast.LENGTH_SHORT).show();
            return;
        }
        // Allowed once in the settings ("Display over other apps"), then the screen is lent when the app comes back
        this.startActivity(new Intent(Settings.ACTION_MANAGE_OVERLAY_PERMISSION, Uri.parse("package:" + this.getPackageName())));
    }

    // What the pad page may ask the app (its token, the screen lent to the network)
    private final class Bridge {
        @JavascriptInterface public String token() {return MainActivity.this._prefs.getString("token", "");}

        @JavascriptInterface public void saveToken(String token) {MainActivity.this._prefs.edit().putString("token", token == null ? "" : token).apply();}

        @JavascriptInterface public void paired(String token, String fp) {this.saveToken(token);}

        @JavascriptInterface public String version() {return MainActivity.this.version();}

        @JavascriptInterface public boolean screenLent() {return ScreenService.running() || OverlayService.running();}

        // Phantom (the cursors drawn, "Display over other apps") or touch (Accessibility: their clicks become touches)
        @JavascriptInterface public void lendScreen() {
            MainActivity.this._ui.post(() -> {
                AlertDialog.Builder dialog = new AlertDialog.Builder(MainActivity.this)
                    .setTitle(MainActivity.this.tr("This screen in the layout", "Cet écran dans la disposition"))
                    .setMessage(MainActivity.this.tr(
                        "Show the cursors: the cursors of the network come on this phone and are drawn over the apps with their name (\"Display over other apps\").\n\nLet them touch: their clicks, drags, scrolls and text also act here (the Accessibility service \"xmouse screen\"; after an install from the browser, Settings > Apps > xmouse > Allow restricted settings first).",
                        "Afficher les curseurs : les curseurs du réseau viennent sur ce téléphone, dessinés par-dessus les applis avec leur nom (« Afficher par-dessus les autres applis »).\n\nLes laisser toucher : leurs clics, glisser, défilements et textes agissent aussi ici (le service d'Accessibilité « xmouse screen » ; après une installation depuis le navigateur : Paramètres > Applis > xmouse > Autoriser les paramètres restreints d'abord)."))
                    .setPositiveButton(MainActivity.this.tr("Show the cursors", "Afficher les curseurs"), (d, w) -> MainActivity.this.lendPhantom())
                    .setNeutralButton(MainActivity.this.tr("Let them touch", "Les laisser toucher"), (d, w) -> MainActivity.this.startActivity(new Intent(Settings.ACTION_ACCESSIBILITY_SETTINGS)));
                if (OverlayService.running()) dialog.setNegativeButton(MainActivity.this.tr("Stop", "Arrêter"), (d, w) -> {
                    MainActivity.this._prefs.edit().putBoolean("lend", false).apply();
                    OverlayService.stop(MainActivity.this);
                });
                dialog.show();
            });
        }

        @JavascriptInterface public void checkUpdate() {MainActivity.this._ui.post(() -> MainActivity.this.checkUpdate(true));}

        @JavascriptInterface public void changeDevice() {MainActivity.this._ui.post(() -> MainActivity.this.showSetup(""));}
    }

    // ---------------------------------------------------------------- //
    // Scanner

    private void openScanner() {
        if (Build.VERSION.SDK_INT >= 23 && this.checkSelfPermission(Manifest.permission.CAMERA) != PackageManager.PERMISSION_GRANTED) {
            this.requestPermissions(new String[]{Manifest.permission.CAMERA}, CAMERA_REQUEST);
            return;
        }
        this.clearRoot();
        WebView web = this.newWeb();
        this._web = web;
        web.setWebChromeClient(new WebChromeClient() {
            @Override
            public void onPermissionRequest(PermissionRequest request) {
                // The camera for the scanner of the app only (its local origin)
                if (request.getOrigin() != null && request.getOrigin().toString().startsWith(SCANNER)) request.grant(new String[]{PermissionRequest.RESOURCE_VIDEO_CAPTURE});
                else request.deny();
            }
        });
        web.setWebViewClient(new WebViewClient() {
            @Override
            public WebResourceResponse shouldInterceptRequest(WebView view, WebResourceRequest request) {
                Uri uri = request.getUrl();
                if (!uri.toString().startsWith(SCANNER)) return null;
                String name = uri.getPath() == null || uri.getPath().equals("/") ? "scan.html" : uri.getPath().substring(1);
                try {
                    InputStream in = MainActivity.this.getAssets().open(name);
                    return new WebResourceResponse(name.endsWith(".js") ? "text/javascript" : "text/html", "UTF-8", in);
                } catch (Exception e) {
                    return new WebResourceResponse("text/plain", "UTF-8", 404, "Not found", null, null);
                }
            }

            @Override
            public boolean shouldOverrideUrlLoading(WebView view, WebResourceRequest request) {
                String url = request.getUrl().toString();
                if (url.startsWith(SCANNER)) return false;
                if (url.startsWith("xmouse-cancel:")) {MainActivity.this._ui.post(MainActivity.this::startOrSetup); return true;}
                MainActivity.this._ui.post(() -> MainActivity.this.useLink(url)); // the link read in the QR code
                return true;
            }
        });
        this._root.addView(web, new FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
        web.loadUrl(SCANNER + "/scan.html" + (this.french() ? "?lang=fr" : ""));
    }

    @Override
    public void onRequestPermissionsResult(int request, String[] permissions, int[] results) {
        if (request != CAMERA_REQUEST) return;
        if (results.length > 0 && results[0] == PackageManager.PERMISSION_GRANTED) this.openScanner();
        else this.showSetup(this.tr("No camera: paste the link of the device instead.", "Pas de caméra : collez plutôt le lien de l'appareil."));
    }

    // ---------------------------------------------------------------- //
    // Updates

    String version() {
        try {
            PackageInfo info = this.getPackageManager().getPackageInfo(this.getPackageName(), 0);
            return info.versionName == null ? "0" : info.versionName;
        } catch (Exception e) {
            return "0";
        }
    }

    // x.y.z.n compared number by number
    private static int compareVersions(String a, String b) {
        String[] x = a.split("\\.");
        String[] y = b.split("\\.");
        for (int i = 0; i < Math.max(x.length, y.length); ++i) {
            long p = 0;
            long q = 0;
            try {p = i < x.length ? Long.parseLong(x[i].replaceAll("[^0-9]", "")) : 0;} catch (Exception e) { /* 0 */ }
            try {q = i < y.length ? Long.parseLong(y[i].replaceAll("[^0-9]", "")) : 0;} catch (Exception e) { /* 0 */ }
            if (p != q) return p < q ? -1 : 1;
        }
        return 0;
    }

    private static byte[] readAll(InputStream in, int max) throws Exception {
        ByteArrayOutputStream out = new ByteArrayOutputStream();
        byte[] buffer = new byte[65536];
        for (int n; (n = in.read(buffer)) > 0;) {
            out.write(buffer, 0, n);
            if (out.size() > max) throw new Exception("too big");
        }
        return out.toByteArray();
    }

    private void checkUpdate(boolean asked) {
        final String origin = this.host();
        final String fp = this.pin();
        new Thread(() -> {
            String remote = "";
            try {
                HttpsURLConnection c = Pinned.open(origin + "/app/version", fp);
                if (c.getResponseCode() == 200) {
                    try (InputStream in = c.getInputStream()) {remote = new String(readAll(in, 64), "UTF-8").trim();}
                }
                c.disconnect();
            } catch (Exception e) {
                remote = "";
            }
            final String latest = remote;
            this._ui.post(() -> {
                if (latest.isEmpty() || compareVersions(latest, this.version()) <= 0) {
                    if (asked) Toast.makeText(this, this.tr("xmouse is up to date (", "xmouse est à jour (") + this.version() + ")", Toast.LENGTH_SHORT).show();
                    return;
                }
                new AlertDialog.Builder(this)
                    .setTitle(this.tr("Update available", "Mise à jour disponible"))
                    .setMessage(this.version() + " -> " + latest + "\n\n" + this.tr("The update comes from the device (its pinned certificate).", "La mise à jour vient de l'appareil (son certificat épinglé)."))
                    .setPositiveButton(this.tr("Update", "Mettre à jour"), (d, w) -> this.downloadAndInstall(origin, fp))
                    .setNegativeButton(this.tr("Later", "Plus tard"), null)
                    .show();
            });
        }).start();
    }

    private void downloadAndInstall(String origin, String fp) {
        Toast.makeText(this, this.tr("Downloading the update...", "Téléchargement de la mise à jour..."), Toast.LENGTH_SHORT).show();
        new Thread(() -> {
            try {
                File apk = new File(this.getCacheDir(), "xmouse.apk");
                HttpsURLConnection c = Pinned.open(origin + "/app/xmouse.apk", fp);
                c.setReadTimeout(60000);
                try (InputStream in = c.getInputStream(); OutputStream out = new FileOutputStream(apk)) {
                    byte[] buffer = new byte[65536];
                    for (int n; (n = in.read(buffer)) > 0;) out.write(buffer, 0, n);
                }
                c.disconnect();
                this.install(apk);
            } catch (Exception e) {
                this._ui.post(() -> Toast.makeText(this, this.tr("Update failed: ", "Échec de la mise à jour : ") + e.getMessage(), Toast.LENGTH_LONG).show());
            }
        }).start();
    }

    private void install(File apk) throws Exception {
        PackageInstaller installer = this.getPackageManager().getPackageInstaller();
        PackageInstaller.SessionParams params = new PackageInstaller.SessionParams(PackageInstaller.SessionParams.MODE_FULL_INSTALL);
        params.setAppPackageName(this.getPackageName());
        int id = installer.createSession(params);
        try (PackageInstaller.Session session = installer.openSession(id)) {
            try (InputStream in = new FileInputStream(apk); OutputStream out = session.openWrite("xmouse.apk", 0, apk.length())) {
                byte[] buffer = new byte[65536];
                for (int n; (n = in.read(buffer)) > 0;) out.write(buffer, 0, n);
                session.fsync(out);
            }
            Intent intent = new Intent(this, InstallReceiver.class);
            int flags = PendingIntent.FLAG_UPDATE_CURRENT | (Build.VERSION.SDK_INT >= 31 ? PendingIntent.FLAG_MUTABLE : 0);
            PendingIntent pending = PendingIntent.getBroadcast(this, id, intent, flags);
            session.commit(pending.getIntentSender());
        }
    }
}
