#include "sistema.h"

void main(void)
{
    int a;
    int b;
    int c;

    a = so_cria_proc("conta");
    b = so_cria_proc("conta");
    c = so_cria_proc("conta");
    so_espera_proc(a);
    so_espera_proc(b);
    so_espera_proc(c);
}
