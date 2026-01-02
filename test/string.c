#include "test.h"

#include <string.h>

int main() {
    ASSERT(0, ""[0]);
    ASSERT(1, sizeof(""));

    ASSERT(97, "abc"[0]);
    ASSERT(98, "abc"[1]);
    ASSERT(99, "abc"[2]);
    ASSERT(0, "abc"[3]);
    ASSERT(4, sizeof("abc"));

    ASSERT(7, sizeof("abc" "def"));
    ASSERT(9, sizeof("abc" "d" "efgh"));
    ASSERT(0, strcmp("abc" "d" "\nefgh", "abcd\nefgh"));
    ASSERT(0, !strcmp("abc" "d", "abcd\nefgh"));
    ASSERT(0, strcmp("\x9" "0", "\t0"));

    printf("OK\n");
    return 0;
}