#include "schubfach.h"
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <float.h>
#include <math.h>
#include <charconv>
#include <limits>
#include <chrono>

double ns(){
    using namespace std::chrono;
    auto now = high_resolution_clock::now();
    auto epoch = now.time_since_epoch();
    return duration_cast<nanoseconds>(epoch).count();
}

double time_schubfach = 0.0;
double time_to_chars = 0.0;

void test(double x){
    char buf[SCHUBFACH_BUFFER_SIZE];

    double start = ns();
    schubfach_dtoa(x, buf);
    time_schubfach += ns() - start;

    if (x == 0.0){
        if (std::signbit(x)){
            assert(0 == strcmp(buf, "-0"));
        } else {
            assert(0 == strcmp(buf, "0"));
        }
        return;
    }

    char expected_buf[SCHUBFACH_BUFFER_SIZE];
    start = ns();
    std::to_chars_result res = std::to_chars(expected_buf, expected_buf + SCHUBFACH_BUFFER_SIZE, x, std::chars_format::scientific);
    time_to_chars += ns() - start;
    char* ptr = res.ptr;
    std::errc ec = res.ec;
    assert(ec == std::errc{});
    *ptr = '\0';

    if (0 != strcmp(buf, expected_buf)){
        printf("Test failed for x = %f\n", x);
        printf("Expected: '%s'\n", expected_buf);
        printf("Got:      '%s'\n", buf);
    }
    assert(0 == strcmp(buf, expected_buf));
}

static uint64_t splitmix64(uint64_t* state){
    uint64_t z = (*state += 0x9e3779b97f4a7c15ULL);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}

int main(){
    const double edge_cases[] = {
        0.0,
        -0.0,
        INFINITY,
        -INFINITY,
        NAN,
        DBL_MIN,
        -DBL_MIN,
        4.9406564584124654e-324,
        -4.9406564584124654e-324,
        DBL_MAX,
        -DBL_MAX,
        1.0,
        -1.0,
        0.1,
        -0.1,
        1.2345678901234567,
        -1.2345678901234567,
        2.2250738585072014e-308,
        1.7976931348623157e+308,
        9007199254740991.0,
        9007199254740992.0,
        9007199254740993.0,
        1e-323,
        5e-324,
        1e-320,
        1e-308,
        1e-100,
        1e-20,
        1e-10,
        1e-5,
        1e5,
        1e10,
        1e20,
        1e100,
        1e308,
        3.141592653589793,
        2.718281828459045,
        6.62607015e-34,
        299792458.0,
        -299792458.0
    };

    size_t edge_count = sizeof(edge_cases) / sizeof(edge_cases[0]);
    for (size_t i = 0; i < edge_count; ++i){
        test(edge_cases[i]);
    }

    uint64_t seed = 123456789ULL;
    for (int i = 0; i < 10000000; ++i){
        uint64_t bits = splitmix64(&seed);
        double x;
        memcpy(&x, &bits, sizeof(x));
        test(x);
    }

    printf("schubfach: %.3f ms\n", time_schubfach * 1e-6);
    printf("to_chars : %.3f ms\n", time_to_chars * 1e-6);

    puts("Tests passed :)");

    return 0;
}
