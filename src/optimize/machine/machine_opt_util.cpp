#include "machine_optimize.h"

unsigned int countPowerOfTwo(unsigned int x) {
    int judge = 0, pos = -1;
    x = x & 0x7fffffffU;
    for (int i = 0; i < 32; ++i) {
        if (x & 0x1U) {
            ++judge;
            if (pos == -1) pos = i;
        }
        x = x >> 1U;
    }
    return judge != 1 ? 0xffffffffU : pos;
}

unsigned int calDivMagicNumber(unsigned int x) {
    int l = ceil(log2(x));
    unsigned long long low = (1ULL << (unsigned) (WORD_BIT + l)) / x;
    unsigned long long high = ((1ULL << (unsigned) (WORD_BIT + l)) + (1ULL << (unsigned) (l + 1))) / x;
    while (low / 2 < high / 2 && l > 0) {
        low /= 2;
        high /= 2;
        l--;
    }
    return high;
}

unsigned int calDivShiftNumber(unsigned int x) {
    int l = ceil(log2(x));
    unsigned long long low = (1ULL << (unsigned) (WORD_BIT + l)) / x;
    unsigned long long high = ((1ULL << (unsigned) (WORD_BIT + l)) + (1ULL << (unsigned) (l + 1))) / x;
    while (low / 2 < high / 2 && l > 0) {
        low /= 2;
        high /= 2;
        l--;
    }
    return l;
}

const int TABLE_LEN = 30;

unsigned int powTable[TABLE_LEN] = {0x2, 0x4, 0x8, 0x10, 0x20, 0x40, 0x80, 0x100, 0x200, 0x400, 0x800, 0x1000, 0x2000,
                                   0x4000,
                                   0x8000, 0x10000, 0x20000, 0x40000, 0x80000, 0x100000, 0x200000, 0x400000, 0x800000,
                                   0x1000000, 0x2000000, 0x4000000, 0x8000000, 0x10000000, 0x20000000, 0x40000000};

/*
 * 0  : false
 * 1  : true for 2^n * (2^m + 1)
 * -1 : true for 2^n * (2^m - 1)
 * ret is a vector, true for triple{1/-1, shift, tableIndex + 1}, false for 0
 * tableIndex + 1 is another shift.
 */
vector<int> canConvertTwoPowMul(unsigned int x) {
    int shift;
    for (int i = 0; i < TABLE_LEN; i++) {
        if (x % powTable[i] == 0) {
            if ((shift = countPowerOfTwo((x / powTable[i]) + 1)) != 0xffffffff) {
                return vector<int>({-1, shift, i + 1});
            } else if ((shift = countPowerOfTwo((x / powTable[i]) - 1)) != 0xffffffff) {
                return vector<int>({1, shift, i + 1});
            } else {
                continue;
            }
        }
    }
    return vector<int>({0});
}
