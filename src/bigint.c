#include "bigint.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 *  * The threshold value for using Schoenhage recursive base conversion. If
 *   * the number of ints in the number are larger than this value,
 *    * the Schoenhage algorithm will be used.  In practice, it appears that the
 *     * Schoenhage routine is faster for any threshold down to 2, and is
 *      * relatively flat for thresholds between 2-25, so this choice may be
 *       * varied within this range for very small effect.
 *        */
#define SCHOENHAGE_BASE_CONVERSION_THRESHOLD 20

/**
 *  * The threshold value for using Karatsuba multiplication.  If the number
 *   * of ints in both mag arrays are greater than this number, then
 *    * Karatsuba multiplication will be used.   This value is found
 *     * experimentally to work well.
 *      */
#define MULTIPLY_SQUARE_THRESHOLD 20

/**
 *  * The threshold value for using Karatsuba multiplication.  If the number of
 *   * ints in the number are larger than this value, {@code multiply} will use
 *    * Karatsuba multiplication.  The choice of 32 is arbitrary, and Karatsuba
 *     * multiplication is faster for any threshold down to 2.
 *      */
#define KARATSUBA_THRESHOLD 80

/**
 *  * The threshold value for using 3-way Toom-Cook multiplication.
 *   * If the number of ints in each mag array is greater than the
 *    * Karatsuba threshold, and the number of ints in at least one of
 *     * the mag arrays is greater than this threshold, then Toom-Cook
 *      * multiplication will be used.
 *       */
#define TOOM_COOK_THRESHOLD 240

#define MAX_VAL 0xFFFFFFFFL

#define MAX_MAG_LENGTH (INT32_MAX / sizeof(uint32_t) + 1)

#define require(p, msg) assert(p &&msg)

static BigInteger createFromInt(int i) {
    if (i == 0) {
        return ZERO;
    }

    int signum;
    uint32_t *mag;

    if (i < 0) {
        signum = -1;
        i = -i;
    } else {
        signum = 1;
    }
    mag = (uint32_t *)malloc(sizeof(uint32_t));
    mag[0] = i;
    return (BigInteger){signum, 1, mag};
}

static BigInteger createFromLong(long val) {
    if (val == 0) {
        return ZERO;
    }

    int signum, magLen;
    uint32_t *mag;

    if (val < 0) {
        signum = -1;
        val = -val;
    } else {
        signum = 1;
    }

    uint32_t highWord = (uint32_t)(val >> 32);
    if (highWord == 0) {
        magLen = 1;
        mag = (uint32_t *)malloc(sizeof(uint32_t));
        mag[0] = (uint32_t)val;
    } else {
        magLen = 2;
        mag = (uint32_t *)malloc(2 * sizeof(uint32_t));
        mag[1] = highWord;
        mag[0] = (uint32_t)val;
    }
    return (BigInteger){signum, magLen, mag};
}

static BigInteger createFromHexString(char *hex) {
    int signum, magLen;
    uint32_t *mag;

    if (hex[0] == '-') {
        signum = -1;
        hex++;
    } else {
        signum = 1;
    }

    while (hex[0] == '0') {
        hex++;
    }

    int len = strlen(hex);
    if (len == 0) {
        return ZERO;
    }

    magLen = (len - 1) / 8 + 1;
    mag = (uint32_t *)malloc(magLen * sizeof(uint32_t));

    int i, j;
    char *p = hex + len - 8;
    for (i = 0; i < magLen - 1; i++) {
        mag[i] = 0;
        for (j = 0; j < 8; j++) {
            if ('0' <= *p && *p <= '9') {
                mag[i] = mag[i] << 4 | (*p - '0');
            } else if ('a' <= *p && *p <= 'f') {
                mag[i] = mag[i] << 4 | (*p - 'a' + 10);
            } else if ('A' <= *p && *p <= 'F') {
                mag[i] = mag[i] << 4 | (*p - 'A' + 10);
            }
            p++;
        }
        p -= 16;
    }

    p = hex;
    mag[i] = 0;
    for (j = 0; j < (len - 1) % 8 + 1; j++) {
        if ('0' <= *p && *p <= '9') {
            mag[i] = mag[i] << 4 | (*p - '0');
        } else if ('a' <= *p && *p <= 'f') {
            mag[i] = mag[i] << 4 | (*p - 'a' + 10);
        } else if ('A' <= *p && *p <= 'F') {
            mag[i] = mag[i] << 4 | (*p - 'A' + 10);
        }
        p++;
    }
    return (BigInteger){signum, magLen, mag};
}

static void finalize(BigInteger n) {
    if (n.mag != NULL) {
        free(n.mag);
        n.mag = NULL;
    }
}

/**
 * Returns the number of zero bits preceding the highest-order
 * ("leftmost") one-bit in the two's complement binary representation
 * of the specified uint32_t value.  Returns 32 if the
 * specified value has no one-bits in its two's complement representation,
 * in other words if it is equal to zero.
 */
static int numberOfLeadingZeros(uint32_t i) {
    // HD, Count leading 0's
    if (i <= 0)
        return i == 0 ? 32 : 0;
    int n = 31;
    if (i >= 1 << 16) {
        n -= 16;
        i >>= 16;
    }
    if (i >= 1 << 8) {
        n -= 8;
        i >>= 8;
    }
    if (i >= 1 << 4) {
        n -= 4;
        i >>= 4;
    }
    if (i >= 1 << 2) {
        n -= 2;
        i >>= 2;
    }
    return n - (i >> 1);
}

/**
 * Returns the number of zero bits following the lowest-order ("rightmost")
 * one-bit in the two's complement binary representation of the specified
 * {@code int} value.  Returns 32 if the specified value has no
 * one-bits in its two's complement representation, in other words if it is
 * equal to zero.
 *
 * @param i the value whose number of trailing zeros is to be computed
 * @return the number of zero bits following the lowest-order ("rightmost")
 *     one-bit in the two's complement binary representation of the
 *     specified {@code int} value, or 32 if the value is equal
 *     to zero.
 */
static int numberOfTrailingZeros(uint32_t i) {
    // HD, Count trailing 0's
    i = ~i & (i - 1);
    int n = 1;
    if (i > 1 << 16) {
        n += 16;
        i >>= 16;
    }
    if (i > 1 << 8) {
        n += 8;
        i >>= 8;
    }
    if (i > 1 << 4) {
        n += 4;
        i >>= 4;
    }
    if (i > 1 << 2) {
        n += 2;
        i >>= 2;
    }
    return n + (i >> 1);
}

/**
 * 将大整数转换为十六进制字符串，不包括0x前缀，返回的内存需要free
 * @param n 大整数
 * @return 大整数的十六进制字符串表示
 */
static char *toHexString(BigInteger n) {
    char *str;
    if (n.signum == 0) {
        str = (char *)malloc(2 * sizeof(char));
        str[0] = '0';
        str[1] = '\0';
        return str;
    }

    int j = n.magLen - 1;
    uint32_t i = n.mag[j];
    int nBytes = 8 - numberOfLeadingZeros(i) / 4;

    int strlen = (n.magLen << 3) + nBytes - 7 + (n.signum < 0 ? 1 : 0);
    str = (char *)malloc(strlen * sizeof(char));
    char *p = str;
    if (n.signum < 0) {
        *p++ = '-';
    }

    char format[6];
    sprintf(format, "%%.%dx", nBytes);
    sprintf(p, format, n.mag[j--]);
    p += nBytes;

    while (j >= 0) {
        sprintf(p, "%.08x", n.mag[j--]);
        p += 8; /* step WORD_SIZE hex-byte(s) forward in the string. */
    }

    return str;
}

int compareMagnitude(BigInteger n, BigInteger m) {
    if (n.magLen < m.magLen) {
        return -1;
    }
    if (n.magLen > m.magLen) {
        return 1;
    }
    int i;
    for (i = n.magLen - 1; i >= 0; i--) {
        if (n.mag[i] < m.mag[i]) {
            return -1;
        }
        if (n.mag[i] > m.mag[i]) {
            return 1;
        }
    }
    return 0;
}

/**
 * 将两个无符号大整数的绝对值数组相加，并返回相加后的大整数数组，返回的数组用完后需要释放内存
 * @param xMag 大整数x的数组
 * @param xlen 大整数x的数组长度
 * @param yMag 大整数y的数组
 * @param ylen 大整数y的数组长度
 * @param pzLen 相加后的大整数z的数组长度
 * @return 相加后的大整数z的数组
 */
static uint32_t *addArray(uint32_t *xMag, int xlen, uint32_t *yMag, int ylen, int *pzLen) {
    // 相加后的大整数z的数组长度，最大为xlen和ylen中较大的那个+1
    int zLen = (xlen > ylen ? xlen : ylen) + 1;
    int minLen = xlen < ylen ? xlen : ylen;
    uint32_t *zMag = (uint32_t *)malloc(zLen * sizeof(uint32_t));
    uint64_t sum;
    int carry = 0;
    // 从低位开始相加
    int i;
    for (i = 0; i < minLen; i++) {
        // 将xMag的每一位，yMag的每一位，以及上一位的进位相加
        sum = (uint64_t)xMag[i] + (uint64_t)yMag[i] + carry;
        // 将sum的低32位赋值给z的当前位
        zMag[i] = (uint32_t)(sum & MAX_VAL);
        // 更新进位
        carry = sum > MAX_VAL;
    }
    uint32_t *mag = xlen > ylen ? xMag : yMag;
    for (i = minLen; i < zLen - 1; i++) {
        sum = (uint64_t)mag[i] + carry;
        zMag[i] = (uint32_t)(sum & MAX_VAL);
        carry = sum > MAX_VAL;
    }
    // 将最高位的进位赋值给z的最后一位
    zMag[zLen - 1] = carry;
    // 如果最高位没有进位，则zLen减1
    if (!carry) {
        zLen--;
        zMag = (uint32_t *)realloc(zMag, zLen * sizeof(uint32_t));
    }
    *pzLen = zLen;
    return zMag;
}

/**
 * 将两个无符号大整数的绝对值数组相减，并返回相减后的大整数数组，返回的数组用完后需要释放内存
 * @param bigMag 被减数的数组
 * @param bigLen 被减数的数组长度
 * @param smallMag 减数的数组
 * @param smallLen 减数的数组长度
 * @param pzLen 相减后的大整数z的数组长度
 * @return 相减后的大整数z的数组
 */
static uint32_t *subArray(uint32_t *bigMag, int bigLen, uint32_t *smallMag, int smallLen, int *pzLen) {
    uint32_t *zMag = (uint32_t *)malloc(bigLen * sizeof(uint32_t));
    int borrow = 0;
    int i = 0;
    int nonZeroIndex = 0;
    for (i = 0; i < smallLen; i++) {
        zMag[i] = bigMag[i] - smallMag[i] - borrow;
        borrow = zMag[i] > bigMag[i];
        if (zMag[i] != 0) {
            nonZeroIndex = i;
        }
    }
    while (borrow) {
        borrow = ((zMag[smallLen] = bigMag[smallLen] - 1) == MAX_VAL);
        if (zMag[smallLen] != 0) {
            nonZeroIndex = smallLen;
        }
        smallLen++;
    }
    if (smallLen < bigLen) {
        nonZeroIndex = bigLen - 1;
        for (i = smallLen; i < bigLen; i++) {
            zMag[i] = bigMag[i];
        }
    }
    *pzLen = nonZeroIndex + 1;
    if (*pzLen < bigLen) {
        zMag = (uint32_t *)realloc(zMag, *pzLen * sizeof(uint32_t));
    }
    return zMag;
}

/**
 * 将两个大整数相加
 * @param n 大整数n
 * @param m 大整数m
 * @param z 相加后的大整数z
 */
static BigInteger add(BigInteger n, BigInteger m) {
    int signum;
    uint32_t *mag;
    int magLen;

    BigInteger *tmp = NULL;
    if (n.signum == 0) {
        tmp = &m;
    } else if (m.signum == 0) {
        tmp = &n;
    }
    if (tmp != NULL) {
        signum = tmp->signum;
        magLen = tmp->magLen;
        if (tmp->magLen > 0) {
            mag = (uint32_t *)malloc(magLen * sizeof(uint32_t));
            int i;
            for (i = 0; i < magLen; i++) {
                mag[i] = tmp->mag[i];
            }
        }
        return (BigInteger){signum, magLen, mag};
    }
    if (n.signum == m.signum) {
        mag = addArray(n.mag, n.magLen, m.mag, m.magLen, &magLen);
        return (BigInteger){n.signum, magLen, mag};
    }
    int cmp = compareMagnitude(n, m);
    if (cmp == 0) {
        return ZERO;
    }
    signum = cmp == n.signum ? 1 : -1;
    if (cmp < 0) {
        mag = subArray(m.mag, m.magLen, n.mag, n.magLen, &magLen);
    } else {
        mag = subArray(n.mag, n.magLen, m.mag, m.magLen, &magLen);
    }
    return (BigInteger){signum, magLen, mag};
}

/**
 * 将两个大整数相减
 * @param n 大整数n
 * @param m 大整数m
 * @return 相减后的大整数
 */
static BigInteger subtract(BigInteger n, BigInteger m) {
    BigInteger *tmp = NULL;
    int signum;
    if (n.signum == 0) {
        tmp = &m;
        signum = m.signum;
    } else if (m.signum == 0) {
        tmp = &n;
        signum = n.signum;
    }
    int magLen;
    uint32_t *mag;
    if (tmp != NULL) {
        magLen = tmp->magLen;
        if (tmp->magLen > 0) {
            mag = (uint32_t *)malloc(magLen * sizeof(uint32_t));
            int i;
            for (i = 0; i < magLen; i++) {
                mag[i] = tmp->mag[i];
            }
        }
        return (BigInteger){signum, magLen, mag};
    }

    if (n.signum != m.signum) {
        mag = addArray(n.mag, n.magLen, m.mag, m.magLen, &magLen);
        return (BigInteger){n.signum, magLen, mag};
    }

    int cmp = compareMagnitude(n, m);
    if (cmp == 0) {
        return ZERO;
    }
    signum = (cmp == n.signum ? 1 : -1);
    if (cmp < 0) {
        mag = subArray(m.mag, m.magLen, n.mag, n.magLen, &magLen);
    } else {
        mag = subArray(n.mag, n.magLen, m.mag, m.magLen, &magLen);
    }
    return (BigInteger){signum, magLen, mag};
}

/**
 * 将一个无符号大整数减去一个整数
 * @param n 大整数n
 * @param m 整数m
 * @return 相减后的大整数
 */
static BigInteger subtractByInt(BigInteger n, int m) {
    BigInteger tmp = createFromInt(m);
    BigInteger z = subtract(n, tmp);
    finalize(tmp);
    return z;
}

/**
 * 判断一个数是否是2的幂
 * @param n 要判断的数
 * @return 如果是2的幂，返回1，否则返回0
 */
static int isTwoPower(uint32_t n) {
    return (n & (n - 1)) == 0;
}

/**
 * 将一个无符号大整数的数组左移指定的位数，并返回左移后的数组，返回的数组用完后需要释放内存
 * @param xMag 大整数x的数组
 * @param xLen 大整数x的数组长度
 * @param shift 左移的位数
 * @param pzLen 左移后的大整数z的数组长度
 * @return 左移后的大整数z的数组
 */
static uint32_t *shiftLeftArray(uint32_t *xMag, int xLen, int shift, int *pzLen) {
    int shiftInt = shift >> 5;
    shift = shift & 0x1F;

    int zLen = xLen + shiftInt + 1;
    uint32_t *zMag = (uint32_t *)malloc(zLen * sizeof(uint32_t));

    uint64_t num = 0;
    uint32_t carry = 0;
    int zi, xi;
    for (zi = 0; zi < shiftInt; zi++) {
        zMag[zi] = 0;
    }
    for (xi = 0; xi < xLen; xi++) {
        num = (uint64_t)xMag[xi] << shift;
        zMag[zi++] = (uint32_t)(num & MAX_VAL) | carry;
        carry = (uint32_t)(num >> 32);
    }
    zMag[zi] = carry;
    if (carry == 0) {
        zLen--;
        zMag = (uint32_t *)realloc(zMag, zLen * sizeof(uint32_t));
    }
    *pzLen = zLen;
    return zMag;
}

/**
 * 将一个无符号大整数的数组右移指定的位数，并返回右移后的数组，返回的数组用完后需要释放内存
 * @param xMag 大整数x的数组
 * @param xLen 大整数x的数组长度
 * @param shift 右移的位数
 * @param pzLen 右移后的大整数z的数组长度
 * @return 右移后的大整数z的数组
 */
static BigInteger shiftRightImpl(BigInteger m, int shift) {
    int nInts = shift >> 5;
    int nBits = shift & 0x1f;

    int magLen = m.magLen;
    int newMagLen;
    uint32_t *newMag = NULL;

    // Special case: entire contents shifted off the end
    if (nInts >= magLen) {
        // If m < 0 and abs(m) < 2^shift, m >> shift = m / 2^shift = -1
        return m.signum >= 0 ? ZERO : createFromInt(-1);
    }

    if (nBits == 0) {
        newMagLen = magLen - nInts;
        newMag = (uint32_t *)malloc(newMagLen * sizeof(uint32_t));
        for (int i = 0; i < newMagLen; i++) {
            newMag[i] = m.mag[i + nInts];
        }
    } else {
        uint32_t highBits = m.mag[magLen - 1] >> nBits;
        if (highBits != 0) {
            newMagLen = magLen - nInts;
            newMag = (uint32_t *)malloc(newMagLen * sizeof(uint32_t));
            newMag[magLen - nInts - 1] = highBits;
        } else {
            newMagLen = magLen - nInts - 1;
            if (newMagLen == 0) {
                // If m < 0 and abs(m) < 2^shift, m >> shift = m / 2^shift = -1
                return m.signum >= 0 ? ZERO : createFromInt(-1);
            }
            newMag = (uint32_t *)malloc(newMagLen * sizeof(uint32_t));
        }
        for (int i = magLen - nInts - 2; i >= 0; i--) {
            newMag[i] = (m.mag[i + nInts] >> nBits) | (m.mag[i + nInts + 1] << (32 - nBits));
        }
    }

    if (m.signum < 0) {
        // Find out whether any one-bits were shifted off the end.
        int onesLost = 0;
        for (int i = 0; i < nInts && onesLost == 0; i++) {
            onesLost = (m.mag[i] != 0);
        }
        if (onesLost == 0 && nBits != 0) {
            onesLost = (m.mag[nInts] << (32 - nBits) != 0);
        }

        if (onesLost) {
            uint32_t lastSum = 0;
            for (int i = 0; i < newMagLen; i++) {
                lastSum = (newMag[i] += 1);
                if (lastSum != 0) {
                    break;
                }
            }
            if (lastSum == 0) {
                newMagLen++;
                newMag = (uint32_t *)realloc(newMag, newMagLen * sizeof(uint32_t));
                newMag[newMagLen - 1] = 1;
            }
        }
    }
    return (BigInteger){m.signum, newMagLen, newMag};
}

/**
 * Returns a BigInteger whose value is m >> n. Sign
 * extension is performed. The shift distance, n, may be
 * negative, in which case this method performs a left shift.
 *
 * @param n shift distance, in bits.
 * @return m >> n
 */
static BigInteger shiftRight(BigInteger m, int n) {
    if (m.signum == 0) {
        return ZERO;
    }
    if (n > 0) {
        return shiftRightImpl(m, n);
    }

    uint32_t *retMag = NULL;
    int retMagLen = 0;
    if (n == 0) {
        retMag = (uint32_t *)malloc(m.magLen * sizeof(uint32_t));
        for (int i = 0; i < m.magLen; i++) {
            retMag[i] = m.mag[i];
        }
        retMagLen = m.magLen;
    } else {
        // Possible int overflow in -n is not a trouble,
        // because shiftLeft considers its argument unsigned
        retMag = shiftLeftArray(m.mag, m.magLen, -n, &retMagLen);
    }
    return (BigInteger){m.signum, retMagLen, retMag};
}

/**
 * 将一个无符号大整数的数组乘以一个无符号整数，并返回乘积的数组，返回的数组用完后需要释放内存
 * @param xMag 大整数x的数组
 * @param xLen 大整数x的数组长度
 * @param y 乘数
 * @param pzLen 乘积z的数组长度
 * @return 乘积z的数组
 */
static uint32_t *multiplyByIntArray(uint32_t *xMag, int xLen, uint32_t y, int *pzLen) {
    require(y != 0, "y must not be 0");
    if (isTwoPower(y)) {
        int shift = 0;
        while (y != 1) {
            y >>= 1;
            shift++;
        }
        return shiftLeftArray(xMag, xLen, shift, pzLen);
    }

    int zLen = xLen + 1;
    uint32_t *zMag = (uint32_t *)malloc(zLen * sizeof(uint32_t));

    uint64_t product = 0;
    uint64_t carry = 0;
    int i;
    for (i = 0; i < xLen; i++) {
        product = (uint64_t)xMag[i] * y + carry;
        zMag[i] = (uint32_t)(product & MAX_VAL);
        carry = product >> 32;
    }
    zMag[i] = (uint32_t)carry;
    if (carry == 0) {
        zLen--;
        zMag = (uint32_t *)realloc(zMag, zLen * sizeof(uint32_t));
    }
    *pzLen = zLen;
    return zMag;
}

/**
 * 将两个无符号大整数的数组相乘，并返回乘积的数组，返回的数组用完后需要释放内存
 * @param xMag 大整数x的数组
 * @param xLen 大整数x的数组长度
 * @param yMag 大整数y的数组
 * @param yLen 大整数y的数组长度
 */
static uint32_t *multiplyArray(uint32_t *xMag, int xLen, uint32_t *yMag, int yLen, int *pzLen) {
    if (xLen <= 0 || yLen <= 0) {
        return NULL;
    }

    int zLen = xLen + yLen;
    uint32_t *zMag = (uint32_t *)malloc(zLen * sizeof(uint32_t));

    uint64_t product = 0;
    uint64_t carry = 0;

    int i, j, k;
    for (j = 0; j < yLen; j++) {
        product = (uint64_t)xMag[0] * yMag[j] + carry;
        zMag[j] = (uint32_t)(product & MAX_VAL);
        carry = product >> 32;
    }
    zMag[j] = (uint32_t)carry;

    for (i = 1; i < xLen; i++) {
        carry = 0;
        for (j = 0, k = i; j < yLen; j++, k++) {
            product = (uint64_t)xMag[i] * yMag[j] + zMag[k] + carry;
            zMag[k] = (uint32_t)(product & MAX_VAL);
            carry = product >> 32;
        }
        zMag[k] = (uint32_t)carry;
    }
    if (carry == 0) {
        zLen--;
        zMag = (uint32_t *)realloc(zMag, zLen * sizeof(uint32_t));
    }
    *pzLen = zLen;
    return zMag;
}

/**
 * Returns a new BigInteger representing n lower ints of the number.
 * This is used by Karatsuba multiplication and Karatsuba squaring.
 */
static BigInteger getLower(BigInteger x, int n) {
    uint32_t *mag;
    int magLen;

    int i;
    if (x.magLen <= n) {
        magLen = x.magLen;
        mag = (uint32_t *)malloc(magLen * sizeof(uint32_t));
        for (i = 0; i < magLen; i++) {
            mag[i] = x.mag[i];
        }
        return (BigInteger){1, magLen, mag};
    }

    mag = (uint32_t *)malloc(n * sizeof(uint32_t));
    for (i = 0; i < n; i++) {
        mag[i] = x.mag[i];
        if (mag[i] != 0) {
            magLen = i + 1;
        }
    }
    if (magLen < n) {
        mag = (uint32_t *)realloc(mag, n * sizeof(uint32_t));
    }
    return (BigInteger){1, magLen, mag};
}

/**
 * Returns a new BigInteger representing mag.length-n upper
 * ints of the number.  This is used by Karatsuba multiplication and
 * Karatsuba squaring.
 */
static BigInteger getUpper(BigInteger x, int n) {
    if (x.magLen <= n) {
        return ZERO;
    }

    int nonZeroIndex = 0;
    int upperLen = x.magLen - n;
    uint32_t *upperInts = (uint32_t *)malloc(upperLen * sizeof(uint32_t));
    int i;
    for (i = 0; i < upperLen; i++) {
        upperInts[i] = x.mag[i + n];
        if (upperInts[i] != 0) {
            nonZeroIndex = i;
        }
    }
    if (nonZeroIndex + 1 < upperLen) {
        upperLen = nonZeroIndex + 1;
        upperInts = (uint32_t *)realloc(upperInts, upperLen * sizeof(uint32_t));
    }

    return (BigInteger){1, upperLen, upperInts};
}

static BigInteger multiply(BigInteger x, BigInteger y);

/**
 * Multiplies two BigIntegers using the Karatsuba multiplication
 * algorithm.  This is a recursive divide-and-conquer algorithm which is
 * more efficient for large numbers than what is commonly called the
 * "grade-school" algorithm used in multiplyToLen.  If the numbers to be
 * multiplied have length n, the "grade-school" algorithm has an
 * asymptotic complexity of O(n^2).  In contrast, the Karatsuba algorithm
 * has complexity of O(n^(log2(3))), or O(n^1.585).  It achieves this
 * increased performance by doing 3 multiplies instead of 4 when
 * evaluating the product.  As it has some overhead, should be used when
 * both numbers are larger than a certain threshold (found experimentally).
 *
 * See:  http://en.wikipedia.org/wiki/Karatsuba_algorithm
 */
static BigInteger multiplyKaratsuba(BigInteger x, BigInteger y) {
    int xlen = x.magLen;
    int ylen = y.magLen;

    // The number of ints in each half of the number.
    int half = ((xlen > ylen ? xlen : ylen) + 1) / 2;

    // xl and yl are the lower halves of x and y respectively,
    // xh and yh are the upper halves.
    BigInteger xl = getLower(x, half);
    BigInteger xh = getUpper(x, half);
    BigInteger yl = getLower(y, half);
    BigInteger yh = getUpper(y, half);

    // p1 = xh * yh, p2 = xl * yl
    BigInteger p1 = multiply(xh, yh);
    BigInteger p2 = multiply(xl, yl);

    // p5 = (xh + xl) * (yh + yl)
    BigInteger p3 = add(xh, xl);
    BigInteger p4 = add(yh, yl);
    BigInteger p5 = multiply(p3, p4);
    finalize(xl);
    finalize(xh);
    finalize(yl);
    finalize(yh);
    finalize(p3);
    finalize(p4);

    // p7 = p5 - p1 - p2 = xh * yl + xl * yh
    BigInteger p6 = subtract(p5, p1);
    BigInteger p7 = subtract(p6, p2);
    finalize(p5);
    finalize(p6);

    // z = p1 * 2^(2 * 32 * half) + p7 * 2^(32 * half) + p2
    BigInteger p8, p10;
    p8.mag = shiftLeftArray(p1.mag, p1.magLen, 32 * half, &p8.magLen);
    p8.signum = 1;
    finalize(p1);

    BigInteger p9 = add(p8, p7);
    p10.mag = shiftLeftArray(p9.mag, p9.magLen, 32 * half, &p10.magLen);
    p10.signum = 1;
    finalize(p7);
    finalize(p8);
    finalize(p9);

    BigInteger z = add(p10, p2);
    finalize(p2);
    finalize(p10);

    if (x.signum != y.signum) {
        z.signum = -1;
    }
    return z;
}

static BigInteger multiplyByInt(BigInteger x, int y) {
    if (y == 0 || x.signum == 0) {
        return ZERO;
    }

    int signum;
    if (y < 0) {
        signum = -x.signum;
        y = -y;
    } else {
        signum = x.signum;
    }

    int magLen;
    uint32_t *mag = multiplyByIntArray(x.mag, x.magLen, (uint32_t)y, &magLen);
    return (BigInteger){signum, magLen, mag};
}

/**
 * 将两个大整数相乘
 * @param x 大整数x
 * @param y 大整数y
 * @return 相乘后的大整数z
 */
static BigInteger multiply(BigInteger x, BigInteger y) {
    if (x.signum == 0 || y.signum == 0) {
        return ZERO;
    }

    int xlen = x.magLen;
    // if (m == n && nlen > MULTIPLY_SQUARE_THRESHOLD) {
    //     square(n, z);
    //     return;
    // }

    int ylen = y.magLen;

    if ((xlen < KARATSUBA_THRESHOLD) || (ylen < KARATSUBA_THRESHOLD)) {
        int magLen;
        uint32_t *mag;
        if (ylen == 1) {
            mag = multiplyByIntArray(x.mag, xlen, y.mag[0], &magLen);
        } else if (xlen == 1) {
            mag = multiplyByIntArray(y.mag, ylen, x.mag[0], &magLen);
        } else {
            mag = multiplyArray(x.mag, xlen, y.mag, ylen, &magLen);
        }
        return (BigInteger){x.signum == y.signum ? 1 : -1, magLen, mag};
    }

    if ((xlen < TOOM_COOK_THRESHOLD) && (ylen < TOOM_COOK_THRESHOLD)) {
        return multiplyKaratsuba(x, y);
    }
    return multiplyKaratsuba(x, y);

    // if (!isRecursion) {
    //     // The bitLength() instance method is not used here as we
    //     // are only considering the magnitudes as non-negative. The
    //     // Toom-Cook multiplication algorithm determines the sign
    //     // at its end from the two signum values.
    //     if (bitLength(mag, mag.length) +
    //             bitLength(val.mag, val.mag.length) >
    //         32L * MAX_MAG_LENGTH) {
    //         reportOverflow();
    //     }
    // }

    // return multiplyToomCook3(this, val);
}

/**
 * Returns the index of the rightmost (lowest-order) one bit in this
 * MyBigInteger (the number of zero bits to the right of the rightmost
 * one bit). Returns -1 if this MyBigInteger contains no one bits.
 * (Computes {@code (this == 0? -1 : log2(this & -this))}.)
 *
 * @return index of the rightmost one bit in this MyBigInteger.
 */
static int getLowestSetBit(BigInteger n) {
    int lsb = 0;
    if (n.signum == 0) {
        lsb -= 1;
    } else {
        // Search for lowest order nonzero int
        int i;
        uint32_t b;
        for (i = 0; (b = n.mag[i]) == 0; i++)
            ;
        lsb += (i << 5) + numberOfTrailingZeros(b);
    }
    return lsb;
}

/**
 * Returns the number of one-bits in the two's complement binary
 * representation of the specified {@code int} value.  This function is
 * sometimes referred to as the <i>population count</i>.
 *
 * @param i the value whose bits are to be counted
 * @return the number of one-bits in the two's complement binary
 *     representation of the specified {@code int} value.
 */
static int bitCount(uint32_t i) {
    i = i - ((i >> 1) & 0x55555555);
    i = (i & 0x33333333) + ((i >> 2) & 0x33333333);
    i = (i + (i >> 4)) & 0x0f0f0f0f;
    i = i + (i >> 8);
    i = i + (i >> 16);
    return i & 0x3f;
}

/**
 * Returns the number of bits in the minimal two's-complement
 * representation of this MyBigInteger, <em>excluding</em> a sign bit.
 * For positive MyBigIntegers, this is equivalent to the number of bits in
 * the ordinary binary representation. For zero this method returns
 * {@code 0}. (Computes {@code (ceil(log2(this < 0 ? -this : this+1)))}.)
 *
 * @return number of bits in the minimal two's-complement
 *         representation of this MyBigInteger, <em>excluding</em> a sign bit.
 */
static int bitLength(BigInteger n) {
    if (n.signum == 0) {
        return 0; // offset by one to initialize
    }
    int len = n.magLen;
    // Calculate the bit length of the magnitude
    int magBitLength = ((len - 1) << 5) + 32 - numberOfLeadingZeros(n.mag[len - 1]);
    if (n.signum > 0) {
        return magBitLength;
    }
    // Check if magnitude is a power of two
    int pow2 = (bitCount(n.mag[len - 1]) == 1);
    for (int i = 0; i < len - 1 && pow2; i++)
        pow2 = (n.mag[i] == 0);

    return (pow2 ? magBitLength - 1 : magBitLength);
}

static BigInteger powForBigInteger(BigInteger n, int exponent) {
    if (exponent < 0) {
        printf("[BigInteger.pow] Negative exponent: %d\n", exponent);
        exit(1);
    }
    if (n.signum == 0) {
        return exponent == 0 ? iBigInteger.createFromInt(1) : ZERO;
    }

    uint32_t *partToSquareMag = (uint32_t *)malloc(n.magLen * sizeof(uint32_t));
    for (int i = 0; i < n.magLen; i++) {
        partToSquareMag[i] = n.mag[i];
    }
    BigInteger partToSquare = (BigInteger){1, n.magLen, partToSquareMag};

    // Factor out powers of two from the base, as the exponentiation of
    // these can be done by left shifts only.
    // The remaining part can then be exponentiated faster. The
    // powers of two will be multiplied back at the end.
    int powersOfTwo = getLowestSetBit(n);
    long bitsToShiftLong = (long)powersOfTwo * exponent;
    if (bitsToShiftLong > INT32_MAX) {
        printf("[BigInteger.pow] Overflow, bitsToShiftLong: %ld\n", bitsToShiftLong);
        exit(1);
    }
    int bitsToShift = (int)bitsToShiftLong;

    int remainingBits;

    // Factor the powers of two out quickly by shifting right, if needed.
    if (powersOfTwo > 0) {
        BigInteger tmp = shiftRight(partToSquare, powersOfTwo);
        finalize(partToSquare);
        partToSquare = tmp;
        remainingBits = bitLength(partToSquare);
        if (remainingBits == 1) { // Nothing left but +/- 1?
            finalize(partToSquare);
            int newSign = n.signum < 0 && (exponent & 1) == 1 ? -1 : 1;
            int newMagLen;
            uint32_t one = 1;
            uint32_t *newMag = shiftLeftArray(&one, 1, bitsToShift, &newMagLen);
            return (BigInteger){newSign, newMagLen, newMag};
        }
    } else {
        remainingBits = bitLength(partToSquare);
        if (remainingBits == 1) { // Nothing left but +/- 1?
            finalize(partToSquare);
            return createFromInt(n.signum < 0 && (exponent & 1) == 1 ? -1 : 1);
        }
    }

    // This is a quick way to approximate the size of the result,
    // similar to doing log2[n] * exponent. This will give an upper bound
    // of how big the result can be, and which algorithm to use.
    long scaleFactor = (long)remainingBits * exponent;

    // Use slightly different algorithms for small and large operands.
    // See if the result will safely fit into a long. (Largest 2^63-1)
    if (partToSquare.magLen == 1 && scaleFactor <= 62) {
        // Small number algorithm. Everything fits into a long.
        int newSign = (n.signum < 0 && (exponent & 1) == 1 ? -1 : 1);
        long result = 1;
        long baseToPow2 = (long)partToSquare.mag[0];
        finalize(partToSquare);

        int workingExponent = exponent;

        // Perform exponentiation using repeated squaring trick
        while (workingExponent != 0) {
            if ((workingExponent & 1) == 1) {
                result = result * baseToPow2;
            }

            if ((workingExponent >>= 1) != 0) {
                baseToPow2 = baseToPow2 * baseToPow2;
            }
        }

        // Multiply back the powers of two (quickly, by shifting left)
        if (powersOfTwo > 0) {
            if (bitsToShift + scaleFactor <= 62) { // Fits in long?
                return createFromLong((result << bitsToShift) * newSign);
            } else {
                BigInteger tmp = createFromLong(result * newSign);
                int newMagLen;
                uint32_t *newMag = shiftLeftArray(tmp.mag, tmp.magLen, bitsToShift, &newMagLen);
                finalize(tmp);
                return (BigInteger){tmp.signum, newMagLen, newMag};
            }
        } else {
            return createFromLong(result * newSign);
        }
    } else {
        if ((long)bitLength(n) * exponent / sizeof(uint32_t) > MAX_MAG_LENGTH) {
            printf("[BigInteger.pow] Overflow");
            exit(1);
        }

        // Large number algorithm. This is basically identical to
        // the algorithm above, but calls multiply() and square()
        // which may use more efficient algorithms for large numbers.
        BigInteger answer = createFromInt(1), tmp;

        int workingExponent = exponent;
        // Perform exponentiation using repeated squaring trick
        while (workingExponent != 0) {
            if ((workingExponent & 1) == 1) {
                tmp = multiply(answer, partToSquare);
                finalize(answer);
                answer = tmp;
            }

            if ((workingExponent >>= 1) != 0) {
                tmp = multiply(partToSquare, partToSquare);
                finalize(partToSquare);
                partToSquare = tmp;
            }
        }
        finalize(partToSquare);

        // Multiply back the (exponentiated) powers of two (quickly,
        // by shifting left)
        if (powersOfTwo > 0) {
            int newMagLen;
            uint32_t *newMag = shiftLeftArray(answer.mag, answer.magLen, bitsToShift, &newMagLen);
            finalize(answer);
            answer = (BigInteger){answer.signum, newMagLen, newMag};
        }

        if (n.signum < 0 && (exponent & 1) == 1) {
            answer.signum = -answer.signum;
        }
        return answer;
    }
}

/**
 * This method is used for division. It multiplies an n word input a by one
 * word input x, and subtracts the n word product from q. This is needed
 * when subtracting qhat*divisor from dividend.
 */
static uint32_t mulsub(uint32_t *q, uint32_t *a, uint32_t x, int len, int offset) {
    uint64_t xLong = x;
    uint64_t carry = 0;
    offset -= len;

    for (int j = 0; j < len; j++) {
        uint64_t product = a[j] * xLong + carry;
        uint64_t difference = q[offset] - product;
        q[offset++] = (uint32_t)difference;
        carry = (product >> 32) + ((((uint32_t)difference) > (((~(uint32_t)product)))) ? 1 : 0);
    }
    return (uint32_t)carry;
}

/**
 * The method is the same as mulsun, except the fact that q array is not
 * updated, the only result of the method is borrow flag.
 */
static uint32_t mulsubBorrow(uint32_t *q, uint32_t *a, uint32_t x, int len, int offset) {
    uint64_t xLong = x;
    uint64_t carry = 0;
    offset -= len;

    for (int j = 0; j < len; j++) {
        uint64_t product = a[j] * xLong + carry;
        uint64_t difference = q[offset++] - product;
        carry = (product >> 32) + ((((uint32_t)difference) > (((~(uint32_t)product)))) ? 1 : 0);
    }
    return (uint32_t)carry;
}

/**
 * A primitive used for division. This method adds in one multiple of the
 * divisor a back to the dividend result at a specified offset. It is used
 * when qhat was estimated too large, and must be adjusted.
 */
static uint32_t divadd(uint32_t *divisor, int dlen, uint32_t *result, int offset) {
    uint64_t carry = 0;
    uint64_t sum;
    for (int j = 0; j < dlen; j++) {
        sum = (uint64_t)divisor[j] + (uint64_t)result[j + offset] + carry;
        result[j + offset] = (uint32_t)sum;
        carry = sum >> 32;
    }
    return (uint32_t)carry;
}

#define PRINT(array, len) // print(#array, array, len, __func__, __LINE__)

void print(char *arrayName, int *array, int len, const char *func, int line) {
    printf("[%s](%d) %s(%d): ", func, line, arrayName, len);
    for (int i = len - 1; i >= 0; i--) {
        printf("%d ", array[i]);
    }
    printf("\n");
}

/**
 * Divide this MutableBigInteger by the divisor.
 * The quotient will be placed into the provided quotient object &
 * the remainder object is returned.
 */
static BigInteger divideMagnitude(BigInteger dividend, BigInteger div, BigInteger *quotient, int needRemainder) {
    // assert div.intLen > 1
    // D1 normalize the divisor
    int dlen = div.magLen;
    PRINT(div.mag, dlen);
    int shift = numberOfLeadingZeros(div.mag[dlen - 1]);
    // Copy divisor value to protect divisor
    uint32_t *divisorMag;

    int remMagLen;
    uint32_t *remMag;
    if (shift > 0) {
        int divisorMagLen;
        divisorMag = shiftLeftArray(div.mag, dlen, shift, &divisorMagLen);
        assert(divisorMagLen == dlen);
        remMag = shiftLeftArray(dividend.mag, dividend.magLen, shift, &remMagLen);
        if (numberOfLeadingZeros(dividend.mag[dividend.magLen - 1]) >= shift) {
            assert(remMagLen == dividend.magLen);
        } else {
            assert(remMagLen == dividend.magLen + 1);
        }
        remMag = (uint32_t *)realloc(remMag, (remMagLen + 1) * sizeof(uint32_t));
        remMag[remMagLen] = 0;
        PRINT(remMag, remMagLen + 1);
    } else {
        int i;
        divisorMag = (uint32_t *)malloc(dlen * sizeof(uint32_t));
        for (i = 0; i < dlen; i++) {
            divisorMag[i] = div.mag[i];
        }
        remMagLen = dividend.magLen;
        remMag = (uint32_t *)malloc((remMagLen + 1) * sizeof(uint32_t));
        for (i = 0; i < remMagLen; i++) {
            remMag[i] = dividend.mag[i];
        }
        remMag[remMagLen] = 0;
        PRINT(remMag, remMagLen + 1);
    }

    // Set the quotient size
    int limit = remMagLen - dlen + 1;
    quotient->signum = dividend.signum * div.signum;
    quotient->mag = (uint32_t *)malloc(limit * sizeof(uint32_t));
    quotient->magLen = limit;
    uint32_t *q = quotient->mag;

    // remMagLen++;

    uint32_t dh = divisorMag[dlen - 1];
    uint64_t dhLong = (uint64_t)dh;
    uint32_t dl = divisorMag[dlen - 2];

    // D2 Initialize j
    for (int j = 0; j < limit - 1; j++) {
        // D3 Calculate qhat
        // estimate qhat
        uint32_t qhat = 0;
        uint32_t qrem = 0;
        int skipCorrection = 0;
        uint32_t nh = remMag[remMagLen - j];
        uint32_t nm = remMag[remMagLen - j - 1];

        if (nh == dh) {
            // todo: need to confirm
            qhat = 0xFFFFFFFF;
            qrem = nh + nm;
            skipCorrection = qrem < nh;
        } else {
            uint64_t nChunk = (((uint64_t)nh) << 32) | (uint64_t)nm;
            // tofo: need to confirm the case of nChunk < 0
            qhat = (uint32_t)(nChunk / dhLong);
            qrem = (uint32_t)(nChunk - qhat * dhLong);
        }

        if (qhat == 0) {
            q[limit - 1 - j] = 0;
            continue;
        }

        if (!skipCorrection) { // Correct qhat
            uint64_t nl = (uint64_t)remMag[remMagLen - j - 2];
            uint64_t rs = (((uint64_t)qrem) << 32) | nl;
            uint64_t estProduct = ((uint64_t)dl) * ((uint64_t)qhat);

            if (estProduct > rs) {
                qhat--;
                qrem = (uint32_t)(qrem + dhLong);
                if (qrem >= dhLong) {
                    estProduct -= dl;
                    rs = (((uint64_t)qrem) << 32) | nl;
                    if (estProduct > rs)
                        qhat--;
                }
            }
        }

        // D4 Multiply and subtract
        remMag[remMagLen - j] = 0;
        uint32_t borrow = mulsub(remMag, divisorMag, qhat, dlen, remMagLen - j);

        // D5 Test remainder
        if (borrow > nh) {
            // D6 Add back
            divadd(divisorMag, dlen, remMag, remMagLen - dlen - j);
            qhat--;
        }

        // Store the quotient digit
        q[limit - 1 - j] = qhat;
    } // D7 loop on j
      // D3 Calculate qhat
      // estimate qhat
    uint32_t qhat = 0;
    uint32_t qrem = 0;
    int skipCorrection = 0;
    uint32_t nh = remMag[dlen];
    uint32_t nm = remMag[dlen - 1];

    if (nh == dh) {
        qhat = ~0;
        qrem = nh + nm;
        skipCorrection = qrem < nh;
    } else {
        uint64_t nChunk = (((uint64_t)nh) << 32) | (uint64_t)nm;
        qhat = (uint32_t)(nChunk / dhLong);
        qrem = (uint32_t)(nChunk - (qhat * dhLong));
    }
    if (qhat != 0) {
        if (!skipCorrection) { // Correct qhat
            uint32_t nl = remMag[dlen - 2];
            uint64_t rs = ((uint64_t)qrem << 32) | (uint64_t)nl;
            uint64_t estProduct = ((uint64_t)dl) * ((uint64_t)qhat);

            if (estProduct > rs) {
                qhat--;
                qrem = (uint32_t)(qrem + dhLong);
                if (qrem >= dhLong) {
                    estProduct -= (uint64_t)dl;
                    rs = ((uint64_t)qrem << 32) | (uint64_t)nl;
                    if (estProduct > rs)
                        qhat--;
                }
            }
        }

        // D4 Multiply and subtract
        uint32_t borrow;
        remMag[dlen] = 0;
        if (needRemainder)
            borrow = mulsub(remMag, divisorMag, qhat, dlen, dlen);
        else
            borrow = mulsubBorrow(remMag, divisorMag, qhat, dlen, dlen);

        // D5 Test remainder
        if (borrow > nh) {
            // D6 Add back
            if (needRemainder)
                divadd(divisorMag, dlen, remMag, 0);
            qhat--;
        }

        // Store the quotient digit
        q[0] = qhat;
    } else {
        q[0] = 0;
    }

    free(divisorMag);

    int i;
    for (i = quotient->magLen - 1; i >= 0; i--) {
        if (quotient->mag[i] != 0) {
            break;
        }
    }
    if (i == -1) {
        free(quotient->mag);
        *quotient = ZERO;
    } else if (i < quotient->magLen - 1) {
        quotient->magLen = i + 1;
        quotient->mag = (uint32_t *)realloc(quotient->mag, (i + 1) * sizeof(uint32_t));
    }

    BigInteger rem;
    if (needRemainder) {
        // D8 Unnormalize
        for (i = remMagLen; i >= 0; i--) {
            if (remMag[i] != 0) {
                break;
            }
        }
        if (i == -1) {
            free(remMag);
            rem = ZERO;
        } else if (i < remMagLen) {
            remMagLen = i + 1;
            remMag = (uint32_t *)realloc(remMag, remMagLen * sizeof(uint32_t));
            rem = (BigInteger){1, remMagLen, remMag};

            if (shift > 0) {
                BigInteger tmp = shiftRight(rem, shift);
                free(remMag);
                rem = tmp;
            }
        }
        PRINT(rem.mag, rem.magLen);
        PRINT(quotient->mag, quotient->magLen);
        return rem;
    } else {
        free(remMag);
        return *quotient;
    }
}

static uint32_t divideOneWord(BigInteger dividend, uint32_t divisor, BigInteger *quotient) {
    uint64_t divisorLong = (uint64_t)divisor;
    int intLen = dividend.magLen;
    
    quotient->signum = 1;

    // Special case of one word dividend
    if (intLen == 1) {
        uint64_t dividendValue = (uint64_t)dividend.mag[0];
        uint32_t q = (uint32_t)(dividendValue / divisorLong);
        uint32_t r = (uint32_t)(dividendValue - q * divisorLong);
        quotient->magLen = 1;
        quotient->mag = (uint32_t *)malloc(1 * sizeof(uint32_t));
        quotient->mag[0] = q;
        return r;
    }

    quotient->magLen = intLen;
    quotient->mag = (uint32_t *)malloc(intLen * sizeof(uint32_t));

    // Normalize the divisor
    int shift = numberOfLeadingZeros(divisor);

    uint64_t remLong = (uint64_t)dividend.mag[intLen - 1];
    if (remLong < divisorLong) {
        quotient->mag[intLen - 1] = 0;
    } else {
        quotient->mag[intLen - 1] = (uint32_t)(remLong / divisorLong);
        remLong -= quotient->mag[intLen - 1] * divisorLong;
    }
    int xlen = intLen - 1;
    uint64_t dividendEstimate;
    while (--xlen >= 0) {
        dividendEstimate = (remLong << 32) | dividend.mag[xlen];
        quotient->mag[xlen] = (uint32_t)(dividendEstimate / divisorLong);
        remLong = dividendEstimate - quotient->mag[xlen] * divisorLong;
    }

    int i;
    for (i = quotient->magLen - 1; i >= 0; i--) {
        if (quotient->mag[i] != 0) {
            break;
        }
    }
    if (i == -1) {
        free(quotient->mag);
        *quotient = ZERO;
    } else if (i < quotient->magLen - 1) {
        quotient->magLen = i + 1;
        quotient->mag = (uint32_t *)realloc(quotient->mag, (i + 1) * sizeof(uint32_t));
    }

    return (uint32_t)remLong;
}

static BigInteger divideKnuth(BigInteger dividend, BigInteger div, BigInteger *quotient, int needRemainder) {
    if (div.signum == 0) {
        printf("div is zero\n");
        exit(1);
    }
    if (dividend.signum == 0) {
        *quotient = ZERO;
        return ZERO;
    }

    int cmp = compareMagnitude(dividend, div);
    if (cmp < 0) {
        *quotient = ZERO;
        uint32_t *remMag = (uint32_t *)malloc(dividend.magLen * sizeof(uint32_t));
        for (int i = 0; i < dividend.magLen; i++) {
            remMag[i] = dividend.mag[i];
        }
        return (BigInteger){dividend.signum, dividend.magLen, remMag};
    }
    if (cmp == 0) {
        *quotient = createFromInt(1);
        return ZERO;
    }

    // Special case one word divisor
    if (div.magLen == 1) {
        uint32_t r = divideOneWord(dividend, div.mag[0], quotient);
        if (needRemainder) {
            if (r == 0)
                return ZERO;
            return createFromInt(r);
        } else {
            return *quotient;
        }
    }

    return divideMagnitude(dividend, div, quotient, needRemainder);
}

static double lgTwo = 0;

static char *smallToString(BigInteger n, char *buf, int digits) {
    require(n.signum >= 0, "n must be non-negative");

    if (n.signum == 0) {
        while (digits > 0) {
            *buf++ = '0';
            digits--;
        }
        *buf = '\0';
        return buf;
    }

    // Compute upper bound on number of digit groups and allocate space
    int maxNumDigitGroups = (4 * n.magLen + 6) / 7;
    long digitGroups[maxNumDigitGroups];

    // Translate number to string, a digit group at a time
    BigInteger tmp = n, d = createFromLong(0xde0b6b3a7640000L), q, r;
    int numGroups = 0;
    while (tmp.signum != 0) {
        r = divideKnuth(tmp, d, &q, 1);
        q.signum = q.signum == 0 ? 0 : tmp.signum * d.signum;
        r.signum = r.signum == 0 ? 0 : tmp.signum * d.signum;

        digitGroups[numGroups++] = r.magLen == 0 ? 0 : (r.magLen == 1 ? (long)r.mag[0] : ((long)r.mag[1] << 32) | r.mag[0]);
        finalize(r);
        if (tmp.mag != n.mag) {
            finalize(tmp);
        }
        tmp = q;
    }
    finalize(d);

    // Get string version of first digit group
    char s[25];
    sprintf(s, "%ld", digitGroups[numGroups - 1]);

    // Pad with internal zeros if necessary.
    int numZeros = digits - (strlen(s) + (numGroups - 1) * 18);
    while (numZeros > 0) {
        *buf++ = '0';
        numZeros--;
    }
    strcpy(buf, s);
    buf += strlen(s);

    // Append remaining digit groups each padded with leading zeros
    for (int i = numGroups - 2; i >= 0; i--) {
        // Prepend (any) leading zeros for this digit group
        sprintf(s, "%ld", digitGroups[i]);
        int numLeadingZeros = 18 - strlen(s);
        if (numLeadingZeros != 0) {
            while (numLeadingZeros > 0) {
                *buf++ = '0';
                numLeadingZeros--;
            }
        }
        strcpy(buf, s);
        buf += strlen(s);
    }
    return buf;
}

static int cacheLineLen = 0;
static BigInteger *cacheLine = NULL;

/**
 * Returns the value 10^(2^exponent) from the cache.
 * If this value doesn't already exist in the cache, it is added.
 */
static BigInteger getRadixConversionCache(int exponent) {
    if (exponent < cacheLineLen) {
        return cacheLine[exponent];
    }
    int oldLength = cacheLineLen;
    cacheLineLen = exponent + 1;
    if (cacheLine == NULL) {
        cacheLine = (BigInteger *)malloc(cacheLineLen * sizeof(BigInteger));
    } else {
        cacheLineLen = exponent + 1;
        cacheLine = (BigInteger *)realloc(cacheLine, cacheLineLen * sizeof(BigInteger));
    }
    for (int i = oldLength; i <= exponent; i++) {
        if (i == 0) {
            cacheLine[i] = iBigInteger.createFromInt(10);
        } else {
            cacheLine[i] = iBigInteger.multiply(cacheLine[i - 1], cacheLine[i - 1]);
        }
    }

    return cacheLine[exponent];
}

static char *recursiveToString(BigInteger u, char *buf, int digits) {
    require(u.signum >= 0, "u must be non-negative");

    // If we're smaller than a certain threshold, use the smallToString
    // method, padding with leading zeroes when necessary unless we're
    // at the beginning of the string or digits <= 0. As u.signum() >= 0,
    // smallToString() will not prepend a negative sign.
    if (u.magLen <= SCHOENHAGE_BASE_CONVERSION_THRESHOLD) {
        return smallToString(u, buf, digits);
    }

    // Calculate a value for n in the equation radix^(2^n) = u
    // and subtract 1 from that value. This is used to find the
    // cache index that contains the best value to divide u.
    if (lgTwo == 0) {
        lgTwo = log10(2);
    }
    int n = (int)round(log2(bitLength(u) * lgTwo) - 1.0);

    BigInteger v, q, r;
    v = getRadixConversionCache(n);
    r = divideKnuth(u, v, &q, 1);

    int expectedDigits = 1 << n;

    // Now recursively build the two halves of each number.
    buf = recursiveToString(q, buf, digits - expectedDigits);
    buf = recursiveToString(r, buf, expectedDigits);
    finalize(q);
    finalize(r);
    return buf;
}

/**
 * 将大整数转换为字符串
 * @param n 大整数
 * @return 大整数的字符串表示
 */
static char *toString(BigInteger n) {
    char *str;
    if (n.signum == 0) {
        str = (char *)malloc(2 * sizeof(char));
        sprintf(str, "%d", 0);
        return str;
    }

    BigInteger abs = n;
    abs.signum = 1;

    int b = bitLength(abs);
    if (lgTwo == 0) {
        lgTwo = log10(2);
    }
    int numChars = (int)(lgTwo * b + 1) + (n.signum < 0 ? 1 : 0);
    str = (char *)malloc((numChars + 1) * sizeof(char));
    char *p = str;
    if (n.signum < 0) {
        *p++ = '-';
    }
    // use recursive toString
    recursiveToString(abs, p, 0);
    return str;
}

static const long LONG_TEN_POWERS_TABLE[] = {
    1,                   // 0 / 10^0
    10,                  // 1 / 10^1
    100,                 // 2 / 10^2
    1000,                // 3 / 10^3
    10000,               // 4 / 10^4
    100000,              // 5 / 10^5
    1000000,             // 6 / 10^6
    10000000,            // 7 / 10^7
    100000000,           // 8 / 10^8
    1000000000,          // 9 / 10^9
    10000000000L,        // 10 / 10^10
    100000000000L,       // 11 / 10^11
    1000000000000L,      // 12 / 10^12
    10000000000000L,     // 13 / 10^13
    100000000000000L,    // 14 / 10^14
    1000000000000000L,   // 15 / 10^15
    10000000000000000L,  // 16 / 10^16
    100000000000000000L, // 17 / 10^17
    1000000000000000000L // 18 / 10^18
};

#define BIG_TEN_POWERS_TABLE_INITLEN 19
#define BIG_TEN_POWERS_TABLE_MAX 304

int bigTenPowersTableLen = 0;
static BigInteger *BIG_TEN_POWERS_TABLE;

static BigInteger expandBigIntegerTenPowers(int n) {
    BigInteger *pows = BIG_TEN_POWERS_TABLE;
    int curLen = bigTenPowersTableLen, newLen, i;
    if (curLen <= n) {
        newLen = curLen << 1;
        while (newLen <= n) {
            newLen <<= 1;
        }
        pows = (BigInteger *)realloc(pows, newLen * sizeof(BigInteger));
        for (i = curLen; i < newLen; i++) {
            pows[i] = iBigInteger.multiply(pows[i - 1], pows[1]);
        }
        BIG_TEN_POWERS_TABLE = pows;
    }
    return pows[n];
}

static BigInteger bigMultiplyPowerTen(BigInteger value, int n) {
    if (n < 0) {
        printf("bigint.bigMultiplyPowerTen: n is negative\n");
        exit(1);
    }

    if (bigTenPowersTableLen == 0) {
        bigTenPowersTableLen = BIG_TEN_POWERS_TABLE_INITLEN;
        BIG_TEN_POWERS_TABLE = (BigInteger *)malloc(bigTenPowersTableLen * sizeof(BigInteger));
        for (int i = 0; i < bigTenPowersTableLen; i++) {
            BIG_TEN_POWERS_TABLE[i] = iBigInteger.createFromLong(LONG_TEN_POWERS_TABLE[i]);
        }
    }

    BigInteger tenPower;

    if (n < BIG_TEN_POWERS_TABLE_MAX) {
        if (n < bigTenPowersTableLen) {
            tenPower = BIG_TEN_POWERS_TABLE[n];
        } else {
            tenPower = expandBigIntegerTenPowers(n);
        }
        return iBigInteger.multiply(value, tenPower);
    } else {
        tenPower = iBigInteger.pow(BIG_TEN_POWERS_TABLE[1], n);
        BigInteger ret = iBigInteger.multiply(value, tenPower);
        finalize(tenPower);
        return ret;
    }
}

BigInteger ZERO = {0, 0, NULL};

BigIntegerInterface iBigInteger = {
    .createFromInt = createFromInt,
    .createFromLong = createFromLong,
    .toString = toString,
    .createFromHexString = createFromHexString,
    .toHexString = toHexString,
    .finalize = finalize,
    .add = add,
    .subtractByInt = subtractByInt,
    .subtract = subtract,
    .multiplyByInt = multiplyByInt,
    .multiply = multiply,
    .pow = powForBigInteger,
    .bigMultiplyPowerTen = bigMultiplyPowerTen,
    .divideKnuth = divideKnuth,
    .compareMagnitude = compareMagnitude,
};
