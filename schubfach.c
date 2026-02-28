#include "schubfach.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

enum {
    MAX_DIGITS_UINT64 = 64,
    MAX_DIGITS_DOUBLE = 53,
    MAX_DIGITS10_DOUBLE = 17,
    POW10_COUNT = 617,
    DEC_EXP_MIN = -292,
};

typedef struct {
    uint64_t hi;
    uint64_t lo;
} uint128;

static uint128 umul128(uint64_t x, uint64_t y){
    uint128 result;
#ifdef __SIZEOF_INT128__
    unsigned __int128 xy = (unsigned __int128)x * y;
    result.hi = (uint64_t)(xy >> 64);
    result.lo = (uint64_t)xy;
#else
    uint64_t mask = 0xffffffffULL;
    uint64_t a = x >> 32;
    uint64_t b = x & mask;
    uint64_t c = y >> 32;
    uint64_t d = y & mask;
    uint64_t ac = a * c;
    uint64_t bc = b * c;
    uint64_t ad = a * d;
    uint64_t bd = b * d;
    uint64_t intermediate = (bd >> 32) + (ad & mask) + (bc & mask);
    result.hi = ac + (intermediate >> 32) + (ad >> 32) + (bc >> 32);
    result.lo = (intermediate << 32) + (bd & mask);
#endif
    return result;
}

static int floor_log2_pow10(int e){
    int64_t n = 1741647ll * e;
    return n >= 0 ? (int)(n >> 19) : -(int)(((-n) + ((1ll << 19) - 1)) >> 19);
}

static int floor_log10_pow2(int e){
    return (int)((e * 661971961083ll) >> 41);
}

static void init_pow10_significands(uint128* out){
    static const uint64_t correction_bits[] = {
        0xb6e84cca18114965ULL, 0x45912150d29a12c8ULL, 0x4f44ca218d4992d8ULL,
        0x242261af29129a8aULL, 0xcd514a66dULL, 0x856512b828000000ULL,
        0x4d661b91a98a354cULL, 0x8504522726522549ULL, 0xc8892cd122aad974ULL,
        0x212895624ULL
    };
    uint128 v = {0x3fddec7f2faf3713ULL, 0xc97a3a2704eec3deULL};
    for (int i = 0, e = DEC_EXP_MIN; i < POW10_COUNT; ++i, ++e){
        uint64_t lo = v.lo + 1;
        uint64_t hi = v.hi + (lo == 0);
        out[i].hi = (hi << 1) | (lo >> 63);
        out[i].lo = lo & 0x7fffffffffffffffULL;
        if (i + 1 == POW10_COUNT) break;

        uint128 p0 = umul128(v.lo, 5);
        uint128 p1 = umul128(v.hi, 5);
        uint64_t m = p1.lo + p0.hi;
        uint64_t t = p1.hi + (m < p1.lo);
        int s = floor_log2_pow10(e + 1) - floor_log2_pow10(e) - 1;
        if (s == 2){
            v.hi = (m >> 2) | (t << 62);
            v.lo = (p0.lo >> 2) | (m << 62);
        } else {
            v.hi = (m >> 3) | (t << 61);
            v.lo = (p0.lo >> 3) | (m << 61);
        }
        if ((correction_bits[i >> 6] >> (i & 63)) & 1){
            if (++v.lo == 0) ++v.hi;
        }
    }
}

static uint64_t umul192_upper64_modified(
    uint64_t pow10_hi, uint64_t pow10_lo, uint64_t scaled_sig
){
    uint64_t x_hi = umul128(pow10_lo, scaled_sig).hi;
    uint128 y = umul128(pow10_hi, scaled_sig);
    uint64_t z = (y.lo >> 1) + x_hi;
    uint64_t result = y.hi + (z >> 63);
    uint64_t mask = ((uint64_t)1 << 63) - 1;
    return result | (((z & mask) + mask) >> 63);
}

static char* write8digits(char* buffer, unsigned n){
    int shift = 28;
    uint64_t magic = 193428131138340668ULL;
    unsigned y = (unsigned)(umul128(((uint64_t)n + 1) << shift, magic).hi >> 20) - 1;
    for (int i = 0; i < 8; ++i){
        unsigned t = 10 * y;
        *buffer++ = (char)('0' + (t >> shift));
        y = t & ((1u << shift) - 1);
    }
    return buffer;
}

static uint64_t pow10[18];

static void write_dec(char* buffer, uint64_t dec_sig, int dec_exp){
    int len = floor_log10_pow2(MAX_DIGITS_UINT64 - __builtin_clzll(dec_sig));
    if (dec_sig >= pow10[len]) ++len;
    dec_sig *= pow10[MAX_DIGITS10_DOUBLE - len];
    dec_exp += len - 1;

    int pow10_8 = 100000000;
    unsigned hi = (unsigned)(dec_sig / (uint64_t)pow10_8);
    *buffer++ = (char)('0' + hi / (unsigned)pow10_8);
    *buffer++ = '.';
    buffer = write8digits(buffer, hi % (unsigned)pow10_8);
    {
        unsigned lo = (unsigned)(dec_sig % (uint64_t)pow10_8);
        if (lo != 0) buffer = write8digits(buffer, lo);
    }

    while (buffer[-1] == '0') --buffer;
    if (buffer[-1] == '.') --buffer;

    *buffer++ = 'e';
    if (dec_exp < 0){
        *buffer++ = '-';
        dec_exp = -dec_exp;
    } else {
        *buffer++ = '+';
    }
    if (dec_exp >= 100){
        *buffer++ = (char)('0' + dec_exp / 100);
        dec_exp %= 100;
    }
    *buffer++ = (char)('0' + dec_exp / 10);
    *buffer++ = (char)('0' + dec_exp % 10);
    *buffer = '\0';
}

void schubfach_dtoa(double value, char* buffer){
    static int initialized = 0;
    static uint128 pow10_significands[POW10_COUNT];
    if (!initialized){
        init_pow10_significands(pow10_significands);
        uint64_t x = 1;
        for (int i = 0; i < 18; i++){
            pow10[i] = x;
            x *= 10;
        }
        initialized = 1;
    }

    uint64_t u;
    memcpy(&u, &value, sizeof(u));

    *buffer = '-';
    buffer += u >> 63;

    int num_sig_bits = MAX_DIGITS_DOUBLE - 1;
    int exp_mask = 0x7ff;
    int bin_exp = (int)(u >> num_sig_bits) & exp_mask;

    uint64_t implicit_bit = (uint64_t)1 << num_sig_bits;
    uint64_t bin_sig = u & (implicit_bit - 1);  // binary significand

    int regular = bin_sig != 0;
    if (((bin_exp + 1) & exp_mask) <= 1){
        if (bin_exp != 0){
            memcpy(buffer, bin_sig == 0 ? "inf" : "nan", 4);
            return;
        }
        if (bin_sig == 0){
            memcpy(buffer, "0", 2);
            return;
        }
        // Handle subnormals.
        bin_sig ^= implicit_bit;
        ++bin_exp;
        regular = 1;
    }
    bin_sig ^= implicit_bit;
    bin_exp -= num_sig_bits + 1023;  // Remove the exponent bias.

    // Handle small integers.
    if ((bin_exp < 0) && (bin_exp >= -num_sig_bits)){
        uint64_t f = bin_sig >> -bin_exp;
        if ((f << -bin_exp) == bin_sig){
            write_dec(buffer, f, 0);
            return;
        }
    }

    // Shift the significand so that boundaries are integer.
    uint64_t bin_sig_shifted = bin_sig << 2;

    // Compute the shifted boundaries of the rounding interval (Rv).
    uint64_t lower = bin_sig_shifted - (regular ? 2 : 1);
    uint64_t upper = bin_sig_shifted + 2;

    // Compute the decimal exponent as floor(log10(2**bin_exp)) if regular or
    // floor(log10(3/4 * 2**bin_exp)) otherwise, without branching.
    int dec_exp = (int)((bin_exp * 661971961083ll +
                  (!regular ? -274743187321ll : 0)) >> 41);

    uint128 p = pow10_significands[-dec_exp - DEC_EXP_MIN];
    uint64_t pow10_hi = p.hi;
    uint64_t pow10_lo = p.lo;

    // floor(log2(10**-dec_exp))
    int pow10_bin_exp = (-dec_exp * 217707) >> 16;

    // Shift to ensure the intermediate result in umul192_upper64_modified has
    // a fixed 128-bit fractional width.
    int shift = bin_exp + pow10_bin_exp + 2;

    uint64_t scaled_sig =
        umul192_upper64_modified(pow10_hi, pow10_lo, bin_sig_shifted << shift);

    // Compute the estimates of lower and upper bounds of the rounding interval
    // by multiplying them by the power of 10 and applying modified rounding.
    lower = umul192_upper64_modified(pow10_hi, pow10_lo, lower << shift);
    upper = umul192_upper64_modified(pow10_hi, pow10_lo, upper << shift);

    uint64_t dec_sig_under = scaled_sig >> 2;
    uint64_t bin_sig_lsb = bin_sig & 1;
    if (dec_sig_under >= 10){
        // Compute the significands of the under- and overestimate.
        uint64_t dec_sig_under2 = 10 * (dec_sig_under / 10);
        uint64_t dec_sig_over2 = dec_sig_under2 + 10;
        // Check if the under- and overestimates are in the interval.
        int under_in = lower + bin_sig_lsb <= (dec_sig_under2 << 2);
        int over_in = (dec_sig_over2 << 2) + bin_sig_lsb <= upper;
        if (under_in != over_in){
            write_dec(buffer, under_in ? dec_sig_under2 : dec_sig_over2, dec_exp);
            return;
        }
    }

    uint64_t dec_sig_over = dec_sig_under + 1;
    int under_in = lower + bin_sig_lsb <= (dec_sig_under << 2);
    int over_in = (dec_sig_over << 2) + bin_sig_lsb <= upper;
    if (under_in != over_in){
        // Only one of dec_sig_under or dec_sig_over is in the rounding interval.
        write_dec(buffer, under_in ? dec_sig_under : dec_sig_over, dec_exp);
        return;
    }

    // Both dec_sig_under and dec_sig_over are in the interval - pick the closest.
    int cmp = (int)(scaled_sig - ((dec_sig_under + dec_sig_over) << 1));
    int under_closer = cmp < 0 || (cmp == 0 && (dec_sig_under & 1) == 0);
    write_dec(buffer, under_closer ? dec_sig_under : dec_sig_over, dec_exp);
}
