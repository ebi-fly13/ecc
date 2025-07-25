#include "test.h"

int assert();
int printf();

int main() {
    ASSERT(1, sizeof(char));
    ASSERT(2, sizeof(short));
    ASSERT(2, sizeof(short int));
    ASSERT(2, sizeof(int short));
    ASSERT(4, sizeof(int));
    ASSERT(8, sizeof(long));
    ASSERT(8, sizeof(long int));
    ASSERT(8, sizeof(long int));
    ASSERT(8, sizeof(char *));
    ASSERT(8, sizeof(int *));
    ASSERT(8, sizeof(long *));
    ASSERT(8, sizeof(int **));
    ASSERT(8, sizeof(int(*)[4]));
    ASSERT(32, sizeof(int*[4]));
    ASSERT(16, sizeof(int[4]));
    ASSERT(48, sizeof(int[3][4]));
    ASSERT(8, sizeof(struct {int a; int b;}));

    ASSERT(8, sizeof(-10 + (long)5));
    ASSERT(8, sizeof(-10 - (long)5));
    ASSERT(8, sizeof(-10 * (long)5));
    ASSERT(8, sizeof(-10 / (long)5));

    ASSERT(1, ({ char i; sizeof(++i); }));
    ASSERT(1, ({ char i; sizeof(i++); }));

    ASSERT(8, sizeof(int(*)[10]));
    ASSERT(8, sizeof(int(*)[][10]));

    ASSERT(1, sizeof(char));
    ASSERT(1, sizeof(signed char));
    ASSERT(1, sizeof(unsigned char));

    ASSERT(2, sizeof(short));
    ASSERT(2, sizeof(int short));
    ASSERT(2, sizeof(short int));
    ASSERT(2, sizeof(signed short));
    ASSERT(2, sizeof(int short signed));
    ASSERT(2, sizeof(unsigned short));
    ASSERT(2, sizeof(int short unsigned));

    ASSERT(4, sizeof(int));
    ASSERT(4, sizeof(signed int));
    ASSERT(4, sizeof(signed));
    ASSERT(4, sizeof(unsigned int));
    ASSERT(4, sizeof(unsigned));

    ASSERT(8, sizeof(long));
    ASSERT(8, sizeof(signed long));
    ASSERT(8, sizeof(signed long int));
    ASSERT(8, sizeof(unsigned long));
    ASSERT(8, sizeof(unsigned long int));

    ASSERT(8, sizeof(long long));
    ASSERT(8, sizeof(signed long long));
    ASSERT(8, sizeof(signed long long int));
    ASSERT(8, sizeof(unsigned long long));
    ASSERT(8, sizeof(unsigned long long int));

    ASSERT(1, sizeof((char)1));
    ASSERT(2, sizeof((short)1));
    ASSERT(4, sizeof((int)1));
    ASSERT(8, sizeof((long)1));

    ASSERT(4, sizeof((char)1 + (char)1));
    ASSERT(4, sizeof((short)1 + (short)1));
    ASSERT(4, sizeof(1?2:3));
    ASSERT(4, sizeof(1?(short)2:(char)3));
    ASSERT(8, sizeof(1?(long)2:(char)3));

    printf("OK\n");
    return 0;
}