/*
 * TLS pinned on the certificate of the device (sha256 of its DER, the "fp" of its QR code): its certificate is
 * self-signed for its LAN / tailnet addresses, the pin replaces the CA and name checks
*/
package io.github.tsukini22.xmouse;

import java.net.URL;
import java.security.MessageDigest;
import java.security.SecureRandom;
import java.security.cert.CertificateException;
import java.security.cert.X509Certificate;

import javax.net.ssl.HostnameVerifier;
import javax.net.ssl.HttpsURLConnection;
import javax.net.ssl.SSLContext;
import javax.net.ssl.SSLSocketFactory;
import javax.net.ssl.TrustManager;
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

    // Trusts ONLY the certificate whose sha256 is expected (an empty pin trusts nothing)
    static final class Trust implements X509TrustManager {
        final String expected;

        Trust(String expected) {this.expected = expected == null ? "" : expected.toLowerCase();}

        @Override public void checkClientTrusted(X509Certificate[] chain, String auth) throws CertificateException {throw new CertificateException("client auth not used");}

        @Override public void checkServerTrusted(X509Certificate[] chain, String auth) throws CertificateException {
            if (chain == null || chain.length == 0) throw new CertificateException("no certificate");
            String seen = sha256(chain[0].getEncoded());
            if (this.expected.isEmpty() || !MessageDigest.isEqual(seen.getBytes(), this.expected.getBytes())) throw new CertificateException("not the certificate of the device");
        }

        @Override public X509Certificate[] getAcceptedIssuers() {return new X509Certificate[0];}
    }

    static SSLSocketFactory factory(String fp) throws Exception {
        SSLContext ctx = SSLContext.getInstance("TLS");
        ctx.init(null, new TrustManager[]{new Trust(fp)}, new SecureRandom());
        return ctx.getSocketFactory();
    }

    static HttpsURLConnection open(String url, String fp) throws Exception {
        HttpsURLConnection c = (HttpsURLConnection) new URL(url).openConnection();
        HostnameVerifier any = (h, s) -> true; // the pin is the check
        c.setSSLSocketFactory(factory(fp));
        c.setHostnameVerifier(any);
        c.setConnectTimeout(TIMEOUT);
        c.setReadTimeout(TIMEOUT);
        return c;
    }
}
