#include "sistema.h"

void init_mostra(char *msg, int valor)
{
    print_str("init: ");
    print_str(msg);
    print_int(valor);
    putchar(10);
}

void init(void)
{
    int a;
    int b;
    int c;

    a = so_cria_proc("escritor");
    init_mostra("criou escritor, pid ", a);
    b = so_cria_proc("escritor");
    init_mostra("criou escritor, pid ", b);
    init_mostra("programa inexistente: ", so_cria_proc("nada"));
    init_mostra("matou o primeiro: ", so_mata_proc(a));
    init_mostra("matou de novo: ", so_mata_proc(a));
    init_mostra("criou outro, pid ", so_cria_proc("escritor"));

    print_str("init: digite um caractere: ");
    c = getchar();
    putchar(c);
    putchar(10);
}
