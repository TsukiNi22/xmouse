/*
 * QR code encoder (byte mode, versions 1 to 40, the mask of the lowest penalty): a port of the QR Code generator
 * library of Project Nayuki (MIT, the C++ one is in third_party/qrcodegen). The phone shows the join link of its
 * network as a QR code, like the panel of a computer.
*/
package io.github.tsukini22.xmouse;

import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.List;

final class Qr {
    // Error correction levels (index of the tables)
    static final int LOW = 0;
    static final int MEDIUM = 1;
    static final int QUARTILE = 2;
    static final int HIGH = 3;

    private static final int[] FORMAT_BITS = {1, 0, 3, 2}; // LOW, MEDIUM, QUARTILE, HIGH
    private static final int PENALTY_N1 = 3;
    private static final int PENALTY_N2 = 3;
    private static final int PENALTY_N3 = 40;
    private static final int PENALTY_N4 = 10;

    private static final byte[][] ECC_CODEWORDS_PER_BLOCK = {
        // version: 0 (unused), 1 .. 40
        {-1,  7, 10, 15, 20, 26, 18, 20, 24, 30, 18, 20, 24, 26, 30, 22, 24, 28, 30, 28, 28, 28, 28, 30, 30, 26, 28, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30},
        {-1, 10, 16, 26, 18, 24, 16, 18, 22, 22, 26, 30, 22, 22, 24, 24, 28, 28, 26, 26, 26, 26, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28},
        {-1, 13, 22, 18, 26, 18, 24, 18, 22, 20, 24, 28, 26, 24, 20, 30, 24, 28, 28, 26, 30, 28, 30, 30, 30, 30, 28, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30},
        {-1, 17, 28, 22, 16, 22, 28, 26, 26, 24, 28, 24, 28, 22, 24, 24, 30, 28, 28, 26, 28, 30, 24, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30},
    };

    private static final byte[][] NUM_ERROR_CORRECTION_BLOCKS = {
        {-1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 4,  4,  4,  4,  4,  6,  6,  6,  6,  7,  8,  8,  9,  9, 10, 12, 12, 12, 13, 14, 15, 16, 17, 18, 19, 19, 20, 21, 22, 24, 25},
        {-1, 1, 1, 1, 2, 2, 4, 4, 4, 5, 5,  5,  8,  9,  9, 10, 10, 11, 13, 14, 16, 17, 17, 18, 20, 21, 23, 25, 26, 28, 29, 31, 33, 35, 37, 38, 40, 43, 45, 47, 49},
        {-1, 1, 1, 2, 2, 4, 4, 6, 6, 8, 8,  8, 10, 12, 16, 12, 17, 16, 18, 21, 20, 23, 23, 25, 27, 29, 34, 34, 35, 38, 40, 43, 45, 48, 51, 53, 56, 59, 62, 65, 68},
        {-1, 1, 1, 2, 4, 4, 4, 5, 6, 8, 8, 11, 11, 16, 16, 18, 16, 19, 21, 25, 25, 25, 34, 30, 32, 35, 37, 40, 42, 45, 48, 51, 54, 57, 60, 63, 66, 70, 74, 77, 81},
    };

    final int size; // modules per side
    private final int version;
    private final int ecl;
    private final boolean[][] modules; // [y][x], true = dark
    private boolean[][] isFunction;

    // ---------------------------------------------------------------- //
    // Encoding

    // The text in UTF-8 bytes, the smallest version for the level, the level raised while it still fits
    static Qr encode(String text, int ecl) {
        byte[] data = text.getBytes(StandardCharsets.UTF_8);
        int version = 1;
        int countBits = 8;
        int used = 0;

        for (;; version++) {
            countBits = version <= 9 ? 8 : 16;
            used = 4 + countBits + data.length * 8;
            if (data.length < (1 << countBits) && used <= dataCodewords(version, ecl) * 8) break;
            if (version >= 40) throw new IllegalArgumentException("text too long for a QR code");
        }
        for (int e = ecl + 1; e <= HIGH; e++)
            if (used <= dataCodewords(version, e) * 8) ecl = e;

        // Mode, count, data, terminator, pad to a byte, pad bytes
        List<Boolean> bits = new ArrayList<>();
        int capacity = dataCodewords(version, ecl) * 8;
        appendBits(bits, 0x4, 4);
        appendBits(bits, data.length, countBits);
        for (byte b: data) appendBits(bits, b & 0xff, 8);
        appendBits(bits, 0, Math.min(4, capacity - bits.size()));
        appendBits(bits, 0, (8 - bits.size() % 8) % 8);
        for (int pad = 0xEC; bits.size() < capacity; pad ^= 0xEC ^ 0x11) appendBits(bits, pad, 8);

        int[] codewords = new int[bits.size() / 8];
        for (int i = 0; i < bits.size(); i++)
            if (bits.get(i)) codewords[i >>> 3] |= 1 << (7 - (i & 7));
        return new Qr(version, ecl, codewords);
    }

    private static void appendBits(List<Boolean> bits, int value, int count) {
        for (int i = count - 1; i >= 0; i--) bits.add(((value >>> i) & 1) != 0);
    }

    private Qr(int version, int ecl, int[] dataCodewords) {
        this.version = version;
        this.ecl = ecl;
        this.size = version * 4 + 17;
        this.modules = new boolean[this.size][this.size];
        this.isFunction = new boolean[this.size][this.size];
        this.drawFunctionPatterns();
        this.drawCodewords(this.addEccAndInterleave(dataCodewords));

        // The mask of the lowest penalty (a mask applied twice is undone)
        int best = 0;
        long lowest = Long.MAX_VALUE;
        for (int mask = 0; mask < 8; mask++) {
            this.applyMask(mask);
            this.drawFormatBits(mask);
            long penalty = this.penalty();
            if (penalty < lowest) {best = mask; lowest = penalty;}
            this.applyMask(mask);
        }
        this.applyMask(best);
        this.drawFormatBits(best);
        this.isFunction = null;
    }

    boolean dark(int x, int y) {return x >= 0 && x < this.size && y >= 0 && y < this.size && this.modules[y][x];}

    // ---------------------------------------------------------------- //
    // Function patterns

    private void drawFunctionPatterns() {
        for (int i = 0; i < this.size; i++) {
            this.setFunction(6, i, i % 2 == 0);
            this.setFunction(i, 6, i % 2 == 0);
        }
        this.drawFinder(3, 3);
        this.drawFinder(this.size - 4, 3);
        this.drawFinder(3, this.size - 4);
        int[] align = this.alignmentPositions();
        int n = align.length;
        for (int i = 0; i < n; i++) {
            for (int j = 0; j < n; j++) {
                if ((i == 0 && j == 0) || (i == 0 && j == n - 1) || (i == n - 1 && j == 0)) continue; // the finder corners
                this.drawAlignment(align[i], align[j]);
            }
        }
        this.drawFormatBits(0); // overwritten once the mask is chosen
        this.drawVersion();
    }

    private void drawFormatBits(int mask) {
        int data = FORMAT_BITS[this.ecl] << 3 | mask;
        int rem = data;
        for (int i = 0; i < 10; i++) rem = (rem << 1) ^ ((rem >>> 9) * 0x537);
        int bits = (data << 10 | rem) ^ 0x5412;

        for (int i = 0; i <= 5; i++) this.setFunction(8, i, bit(bits, i));
        this.setFunction(8, 7, bit(bits, 6));
        this.setFunction(8, 8, bit(bits, 7));
        this.setFunction(7, 8, bit(bits, 8));
        for (int i = 9; i < 15; i++) this.setFunction(14 - i, 8, bit(bits, i));

        for (int i = 0; i < 8; i++) this.setFunction(this.size - 1 - i, 8, bit(bits, i));
        for (int i = 8; i < 15; i++) this.setFunction(8, this.size - 15 + i, bit(bits, i));
        this.setFunction(8, this.size - 8, true); // always dark
    }

    private void drawVersion() {
        if (this.version < 7) return;
        int rem = this.version;
        for (int i = 0; i < 12; i++) rem = (rem << 1) ^ ((rem >>> 11) * 0x1F25);
        int bits = this.version << 12 | rem;
        for (int i = 0; i < 18; i++) {
            boolean b = bit(bits, i);
            int a = this.size - 11 + i % 3;
            int c = i / 3;
            this.setFunction(a, c, b);
            this.setFunction(c, a, b);
        }
    }

    private void drawFinder(int x, int y) {
        for (int dy = -4; dy <= 4; dy++) {
            for (int dx = -4; dx <= 4; dx++) {
                int dist = Math.max(Math.abs(dx), Math.abs(dy));
                int xx = x + dx;
                int yy = y + dy;
                if (xx >= 0 && xx < this.size && yy >= 0 && yy < this.size) this.setFunction(xx, yy, dist != 2 && dist != 4);
            }
        }
    }

    private void drawAlignment(int x, int y) {
        for (int dy = -2; dy <= 2; dy++)
            for (int dx = -2; dx <= 2; dx++) this.setFunction(x + dx, y + dy, Math.max(Math.abs(dx), Math.abs(dy)) != 1);
    }

    private void setFunction(int x, int y, boolean dark) {
        this.modules[y][x] = dark;
        this.isFunction[y][x] = true;
    }

    private int[] alignmentPositions() {
        if (this.version == 1) return new int[0];
        int count = this.version / 7 + 2;
        int step = this.version == 32 ? 26 : (this.version * 4 + count * 2 + 1) / (count * 2 - 2) * 2;
        int[] result = new int[count];
        result[0] = 6;
        for (int i = count - 1, pos = this.size - 7; i >= 1; i--, pos -= step) result[i] = pos;
        return result;
    }

    // ---------------------------------------------------------------- //
    // Codewords

    private static int rawDataModules(int version) {
        int result = (16 * version + 128) * version + 64;
        if (version >= 2) {
            int count = version / 7 + 2;
            result -= (25 * count - 10) * count - 55;
            if (version >= 7) result -= 36;
        }
        return result;
    }

    private static int dataCodewords(int version, int ecl) {
        return rawDataModules(version) / 8 - ECC_CODEWORDS_PER_BLOCK[ecl][version] * NUM_ERROR_CORRECTION_BLOCKS[ecl][version];
    }

    private int[] addEccAndInterleave(int[] data) {
        int blocks = NUM_ERROR_CORRECTION_BLOCKS[this.ecl][this.version];
        int eccLength = ECC_CODEWORDS_PER_BLOCK[this.ecl][this.version];
        int raw = rawDataModules(this.version) / 8;
        int shortBlocks = blocks - raw % blocks;
        int shortLength = raw / blocks;
        int[] divisor = reedSolomonDivisor(eccLength);
        int[][] all = new int[blocks][];

        // Each block: its data, a padding byte in the short ones, its error correction
        for (int i = 0, k = 0; i < blocks; i++) {
            int length = shortLength - eccLength + (i < shortBlocks ? 0 : 1);
            int[] dat = new int[length];
            System.arraycopy(data, k, dat, 0, length);
            k += length;
            int[] ecc = reedSolomonRemainder(dat, divisor);
            int[] block = new int[shortLength + 1];
            System.arraycopy(dat, 0, block, 0, length);
            System.arraycopy(ecc, 0, block, shortLength + 1 - eccLength, eccLength);
            all[i] = block;
        }

        // Interleaved: the byte i of every block, the padding bytes skipped
        int[] result = new int[raw];
        int n = 0;
        for (int i = 0; i < shortLength + 1; i++) {
            for (int j = 0; j < blocks; j++) {
                if (i != shortLength - eccLength || j >= shortBlocks) result[n++] = all[j][i];
            }
        }
        return result;
    }

    private void drawCodewords(int[] data) {
        int i = 0;
        for (int right = this.size - 1; right >= 1; right -= 2) {
            if (right == 6) right = 5;
            for (int vert = 0; vert < this.size; vert++) {
                for (int j = 0; j < 2; j++) {
                    int x = right - j;
                    boolean upward = ((right + 1) & 2) == 0;
                    int y = upward ? this.size - 1 - vert : vert;
                    if (!this.isFunction[y][x] && i < data.length * 8) {
                        this.modules[y][x] = bit(data[i >>> 3], 7 - (i & 7));
                        i++;
                    }
                }
            }
        }
    }

    private void applyMask(int mask) {
        for (int y = 0; y < this.size; y++) {
            for (int x = 0; x < this.size; x++) {
                boolean invert;
                switch (mask) {
                    case 0: invert = (x + y) % 2 == 0; break;
                    case 1: invert = y % 2 == 0; break;
                    case 2: invert = x % 3 == 0; break;
                    case 3: invert = (x + y) % 3 == 0; break;
                    case 4: invert = (x / 3 + y / 2) % 2 == 0; break;
                    case 5: invert = x * y % 2 + x * y % 3 == 0; break;
                    case 6: invert = (x * y % 2 + x * y % 3) % 2 == 0; break;
                    default: invert = ((x + y) % 2 + x * y % 3) % 2 == 0; break;
                }
                if (invert && !this.isFunction[y][x]) this.modules[y][x] = !this.modules[y][x];
            }
        }
    }

    // ---------------------------------------------------------------- //
    // Penalty of a mask

    private long penalty() {
        long result = 0;

        for (int y = 0; y < this.size; y++) {
            boolean color = false;
            int run = 0;
            int[] history = new int[7];
            for (int x = 0; x < this.size; x++) {
                if (this.modules[y][x] == color) {
                    run++;
                    if (run == 5) result += PENALTY_N1;
                    else if (run > 5) result++;
                } else {
                    this.addHistory(run, history);
                    if (!color) result += this.countPatterns(history) * PENALTY_N3;
                    color = this.modules[y][x];
                    run = 1;
                }
            }
            result += this.terminateAndCount(color, run, history) * PENALTY_N3;
        }
        for (int x = 0; x < this.size; x++) {
            boolean color = false;
            int run = 0;
            int[] history = new int[7];
            for (int y = 0; y < this.size; y++) {
                if (this.modules[y][x] == color) {
                    run++;
                    if (run == 5) result += PENALTY_N1;
                    else if (run > 5) result++;
                } else {
                    this.addHistory(run, history);
                    if (!color) result += this.countPatterns(history) * PENALTY_N3;
                    color = this.modules[y][x];
                    run = 1;
                }
            }
            result += this.terminateAndCount(color, run, history) * PENALTY_N3;
        }

        for (int y = 0; y < this.size - 1; y++) {
            for (int x = 0; x < this.size - 1; x++) {
                boolean c = this.modules[y][x];
                if (c == this.modules[y][x + 1] && c == this.modules[y + 1][x] && c == this.modules[y + 1][x + 1]) result += PENALTY_N2;
            }
        }

        int dark = 0;
        for (boolean[] row: this.modules)
            for (boolean c: row) if (c) dark++;
        int total = this.size * this.size;
        int k = (int) ((Math.abs(dark * 20L - total * 10L) + total - 1) / total) - 1;
        result += (long) k * PENALTY_N4;
        return result;
    }

    private int countPatterns(int[] h) {
        int n = h[1];
        boolean core = n > 0 && h[2] == n && h[3] == n * 3 && h[4] == n && h[5] == n;
        return (core && h[0] >= n * 4 && h[6] >= n ? 1 : 0) + (core && h[6] >= n * 4 && h[0] >= n ? 1 : 0);
    }

    private int terminateAndCount(boolean color, int run, int[] history) {
        if (color) {
            this.addHistory(run, history);
            run = 0;
        }
        run += this.size; // light border after the last run
        this.addHistory(run, history);
        return this.countPatterns(history);
    }

    private void addHistory(int run, int[] history) {
        if (history[0] == 0) run += this.size; // light border before the first run
        System.arraycopy(history, 0, history, 1, history.length - 1);
        history[0] = run;
    }

    // ---------------------------------------------------------------- //
    // Reed-Solomon over GF(2^8 / 0x11D)

    private static int[] reedSolomonDivisor(int degree) {
        int[] result = new int[degree];
        int root = 1;
        result[degree - 1] = 1;
        for (int i = 0; i < degree; i++) {
            for (int j = 0; j < result.length; j++) {
                result[j] = multiply(result[j], root);
                if (j + 1 < result.length) result[j] ^= result[j + 1];
            }
            root = multiply(root, 0x02);
        }
        return result;
    }

    private static int[] reedSolomonRemainder(int[] data, int[] divisor) {
        int[] result = new int[divisor.length];
        for (int b: data) {
            int factor = b ^ result[0];
            System.arraycopy(result, 1, result, 0, result.length - 1);
            result[result.length - 1] = 0;
            for (int i = 0; i < result.length; i++) result[i] ^= multiply(divisor[i], factor);
        }
        return result;
    }

    private static int multiply(int x, int y) {
        int z = 0;
        for (int i = 7; i >= 0; i--) {
            z = (z << 1) ^ ((z >>> 7) * 0x11D);
            z ^= ((y >>> i) & 1) * x;
        }
        return z;
    }

    private static boolean bit(int x, int i) {return ((x >>> i) & 1) != 0;}
}
