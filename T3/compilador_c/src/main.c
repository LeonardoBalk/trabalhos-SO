// main.c -- ponto de entrada do mcc: lê um (ou mais) arquivo(s) .c,
// pré-processa, tokeniza e compila para linguagem de montagem do Mancha
// completo, escrevendo o resultado em um arquivo .asm.

#include "pre.h"
#include "lexer.h"
#include "parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
  const char *saida_nome = NULL;
  const char *entrada = NULL;
  const char *dirs_include[16];
  int n_dirs = 0;

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) { saida_nome = argv[++i]; }
    else if (strncmp(argv[i], "-I", 2) == 0 && strlen(argv[i]) > 2) { if (n_dirs < 15) dirs_include[n_dirs++] = argv[i] + 2; }
    else if (strcmp(argv[i], "-I") == 0 && i + 1 < argc) { if (n_dirs < 15) dirs_include[n_dirs++] = argv[++i]; }
    else if (entrada == NULL) { entrada = argv[i]; }
    else { fprintf(stderr, "mcc: argumento inesperado: %s\n", argv[i]); return 1; }
  }
  dirs_include[n_dirs] = NULL;

  if (entrada == NULL) {
    fprintf(stderr, "uso: %s [-I diretorio] [-o saida.asm] entrada.c\n", argv[0]);
    return 1;
  }

  static char nome_padrao[512];
  if (saida_nome == NULL) {
    snprintf(nome_padrao, sizeof(nome_padrao), "%s", entrada);
    char *ponto = strrchr(nome_padrao, '.');
    if (ponto != NULL) *ponto = '\0';
    strncat(nome_padrao, ".asm", sizeof(nome_padrao) - strlen(nome_padrao) - 1);
    saida_nome = nome_padrao;
  }

  char *expandido = pre_processa_arquivo(entrada, dirs_include);

  int n_tokens = 0;
  token_t *tokens = lexer_tokeniza(expandido, &n_tokens);

  FILE *saida = fopen(saida_nome, "w");
  if (saida == NULL) { fprintf(stderr, "mcc: não foi possível criar '%s'\n", saida_nome); return 1; }

  unsigned prefixo = (unsigned)time(NULL) ^ ((unsigned)getpid() << 16);
  compilador_compila(tokens, n_tokens, saida, prefixo);

  fclose(saida);
  printf("compilado: %s\n", saida_nome);
  return 0;
}
