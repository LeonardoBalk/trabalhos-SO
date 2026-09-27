#include "sistema.h"

void init(void)
{
    int pids[4];
    int i;

    pids[0] = so_cria_proc("cpu_a");
    pids[1] = so_cria_proc("cpu_b");
    pids[2] = so_cria_proc("cpu_c");
    pids[3] = so_cria_proc("leitor");
    for (i = 0; i < 4; i++) {
        so_espera_proc(pids[i]);
    }
    puts("\ninit: todos terminaram");
}
