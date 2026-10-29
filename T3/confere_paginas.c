// percorre o código de um .mob e acusa instrução que atravessa fronteira de
// página: numa falta de busca o ip salvo apontaria para o imediato, e o
// Mancha não permite ao SO descobrir onde a instrução começou

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "instrucao.h"

#define TAM_PAGINA 512
#define TAM_ESPACO 65536

static uint8_t mem[TAM_ESPACO];
static uint8_t tem[TAM_ESPACO];

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "uso: confere_paginas <arquivo.mob>\n");
        return 2;
    }
    FILE *f = fopen(argv[1], "r");
    if (f == NULL) {
        fprintf(stderr, "confere_paginas: nao abriu '%s'\n", argv[1]);
        return 2;
    }

    char linha[256];
    long max_codigo = -1;
    while (fgets(linha, sizeof(linha), f) != NULL) {
        unsigned endereco;
        unsigned valor;
        if (linha[0] == 'T' && sscanf(linha + 1, "%x %x", &endereco, &valor) == 2) {
            mem[endereco] = (uint8_t) valor;
            tem[endereco] = 1;
            if ((long) endereco > max_codigo) max_codigo = endereco;
        }
    }
    fclose(f);

    int problemas = 0;
    for (long a = 0; a + 1 <= max_codigo; ) {
        if (!tem[a]) { a += 2; continue; }
        uint16_t palavra = (uint16_t) ((mem[a] << 8) | mem[a + 1]);
        instr_decod_t d;
        instrucao_decodifica(palavra, &d);
        long tam = instrucao_requer_imediato(&d) ? 4 : 2;
        if (tam == 4 && (a / TAM_PAGINA) != ((a + 2) / TAM_PAGINA)) {
            printf("%s: instrucao em %04lX atravessa a fronteira da pagina %ld\n",
                   argv[1], a, a / TAM_PAGINA);
            problemas++;
        }
        a += tam;
    }
    return problemas > 0 ? 1 : 0;
}
