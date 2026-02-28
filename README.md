# Schubfach

Fast and exact conversion of doubles to decimal strings in C.

This repository is a slightly more compact C translation of [Schubfach (C++).](https://github.com/vitaut/schubfach) Big thanks to Victor Zverovich!

# Example

```C
#include "schubfach.h"
#include <stdio.h>

int main() {
    char buf[SCHUBFACH_BUFFER_SIZE];
    schubfach_dtoa(6.62607015e-34, buf);
    puts(buf);
    return 0;
}
```

# Size

Binary size is just 3536 bytes on x86:

```
gcc -c -Os schubfach.c
du -b schubfach.o
# prints 3536
```

# Performance

With gcc version 13.3.0, Schubfach is slightly slower than `std::to_chars` (based on [Ryu](https://github.com/gcc-mirror/gcc/blob/master/libstdc%2B%2B-v3/src/c%2B%2B17/ryu/d2s.c)). For 10 million doubles on AMD Ryzen 5 8500G (using `./test.sh`):

```
schubfach: 439.369 ms
to_chars : 392.564 ms
```
