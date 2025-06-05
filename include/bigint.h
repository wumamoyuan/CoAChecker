#ifndef BN_JAVA_H
#define BN_JAVA_H

#include <stdint.h>

typedef struct _BigInteger {
    int signum;
    int magLen;
    uint32_t *mag;
} BigInteger;

typedef struct _BigIntegerInterface {
    /* Initialization functions: */
    void (*bigInteger_init)(BigInteger *n);
    BigInteger (*createFromInt)(int i);
    BigInteger (*createFromLong)(long i);
    // int (*toInt)(BigInteger *n);
    // void (*createFromString)(BigInteger *n, char *str, int nbytes);
    char *(*toString)(BigInteger n);

    BigInteger (*createFromHexString)(char *hex);
    // 将大整数转换为十六进制字符串，不包括0x前缀，返回的内存需要free
    char *(*toHexString)(BigInteger n);

    void (*finalize)(BigInteger n);

    BigInteger (*add)(BigInteger n, BigInteger m);
    BigInteger (*subtractByInt)(BigInteger n, int m);
    BigInteger (*subtract)(BigInteger n, BigInteger m);
    BigInteger (*multiplyByInt)(BigInteger n, int m);
    BigInteger (*multiply)(BigInteger n, BigInteger m);
    BigInteger (*pow)(BigInteger n, int m);
    BigInteger (*bigMultiplyPowerTen)(BigInteger n, int m);
    BigInteger (*divideKnuth)(BigInteger dividend, BigInteger div, BigInteger *quotient, int needRemainder);
    int (*compareMagnitude)(BigInteger n, BigInteger m);
} BigIntegerInterface;

extern BigInteger ZERO;
extern BigIntegerInterface iBigInteger;

#endif // BN_JAVA_H
