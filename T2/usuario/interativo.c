#include "sistema.h"

void interativo(void)
{
    int n;
    int c;

    for (n = 0; n < 3; n++) {
        c = getchar();
        print_str("\neco: ");
        while (c != 10) {
            putchar(c);
            c = getchar();
        }
        putchar(10);
    }
}
