// structs.c -- structs (inclusive aninhadas), ponteiro para struct com
// "->", passagem de struct por ponteiro para uma função, e um array
// global de structs.

#include "mancha.h"

struct Ponto {
    int x;
    int y;
};

struct Retangulo {
    struct Ponto topo_esq;
    struct Ponto baixo_dir;
};

int area(struct Retangulo *r)
{
    int largura;
    int altura;
    largura = r->baixo_dir.x - r->topo_esq.x;
    altura = r->baixo_dir.y - r->topo_esq.y;
    return largura * altura;
}

void desloca(struct Ponto *p, int dx, int dy)
{
    p->x = p->x + dx;
    p->y = p->y + dy;
}

struct Ponto vetores[3];

int main(void)
{
    struct Retangulo r;
    struct Ponto origem;
    int i;

    r.topo_esq.x = 1;
    r.topo_esq.y = 1;
    r.baixo_dir.x = 5;
    r.baixo_dir.y = 4;
    print_int(area(&r));
    putchar('\n');

    origem.x = 0;
    origem.y = 0;
    desloca(&origem, 10, -3);
    print_int(origem.x);
    putchar(',');
    print_int(origem.y);
    putchar('\n');

    i = 0;
    while (i < 3) {
        vetores[i].x = i * 2;
        vetores[i].y = i * 3;
        i = i + 1;
    }
    i = 0;
    while (i < 3) {
        print_int(vetores[i].x);
        putchar('/');
        print_int(vetores[i].y);
        putchar(' ');
        i = i + 1;
    }
    putchar('\n');

    return 0;
}
