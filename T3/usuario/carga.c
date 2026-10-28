#include "sistema.h"

void carga_cpu(int letra)
{
    int k;
    int i;

    for (k = 0; k < 10; k++) {
        for (i = 0; i < 1000; i++) {
        }
        putchar(letra);
    }
}

void cpu_a(void)
{
    carga_cpu('A');
}

void cpu_b(void)
{
    carga_cpu('B');
}

void cpu_c(void)
{
    carga_cpu('C');
}
