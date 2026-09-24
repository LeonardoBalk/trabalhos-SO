#include "objeto.h"
#include <stdlib.h>
#include <string.h>

bool obj_escreve_cabecalho(FILE *f)
{
  return fprintf(f, "mobj 1\n") > 0;
}

void obj_escreve_byte(FILE *f, char segmento, uint16_t endereco, uint8_t valor)
{
  fprintf(f, "%c %04X %02X\n", segmento, endereco, valor);
}

void obj_escreve_publico(FILE *f, const char *nome, uint16_t endereco)
{
  fprintf(f, "P %s %04X\n", nome, endereco);
}

bool obj_carrega(const char *arquivo, mem_t *mem, simbolo_t **simbolos, char *erro, size_t erro_tam)
{
  FILE *f = fopen(arquivo, "r");
  if (f == NULL) {
    snprintf(erro, erro_tam, "não foi possível abrir '%s'", arquivo);
    return false;
  }

  char linha[256];
  if (fgets(linha, sizeof(linha), f) == NULL || strncmp(linha, "mobj", 4) != 0) {
    snprintf(erro, erro_tam, "'%s' não é um arquivo objeto reconhecido (esperava 'mobj 1')", arquivo);
    fclose(f);
    return false;
  }

  simbolo_t *lista = NULL;
  int n_linha = 1;
  while (fgets(linha, sizeof(linha), f) != NULL) {
    n_linha++;
    if (linha[0] == '\n' || linha[0] == '\0') continue;

    if (linha[0] == 'T' || linha[0] == 'D') {
      unsigned endereco, valor;
      if (sscanf(linha + 1, "%x %x", &endereco, &valor) != 2) {
        snprintf(erro, erro_tam, "%s:%d: linha de código malformada", arquivo, n_linha);
        fclose(f);
        obj_libera_simbolos(lista);
        return false;
      }
      mem_escreve_byte(mem, endereco, (uint8_t)valor);
    } else if (linha[0] == 'P') {
      char nome[64];
      unsigned endereco;
      if (sscanf(linha + 1, "%63s %x", nome, &endereco) != 2) {
        snprintf(erro, erro_tam, "%s:%d: linha de símbolo malformada", arquivo, n_linha);
        fclose(f);
        obj_libera_simbolos(lista);
        return false;
      }
      simbolo_t *s = calloc(1, sizeof(simbolo_t));
      snprintf(s->nome, sizeof(s->nome), "%s", nome);
      s->endereco = (uint16_t)endereco;
      s->prox = lista;
      lista = s;
    }
    // linhas com outro prefixo são ignoradas (compatibilidade futura)
  }

  fclose(f);
  if (simbolos != NULL) *simbolos = lista; else obj_libera_simbolos(lista);
  return true;
}

void obj_libera_simbolos(simbolo_t *lista)
{
  while (lista != NULL) {
    simbolo_t *prox = lista->prox;
    free(lista);
    lista = prox;
  }
}

uint16_t obj_busca_simbolo(simbolo_t *lista, const char *nome, bool *achou)
{
  for (simbolo_t *s = lista; s != NULL; s = s->prox) {
    if (strcmp(s->nome, nome) == 0) { *achou = true; return s->endereco; }
  }
  *achou = false;
  return 0;
}
