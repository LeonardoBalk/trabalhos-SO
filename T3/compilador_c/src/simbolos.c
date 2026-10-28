#include "simbolos.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static simbolo_t *globais = NULL;

#define MAX_ESCOPO 1024
static simbolo_t *pilha_escopo[MAX_ESCOPO];
static int topo_escopo = 0;

void simbolos_inicializa(void) { globais = NULL; topo_escopo = 0; }

simbolo_t *global_acha(const char *nome)
{
  for (simbolo_t *s = globais; s != NULL; s = s->prox)
    if (strcmp(s->nome, nome) == 0) return s;
  return NULL;
}

simbolo_t *global_declara_var(const char *nome, tipo_t *tipo)
{
  simbolo_t *s = calloc(1, sizeof(simbolo_t));
  snprintf(s->nome, sizeof(s->nome), "%s", nome);
  s->categoria = SIMB_GLOBAL;
  s->tipo = tipo;
  snprintf(s->rotulo, sizeof(s->rotulo), "_g_%s", nome);
  s->prox = globais;
  globais = s;
  return s;
}

simbolo_t *global_declara_funcao(const char *nome, tipo_t *ret)
{
  simbolo_t *s = global_acha(nome);
  if (s != NULL) return s;
  s = calloc(1, sizeof(simbolo_t));
  snprintf(s->nome, sizeof(s->nome), "%s", nome);
  s->categoria = SIMB_FUNCAO;
  s->tipo = ret;
  snprintf(s->rotulo, sizeof(s->rotulo), "_f_%s", nome);
  s->prox = globais;
  globais = s;
  return s;
}

void escopo_reinicia(void) { topo_escopo = 0; }
int escopo_marca(void) { return topo_escopo; }
void escopo_volta(int marcador) { topo_escopo = marcador; }

void escopo_declara(simbolo_t *s)
{
  if (topo_escopo >= MAX_ESCOPO) { fprintf(stderr, "mcc: escopo local profundo demais\n"); exit(1); }
  pilha_escopo[topo_escopo++] = s;
}

simbolo_t *simbolo_local_cria(const char *nome, tipo_t *tipo, int offset)
{
  simbolo_t *s = calloc(1, sizeof(simbolo_t));
  snprintf(s->nome, sizeof(s->nome), "%s", nome);
  s->categoria = SIMB_LOCAL;
  s->tipo = tipo;
  s->offset = offset;
  return s;
}

simbolo_t *simbolo_acha(const char *nome)
{
  for (int i = topo_escopo - 1; i >= 0; i--)
    if (strcmp(pilha_escopo[i]->nome, nome) == 0) return pilha_escopo[i];
  return global_acha(nome);
}
