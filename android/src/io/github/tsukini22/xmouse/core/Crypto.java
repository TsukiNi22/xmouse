/*
 * Crypto of the member core: random, hashes, HMAC, the one-time codes, the transcript of a link (the same bytes as the
 * desktop: "xmouse-auth-v1|" + role, the two nonces, the sha256 of the certificate of the server side)
*/
package io.github.tsukini22.xmouse.core;

import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.security.SecureRandom;
import java.security.cert.Certificate;
import java.util.Base64;

import javax.crypto.Mac;
import javax.crypto.spec.SecretKeySpec;

public final class Crypto {
    public static final int ARGON2_MEMORY = 65536; // KiB (the desktop XMOUSE_ARGON2_*)
    public static final int ARGON2_PASSES = 3;
    public static final int ARGON2_LANES = 1;
    private static final String CODE_ALPHABET = "0123456789ABCDEFGHJKMNPQRSTVWXYZ"; // Crockford base32 without I L O U
    private static final SecureRandom RANDOM = new SecureRandom();

    private Crypto() {}

    public static byte[] random(int size) {
        byte[] out = new byte[size];
        RANDOM.nextBytes(out);
        return out;
    }

    public static String hex(byte[] data) {
        StringBuilder out = new StringBuilder();
        for (byte b: data) out.append(String.format("%02x", b & 0xff));
        return out.toString();
    }

    public static byte[] unhex(String text) {
        if (text == null || text.length() % 2 != 0) return new byte[0];
        byte[] out = new byte[text.length() / 2];
        try {
            for (int i = 0; i < out.length; i++) out[i] = (byte) Integer.parseInt(text.substring(2 * i, 2 * i + 2), 16);
        } catch (NumberFormatException e) {
            return new byte[0];
        }
        return out;
    }

    public static String b64(byte[] data) {return Base64.getEncoder().encodeToString(data);}

    public static byte[] unb64(String text) {
        try {
            return text == null ? new byte[0] : Base64.getDecoder().decode(text);
        } catch (IllegalArgumentException e) {
            return new byte[0];
        }
    }

    public static String randomId(int bytes) {return hex(random(bytes));}

    public static byte[] sha256(byte[] data) {
        try {
            return MessageDigest.getInstance("SHA-256").digest(data);
        } catch (Exception e) {
            throw new IllegalStateException(e);
        }
    }

    public static byte[] hmac(byte[] key, byte[] message) {
        try {
            Mac mac = Mac.getInstance("HmacSHA256");
            mac.init(new SecretKeySpec(key, "HmacSHA256"));
            return mac.doFinal(message);
        } catch (Exception e) {
            throw new IllegalStateException(e);
        }
    }

    public static boolean equal(byte[] a, byte[] b) {return a != null && b != null && a.length > 0 && MessageDigest.isEqual(a, b);}

    public static byte[] fingerprint(Certificate certificate) {
        try {
            return sha256(certificate.getEncoded());
        } catch (Exception e) {
            return new byte[0];
        }
    }

    // "K7F-29Q-XM4": 256 % 32 == 0, no modulo bias
    public static String randomCode(int length) {
        byte[] raw = random(length);
        StringBuilder out = new StringBuilder();
        for (int i = 0; i < length; i++) out.append(CODE_ALPHABET.charAt((raw[i] & 0xff) % 32));
        return out.toString();
    }

    public static String normalizeCode(String code) {
        StringBuilder out = new StringBuilder();
        for (char c: code.toUpperCase().toCharArray()) {
            if (c == '-' || Character.isWhitespace(c)) continue;
            if (c == 'O') out.append('0');
            else if (c == 'I' || c == 'L') out.append('1');
            else out.append(c);
        }
        return out.toString();
    }

    public static String displayCode(String code) {
        StringBuilder out = new StringBuilder();
        for (int i = 0; i < code.length(); i++) {
            if (i > 0 && i % 3 == 0) out.append('-');
            out.append(code.charAt(i));
        }
        return out.toString();
    }

    public static byte[] codeKey(String code, byte[] salt) {
        return Argon2.argon2id(normalizeCode(code).getBytes(StandardCharsets.UTF_8), salt, ARGON2_PASSES, ARGON2_MEMORY, ARGON2_LANES, 32);
    }

    public static byte[] transcript(byte[] serverNonce, byte[] clientNonce, byte[] serverFingerprint, String role) {
        byte[] label = ("xmouse-auth-v1|" + role).getBytes(StandardCharsets.UTF_8);
        byte[] out = new byte[label.length + serverNonce.length + clientNonce.length + serverFingerprint.length];
        int pos = 0;
        System.arraycopy(label, 0, out, pos, label.length);
        pos += label.length;
        System.arraycopy(serverNonce, 0, out, pos, serverNonce.length);
        pos += serverNonce.length;
        System.arraycopy(clientNonce, 0, out, pos, clientNonce.length);
        pos += clientNonce.length;
        System.arraycopy(serverFingerprint, 0, out, pos, serverFingerprint.length);
        return out;
    }
}
