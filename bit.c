#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

// number of bits per byte/char
#define BITS_PER_BYTE 8

// number of bits per long
#define BITS_PER_LONG (sizeof(unsigned long) * BITS_PER_BYTE)

/**
 * DECLARE_BITMAP - declare bitmap to store at least bit 0..(bits -1)
 * @bitmap: name for the new bitmap
 * @bits: number of required bits
 */
#define DECLARE_BITMAP(bitmap, bits) unsigned long bitmap[BITS_TO_LONGS(bits)]

/**
 * BITOPS_DIV_CEIL - calculate quotient of integer division (round up)
 * @numerator: side effect free expression for numerator of division
 * @denominator: side effect free expression for denominator of division
 *
 * numerator and denominator must be from a type which can store
 * denominator + numerator without overflow. denominator must be larger than 0
 * and numerator must be positive.
 *
 * WARNING @numerator expression must be side-effect free
 */
#define BITOPS_DIV_CEIL(numerator, denominator) \
    (((numerator) + (denominator) -1) / (denominator))

// BITS_TO_LONGS - return number of longs to save at least bit 0..(bits - 1)
#define BITS_TO_LONGS(bits) BITOPS_DIV_CEIL(bits, BITS_PER_LONG)

/**
 * bitops_ffs() - find (least significant) first set bit plus one
 * @x: unsigned long to check
 * Return: plus-one index of first set bit; zero when x is zero
 */
static inline size_t bitops_ffs(unsigned long x) {
    return __builtin_ffsl(x);
}

/**
 * test_bit() - Get state of bit
 * @bit: address of bit to test
 * @bitmap: bitmap to test
 * Return: true when bit is one and false when bit is zero
 */
static inline bool test_bit(size_t bit, const unsigned long *bitmap) {
    size_t l = bit / BITS_PER_LONG;
    size_t b = bit % BITS_PER_LONG;

    return !!(bitmap[l] & (1UL << b));
}

/**
 * BITMAP_FIRST_WORD_MASK - return unsigned long mask for least significant long
 * @start: offset to first bits
 *
 * All bits which can be modified in the least significant unsigned long for
 * offset @start in the bitmap will be set to 1. All other bits will be set to
 * zero
 */
#define BITMAP_FIRST_WORD_MASK(start) (~0UL << ((start) % BITS_PER_LONG))

/**
 * BITMAP_LAST_WORD_MASK - return unsigned long mask for most significant long
 * @bits: number of bits in complete bitmap
 *
 * All bits which can be modified in the most significant unsigned long in the
 * bitmap will be set to 1. All other bits will be set to zero
 */
#define BITMAP_LAST_WORD_MASK(bits) (~0UL >> (-(bits) % BITS_PER_LONG))

/**
 * find_next_zero_bit() - Find next clear bit in bitmap
 * @bitmap: bitmap to check
 * @bits: number of bits in @bitmap
 * @start: start of bits to check
 *
 * Checks the modifiable bits in the bitmap. The overhead bits in the last
 * unsigned long will not be checked
 *
 * Return: bit position of next clear bit, @bits when no clear bit was found
 */
static inline size_t find_next_zero_bit(const unsigned long *bitmap,
                                        size_t bits,
                                        size_t start)
{
    size_t pos;
    unsigned long t;
    size_t l = BITS_TO_LONGS(bits);
    size_t first_long = start / BITS_PER_LONG;
    size_t long_lower = start - (start % BITS_PER_LONG);

    if (start >= bits)
        return bits;

    t = bitmap[first_long] | ~BITMAP_FIRST_WORD_MASK(start);
    t ^= ~0UL;

    for (size_t i = first_long + 1; !t && i < l; i++) {
        /* search until valid t is found */
        long_lower += BITS_PER_LONG;
        t = bitmap[i];
        t ^= ~0UL;
    }

    if (!t)
        return bits;

    pos = long_lower + bitops_ffs(t) - 1;
    if (pos >= bits)
        return bits;

    return pos;
}

#define MAX_TEST_BITS 400
static DECLARE_BITMAP(test, MAX_TEST_BITS);

static size_t find_next(unsigned long *bitmap, size_t bits, size_t start)
{
    for (size_t i = start; i < bits; i++) {
        if (test_bit(i, bitmap))
            return i;
    }

    return bits;
}

/**
 * find_next_bit() - Find next set bit in bitmap
 * @bitmap: bitmap to check
 * @bits: number of bits in @bitmap
 * @start: start of bits to check
 *
 * Checks the modifiable bits in the bitmap. The overhead bits in the last
 * unsigned long will not be checked
 *
 * Return: bit position of next set bit, @bits when no set bit was found
 */
static inline size_t find_next_bit(const unsigned long *bitmap,
                                   size_t bits,
                                   size_t start)
{
    unsigned long t;
    size_t l = BITS_TO_LONGS(bits);
    size_t first_long = start / BITS_PER_LONG;
    size_t long_lower = start - (start % BITS_PER_LONG);

    if (start >= bits)
        return bits;

    t = bitmap[first_long] & BITMAP_FIRST_WORD_MASK(start);
    for (size_t i = first_long + 1; !t && i < l; i++) {
        long_lower += BITS_PER_LONG;
        t = bitmap[i];
//      t ^= ~0UL;
    }
    
    if (!t)
        return bits;

    size_t pos;
    pos = long_lower + bitops_ffs(t) - 1; //bitops_ffs 找到最右邊bit=1 的位置
    if (pos >= bits)
        return bits;
    return pos;
}

static void check_bits(unsigned long *bitmap, size_t bits) {
    size_t i = 0;
    while (i < bits) {
        size_t r1 = find_next(bitmap, bits, i);
        size_t r2 = find_next_bit(bitmap, bits, i);
        assert(r1 == r2);
        i = r1 + 1;
    }
}

/**
 * bitmap_fill() - Initializes bitmap with one
 * @bitmap: bitmap to modify
 * @bits: number of bits
 *
 * Initializes all modifiable bits to one. The overhead bits in the last
 * unsigned long will be set to zero.
 */
static inline void bitmap_fill(unsigned long *bitmap, size_t bits) {
    size_t l = BITS_TO_LONGS(bits);
    if (l > 1)
        memset(bitmap, 0xff, (l - 1) * sizeof(unsigned long));
    bitmap[l - 1] = BITMAP_LAST_WORD_MASK(bits);
}

/**
 * bitmap_zero() - Initializes bitmap with zero
 * @bitmap: bitmap to modify
 * @bits: number of bits
 *
 * Initializes all bits to zero. This also includes the overhead bits in the
 * last unsigned long which will not be used.
 */
static inline void bitmap_zero(unsigned long *bitmap, size_t bits) {
    memset(bitmap, 0, BITS_TO_LONGS(bits) * sizeof(unsigned long));
}

int main(void) {
    for (int i = 1; i < MAX_TEST_BITS; i++) {
        bitmap_fill(test, i);
        assert(find_next_bit(test, i, 0) == 0);

        bitmap_zero(test, i);
        assert(find_next_bit(test, i, 0) == i);
    }
    return 0;
}
