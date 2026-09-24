// eco.c -- entrada com getchar() (espera ocupada pela console, ver
// rt/biblioteca.c), interativo: use o comando de operador "E<texto>" do
// simulador para digitar uma linha.

#include "mancha.h"

int main(void)
{
    int c;
    c = getchar();
    while (c != '\n') {
        putchar(c);
        c = getchar();
    }
    putchar('!');
    putchar('\n');
    return 0;
}
