/*
 * The identity of the phone as a member: an EC P-256 key in the Android KeyStore (never leaves it) with its
 * self-signed certificate (the members pin its sha256), served by the TLS 1.3 server of the node
*/
package io.github.tsukini22.xmouse;

import android.security.keystore.KeyGenParameterSpec;
import android.security.keystore.KeyProperties;

import java.math.BigInteger;
import java.net.Socket;
import java.security.KeyPairGenerator;
import java.security.KeyStore;
import java.security.Principal;
import java.security.PrivateKey;
import java.security.cert.X509Certificate;
import java.security.spec.ECGenParameterSpec;
import java.util.Date;

import javax.net.ssl.KeyManager;
import javax.net.ssl.SSLContext;
import javax.net.ssl.SSLEngine;
import javax.net.ssl.X509ExtendedKeyManager;
import javax.security.auth.x500.X500Principal;

final class Identity {
    private static final String ALIAS = "xmouse-identity";

    final SSLContext server;
    final byte[] fingerprint;

    private Identity(SSLContext server, byte[] fingerprint) {
        this.server = server;
        this.fingerprint = fingerprint;
    }

    static Identity load() throws Exception {
        KeyStore ks = KeyStore.getInstance("AndroidKeyStore");
        ks.load(null);
        if (!ks.containsAlias(ALIAS)) {
            KeyPairGenerator generator = KeyPairGenerator.getInstance(KeyProperties.KEY_ALGORITHM_EC, "AndroidKeyStore");
            generator.initialize(new KeyGenParameterSpec.Builder(ALIAS, KeyProperties.PURPOSE_SIGN | KeyProperties.PURPOSE_VERIFY)
                .setAlgorithmParameterSpec(new ECGenParameterSpec("secp256r1"))
                .setDigests(KeyProperties.DIGEST_SHA256, KeyProperties.DIGEST_NONE)
                .setCertificateSubject(new X500Principal("CN=xmouse phone"))
                .setCertificateSerialNumber(BigInteger.valueOf(System.currentTimeMillis()))
                .setCertificateNotBefore(new Date(System.currentTimeMillis() - 86400000L))
                .setCertificateNotAfter(new Date(System.currentTimeMillis() + 3650L * 86400000L))
                .build());
            generator.generateKeyPair(); // the keystore signs its own certificate
        }
        final X509Certificate cert = (X509Certificate) ks.getCertificate(ALIAS);
        final PrivateKey key = (PrivateKey) ks.getKey(ALIAS, null);
        X509ExtendedKeyManager manager = new X509ExtendedKeyManager() {
            @Override public String[] getClientAliases(String type, Principal[] issuers) {return null;}
            @Override public String chooseClientAlias(String[] types, Principal[] issuers, Socket socket) {return null;}
            @Override public String[] getServerAliases(String type, Principal[] issuers) {return new String[]{ALIAS};}
            @Override public String chooseServerAlias(String type, Principal[] issuers, Socket socket) {return ALIAS;}
            @Override public String chooseEngineServerAlias(String type, Principal[] issuers, SSLEngine engine) {return ALIAS;}
            @Override public X509Certificate[] getCertificateChain(String alias) {return new X509Certificate[]{cert};}
            @Override public PrivateKey getPrivateKey(String alias) {return key;}
        };
        SSLContext ctx = SSLContext.getInstance("TLS");
        ctx.init(new KeyManager[]{manager}, null, null);
        return new Identity(ctx, io.github.tsukini22.xmouse.core.Crypto.fingerprint(cert));
    }
}
