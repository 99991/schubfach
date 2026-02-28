#include "schubfach.h"
#include <stdio.h>

int main() {
    char buf[SCHUBFACH_BUFFER_SIZE];
    schubfach_dtoa(6.62607015e-34, buf);
    puts(buf);
    return 0;
}
