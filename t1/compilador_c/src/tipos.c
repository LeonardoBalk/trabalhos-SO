#include "tipos.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_TIPOS 4096

static tipo_t pool[MAX_TIPOS];
static int n_pool = 0;

static tipo_t *T_VOID_S, *T_INT_S, *T_CHAR_S;

static estrutura_t *estruturas = NULL;

static tipo_t *novo_tipo(categoria_tipo_t cat)
{
  if (n_pool >= MAX_TIPOS) { fprintf(stderr, "mcc: tabela de tipos cheia\n"); exit(1); }
  tipo_t *t = &pool[n_pool++];
  memset(t, 0, sizeof(*t));
  t->cat = cat;
  return t;
}

void tipos_inicializa(void)
{
  T_VOID_S = novo_tipo(T_VOID);
  T_INT_S = novo_tipo(T_INT);
  T_CHAR_S = novo_tipo(T_CHAR);
}

tipo_t *tipo_void(void) { return T_VOID_S; }
tipo_t *tipo_int(void) { return T_INT_S; }
tipo_t *tipo_char(void) { return T_CHAR_S; }

tipo_t *tipo_ponteiro(tipo_t *base)
{
  tipo_t *t = novo_tipo(T_PONTEIRO);
  t->base = base;
  return t;
}

tipo_t *tipo_array(tipo_t *base, int qtd)
{
  tipo_t *t = novo_tipo(T_ARRAY);
  t->base = base;
  t->qtd = qtd;
  return t;
}

tipo_t *tipo_de_struct(estrutura_t *e)
{
  tipo_t *t = novo_tipo(T_STRUCT);
  t->estrutura = e;
  return t;
}

estrutura_t *estrutura_acha(const char *nome)
{
  for (estrutura_t *e = estruturas; e != NULL; e = e->prox)
    if (strcmp(e->nome, nome) == 0) return e;
  return NULL;
}

estrutura_t *estrutura_declara(const char *nome)
{
  estrutura_t *e = estrutura_acha(nome);
  if (e != NULL) return e;
  e = calloc(1, sizeof(estrutura_t));
  snprintf(e->nome, sizeof(e->nome), "%s", nome);
  e->prox = estruturas;
  estruturas = e;
  return e;
}

membro_t *estrutura_acha_membro(estrutura_t *e, const char *nome)
{
  for (membro_t *m = e->membros; m != NULL; m = m->prox)
    if (strcmp(m->nome, nome) == 0) return m;
  return NULL;
}

void estrutura_adiciona_membro(estrutura_t *e, const char *nome, tipo_t *tipo)
{
  membro_t *m = calloc(1, sizeof(membro_t));
  snprintf(m->nome, sizeof(m->nome), "%s", nome);
  m->tipo = tipo;
  m->offset = e->tamanho;
  e->tamanho += tipo_tamanho(tipo);
  membro_t **p = &e->membros;
  while (*p != NULL) p = &(*p)->prox;
  *p = m;
}

int tipo_tamanho(const tipo_t *t)
{
  switch (t->cat) {
    case T_VOID: return 0;
    case T_INT: return 2;
    case T_CHAR: return 1;
    case T_PONTEIRO: return 2;
    case T_ARRAY: return t->qtd * tipo_tamanho(t->base);
    case T_STRUCT: return t->estrutura->tamanho;
  }
  return 0;
}

bool tipo_eh_ponteiro_ou_array(const tipo_t *t)
{
  return t->cat == T_PONTEIRO || t->cat == T_ARRAY;
}

tipo_t *tipo_decai(tipo_t *t)
{
  if (t->cat == T_ARRAY) return tipo_ponteiro(t->base);
  return t;
}

bool tipo_igual(const tipo_t *a, const tipo_t *b)
{
  if (a->cat != b->cat) return false;
  switch (a->cat) {
    case T_PONTEIRO: return tipo_igual(a->base, b->base);
    case T_ARRAY: return a->qtd == b->qtd && tipo_igual(a->base, b->base);
    case T_STRUCT: return a->estrutura == b->estrutura;
    default: return true;
  }
}

bool tipo_escalar(const tipo_t *t)
{
  return t->cat == T_INT || t->cat == T_CHAR || t->cat == T_PONTEIRO;
}

const char *tipo_texto(const tipo_t *t)
{
  static char buf[8][160];
  static int idx = 0;
  char *b = buf[idx++ & 7];
  switch (t->cat) {
    case T_VOID: snprintf(b, 160, "void"); break;
    case T_INT: snprintf(b, 160, "int"); break;
    case T_CHAR: snprintf(b, 160, "char"); break;
    case T_PONTEIRO: snprintf(b, 160, "%s *", tipo_texto(t->base)); break;
    case T_ARRAY: snprintf(b, 160, "%s [%d]", tipo_texto(t->base), t->qtd); break;
    case T_STRUCT: snprintf(b, 160, "struct %s", t->estrutura->nome); break;
  }
  return b;
}
