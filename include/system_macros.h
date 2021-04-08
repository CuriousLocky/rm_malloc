#ifndef MACROS_H
#define MACROS_H
#define tls __thread
// #define tls thread_local

#include <stdio.h>
#include <inttypes.h>

inline void print_memory(const void *addr, size_t size)
{
    size_t printed = 0;
    size_t i;
    const unsigned char* pc = addr;
    for (i=0; i<size; ++i)
    {
        int  g;
        g = (*(pc+i) >> 4) & 0xf;
        g += g >= 10 ? 'a'-10 : '0';
        putchar(g);
        printed++;

        g = *(pc+i) & 0xf;
        g += g >= 10 ? 'a'-10 : '0';
        putchar(g);
        printed++;
        if (printed % 32 == 0) putchar('\n');
        else if (printed % 4 == 0) putchar(' ');
    }
}

// inline void print_64h(const uint64_t num){
//     char c[16] = {'0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f'};
//     write(1, "0x", sizeof("0x")-1);
//     for(int i = 0; i < 16; i++){
//         int dig = (num>>(60-i*4))&16;
//         write(1, c+dig, 1);
//     }
//     write(1, "\n", 1);
// }

inline void print_64h(uint64_t p) {
    uintptr_t x = (uintptr_t)p;
    char buf[2 + sizeof(x) * 2+1];
    size_t i;

    buf[0] = '0';
    buf[1] = 'x';
    for (i = 0; i < sizeof(x) * 2; i++) {
        buf[i + 2] = "0123456789abcdef"[(x >> ((sizeof(x) * 2 - 1 - i) * 4)) & 0xf];
    }
    buf[2 + sizeof(x) * 2] = '\n';
    write(1, buf, sizeof(buf));
}

#endif