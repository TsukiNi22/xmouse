/*
 * Argon2id (RFC 9106, version 0x13) in plain Java: the proof of the one-time code (the same value as the
 * EVP_KDF ARGON2ID of OpenSSL used by the desktop: 64 MiB, 3 passes, 1 lane, 32 bytes)
*/
package io.github.tsukini22.xmouse.core;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;

public final class Argon2 {
    private static final int VERSION = 0x13;
    private static final int TYPE_ID = 2; // Argon2id
    private static final int BLOCK_LONGS = 128; // 1 KiB blocks
    private static final int SYNC_POINTS = 4;

    private Argon2() {}

    public static byte[] argon2id(byte[] password, byte[] salt, int passes, int memoryKiB, int lanes, int length) {
        int segment = memoryKiB / (SYNC_POINTS * lanes);
        int laneLength = segment * SYNC_POINTS;
        int blocks = laneLength * lanes;
        long[][] memory = new long[blocks][BLOCK_LONGS];

        // H0 = H^(64)(p, T, m, t, v, y, <P>, P, <S>, S, <K>, K, <X>, X)
        Blake2b h = new Blake2b(64);
        h.update(le32(lanes));
        h.update(le32(length));
        h.update(le32(memoryKiB));
        h.update(le32(passes));
        h.update(le32(VERSION));
        h.update(le32(TYPE_ID));
        h.update(le32(password.length));
        h.update(password);
        h.update(le32(salt.length));
        h.update(salt);
        h.update(le32(0));
        h.update(le32(0));
        byte[] h0 = h.digest();

        // The first two blocks of each lane
        for (int lane = 0; lane < lanes; lane++) {
            for (int i = 0; i < 2; i++) {
                byte[] input = new byte[72];
                System.arraycopy(h0, 0, input, 0, 64);
                System.arraycopy(le32(i), 0, input, 64, 4);
                System.arraycopy(le32(lane), 0, input, 68, 4);
                toLongs(hashLong(input, 1024), memory[lane * laneLength + i]);
            }
        }

        // Passes
        for (int pass = 0; pass < passes; pass++) {
            for (int slice = 0; slice < SYNC_POINTS; slice++) {
                for (int lane = 0; lane < lanes; lane++) {
                    fillSegment(memory, pass, lane, slice, lanes, laneLength, segment, blocks, passes);
                }
            }
        }

        // Final block: the XOR of the last block of every lane
        long[] last = memory[laneLength - 1].clone();
        for (int lane = 1; lane < lanes; lane++) {
            long[] b = memory[lane * laneLength + laneLength - 1];
            for (int i = 0; i < BLOCK_LONGS; i++) last[i] ^= b[i];
        }
        return hashLong(toBytes(last), length);
    }

    private static void fillSegment(long[][] memory, int pass, int lane, int slice, int lanes, int laneLength, int segment, int blocks, int passes) {
        boolean dataIndependent = pass == 0 && slice < SYNC_POINTS / 2; // Argon2id: i for the first half of the first pass
        long[] address = new long[BLOCK_LONGS];
        long[] input = new long[BLOCK_LONGS];
        long[] zero = new long[BLOCK_LONGS];
        int start = pass == 0 && slice == 0 ? 2 : 0;

        if (dataIndependent) {
            input[0] = pass;
            input[1] = lane;
            input[2] = slice;
            input[3] = blocks;
            input[4] = passes;
            input[5] = TYPE_ID;
            if (start == 2) nextAddresses(address, input, zero);
        }
        for (int i = start; i < segment; i++) {
            int current = lane * laneLength + slice * segment + i;
            int previous = (current % laneLength == 0) ? current + laneLength - 1 : current - 1;
            long pseudo;
            if (dataIndependent) {
                if (i % BLOCK_LONGS == 0) nextAddresses(address, input, zero);
                pseudo = address[i % BLOCK_LONGS];
            } else pseudo = memory[previous][0];

            // Reference block: its lane, then its index in the window allowed
            int refLane = (int) Long.remainderUnsigned(pseudo >>> 32, lanes);
            if (pass == 0 && slice == 0) refLane = lane;
            boolean sameLane = refLane == lane;
            int area;
            if (pass == 0) {
                if (slice == 0) area = i - 1;
                else area = sameLane ? slice * segment + i - 1 : slice * segment + (i == 0 ? -1 : 0);
            } else area = sameLane ? laneLength - segment + i - 1 : laneLength - segment + (i == 0 ? -1 : 0);
            long x = pseudo & 0xffffffffL;
            long y = (x * x) >>> 32;
            long z = area - 1 - ((area * y) >>> 32);
            int startPosition = pass != 0 && slice != SYNC_POINTS - 1 ? (slice + 1) * segment : 0;
            int refIndex = (int) ((startPosition + z) % laneLength);
            long[] ref = memory[refLane * laneLength + refIndex];

            if (pass == 0) compress(memory[current], memory[previous], ref, false);
            else compress(memory[current], memory[previous], ref, true);
        }
    }

    // The next 128 pseudo random indexes of the data independent addressing
    private static void nextAddresses(long[] address, long[] input, long[] zero) {
        long[] tmp = new long[BLOCK_LONGS];
        input[6]++;
        compress(tmp, zero, input, false);
        compress(address, zero, tmp, false);
    }

    // out = G(x, y) (xored into out after the first pass)
    private static void compress(long[] out, long[] x, long[] y, boolean xor) {
        long[] r = new long[BLOCK_LONGS];
        long[] q = new long[BLOCK_LONGS];
        for (int i = 0; i < BLOCK_LONGS; i++) {
            r[i] = x[i] ^ y[i];
            q[i] = r[i];
        }
        for (int i = 0; i < 8; i++) { // rows
            int b = i * 16;
            round(q, b, b + 1, b + 2, b + 3, b + 4, b + 5, b + 6, b + 7, b + 8, b + 9, b + 10, b + 11, b + 12, b + 13, b + 14, b + 15);
        }
        for (int i = 0; i < 8; i++) { // columns
            int b = i * 2;
            round(q, b, b + 1, b + 16, b + 17, b + 32, b + 33, b + 48, b + 49, b + 64, b + 65, b + 80, b + 81, b + 96, b + 97, b + 112, b + 113);
        }
        for (int i = 0; i < BLOCK_LONGS; i++) {
            long v = q[i] ^ r[i];
            out[i] = xor ? out[i] ^ v : v;
        }
    }

    private static void round(long[] v, int i0, int i1, int i2, int i3, int i4, int i5, int i6, int i7, int i8, int i9, int i10, int i11, int i12, int i13, int i14, int i15) {
        gb(v, i0, i4, i8, i12);
        gb(v, i1, i5, i9, i13);
        gb(v, i2, i6, i10, i14);
        gb(v, i3, i7, i11, i15);
        gb(v, i0, i5, i10, i15);
        gb(v, i1, i6, i11, i12);
        gb(v, i2, i7, i8, i13);
        gb(v, i3, i4, i9, i14);
    }

    private static long fblamka(long x, long y) {return x + y + 2 * (x & 0xffffffffL) * (y & 0xffffffffL);}

    private static void gb(long[] v, int a, int b, int c, int d) {
        v[a] = fblamka(v[a], v[b]);
        v[d] = Long.rotateRight(v[d] ^ v[a], 32);
        v[c] = fblamka(v[c], v[d]);
        v[b] = Long.rotateRight(v[b] ^ v[c], 24);
        v[a] = fblamka(v[a], v[b]);
        v[d] = Long.rotateRight(v[d] ^ v[a], 16);
        v[c] = fblamka(v[c], v[d]);
        v[b] = Long.rotateRight(v[b] ^ v[c], 63);
    }

    // H' (variable length hash of Argon2)
    private static byte[] hashLong(byte[] input, int length) {
        byte[] out = new byte[length];
        if (length <= 64) {
            Blake2b h = new Blake2b(length);
            h.update(le32(length));
            h.update(input);
            return h.digest();
        }
        Blake2b h = new Blake2b(64);
        h.update(le32(length));
        h.update(input);
        byte[] v = h.digest();
        int pos = 0;
        System.arraycopy(v, 0, out, pos, 32);
        pos += 32;
        int remaining = length - 32;
        while (remaining > 64) {
            Blake2b next = new Blake2b(64);
            next.update(v);
            v = next.digest();
            System.arraycopy(v, 0, out, pos, 32);
            pos += 32;
            remaining -= 32;
        }
        Blake2b next = new Blake2b(remaining);
        next.update(v);
        System.arraycopy(next.digest(), 0, out, pos, remaining);
        return out;
    }

    private static byte[] le32(int v) {return ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(v).array();}

    private static void toLongs(byte[] bytes, long[] out) {
        ByteBuffer.wrap(bytes).order(ByteOrder.LITTLE_ENDIAN).asLongBuffer().get(out);
    }

    private static byte[] toBytes(long[] longs) {
        ByteBuffer buffer = ByteBuffer.allocate(longs.length * 8).order(ByteOrder.LITTLE_ENDIAN);
        buffer.asLongBuffer().put(longs);
        return buffer.array();
    }

    // ---------------------------------------------------------------- //
    // Blake2b (RFC 7693, no key)

    static final class Blake2b {
        private static final long[] IV = {
            0x6a09e667f3bcc908L, 0xbb67ae8584caa73bL, 0x3c6ef372fe94f82bL, 0xa54ff53a5f1d36f1L,
            0x510e527fade682d1L, 0x9b05688c2b3e6c1fL, 0x1f83d9abfb41bd6bL, 0x5be0cd19137e2179L};
        private static final int[][] SIGMA = {
            {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15}, {14, 10, 4, 8, 9, 15, 13, 6, 1, 12, 0, 2, 11, 7, 5, 3},
            {11, 8, 12, 0, 5, 2, 15, 13, 10, 14, 3, 6, 7, 1, 9, 4}, {7, 9, 3, 1, 13, 12, 11, 14, 2, 6, 5, 10, 4, 0, 15, 8},
            {9, 0, 5, 7, 2, 4, 10, 15, 14, 1, 11, 12, 6, 8, 3, 13}, {2, 12, 6, 10, 0, 11, 8, 3, 4, 13, 7, 5, 15, 14, 1, 9},
            {12, 5, 1, 15, 14, 13, 4, 10, 0, 7, 6, 3, 9, 2, 8, 11}, {13, 11, 7, 14, 12, 1, 3, 9, 5, 0, 15, 4, 8, 6, 2, 10},
            {6, 15, 14, 9, 11, 3, 0, 8, 12, 2, 13, 7, 1, 4, 10, 5}, {10, 2, 8, 4, 7, 6, 1, 5, 15, 11, 9, 14, 3, 12, 13, 0},
            {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15}, {14, 10, 4, 8, 9, 15, 13, 6, 1, 12, 0, 2, 11, 7, 5, 3}};
        private final long[] _h = new long[8];
        private final byte[] _buffer = new byte[128];
        private int _filled = 0;
        private long _counter = 0;
        private final int _length;

        Blake2b(int length) {
            this._length = length;
            System.arraycopy(IV, 0, this._h, 0, 8);
            this._h[0] ^= 0x01010000L ^ length;
        }

        void update(byte[] data) {
            for (byte b: data) {
                if (this._filled == 128) {
                    this._counter += 128;
                    this.compress(false);
                    this._filled = 0;
                }
                this._buffer[this._filled++] = b;
            }
        }

        byte[] digest() {
            this._counter += this._filled;
            for (int i = this._filled; i < 128; i++) this._buffer[i] = 0;
            this.compress(true);
            byte[] all = new byte[64];
            ByteBuffer.wrap(all).order(ByteOrder.LITTLE_ENDIAN).asLongBuffer().put(this._h);
            byte[] out = new byte[this._length];
            System.arraycopy(all, 0, out, 0, this._length);
            return out;
        }

        private void compress(boolean last) {
            long[] m = new long[16];
            long[] v = new long[16];
            ByteBuffer.wrap(this._buffer).order(ByteOrder.LITTLE_ENDIAN).asLongBuffer().get(m);
            System.arraycopy(this._h, 0, v, 0, 8);
            System.arraycopy(IV, 0, v, 8, 8);
            v[12] ^= this._counter;
            if (last) v[14] = ~v[14];
            for (int r = 0; r < 12; r++) {
                int[] s = SIGMA[r];
                g(v, 0, 4, 8, 12, m[s[0]], m[s[1]]);
                g(v, 1, 5, 9, 13, m[s[2]], m[s[3]]);
                g(v, 2, 6, 10, 14, m[s[4]], m[s[5]]);
                g(v, 3, 7, 11, 15, m[s[6]], m[s[7]]);
                g(v, 0, 5, 10, 15, m[s[8]], m[s[9]]);
                g(v, 1, 6, 11, 12, m[s[10]], m[s[11]]);
                g(v, 2, 7, 8, 13, m[s[12]], m[s[13]]);
                g(v, 3, 4, 9, 14, m[s[14]], m[s[15]]);
            }
            for (int i = 0; i < 8; i++) this._h[i] ^= v[i] ^ v[i + 8];
        }

        private static void g(long[] v, int a, int b, int c, int d, long x, long y) {
            v[a] = v[a] + v[b] + x;
            v[d] = Long.rotateRight(v[d] ^ v[a], 32);
            v[c] = v[c] + v[d];
            v[b] = Long.rotateRight(v[b] ^ v[c], 24);
            v[a] = v[a] + v[b] + y;
            v[d] = Long.rotateRight(v[d] ^ v[a], 16);
            v[c] = v[c] + v[d];
            v[b] = Long.rotateRight(v[b] ^ v[c], 63);
        }
    }
}
