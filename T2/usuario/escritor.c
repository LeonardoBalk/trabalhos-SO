#include "sistema.h"

void escritor(void)
{
    int i;
    for (i = 0; i < 5; i++) {
        print_str("escritor ");
        print_int(i);
        putchar(10);
    }
}
