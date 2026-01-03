#include "test.h"

int main() {
    ASSERT(97, 'a');
    ASSERT(10, '\n');
    ASSERT(-128, '\x80');

    ASSERT(511, 0777);
    ASSERT(0, 0x0);
    ASSERT(10, 0xa);
    ASSERT(10, 0XA);
    ASSERT(48879, 0xbeef);
    ASSERT(48879, 0xBEEF);
    ASSERT(48879, 0XBEEF);
    ASSERT(0, 0b0);
    ASSERT(1, 0b1);
    ASSERT(47, 0b101111);
    ASSERT(47, 0B101111);

    ASSERT(8, sizeof(4UL));
    ASSERT(8, sizeof(4L));

    assert(1, size\
of(char),
           "sizeof(char)");

    assert(1, size\
\
of(char),\
           "sizeof(char)");

    ASSERT(4, sizeof(L'\0'));
    ASSERT(97, L'a');

    0.0;
    1.0;
    3e+8;
    0x10.1p0;
    .1E4f;

    ASSERT(4, sizeof(8.0f));
    ASSERT(4, sizeof(0.3F));
    ASSERT(8, sizeof(0.));
    ASSERT(8, sizeof(.0));
    ASSERT(8, sizeof(5.l));
    ASSERT(8, sizeof(2.0L));

    printf("OK\n");
    return 0;
}