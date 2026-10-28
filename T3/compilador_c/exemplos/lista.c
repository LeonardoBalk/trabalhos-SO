// lista.c -- struct autorreferenciada (lista ligada) por ponteiro (o mcc
// não tem malloc/free -- os nós vêm de um "pool" estático), e um array
// multidimensional (matriz).

#include "mancha.h"

struct No {
    int valor;
    struct No *prox;
};

#define MAX_NOS 10
struct No pool[MAX_NOS];
int usados;

struct No *aloca(int valor)
{
    struct No *n;
    n = &pool[usados];
    usados = usados + 1;
    n->valor = valor;
    n->prox = 0;
    return n;
}

struct No *insere(struct No *cabeca, int valor)
{
    struct No *n;
    n = aloca(valor);
    n->prox = cabeca;
    return n;
}

void imprime(struct No *cabeca)
{
    struct No *p;
    p = cabeca;
    while (p != 0) {
        print_int(p->valor);
        if (p->prox != 0) putchar(' ');
        p = p->prox;
    }
    putchar('\n');
}

int soma(struct No *cabeca)
{
    int total;
    total = 0;
    while (cabeca != 0) {
        total = total + cabeca->valor;
        cabeca = cabeca->prox;
    }
    return total;
}

int matriz[3][4];

int main(void)
{
    struct No *lista;
    int i, j;

    usados = 0;
    lista = 0;
    lista = insere(lista, 3);
    lista = insere(lista, 2);
    lista = insere(lista, 1);
    imprime(lista);
    print_int(soma(lista));
    putchar('\n');

    for (i = 0; i < 3; i++) {
        for (j = 0; j < 4; j++) {
            matriz[i][j] = i * 4 + j;
        }
    }
    for (i = 0; i < 3; i++) {
        for (j = 0; j < 4; j++) {
            print_int(matriz[i][j]);
            putchar(' ');
        }
    }
    putchar('\n');

    return 0;
}
