/*
 * TLS pinned on the certificate of the device (sha256 of its DER, the "fp" of its QR code): its certificate is
 * self-signed for its LAN / tailnet addresses, the pin replaces the CA and name checks. On its tailnet name the device
 * gives its Tailscale certificate instead (the browsers need a real one there): a certificate signed by a CA of the
 * phone FOR THAT NAME is accepted too, as the WebView of the touchpad does.
*/
package io.github.tsukini22.xmouse;

import java.net.URL;
import java.security.KeyStore;
import java.security.MessageDigest;
import java.security.SecureRandom;
import java.security.cert.CertificateException;
import java.security.cert.X509Certificate;
import java.util.Collection;
import java.util.List;
import java.util.Locale;

import javax.net.ssl.HostnameVerifier;
import javax.net.ssl.HttpsURLConnection;
import javax.net.ssl.SSLContext;
import javax.net.ssl.SSLSocketFactory;
import javax.net.ssl.TrustManager;
import javax.net.ssl.TrustManagerFactory;
import javax.net.ssl.X509TrustManager;

final class Pinned {
    static final int TIMEOUT = 8000; // ms

    private Pinned() {}

    static String sha256(byte[] data) {
        try {
            byte[] digest = MessageDigest.getInstance("SHA-256").digest(data);
            StringBuilder out = new StringBuilder();
            for (byte b: digest) out.append(String.format("%02x", b & 0xff));
            return out.toString();
        } catch (Exception e) {
            return "";
        }
    }

    // The certificate whose sha256 is expected, or one signed by a CA of the phone for the name asked (an empty pin
    // and no name trust nothing)
    static final class Trust implements X509TrustManager {
        final String expected;
        final String host;

        Trust(String expected, String host) {
            this.expected = expected == null ? "" : expected.toLowerCase(Locale.ROOT);
            this.host = host == null ? "" : host.toLowerCase(Locale.ROOT);
        }

        @Override public void checkClientTrusted(X509Certificate[] chain, String auth) throws CertificateException {throw new CertificateException("client auth not used");}

        @Override public void checkServerTrusted(X509Certificate[] chain, String auth) throws CertificateException {
            if (chain == null || chain.length == 0) throw new CertificateException("no certificate");
            String seen = sha256(chain[0].getEncoded());
            if (!this.expected.isEmpty() && MessageDigest.isEqual(seen.getBytes(), this.expected.getBytes())) return;
            if (this.host.isEmpty() || isAddress(this.host)) throw new CertificateException("not the certificate of the device");
            system().checkServerTrusted(chain, auth); // a real certificate (the tailnet name of the device)...
            if (!names(chain[0], this.host)) throw new CertificateException("certificate not made for " + this.host); // ...for this name
        }

        @Override public X509Certificate[] getAcceptedIssuers() {return new X509Certificate[0];}
    }

    // The trust store of the phone (the CAs the browser trusts)
    private static X509TrustManager system() throws CertificateException {
        try {
            TrustManagerFactory factory = TrustManagerFactory.getInstance(TrustManagerFactory.getDefaultAlgorithm());
            factory.init((KeyStore) null);
            for (TrustManager m: factory.getTrustManagers())
                if (m instanceof X509TrustManager) return (X509TrustManager) m;
        } catch (Exception e) {
            throw new CertificateException("no trust store: " + e.getMessage());
        }
        throw new CertificateException("no trust store");
    }

    private static boolean isAddress(String host) {return host.matches("[0-9.]+") || host.contains(":");}

    // The DNS names of the certificate cover the host (exact, or a wildcard on its first label)
    private static boolean names(X509Certificate cert, String host) throws CertificateException {
        Collection<List<?>> names;
        try {
            names = cert.getSubjectAlternativeNames();
        } catch (Exception e) {
            throw new CertificateException("unreadable names");
        }
        if (names == null) return false;
        for (List<?> entry: names) {
            if (entry.size() < 2 || !(entry.get(0) instanceof Integer) || (Integer) entry.get(0) != 2) continue; // dNSName
            String name = String.valueOf(entry.get(1)).toLowerCase(Locale.ROOT);
            if (name.equals(host)) return true;
            if (name.startsWith("*.") && host.indexOf('.') > 0 && host.substring(host.indexOf('.')).equals(name.substring(1))) return true;
        }
        return false;
    }

    static SSLSocketFactory factory(String fp, String host) throws Exception {
        SSLContext ctx = SSLContext.getInstance("TLS");
        ctx.init(null, new TrustManager[]{new Trust(fp, host)}, new SecureRandom());
        return ctx.getSocketFactory();
    }

    static HttpsURLConnection open(String url, String fp) throws Exception {
        URL target = new URL(url);
        HttpsURLConnection c = (HttpsURLConnection) target.openConnection();
        HostnameVerifier any = (h, s) -> true; // checked by Trust (the pin, or the name of a real certificate)
        c.setSSLSocketFactory(factory(fp, target.getHost()));
        c.setHostnameVerifier(any);
        c.setConnectTimeout(TIMEOUT);
        c.setReadTimeout(TIMEOUT);
        return c;
    }
}
