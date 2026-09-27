#include "sistema.h"

void leitor(void)
{
    char linha[40];
    int n;
    int c;

    n = 0;
    c = getchar();
    while (c != 10 && n < 39) {
        linha[n] = c;
        n = n + 1;
        c = getchar();
    }
    linha[n] = 0;
    print_str("leitor leu: ");
    puts(linha);
}
