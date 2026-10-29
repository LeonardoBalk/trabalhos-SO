#include "sistema.h"

void main(void)
{
    int *p;

    p = (int *) 0x7000;
    *p = 1;
    puts("ERRO: acesso fora do mapa nao foi barrado");
}
