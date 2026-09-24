// ola.c -- primeiro programa: E/S básica com a "biblioteca padrão" mínima
// (puts/print_int/print_hex/putchar, ver ../inc/mancha.h) e expressões
// aritméticas simples.

#include "mancha.h"

int main(void)
{
    puts("Ola, Mancha!");
    print_int(2 + 3 * 4);
    putchar('\n');
    print_hex(4660);
    putchar('\n');
    return 0;
}
