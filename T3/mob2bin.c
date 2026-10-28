// converte um .mob num binário achatado, página a página
// o número de páginas de dados vem por parâmetro porque .ds não emite bytes

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define TAM_PAGINA 512
#define TAM_ESPACO 65536

static uint8_t mem[TAM_ESPACO];

static void uso(void)
{
    fprintf(stderr,
        "uso: mob2bin <entrada.mob> -o <saida.bin>"
        " -d <pagina-dados> -n <n-paginas-dados>\n");
    exit(1);
}

int main(int argc, char **argv)
{
    const char *entrada = NULL;
    const char *saida = NULL;
    long pagina_dados = -1;
    long n_pag_dados = -1;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            saida = argv[++i];
        } else if (strcmp(argv[i], "-d") == 0 && i + 1 < argc) {
            pagina_dados = strtol(argv[++i], NULL, 0);
        } else if (strcmp(argv[i], "-n") == 0 && i + 1 < argc) {
            n_pag_dados = strtol(argv[++i], NULL, 0);
        } else if (argv[i][0] != '-' && entrada == NULL) {
            entrada = argv[i];
        } else {
            uso();
        }
    }
    if (entrada == NULL || saida == NULL || pagina_dados < 0 || n_pag_dados <= 0) {
        uso();
    }

    FILE *f = fopen(entrada, "r");
    if (f == NULL) {
        fprintf(stderr, "mob2bin: nao abriu '%s'\n", entrada);
        return 1;
    }

    char linha[256];
    if (fgets(linha, sizeof(linha), f) == NULL || strncmp(linha, "mobj", 4) != 0) {
        fprintf(stderr, "mob2bin: '%s' nao e' um arquivo objeto\n", entrada);
        fclose(f);
        return 1;
    }

    long max_codigo = -1;
    long min_dados = TAM_ESPACO;
    long max_dados = -1;

    while (fgets(linha, sizeof(linha), f) != NULL) {
        unsigned endereco;
        unsigned valor;
        if (linha[0] == 'T' || linha[0] == 'D') {
            if (sscanf(linha + 1, "%x %x", &endereco, &valor) != 2) {
                fprintf(stderr, "mob2bin: registro invalido: %s", linha);
                fclose(f);
                return 1;
            }
            mem[endereco] = (uint8_t) valor;
            if (linha[0] == 'T') {
                if ((long) endereco > max_codigo) max_codigo = endereco;
            } else {
                if ((long) endereco < min_dados) min_dados = endereco;
                if ((long) endereco > max_dados) max_dados = endereco;
            }
        }
    }
    fclose(f);

    if (max_codigo < 0) {
        fprintf(stderr, "mob2bin: '%s' nao tem segmento de codigo\n", entrada);
        return 1;
    }

    long n_pag_codigo = max_codigo / TAM_PAGINA + 1;
    long base_dados = pagina_dados * TAM_PAGINA;

    if (n_pag_codigo > pagina_dados) {
        fprintf(stderr, "mob2bin: codigo ocupa %ld paginas e invade a pagina"
            " de dados %ld\n", n_pag_codigo, pagina_dados);
        return 1;
    }
    if (max_dados >= 0) {
        if (min_dados < base_dados) {
            fprintf(stderr, "mob2bin: dados inicializados em %04lX, antes da"
                " pagina de dados %ld\n", min_dados, pagina_dados);
            return 1;
        }
        if (max_dados >= base_dados + n_pag_dados * TAM_PAGINA) {
            fprintf(stderr, "mob2bin: dados inicializados nao cabem em %ld"
                " paginas\n", n_pag_dados);
            return 1;
        }
    }

    FILE *s = fopen(saida, "wb");
    if (s == NULL) {
        fprintf(stderr, "mob2bin: nao criou '%s'\n", saida);
        return 1;
    }
    fwrite(mem, 1, (size_t) n_pag_codigo * TAM_PAGINA, s);
    fwrite(mem + base_dados, 1, (size_t) n_pag_dados * TAM_PAGINA, s);
    if (fclose(s) != 0) {
        fprintf(stderr, "mob2bin: erro ao gravar '%s'\n", saida);
        return 1;
    }

    printf("%ld\n", n_pag_codigo);
    return 0;
}
