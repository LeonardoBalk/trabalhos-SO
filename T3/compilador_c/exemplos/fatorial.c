// fatorial.c -- funções recursivas (fatorial e fibonacci), laço "while" e
// chamadas de função com valor de retorno.

#include "mancha.h"

int fatorial(int n)
{
    if (n <= 1) return 1;
    return n * fatorial(n - 1);
}

int fibonacci(int n)
{
    if (n < 2) return n;
    return fibonacci(n - 1) + fibonacci(n - 2);
}

int main(void)
{
    int i;
    i = 0;
    while (i <= 7) {
        print_int(i);
        putchar(':');
        print_int(fatorial(i));
        putchar(' ');
        print_int(fibonacci(i));
        putchar('\n');
        i = i + 1;
    }
    return 0;
}
