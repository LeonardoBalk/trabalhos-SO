// preprocessador.c -- macros de objeto e de função (inclusive aninhadas),
// e compilação condicional (#ifndef/#define, #if, #ifdef).

#include "mancha.h"

#define TAMANHO 5
#define DOBRO(x) ((x) * 2)
#define MAX(a, b) ((a) > (b) ? (a) : (b))

#ifndef MODO_DEBUG
#define MODO_DEBUG 1
#endif

int quadrados[TAMANHO];

int main(void)
{
    int i;

    i = 0;
    while (i < TAMANHO) {
        quadrados[i] = DOBRO(i) + 1;
        i = i + 1;
    }
    i = 0;
    while (i < TAMANHO) {
        print_int(quadrados[i]);
        putchar(' ');
        i = i + 1;
    }
    putchar('\n');

    print_int(MAX(3, 7));
    putchar(' ');
    print_int(MAX(DOBRO(10), 5));
    putchar('\n');

#if MODO_DEBUG
    puts("modo debug ligado");
#else
    puts("modo debug desligado");
#endif

#ifdef NAO_DEFINIDO
    puts("nao deveria aparecer");
#endif

    return 0;
}
