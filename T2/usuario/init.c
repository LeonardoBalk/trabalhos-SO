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
    int escritor;
    int leitor;

    leitor = so_cria_proc("leitor");
    escritor = so_cria_proc("escritor");
    init_mostra("criou leitor, pid ", leitor);
    init_mostra("criou escritor, pid ", escritor);
    init_mostra("esperar a si mesmo: ", so_espera_proc(1));
    init_mostra("esperar pid inexistente: ", so_espera_proc(99));

    init_mostra("esperou o leitor: ", so_espera_proc(leitor));
    init_mostra("esperou o escritor ja morto: ", so_espera_proc(escritor));
}
