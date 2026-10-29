#include "sistema.h"

void main(void)
{
    int a;
    int b;

    puts("teste 1: dois processos do mesmo binario");
    a = so_cria_proc("cpu_a");
    b = so_cria_proc("cpu_a");
    print_str("  pids ");
    print_int(a);
    print_str(" e ");
    print_int(b);
    print_str("\n");
    so_espera_proc(a);
    so_espera_proc(b);

    puts("teste 2: escrita na area de codigo");
    a = so_cria_proc("escreve_codigo");
    so_espera_proc(a);

    puts("teste 3: acesso a pagina nao mapeada");
    a = so_cria_proc("fora_mapa");
    so_espera_proc(a);

    puts("testes terminados");
}
