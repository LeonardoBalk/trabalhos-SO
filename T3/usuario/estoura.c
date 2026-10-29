#include "sistema.h"

void main(void)
{
    int pid;
    int n;

    n = 0;
    pid = so_cria_proc("cpu_a");
    while (pid > 0) {
        n = n + 1;
        pid = so_cria_proc("cpu_a");
    }
    print_str("criou ");
    print_int(n);
    puts(" processos ate faltar recurso");
}
