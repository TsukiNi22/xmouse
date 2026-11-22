/*
 * xmouse android application:
 *  - home: this phone in a network like a computer (NodeService: a member and a server itself, it creates a network
 *    shown as a QR code / code, joins one, disconnects, leaves; its touchpad drives its own cursor across the devices,
 *    its screen is lent to the layout; its settings)
 *  - the touchpad of one computer (paired, without the phone in its network): the /pad page of the device in a WebView, with
 *  - the device taken from its QR code / link (https://host:port/pad?code=..&fp=..): the fp pins its certificate, every
 *    TLS error is accepted ONLY for that exact certificate
 *  - a bridge for the page (XmouseAndroid): the token of the pad shared with the screen service, the screen lent
 *  - updates: the xmouse website (gh-pages app/version.json, its sha256) and the device of the network, the newest one,
 *    installed with the PackageInstaller
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
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
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
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.Switch;
import android.widget.TextView;
import android.widget.ScrollView;
import android.widget.Toast;

import io.github.tsukini22.xmouse.core.Node;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.URL;
import java.security.MessageDigest;
import java.util.List;
import java.util.Locale;

import javax.net.ssl.HttpsURLConnection;

public class MainActivity extends Activity {
    // ---------------------------------------------------------------- //
    // Constants

    static final String PREFS = "xmouse";
    private static final int DEFAULT_PORT = 47900;
    private static final int CAMERA_REQUEST = 7;
    private static final String SCANNER = "https://appassets.androidplatform.net"; // local origin of the scanner page (assets)
    private static final String PAGES = "https://tsukini22.github.io/xmouse/app/"; // the app on the xmouse website (version.json, xmouse.apk)

    /* palette (dark tokens of the panel) */
    private static final int BG = Color.parseColor("#0f1214");
    private static final int TEXT = Color.parseColor("#e4e7e9");
    private static final int MUTED = Color.parseColor("#98a2a9");
    private static final int ACCENT = Color.parseColor("#3ddc97");
    private static final int WARN = Color.parseColor("#f0a742");
    private static final int CARD = Color.parseColor("#171b1e");

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
        this.showLastCrash();
    }

    // The app stopped last time (App wrote it down): its trace, to copy and send
    private void showLastCrash() {
        File file = new File(this.getFilesDir(), App.CRASH);
        String trace;
        if (!file.exists()) return;
        try (InputStream in = new FileInputStream(file)) {
            trace = new String(readAll(in, 65536), "UTF-8");
        } catch (Exception e) {
            trace = "";
        }
        file.delete();
        if (trace.isEmpty()) return;
        final String text = trace;
        TextView view = this.text(text, 11, TEXT);
        ScrollView scroll = new ScrollView(this);
        view.setTextIsSelectable(true);
        view.setTypeface(Typeface.MONOSPACE);
        view.setPadding(this.dp(16), this.dp(8), this.dp(16), this.dp(8));
        scroll.addView(view);
        new AlertDialog.Builder(this)
            .setTitle(this.tr("xmouse stopped last time", "xmouse s'est arrêté la dernière fois"))
            .setMessage(this.tr("The lent screen and the network of the phone were turned off so that it does not happen at every launch: turn them on again from the home. Copy this report to send it:",
                "L'écran prêté et le réseau du téléphone ont été coupés pour que cela ne recommence pas à chaque lancement : réactivez-les depuis l'accueil. Copiez ce rapport pour l'envoyer :"))
            .setView(scroll)
            .setPositiveButton(this.tr("Copy", "Copier"), (d, w) -> {
                android.content.ClipboardManager clipboard = (android.content.ClipboardManager) this.getSystemService(CLIPBOARD_SERVICE);
                if (clipboard != null) clipboard.setPrimaryClip(android.content.ClipData.newPlainText("xmouse crash", text));
            })
            .setNegativeButton("OK", null)
            .show();
    }

    @Override
    protected void onNewIntent(Intent intent) {
        super.onNewIntent(intent);
        this.handleIntent(intent);
    }

    @Override
    public void onBackPressed() {
        if (!this._homeShown) {this.showHome(); return;} // touchpad, scanner, pairing: back to the home
        this.moveTaskToBack(true); // the network stays connected
    }

    @Override
    protected void onDestroy() {
        NodeService.unlisten(this._memberListener);
        super.onDestroy();
    }

    private String host() {return this._prefs.getString("host", "");}

    private String pin() {return this._prefs.getString("fp", "");}

    private String deviceName() {return this._prefs.getString("name", Build.MODEL);}

    private boolean online() {return this._prefs.getBoolean("online", true);}

    // Every launch: the home (the node started unless the person disconnected it)
    private void startOrSetup() {
        if (this.online()) NodeService.start(this);
        this.showHome();
        this._ui.postDelayed(() -> {this.autoUpdate(); this.resumeScreen();}, 3000); // once linked to its network
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
            this.showHome();
            Toast.makeText(this, this.tr("This is not the link of an xmouse device (its QR code holds its identity).", "Ce n'est pas le lien d'un appareil xmouse (son QR code contient son identité)."), Toast.LENGTH_LONG).show();
            return false;
        }

        // The network of a member (its QR code / link): this phone joins it
        if (uri.getPath() == null || !uri.getPath().startsWith("/pad")) {
            Node node = NodeService.node();
            this._prefs.edit().putBoolean("online", true).apply();
            this.showHome();
            if (node != null && node.joined()) {
                new AlertDialog.Builder(this)
                    .setTitle(this.tr("Join another network?", "Rejoindre un autre réseau ?"))
                    .setMessage(this.tr("This phone is in the network ", "Ce téléphone est dans le réseau ") + node.networkName() + this.tr(": it leaves it to join the new one.", " : il le quitte pour rejoindre le nouveau."))
                    .setPositiveButton(this.tr("Join", "Rejoindre"), (d, w) -> {node.leave(); node.join(link, "");})
                    .setNegativeButton(this.tr("Cancel", "Annuler"), null)
                    .show();
            } else this.joinWhenReady(link, 0);
            return true;
        }

        // The touchpad of a computer (its "Pair a phone" QR code)
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
        this._homeShown = false;
        this._ui.removeCallbacks(this._tick);
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
        box.addView(this.text(this.tr("The touchpad and the keyboard of one computer, paired with it (this phone stays out of its network).", "Le touchpad et le clavier d'un ordinateur, associé à lui (ce téléphone reste hors de son réseau)."), 15, MUTED));
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
        box.addView(this.button(this.tr("Home", "Accueil"), v -> this.showHome()));
        this._root.addView(box, new FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT, Gravity.CENTER_VERTICAL));
    }

    // ---------------------------------------------------------------- //
    // Home: this phone in a network like a computer (a member, and a server the others join)

    // The node starts with the service: the join waits for it a few seconds
    private void joinWhenReady(String link, int tries) {
        Node node = NodeService.node();
        NodeService.start(this);
        if (node != null) {node.join(link, ""); return;}
        if (tries < 40) this._ui.postDelayed(() -> this.joinWhenReady(link, tries + 1), 250);
    }

    private final Node.Listener _memberListener = new Node.Listener() {
        @Override public void changed() {
            if (MainActivity.this._homeShown && !MainActivity.this.homeState().equals(MainActivity.this._homeState)) MainActivity.this.refreshHome();
        }
        @Override public void joinRequest(String pending, String name, String os) {
            new AlertDialog.Builder(MainActivity.this)
                .setTitle(MainActivity.this.tr("A device wants to join", "Un appareil veut rejoindre"))
                .setMessage(name + " (" + os + ")" + MainActivity.this.tr(" gave the right code. Refused by default in 30 s.", " a donné le bon code. Refusé par défaut dans 30 s."))
                .setPositiveButton(MainActivity.this.tr("Accept", "Accepter"), (d, w) -> {Node n = NodeService.node(); if (n != null) n.approve(pending, true); NodeService.cancel(MainActivity.this, pending);})
                .setNegativeButton(MainActivity.this.tr("Refuse", "Refuser"), (d, w) -> {Node n = NodeService.node(); if (n != null) n.approve(pending, false); NodeService.cancel(MainActivity.this, pending);})
                .show();
        }
        @Override public void joinResult(boolean ok, String message) {if (MainActivity.this._homeShown) MainActivity.this.refreshHome();}
        @Override public void notice(String text) { /* toasts of the service */ }
        @Override public void screen(java.util.Map<String, Object> event) { /* the lent screen */ }
        @Override public void map(java.util.Map<String, Object> map) {
            String json = io.github.tsukini22.xmouse.core.Json.write(map);
            MainActivity.this._ui.post(() -> {
                if (MainActivity.this._localPad != null) MainActivity.this._localPad.evaluateJavascript("window.xmouseNodeMessage && window.xmouseNodeMessage(" + org.json.JSONObject.quote(json) + ")", null);
            });
        }
        @Override public void clipboard(String text) { /* the service sets it */ }
        @Override public void link(String from, String url) { /* a notification of the service */ }
    };

    private boolean _homeShown = false;
    private String _homeState = ""; // what the network and members cards show (rebuilt only when it changes)
    private LinearLayout _dynamic; // network and members: rebuilt when the node changes
    private WebView _localPad; // the touchpad page of the app (assets) driving the node
    private TextView _codeText;
    private ImageView _qrImage;
    private String _qrLink = "";
    private Bitmap _qrBitmap;
    private Button _screenButton;

    // Every second while the home is shown: the time left of the code, the QR code once the code changed
    private final Runnable _tick = new Runnable() {
        @Override public void run() {
            if (!MainActivity.this._homeShown) return;
            MainActivity.this.tickCode();
            MainActivity.this._ui.postDelayed(this, 1000);
        }
    };

    private Button button(String label, android.view.View.OnClickListener click) {
        Button b = new Button(this);
        b.setText(label);
        b.setAllCaps(false);
        b.setOnClickListener(click);
        return b;
    }

    // Two buttons side by side
    private LinearLayout pair(Button a, Button b) {
        LinearLayout row = new LinearLayout(this);
        row.setOrientation(LinearLayout.HORIZONTAL);
        row.addView(a, new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1));
        row.addView(b, new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1));
        return row;
    }

    // A section of the home: a rounded card with its title
    private LinearLayout card(LinearLayout parent, String title) {
        LinearLayout card = new LinearLayout(this);
        GradientDrawable background = new GradientDrawable();
        LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        TextView label = this.text(title.toUpperCase(Locale.ROOT), 12, MUTED);
        card.setOrientation(LinearLayout.VERTICAL);
        background.setColor(CARD);
        background.setCornerRadius(this.dp(12));
        card.setBackground(background);
        card.setPadding(this.dp(16), this.dp(12), this.dp(16), this.dp(14));
        params.topMargin = this.dp(12);
        label.setTypeface(Typeface.DEFAULT_BOLD);
        label.setLetterSpacing(0.08f);
        card.addView(label);
        parent.addView(card, params);
        return card;
    }

    private void showHome() {
        ScrollView scroll = new ScrollView(this);
        LinearLayout box = new LinearLayout(this);
        TextView title = this.text("xmouse", 26, TEXT);

        this.clearRoot();
        this._localPad = null;
        this._homeShown = true;
        NodeService.listen(this._memberListener);
        box.setOrientation(LinearLayout.VERTICAL);
        box.setPadding(this.dp(16), this.dp(28), this.dp(16), this.dp(24));
        scroll.addView(box);
        title.setTypeface(Typeface.DEFAULT_BOLD);
        box.addView(title);
        box.addView(this.text(this.deviceName() + "  ·  " + this.version(), 14, MUTED));
        this._dynamic = new LinearLayout(this);
        this._dynamic.setOrientation(LinearLayout.VERTICAL);
        box.addView(this._dynamic);
        this.phoneCard(box);
        this.computerCard(box);
        this.settingsCard(box);
        this._root.addView(scroll, new FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
        this.refreshHome();
        this._ui.post(this._tick);
    }

    // Connected, in a network, its name, its members (online, rtt by 20 ms): the node tells a change every second
    private String homeState() {
        Node node = NodeService.node();
        StringBuilder state = new StringBuilder();
        state.append(this.online()).append('|').append(node == null ? "-" : (node.joined() ? "joined " + node.networkName() : "alone"));
        try {
            if (node != null && node.joined())
                for (Node.Member m: node.members()) state.append('|').append(m.id).append(m.name).append(m.kind).append(m.online).append(m.rtt / 20);
        } catch (RuntimeException e) {
            state.append("|changing"); // the node changed its members meanwhile: drawn again next time
        }
        return state.toString();
    }

    private void refreshHome() {
        LinearLayout box = this._dynamic;
        Node node = NodeService.node();

        if (box == null || !this._homeShown) return;
        this._homeState = this.homeState();
        box.removeAllViews();
        this._codeText = null;
        this._qrImage = null;
        if (this._screenButton != null) this._screenButton.setText(this.screenLabel());
        LinearLayout net = this.card(box, this.tr("Network", "Réseau"));

        // Disconnected by the person: nothing listens, the network waits
        if (!this.online()) {
            net.addView(this.text(this.tr("Disconnected: the other devices do not reach this phone (it stays a member of its network).", "Déconnecté : les autres appareils n'atteignent pas ce téléphone (il reste membre de son réseau)."), 15, MUTED));
            net.addView(this.button(this.tr("Connect", "Se connecter"), v -> {
                this._prefs.edit().putBoolean("online", true).apply();
                NodeService.start(this);
                this._ui.postDelayed(this::refreshHome, 600);
            }));
            return;
        }
        if (node == null) {
            net.addView(this.text(this.tr("Starting...", "Démarrage..."), 15, MUTED));
            NodeService.start(this);
            this._ui.postDelayed(this::refreshHome, 500);
            return;
        }

        // In no network: create one (this phone is its first device and its server) or join one
        if (!node.joined()) {
            final EditText name = new EditText(this);
            final EditText link = new EditText(this);
            net.addView(this.text(this.tr("In no network yet: create one (this phone is its first device, the others join it), or join the network of another device.",
                "Dans aucun réseau : créez-en un (ce téléphone en est le premier appareil, les autres le rejoignent), ou rejoignez le réseau d'un autre appareil."), 15, MUTED));
            name.setText(this.deviceName() + " network");
            name.setTextColor(TEXT);
            name.setSingleLine(true);
            net.addView(name);
            net.addView(this.button(this.tr("Create a network", "Créer un réseau"), v -> node.createNetwork(name.getText().toString())));
            net.addView(this.text(this.tr("Join: scan the QR code of a member (its Network page), or paste its link.", "Rejoindre : scannez le QR code d'un membre (sa page Réseau), ou collez son lien."), 15, MUTED));
            net.addView(this.button(this.tr("Scan a QR code", "Scanner un QR code"), v -> this.openScanner()));
            link.setHint("https://192.168.1.20:47900/?code=...&fp=...");
            link.setTextColor(TEXT);
            link.setHintTextColor(MUTED);
            link.setSingleLine(true);
            net.addView(link);
            net.addView(this.pair(this.button(this.tr("Join with this link", "Rejoindre avec ce lien"), v -> this.useLink(link.getText().toString().trim())),
                this.button(this.tr("Disconnect", "Se déconnecter"), v -> this.disconnect())));
            return;
        }

        // In a network: its QR code and its code for a new device, the members
        TextView networkName = this.text(node.networkName(), 20, ACCENT);
        LinearLayout.LayoutParams qr = new LinearLayout.LayoutParams(this.dp(230), this.dp(230));
        networkName.setTypeface(Typeface.DEFAULT_BOLD);
        net.addView(networkName);
        net.addView(this.text(this.tr("A device joins with this QR code or its link (Connect to a network), then this phone accepts it.", "Un appareil rejoint avec ce QR code ou son lien (Se connecter à un réseau), puis ce téléphone l'accepte."), 14, MUTED));
        this._qrImage = new ImageView(this);
        this._qrImage.setScaleType(ImageView.ScaleType.FIT_CENTER);
        qr.gravity = Gravity.CENTER_HORIZONTAL;
        qr.topMargin = this.dp(8);
        net.addView(this._qrImage, qr);
        if (this._qrBitmap != null) this._qrImage.setImageBitmap(this._qrBitmap);
        this._codeText = this.text("", 22, TEXT);
        this._codeText.setTypeface(Typeface.MONOSPACE, Typeface.BOLD);
        this._codeText.setGravity(Gravity.CENTER_HORIZONTAL);
        net.addView(this._codeText);
        this.tickCode();
        net.addView(this.pair(this.button(this.tr("Share the link", "Partager le lien"), v -> {
            Intent share = new Intent(Intent.ACTION_SEND).setType("text/plain").putExtra(Intent.EXTRA_TEXT, node.joinLink());
            this.startActivity(Intent.createChooser(share, "xmouse"));
        }), this.button(this.tr("New code", "Nouveau code"), v -> {node.newCode(); this._ui.postDelayed(this::tickCode, 300);})));
        net.addView(this.pair(this.button(this.tr("Disconnect", "Se déconnecter"), v -> this.disconnect()),
            this.button(this.tr("Leave the network", "Quitter le réseau"), v -> new AlertDialog.Builder(this)
                .setMessage(this.tr("This phone leaves the network: it needs a new code to come back.", "Ce téléphone quitte le réseau : il lui faudra un nouveau code pour revenir."))
                .setPositiveButton(this.tr("Leave", "Quitter"), (d, w) -> node.leave()).setNegativeButton(this.tr("Cancel", "Annuler"), null).show())));

        List<Node.Member> list;
        try {
            list = node.members();
        } catch (RuntimeException e) {
            list = new java.util.ArrayList<>(); // changed meanwhile: the next change draws them
        }
        LinearLayout members = this.card(box, this.tr("Members", "Membres") + " (" + list.size() + ")");
        for (Node.Member m: list) {
            LinearLayout row = new LinearLayout(this);
            TextView label = this.text((m.online ? "● " : "○ ") + m.name + "  " + m.kind + (m.self ? this.tr("  (this phone)", "  (ce téléphone)") : (m.online ? (m.rtt >= 0 ? "  " + m.rtt + " ms" : "") : this.tr("  offline", "  hors ligne"))), 15, m.online ? TEXT : MUTED);
            row.setOrientation(LinearLayout.HORIZONTAL);
            row.setGravity(Gravity.CENTER_VERTICAL);
            row.addView(label, new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1));
            if (!m.self) {
                final String id = m.id;
                final String who = m.name;
                row.addView(this.button(this.tr("Remove", "Retirer"), v -> new AlertDialog.Builder(this).setTitle(who)
                    .setMessage(this.tr("It leaves the network (the key changes).", "Il quitte le réseau (la clé change)."))
                    .setPositiveButton(this.tr("Remove", "Retirer"), (d, w) -> node.removeMember(id)).setNegativeButton(this.tr("Cancel", "Annuler"), null).show()));
            }
            members.addView(row);
        }
    }

    // The code and its time left; the QR code drawn again once the link changed (a new code)
    private void tickCode() {
        Node node = NodeService.node();
        if (node == null || !node.joined() || this._codeText == null) return;
        int left = node.codeSecondsLeft();
        this._codeText.setText(node.code() + "   " + left / 60 + ":" + String.format(Locale.ROOT, "%02d", left % 60));
        String link = node.joinLink();
        if (link.equals(this._qrLink) && this._qrBitmap != null) return;
        try {
            this._qrBitmap = qrBitmap(link);
            this._qrLink = link;
        } catch (Exception e) {
            this._qrBitmap = null; // a link too long for a QR code: the link button stays
        }
        if (this._qrImage != null) this._qrImage.setImageBitmap(this._qrBitmap);
    }

    // Black modules on white with the quiet zone of 4 modules, 8 px a module
    private static Bitmap qrBitmap(String text) {
        Qr qr = Qr.encode(text, Qr.MEDIUM);
        int quiet = 4;
        int scale = 8;
        int width = (qr.size + 2 * quiet) * scale;
        int[] pixels = new int[width * width];
        for (int y = 0; y < width; y++)
            for (int x = 0; x < width; x++) pixels[y * width + x] = qr.dark(x / scale - quiet, y / scale - quiet) ? Color.BLACK : Color.WHITE;
        return Bitmap.createBitmap(pixels, width, width, Bitmap.Config.ARGB_8888);
    }

    // Nothing listens until Connect: the network waits, this phone stays a member
    private void disconnect() {
        this._prefs.edit().putBoolean("online", false).apply();
        NodeService.stop(this);
        this._ui.postDelayed(this::refreshHome, 400);
    }

    private String screenLabel() {
        String state = ScreenService.running() ? this.tr(": touch", " : toucher") : OverlayService.running() ? this.tr(": cursors shown", " : curseurs affichés") : this.tr(": off", " : désactivé");
        return this.tr("This screen in the layout", "Cet écran dans la disposition") + state;
    }

    private void phoneCard(LinearLayout box) {
        LinearLayout card = this.card(box, this.tr("This phone", "Ce téléphone"));
        card.addView(this.text(this.tr("Its touchpad drives its cursor across the screens of the network; its screen can be in the layout (the cursors of the others come on it).",
            "Son touchpad pilote son curseur sur les écrans du réseau ; son écran peut être dans la disposition (les curseurs des autres viennent dessus)."), 14, MUTED));
        card.addView(this.button(this.tr("Open the touchpad", "Ouvrir le touchpad"), v -> {
            Node node = NodeService.node();
            if (node == null || !node.joined()) {Toast.makeText(this, this.tr("Create or join a network first", "Créez ou rejoignez d'abord un réseau"), Toast.LENGTH_SHORT).show(); return;}
            this.openLocalPad();
        }));
        this._screenButton = this.button(this.screenLabel(), v -> this.lendScreenDialog());
        card.addView(this._screenButton);
    }

    private void computerCard(LinearLayout box) {
        LinearLayout card = this.card(box, this.tr("Touchpad of a computer", "Touchpad d'un ordinateur"));
        card.addView(this.text(this.tr("Paired with one computer (its \"Pair a phone\" QR code): the touchpad of that computer, without this phone in its network.",
            "Associé à un ordinateur (son QR code « Associer un téléphone ») : le touchpad de cet ordinateur, sans ce téléphone dans son réseau."), 14, MUTED));
        if (!this.host().isEmpty() && !this.pin().isEmpty()) card.addView(this.button(this.tr("Open the touchpad of ", "Ouvrir le touchpad de ") + Uri.parse(this.host()).getHost(), v -> this.openPad()));
        card.addView(this.button(this.tr("Pair with a computer", "Associer à un ordinateur"), v -> this.showSetup("")));
    }

    private Switch toggle(String label, String key, boolean byDefault, Runnable changed) {
        Switch s = new Switch(this);
        s.setText(label);
        s.setTextColor(TEXT);
        s.setTextSize(15);
        s.setPadding(0, this.dp(8), 0, this.dp(8));
        s.setChecked(this._prefs.getBoolean(key, byDefault));
        s.setOnCheckedChangeListener((v, on) -> {
            this._prefs.edit().putBoolean(key, on).apply();
            if (changed != null) changed.run();
        });
        return s;
    }

    private void settingsCard(LinearLayout box) {
        LinearLayout card = this.card(box, this.tr("Settings", "Réglages"));
        final EditText name = new EditText(this);
        LinearLayout row = new LinearLayout(this);

        card.addView(this.text(this.tr("Name of this phone", "Nom de ce téléphone"), 14, MUTED));
        name.setText(this.deviceName());
        name.setTextColor(TEXT);
        name.setSingleLine(true);
        row.setOrientation(LinearLayout.HORIZONTAL);
        row.addView(name, new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1));
        row.addView(this.button(this.tr("Save", "Enregistrer"), v -> {
            String value = name.getText().toString().trim();
            Node node = NodeService.node();
            if (value.isEmpty()) return;
            this._prefs.edit().putString("name", value.length() > 64 ? value.substring(0, 64) : value).apply();
            if (node != null) node.rename(this.deviceName());
            Toast.makeText(this, this.tr("Name saved", "Nom enregistré"), Toast.LENGTH_SHORT).show();
        }));
        card.addView(row);
        card.addView(this.toggle(this.tr("Private mode: the cursors of the others pass over this screen without touching anything", "Mode privé : les curseurs des autres passent sur cet écran sans rien toucher"), "private", false, null));
        card.addView(this.toggle(this.tr("Check for updates at every launch", "Chercher les mises à jour à chaque lancement"), "autoUpdate", true, null));
        card.addView(this.button(this.tr("Check for updates now", "Chercher les mises à jour maintenant"), v -> this.checkUpdate(true)));
        card.addView(this.text(this.tr("Version ", "Version ") + this.version() + this.tr(" - updates from the xmouse website and the computers of the network", " - mises à jour depuis le site xmouse et les ordinateurs du réseau"), 13, MUTED));
    }

    // The touchpad page of the app (its assets): its commands go to the node of the phone
    private final class NodeBridge {
        @JavascriptInterface public String device() {return MainActivity.this._prefs.getString("name", Build.MODEL);}

        @JavascriptInterface public void send(String json) {
            Node node = NodeService.node();
            org.json.JSONObject o;
            if (node == null) return;
            try {o = new org.json.JSONObject(json);} catch (Exception e) {return;}
            switch (o.optString("t")) {
                case "move": node.move(o.optDouble("dx"), o.optDouble("dy")); break;
                case "button": node.button(o.optInt("b"), o.optBoolean("down")); break;
                case "wheel": node.wheel(o.optInt("dx"), o.optInt("dy")); break;
                case "key": node.key(o.optString("code"), o.optBoolean("down")); break;
                case "text": node.text(o.optString("text")); break;
                case "home": node.home(); break;
                case "ping": node.ping(); break;
                case "laser": node.laser(o.optBoolean("on")); break;
                case "link": node.sendLink(o.optString("device"), o.optString("url")); break;
                default: break;
            }
        }
    }

    private void openLocalPad() {
        this.clearRoot();
        WebView web = this.newWeb();
        this._web = web;
        this._localPad = web;
        web.addJavascriptInterface(new NodeBridge(), "XmouseNode");
        web.addJavascriptInterface(new Bridge(), "XmouseAndroid");
        web.setWebViewClient(new WebViewClient() {
            @Override
            public WebResourceResponse shouldInterceptRequest(WebView view, WebResourceRequest request) {return MainActivity.this.asset(request.getUrl());}

            @Override
            public boolean shouldOverrideUrlLoading(WebView view, WebResourceRequest request) {return !request.getUrl().toString().startsWith(SCANNER);}
        });
        this._root.addView(web, new FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
        web.loadUrl(SCANNER + "/pad.html");
    }

    // A file of the assets of the app on its local https origin (scanner, touchpad page)
    private WebResourceResponse asset(Uri uri) {
        if (!uri.toString().startsWith(SCANNER)) return null;
        String name = uri.getPath() == null || uri.getPath().equals("/") ? "scan.html" : uri.getPath().substring(1);
        String type = name.endsWith(".js") ? "text/javascript" : name.endsWith(".css") ? "text/css" : name.endsWith(".svg") ? "image/svg+xml" : "text/html";
        try {
            return new WebResourceResponse(type, "UTF-8", this.getAssets().open(name));
        } catch (Exception e) {
            return new WebResourceResponse("text/plain", "UTF-8", 404, "Not found", null, null);
        }
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
        this.autoUpdate();
        this.resumeScreen(); // back from the overlay permission: the screen is lent at once
        if (this._homeShown) this.refreshHome();
    }

    // Every launch / return to the app: the website and the device are asked for a newer app (a dialog only when there is one)
    private void autoUpdate() {
        long now = System.currentTimeMillis();
        if (!this._prefs.getBoolean("autoUpdate", true)) return;
        if (now - this._lastUpdateCheck < 10 * 60 * 1000) return;
        this._lastUpdateCheck = now;
        this.checkUpdate(false);
    }

    // The screen stays lent (phantom) as long as the person wants it and the overlay is allowed
    private void resumeScreen() {
        Node node = NodeService.node();
        if (this._prefs.getBoolean("lend", false) && ((node != null && node.joined()) || !this._prefs.getString("token", "").isEmpty())) OverlayService.start(this);
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
        @JavascriptInterface public void lendScreen() {MainActivity.this._ui.post(MainActivity.this::lendScreenDialog);}

        @JavascriptInterface public void checkUpdate() {MainActivity.this._ui.post(() -> MainActivity.this.checkUpdate(true));}

        @JavascriptInterface public void changeDevice() {
            MainActivity.this._ui.post(MainActivity.this::showHome);
        }
    }

    // Android lets an app touch the others only through an Accessibility service, turned on by the person; an app
    // installed from the browser needs "Allow restricted settings" first (its App info page, the menu at the top)
    private void enableTouch() {
        if (ScreenService.running()) {
            Toast.makeText(this, this.tr("The cursors already touch this phone", "Les curseurs touchent déjà ce téléphone"), Toast.LENGTH_SHORT).show();
            return;
        }
        new AlertDialog.Builder(this)
            .setTitle(this.tr("Let the cursors touch", "Laisser les curseurs toucher"))
            .setMessage(this.tr(
                "Android lets an app click in the other apps only through an Accessibility service:\n\n"
                    + "1. Accessibility: Installed apps (or Downloaded apps) > xmouse screen > on.\n\n"
                    + "2. Greyed out, \"Restricted setting\"? App info > the menu at the top right > Allow restricted settings, then step 1 again.\n\n"
                    + "3. App info > Battery > Unrestricted: else Android stops the service now and then (the clicks then do nothing until it comes back).\n\n"
                    + "The cursors then click, drag, scroll and type on this phone (off at any time in the same place).",
                "Android ne laisse une appli cliquer dans les autres que par un service d'Accessibilité :\n\n"
                    + "1. Accessibilité : Applications installées (ou téléchargées) > xmouse screen > activé.\n\n"
                    + "2. Grisé, « Paramètre restreint » ? Infos de l'appli > le menu en haut à droite > Autoriser les paramètres restreints, puis l'étape 1 à nouveau.\n\n"
                    + "3. Infos de l'appli > Batterie > Sans restriction : sinon Android arrête le service de temps en temps (les clics ne font alors rien jusqu'à son retour).\n\n"
                    + "Les curseurs cliquent, glissent, défilent et tapent alors sur ce téléphone (désactivable à tout moment au même endroit)."))
            .setPositiveButton(this.tr("1. Accessibility", "1. Accessibilité"), (d, w) -> this.startActivity(new Intent(Settings.ACTION_ACCESSIBILITY_SETTINGS)))
            .setNeutralButton(this.tr("2. App info", "2. Infos de l'appli"), (d, w) -> this.startActivity(new Intent(Settings.ACTION_APPLICATION_DETAILS_SETTINGS, Uri.parse("package:" + this.getPackageName()))))
            .setNegativeButton(this.tr("Cancel", "Annuler"), null)
            .show();
    }

    private void lendScreenDialog() {
            MainActivity.this._ui.post(() -> {
                String now = ScreenService.running() ? MainActivity.this.tr("Now: the cursors touch this phone (Accessibility).\n\n", "Maintenant : les curseurs touchent ce téléphone (Accessibilité).\n\n")
                    : OverlayService.running() ? MainActivity.this.tr("Now: the cursors are only shown - their clicks do nothing here (Android lets an app touch other apps only through the Accessibility service: Let them touch).\n\n",
                        "Maintenant : les curseurs sont seulement affichés - leurs clics ne font rien ici (Android ne laisse une appli toucher les autres que par le service d'Accessibilité : Les laisser toucher).\n\n")
                    : MainActivity.this.tr("Now: this screen is not in the layout.\n\n", "Maintenant : cet écran n'est pas dans la disposition.\n\n");
                AlertDialog.Builder dialog = new AlertDialog.Builder(MainActivity.this)
                    .setTitle(MainActivity.this.tr("This screen in the layout", "Cet écran dans la disposition"))
                    .setMessage(now + MainActivity.this.tr(
                        "Show the cursors: the cursors of the network come on this phone and are drawn over the apps with their name (\"Display over other apps\").\n\nLet them touch: their clicks, drags, scrolls and text also act here (the Accessibility service \"xmouse screen\"; after an install from the browser, Settings > Apps > xmouse > Allow restricted settings first).",
                        "Afficher les curseurs : les curseurs du réseau viennent sur ce téléphone, dessinés par-dessus les applis avec leur nom (« Afficher par-dessus les autres applis »).\n\nLes laisser toucher : leurs clics, glisser, défilements et textes agissent aussi ici (le service d'Accessibilité « xmouse screen » ; après une installation depuis le navigateur : Paramètres > Applis > xmouse > Autoriser les paramètres restreints d'abord)."))
                    .setPositiveButton(MainActivity.this.tr("Show the cursors", "Afficher les curseurs"), (d, w) -> MainActivity.this.lendPhantom())
                    .setNeutralButton(MainActivity.this.tr("Let them touch", "Les laisser toucher"), (d, w) -> MainActivity.this.enableTouch());
                if (OverlayService.running()) dialog.setNegativeButton(MainActivity.this.tr("Stop", "Arrêter"), (d, w) -> {
                    MainActivity.this._prefs.edit().putBoolean("lend", false).apply();
                    OverlayService.stop(MainActivity.this);
                });
                dialog.show();
            });
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
            public WebResourceResponse shouldInterceptRequest(WebView view, WebResourceRequest request) {return MainActivity.this.asset(request.getUrl());}

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
        else {
            this.showHome();
            Toast.makeText(this, this.tr("No camera: paste the link of the device instead.", "Pas de caméra : collez plutôt le lien de l'appareil."), Toast.LENGTH_LONG).show();
        }
    }

    // ---------------------------------------------------------------- //
    // Updates

    String version() {return versionOf(this);}

    static String versionOf(android.content.Context context) {
        try {
            PackageInfo info = context.getPackageManager().getPackageInfo(context.getPackageName(), 0);
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

    // Where a device carries the newest app: a computer of the network of this phone, else the paired computer
    private String[] updateDevice() {
        Node node = NodeService.node();
        String[] source = node == null || !node.joined() ? null : node.updateSource();
        if (source != null) return source;
        if (!this.host().isEmpty() && !this.pin().isEmpty()) return new String[]{this.host(), this.pin()};
        return null;
    }

    // The newest of the xmouse website (independent of any device) and the device; the same version: the device (LAN)
    private void checkUpdate(boolean asked) {
        final String[] device = this.updateDevice();
        new Thread(() -> {
            String fromDevice = "";
            String fromPages = "";
            String sha256 = "";
            if (device != null) {
                try {
                    HttpsURLConnection c = Pinned.open(device[0] + "/app/version", device[1]);
                    if (c.getResponseCode() == 200) {
                        try (InputStream in = c.getInputStream()) {fromDevice = new String(readAll(in, 64), "UTF-8").trim();}
                    }
                    c.disconnect();
                } catch (Exception e) {
                    fromDevice = "";
                }
            }
            try {
                HttpsURLConnection c = (HttpsURLConnection) new URL(PAGES + "version.json").openConnection();
                c.setConnectTimeout(Pinned.TIMEOUT);
                c.setReadTimeout(Pinned.TIMEOUT);
                c.setUseCaches(false);
                if (c.getResponseCode() == 200) {
                    try (InputStream in = c.getInputStream()) {
                        org.json.JSONObject o = new org.json.JSONObject(new String(readAll(in, 4096), "UTF-8"));
                        fromPages = o.optString("version", "");
                        sha256 = o.optString("sha256", "").toLowerCase(Locale.ROOT);
                    }
                }
                c.disconnect();
            } catch (Exception e) {
                fromPages = "";
            }
            final boolean pages = !fromPages.isEmpty() && sha256.length() == 64 && (fromDevice.isEmpty() || compareVersions(fromPages, fromDevice) > 0);
            final String latest = pages ? fromPages : fromDevice;
            final String hash = sha256;
            this._ui.post(() -> {
                if (latest.isEmpty() || compareVersions(latest, this.version()) <= 0) {
                    if (asked) Toast.makeText(this, latest.isEmpty() && device == null ? this.tr("No update source reachable (website, device)", "Aucune source de mise à jour joignable (site, appareil)")
                        : this.tr("xmouse is up to date (", "xmouse est à jour (") + this.version() + ")", Toast.LENGTH_SHORT).show();
                    return;
                }
                new AlertDialog.Builder(this)
                    .setTitle(this.tr("Update available", "Mise à jour disponible"))
                    .setMessage(this.version() + " -> " + latest + "\n\n" + (pages
                        ? this.tr("From the xmouse website (its sha256 checked, the same signature as this app).", "Depuis le site xmouse (son sha256 vérifié, la même signature que cette appli).")
                        : this.tr("From the device (its pinned certificate).", "Depuis l'appareil (son certificat épinglé).")))
                    .setPositiveButton(this.tr("Update", "Mettre à jour"), (d, w) -> {
                        if (pages) this.downloadAndInstall(PAGES + "xmouse.apk", "", hash);
                        else this.downloadAndInstall(device[0] + "/app/xmouse.apk", device[1], "");
                    })
                    .setNegativeButton(this.tr("Later", "Plus tard"), null)
                    .show();
            });
        }).start();
    }

    // From the device (pinned TLS) or the website (system TLS, then its sha256)
    private void downloadAndInstall(String url, String fp, String sha256) {
        Toast.makeText(this, this.tr("Downloading the update...", "Téléchargement de la mise à jour..."), Toast.LENGTH_SHORT).show();
        new Thread(() -> {
            try {
                File apk = new File(this.getCacheDir(), "xmouse.apk");
                HttpsURLConnection c = fp.isEmpty() ? (HttpsURLConnection) new URL(url).openConnection() : Pinned.open(url, fp);
                c.setConnectTimeout(Pinned.TIMEOUT);
                c.setReadTimeout(60000);
                c.setUseCaches(false);
                try (InputStream in = c.getInputStream(); OutputStream out = new FileOutputStream(apk)) {
                    byte[] buffer = new byte[65536];
                    for (int n; (n = in.read(buffer)) > 0;) out.write(buffer, 0, n);
                }
                c.disconnect();
                if (!sha256.isEmpty()) {
                    try (InputStream in = new FileInputStream(apk)) {
                        MessageDigest digest = MessageDigest.getInstance("SHA-256");
                        byte[] buffer = new byte[65536];
                        for (int n; (n = in.read(buffer)) > 0;) digest.update(buffer, 0, n);
                        StringBuilder hex = new StringBuilder();
                        for (byte b: digest.digest()) hex.append(String.format("%02x", b & 0xff));
                        if (!hex.toString().equals(sha256)) throw new Exception("the downloaded app does not match its sha256");
                    }
                }
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
