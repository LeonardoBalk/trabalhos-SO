#include "sistema.h"

void main(void)
{
    char *p;

    p = (char *) 0;
    *p = 1;
    puts("ERRO: escrita no codigo nao foi barrada");
}
