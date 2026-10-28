#include "pre.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdarg.h>

#define MAX_LINHA 4096
#define MAX_PARAMS 16
#define MAX_COND 64

// ------------------------------------------------------------ buffer dinâmico

typedef struct { char *dados; size_t tam; size_t cap; } dyn_t;

static void dyn_init(dyn_t *d) { d->cap = 65536; d->dados = malloc(d->cap); d->dados[0] = '\0'; d->tam = 0; }

static void dyn_add(dyn_t *d, const char *s, size_t n)
{
  if (d->tam + n + 1 > d->cap) {
    while (d->tam + n + 1 > d->cap) d->cap *= 2;
    d->dados = realloc(d->dados, d->cap);
  }
  memcpy(d->dados + d->tam, s, n);
  d->tam += n;
  d->dados[d->tam] = '\0';
}

static void dyn_str(dyn_t *d, const char *s) { dyn_add(d, s, strlen(s)); }
static void dyn_fmt(dyn_t *d, const char *fmt, ...)
{
  char buf[512];
  va_list ap; va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  dyn_str(d, buf);
}

// --------------------------------------------------------------------- erro

static void erro_fatal(const char *arquivo, int linha, const char *fmt, ...)
{
  fprintf(stderr, "%s:%d: erro: ", arquivo ? arquivo : "?", linha);
  va_list ap; va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  fprintf(stderr, "\n");
  exit(1);
}

// ------------------------------------------------------------------- macros

typedef struct macro {
  char nome[64];
  bool funcional;
  char params[MAX_PARAMS][64];
  int n_params;
  char *corpo;
  struct macro *prox;
} macro_t;

static macro_t *macros = NULL;

static macro_t *macro_acha(const char *nome)
{
  for (macro_t *m = macros; m != NULL; m = m->prox)
    if (strcmp(m->nome, nome) == 0) return m;
  return NULL;
}

static void macro_undef(const char *nome)
{
  macro_t **p = &macros;
  while (*p != NULL) {
    if (strcmp((*p)->nome, nome) == 0) { macro_t *m = *p; *p = m->prox; free(m->corpo); free(m); return; }
    p = &(*p)->prox;
  }
}

static void macro_define(const char *nome, bool funcional, char params[][64], int n_params, const char *corpo)
{
  macro_undef(nome);
  macro_t *m = calloc(1, sizeof(macro_t));
  snprintf(m->nome, sizeof(m->nome), "%s", nome);
  m->funcional = funcional;
  m->n_params = n_params;
  for (int i = 0; i < n_params; i++) snprintf(m->params[i], 64, "%s", params[i]);
  m->corpo = strdup(corpo);
  m->prox = macros;
  macros = m;
}

static bool eh_ident_inicio(char c) { return isalpha((unsigned char)c) || c == '_'; }
static bool eh_ident_meio(char c) { return isalnum((unsigned char)c) || c == '_'; }

// pilha de macros "em expansão" (para evitar recursão infinita)
#define MAX_PILHA_MACRO 64
typedef struct { const char *nomes[MAX_PILHA_MACRO]; int n; } pilha_macro_t;

static bool na_pilha(pilha_macro_t *p, const char *nome)
{
  for (int i = 0; i < p->n; i++) if (strcmp(p->nomes[i], nome) == 0) return true;
  return false;
}

static void expande_texto(const char *entrada, dyn_t *saida, pilha_macro_t *pilha,
                           const char *arquivo, int linha);

// separa argumentos de uma chamada de macro funcional, respeitando parênteses
// aninhados; "s" deve apontar logo após o "(" de abertura. devolve ponteiro
// para o caractere logo após o ")" de fechamento correspondente.
static const char *separa_argumentos(const char *s, char *args[], int *n_args,
                                      const char *arquivo, int linha)
{
  *n_args = 0;
  // caso especial: chamada sem argumentos, ex. FOO()
  const char *p = s;
  while (isspace((unsigned char)*p)) p++;
  if (*p == ')') return p + 1;

  int prof = 0;
  const char *inicio = s;
  bool dentro_str = false, dentro_chr = false;
  while (*s != '\0') {
    char c = *s;
    if (!dentro_str && !dentro_chr) {
      if (c == '"') dentro_str = true;
      else if (c == '\'') dentro_chr = true;
      else if (c == '(') prof++;
      else if (c == ')') {
        if (prof == 0) {
          if (*n_args >= MAX_PARAMS) erro_fatal(arquivo, linha, "macro com argumentos demais");
          size_t n = (size_t)(s - inicio);
          char *arg = malloc(n + 1);
          memcpy(arg, inicio, n); arg[n] = '\0';
          args[(*n_args)++] = arg;
          return s + 1;
        }
        prof--;
      } else if (c == ',' && prof == 0) {
        if (*n_args >= MAX_PARAMS) erro_fatal(arquivo, linha, "macro com argumentos demais");
        size_t n = (size_t)(s - inicio);
        char *arg = malloc(n + 1);
        memcpy(arg, inicio, n); arg[n] = '\0';
        args[(*n_args)++] = arg;
        inicio = s + 1;
      }
    } else if (dentro_str) {
      if (c == '\\' && s[1] != '\0') s++;
      else if (c == '"') dentro_str = false;
    } else if (dentro_chr) {
      if (c == '\\' && s[1] != '\0') s++;
      else if (c == '\'') dentro_chr = false;
    }
    s++;
  }
  erro_fatal(arquivo, linha, "parênteses de macro não fechados");
  return s;
}

// substitui ocorrências de um parâmetro formal por seu argumento (já
// expandido), dentro do texto do corpo de uma macro funcional -- respeita
// fronteiras de identificador e não mexe em literais de string/char.
static char *substitui_parametros(const char *corpo, char params[][64], int n_params, char *args_expandidos[])
{
  dyn_t out; dyn_init(&out);
  const char *s = corpo;
  while (*s != '\0') {
    char c = *s;
    if (c == '"' || c == '\'') {
      char aspas = c;
      dyn_add(&out, s, 1); s++;
      while (*s != '\0' && *s != aspas) {
        if (*s == '\\' && s[1] != '\0') { dyn_add(&out, s, 2); s += 2; }
        else { dyn_add(&out, s, 1); s++; }
      }
      if (*s == aspas) { dyn_add(&out, s, 1); s++; }
      continue;
    }
    if (eh_ident_inicio(c)) {
      const char *ini = s;
      while (eh_ident_meio(*s)) s++;
      size_t n = (size_t)(s - ini);
      int achou = -1;
      for (int i = 0; i < n_params; i++)
        if (strlen(params[i]) == n && strncmp(params[i], ini, n) == 0) { achou = i; break; }
      if (achou >= 0) dyn_str(&out, args_expandidos[achou]);
      else dyn_add(&out, ini, n);
      continue;
    }
    dyn_add(&out, s, 1); s++;
  }
  return out.dados; // chamador libera
}

static void expande_texto(const char *entrada, dyn_t *saida, pilha_macro_t *pilha,
                           const char *arquivo, int linha)
{
  const char *s = entrada;
  while (*s != '\0') {
    char c = *s;
    if (c == '"' || c == '\'') {
      char aspas = c;
      dyn_add(saida, s, 1); s++;
      while (*s != '\0' && *s != aspas) {
        if (*s == '\\' && s[1] != '\0') { dyn_add(saida, s, 2); s += 2; }
        else { dyn_add(saida, s, 1); s++; }
      }
      if (*s == aspas) { dyn_add(saida, s, 1); s++; }
      continue;
    }
    if (eh_ident_inicio(c)) {
      const char *ini = s;
      while (eh_ident_meio(*s)) s++;
      size_t n = (size_t)(s - ini);
      char nome[64];
      if (n >= sizeof(nome)) n = sizeof(nome) - 1;
      memcpy(nome, ini, n); nome[n] = '\0';

      macro_t *m = macro_acha(nome);
      if (m == NULL || na_pilha(pilha, nome)) { dyn_add(saida, ini, (size_t)(s - ini)); continue; }

      if (!m->funcional) {
        if (pilha->n >= MAX_PILHA_MACRO) erro_fatal(arquivo, linha, "expansão de macro profunda demais (recursão?): %s", nome);
        pilha->nomes[pilha->n++] = m->nome;
        expande_texto(m->corpo, saida, pilha, arquivo, linha);
        pilha->n--;
        continue;
      }

      // macro funcional: precisa de "(" a seguir (ignorando espaços)
      const char *p = s;
      while (isspace((unsigned char)*p)) p++;
      if (*p != '(') { dyn_add(saida, ini, (size_t)(s - ini)); continue; }
      p++;
      char *args[MAX_PARAMS]; int n_args = 0;
      const char *depois = separa_argumentos(p, args, &n_args, arquivo, linha);
      if (n_args != m->n_params && !(n_args == 0 && m->n_params == 0)) {
        if (!(n_args == 1 && m->n_params == 0 && strlen(args[0]) == 0)) // FOO() com 0 parâmetros
          erro_fatal(arquivo, linha, "macro %s espera %d argumento(s), recebeu %d", nome, m->n_params, n_args);
      }
      char *args_exp[MAX_PARAMS];
      for (int i = 0; i < n_args; i++) {
        dyn_t tmp; dyn_init(&tmp);
        expande_texto(args[i], &tmp, pilha, arquivo, linha);
        args_exp[i] = tmp.dados;
      }
      char *substituido = substitui_parametros(m->corpo, m->params, m->n_params, args_exp);
      if (pilha->n >= MAX_PILHA_MACRO) erro_fatal(arquivo, linha, "expansão de macro profunda demais (recursão?): %s", nome);
      pilha->nomes[pilha->n++] = m->nome;
      expande_texto(substituido, saida, pilha, arquivo, linha);
      pilha->n--;
      free(substituido);
      for (int i = 0; i < n_args; i++) { free(args[i]); free(args_exp[i]); }
      s = depois;
      continue;
    }
    dyn_add(saida, s, 1); s++;
  }
}

// ------------------------------------------------------- avaliador de #if

// gramática suportada: expr := ou ; ou := e ('||' e)* ; e := prim ('&&' prim)*
// prim := '!' prim | '(' ou ')' | 'defined' '(' IDENT ')' | 'defined' IDENT
//       | NUM (op)? NUM  onde op é '==' '!=' '<' '<=' '>' '>='  | NUM
typedef struct { const char *s; const char *arquivo; int linha; } avaliador_t;

static void av_pula_esp(avaliador_t *a) { while (isspace((unsigned char)*a->s)) a->s++; }

static long av_expr(avaliador_t *a);

static long av_primario(avaliador_t *a)
{
  av_pula_esp(a);
  if (*a->s == '!') { a->s++; return !av_primario(a); }
  if (*a->s == '(') {
    a->s++;
    long v = av_expr(a);
    av_pula_esp(a);
    if (*a->s != ')') erro_fatal(a->arquivo, a->linha, "#if: esperado ')'");
    a->s++;
    return v;
  }
  if (strncmp(a->s, "defined", 7) == 0 && !eh_ident_meio(a->s[7])) {
    a->s += 7;
    av_pula_esp(a);
    bool paren = false;
    if (*a->s == '(') { paren = true; a->s++; av_pula_esp(a); }
    if (!eh_ident_inicio(*a->s)) erro_fatal(a->arquivo, a->linha, "#if: esperado identificador após 'defined'");
    const char *ini = a->s;
    while (eh_ident_meio(*a->s)) a->s++;
    char nome[64]; size_t n = (size_t)(a->s - ini); if (n >= sizeof(nome)) n = sizeof(nome) - 1;
    memcpy(nome, ini, n); nome[n] = '\0';
    if (paren) { av_pula_esp(a); if (*a->s != ')') erro_fatal(a->arquivo, a->linha, "#if: esperado ')' após defined(...)"); a->s++; }
    return macro_acha(nome) != NULL;
  }
  if (isdigit((unsigned char)*a->s)) {
    char *fim; long v = strtol(a->s, &fim, 0);
    a->s = fim;
    return v;
  }
  if (eh_ident_inicio(*a->s)) {
    // identificador não definido/macro não numérica em #if vale 0 (regra do C)
    while (eh_ident_meio(*a->s)) a->s++;
    return 0;
  }
  erro_fatal(a->arquivo, a->linha, "#if: expressão inválida perto de '%s'", a->s);
  return 0;
}

static long av_comparacao(avaliador_t *a)
{
  long v = av_primario(a);
  av_pula_esp(a);
  const char *ops[] = {"==", "!=", "<=", ">=", "<", ">", NULL};
  for (int i = 0; ops[i] != NULL; i++) {
    size_t ln = strlen(ops[i]);
    if (strncmp(a->s, ops[i], ln) == 0) {
      a->s += ln;
      long v2 = av_primario(a);
      if (strcmp(ops[i], "==") == 0) return v == v2;
      if (strcmp(ops[i], "!=") == 0) return v != v2;
      if (strcmp(ops[i], "<=") == 0) return v <= v2;
      if (strcmp(ops[i], ">=") == 0) return v >= v2;
      if (strcmp(ops[i], "<") == 0) return v < v2;
      return v > v2;
    }
  }
  return v;
}

static long av_e(avaliador_t *a)
{
  long v = av_comparacao(a);
  av_pula_esp(a);
  while (strncmp(a->s, "&&", 2) == 0) {
    a->s += 2;
    long v2 = av_comparacao(a);
    v = v && v2;
    av_pula_esp(a);
  }
  return v;
}

static long av_expr(avaliador_t *a)
{
  long v = av_e(a);
  av_pula_esp(a);
  while (strncmp(a->s, "||", 2) == 0) {
    a->s += 2;
    long v2 = av_e(a);
    v = v || v2;
    av_pula_esp(a);
  }
  return v;
}

static long avalia_if(const char *expr_expandida, const char *arquivo, int linha)
{
  avaliador_t a = { .s = expr_expandida, .arquivo = arquivo, .linha = linha };
  return av_expr(&a);
}

// ---------------------------------------------------------- pilha de #if

typedef struct { bool ativo; bool algum_ramo_ativo; bool pai_ativo; bool teve_else; } cond_t;
static cond_t pilha_cond[MAX_COND];
static int n_cond = 0;

static bool ativo_agora(void)
{
  for (int i = 0; i < n_cond; i++) if (!pilha_cond[i].ativo) return false;
  return true;
}

// ------------------------------------------------------------- arquivo I/O

static char *le_arquivo_inteiro(const char *caminho)
{
  FILE *f = fopen(caminho, "rb");
  if (f == NULL) return NULL;
  fseek(f, 0, SEEK_END);
  long tam = ftell(f);
  fseek(f, 0, SEEK_SET);
  char *buf = malloc((size_t)tam + 1);
  size_t lido = fread(buf, 1, (size_t)tam, f);
  buf[lido] = '\0';
  fclose(f);
  return buf;
}

static bool resolve_include(const char *nome, bool local, const char *arquivo_atual,
                             const char *dirs_include[], char *achado, size_t achado_tam)
{
  char tentativa[1024];
  if (local) {
    const char *barra = strrchr(arquivo_atual, '/');
    if (barra != NULL) {
      snprintf(tentativa, sizeof(tentativa), "%.*s/%s", (int)(barra - arquivo_atual), arquivo_atual, nome);
    } else {
      snprintf(tentativa, sizeof(tentativa), "%s", nome);
    }
    FILE *f = fopen(tentativa, "r");
    if (f != NULL) { fclose(f); snprintf(achado, achado_tam, "%s", tentativa); return true; }
  }
  for (int i = 0; dirs_include[i] != NULL; i++) {
    snprintf(tentativa, sizeof(tentativa), "%s/%s", dirs_include[i], nome);
    FILE *f = fopen(tentativa, "r");
    if (f != NULL) { fclose(f); snprintf(achado, achado_tam, "%s", tentativa); return true; }
  }
  if (!local) {
    FILE *f = fopen(nome, "r");
    if (f != NULL) { fclose(f); snprintf(achado, achado_tam, "%s", nome); return true; }
  }
  return false;
}

static void processa_arquivo(const char *caminho, dyn_t *saida, const char *dirs_include[], int profundidade);

static void processa_diretiva(char *linha, const char *arquivo, int numero_linha,
                               dyn_t *saida, const char *dirs_include[], int profundidade)
{
  char *s = linha + 1; // pula '#'
  while (isspace((unsigned char)*s)) s++;
  char nome[32]; int k = 0;
  while (isalpha((unsigned char)*s) && k < 31) nome[k++] = *s++;
  nome[k] = '\0';
  while (isspace((unsigned char)*s)) s++;

  bool ativo_pai = ativo_agora();

  if (strcmp(nome, "ifdef") == 0 || strcmp(nome, "ifndef") == 0) {
    char alvo[64]; int j = 0;
    while (eh_ident_meio(*s) && j < 63) alvo[j++] = *s++;
    alvo[j] = '\0';
    bool def = macro_acha(alvo) != NULL;
    bool cond = (strcmp(nome, "ifndef") == 0) ? !def : def;
    if (n_cond >= MAX_COND) erro_fatal(arquivo, numero_linha, "#if aninhado demais");
    pilha_cond[n_cond].pai_ativo = ativo_pai;
    pilha_cond[n_cond].ativo = ativo_pai && cond;
    pilha_cond[n_cond].algum_ramo_ativo = pilha_cond[n_cond].ativo;
    pilha_cond[n_cond].teve_else = false;
    n_cond++;
    return;
  }
  if (strcmp(nome, "if") == 0) {
    dyn_t exp; dyn_init(&exp);
    pilha_macro_t pilha = { .n = 0 };
    expande_texto(s, &exp, &pilha, arquivo, numero_linha);
    long v = ativo_pai ? avalia_if(exp.dados, arquivo, numero_linha) : 0;
    free(exp.dados);
    if (n_cond >= MAX_COND) erro_fatal(arquivo, numero_linha, "#if aninhado demais");
    pilha_cond[n_cond].pai_ativo = ativo_pai;
    pilha_cond[n_cond].ativo = ativo_pai && (v != 0);
    pilha_cond[n_cond].algum_ramo_ativo = pilha_cond[n_cond].ativo;
    pilha_cond[n_cond].teve_else = false;
    n_cond++;
    return;
  }
  if (strcmp(nome, "elif") == 0) {
    if (n_cond == 0) erro_fatal(arquivo, numero_linha, "#elif sem #if correspondente");
    cond_t *c = &pilha_cond[n_cond - 1];
    if (c->teve_else) erro_fatal(arquivo, numero_linha, "#elif depois de #else");
    dyn_t exp; dyn_init(&exp);
    pilha_macro_t pilha = { .n = 0 };
    bool pode_avaliar = c->pai_ativo && !c->algum_ramo_ativo;
    expande_texto(s, &exp, &pilha, arquivo, numero_linha);
    long v = pode_avaliar ? avalia_if(exp.dados, arquivo, numero_linha) : 0;
    free(exp.dados);
    c->ativo = pode_avaliar && (v != 0);
    if (c->ativo) c->algum_ramo_ativo = true;
    return;
  }
  if (strcmp(nome, "else") == 0) {
    if (n_cond == 0) erro_fatal(arquivo, numero_linha, "#else sem #if correspondente");
    cond_t *c = &pilha_cond[n_cond - 1];
    if (c->teve_else) erro_fatal(arquivo, numero_linha, "#else duplicado");
    c->teve_else = true;
    c->ativo = c->pai_ativo && !c->algum_ramo_ativo;
    if (c->ativo) c->algum_ramo_ativo = true;
    return;
  }
  if (strcmp(nome, "endif") == 0) {
    if (n_cond == 0) erro_fatal(arquivo, numero_linha, "#endif sem #if correspondente");
    n_cond--;
    return;
  }

  if (!ativo_pai) return; // demais diretivas não têm efeito em região inativa

  if (strcmp(nome, "define") == 0) {
    char mnome[64]; int j = 0;
    while (eh_ident_meio(*s) && j < 63) mnome[j++] = *s++;
    mnome[j] = '\0';
    if (j == 0) erro_fatal(arquivo, numero_linha, "#define sem nome de macro");
    if (*s == '(') {
      s++;
      char params[MAX_PARAMS][64]; int n_params = 0;
      while (*s != ')') {
        while (isspace((unsigned char)*s) || *s == ',') s++;
        if (*s == ')') break;
        int m = 0;
        while (eh_ident_meio(*s) && m < 63) params[n_params][m++] = *s++;
        params[n_params][m] = '\0';
        n_params++;
        while (isspace((unsigned char)*s)) s++;
      }
      if (*s == ')') s++;
      while (isspace((unsigned char)*s)) s++;
      macro_define(mnome, true, params, n_params, s);
    } else {
      while (isspace((unsigned char)*s)) s++;
      macro_define(mnome, false, NULL, 0, s);
    }
    return;
  }
  if (strcmp(nome, "undef") == 0) {
    char alvo[64]; int j = 0;
    while (eh_ident_meio(*s) && j < 63) alvo[j++] = *s++;
    alvo[j] = '\0';
    macro_undef(alvo);
    return;
  }
  if (strcmp(nome, "include") == 0) {
    bool local = (*s == '"');
    char delim_fim = local ? '"' : '>';
    if (*s != '"' && *s != '<') erro_fatal(arquivo, numero_linha, "#include: esperado \"arquivo\" ou <arquivo>");
    s++;
    char alvo[512]; int j = 0;
    while (*s != delim_fim && *s != '\0' && j < 511) alvo[j++] = *s++;
    alvo[j] = '\0';
    char achado[1024];
    if (!resolve_include(alvo, local, arquivo, dirs_include, achado, sizeof(achado)))
      erro_fatal(arquivo, numero_linha, "#include: arquivo não encontrado: %s", alvo);
    processa_arquivo(achado, saida, dirs_include, profundidade + 1);
    dyn_fmt(saida, "#LINHA %d \"%s\"\n", numero_linha + 1, arquivo);
    return;
  }
  if (strcmp(nome, "error") == 0) erro_fatal(arquivo, numero_linha, "#error: %s", s);
  if (strcmp(nome, "pragma") == 0) return;
  if (strcmp(nome, "line") == 0) return;

  erro_fatal(arquivo, numero_linha, "diretiva de pré-processador desconhecida: #%s", nome);
}

static void processa_linha_logica(char *linha, const char *arquivo, int numero_linha,
                                   dyn_t *saida, const char *dirs_include[], int profundidade)
{
  char *s = linha;
  while (isspace((unsigned char)*s)) s++;
  if (*s == '\0') return;

  if (*s == '#') { processa_diretiva(s, arquivo, numero_linha, saida, dirs_include, profundidade); return; }

  if (!ativo_agora()) return;

  dyn_t exp; dyn_init(&exp);
  pilha_macro_t pilha = { .n = 0 };
  expande_texto(linha, &exp, &pilha, arquivo, numero_linha);
  dyn_fmt(saida, "#LINHA %d \"%s\"\n%s\n", numero_linha, arquivo, exp.dados);
  free(exp.dados);
}

static void processa_arquivo(const char *caminho, dyn_t *saida, const char *dirs_include[], int profundidade)
{
  if (profundidade > 24) erro_fatal(caminho, 0, "#include aninhado demais (possível ciclo)");
  char *conteudo = le_arquivo_inteiro(caminho);
  if (conteudo == NULL) erro_fatal(caminho, 0, "não foi possível abrir o arquivo");

  int n_cond_entrada = n_cond;
  int fisica = 1;
  int linha_logica_inicio = 1;
  char linebuf[MAX_LINHA]; size_t lp = 0;
  bool dentro_str = false, dentro_chr = false, dentro_bloco = false;

  size_t n = strlen(conteudo);
  size_t i = 0;
  while (i <= n) {
    char c = (i < n) ? conteudo[i] : '\n';

    if (dentro_bloco) {
      if (c == '*' && i + 1 < n && conteudo[i + 1] == '/') { dentro_bloco = false; i += 2; continue; }
      if (c == '\n') fisica++;
      i++;
      continue;
    }
    if (!dentro_str && !dentro_chr && c == '/' && i + 1 < n && conteudo[i + 1] == '*') {
      dentro_bloco = true;
      if (lp < MAX_LINHA - 1) linebuf[lp++] = ' ';
      i += 2;
      continue;
    }
    if (!dentro_str && !dentro_chr && c == '/' && i + 1 < n && conteudo[i + 1] == '/') {
      while (i < n && conteudo[i] != '\n') i++;
      continue; // o '\n' será tratado na próxima iteração
    }
    if (!dentro_str && !dentro_chr && c == '\\' && i + 1 < n &&
        (conteudo[i + 1] == '\n' || (conteudo[i+1]=='\r' && i+2<n && conteudo[i+2]=='\n'))) {
      i += (conteudo[i+1] == '\r') ? 3 : 2;
      fisica++;
      continue; // continuação de linha: junta sem terminar a linha lógica
    }
    if (!dentro_chr && c == '"' && !dentro_str) { dentro_str = true; }
    else if (dentro_str && c == '"') { dentro_str = false; }
    else if (!dentro_str && c == '\'' && !dentro_chr) { dentro_chr = true; }
    else if (dentro_chr && c == '\'') { dentro_chr = false; }
    else if ((dentro_str || dentro_chr) && c == '\\' && i + 1 < n) {
      if (lp < MAX_LINHA - 2) { linebuf[lp++] = c; linebuf[lp++] = conteudo[i + 1]; }
      i += 2;
      continue;
    }

    if (c == '\n') {
      linebuf[lp] = '\0';
      processa_linha_logica(linebuf, caminho, linha_logica_inicio, saida, dirs_include, profundidade);
      lp = 0;
      fisica++;
      linha_logica_inicio = fisica;
      i++;
      continue;
    }

    if (lp < MAX_LINHA - 1) linebuf[lp++] = c;
    i++;
  }

  if (n_cond != n_cond_entrada) erro_fatal(caminho, fisica, "#if/#ifdef sem #endif correspondente");

  free(conteudo);
}

char *pre_processa_arquivo(const char *caminho, const char *dirs_include[])
{
  dyn_t saida; dyn_init(&saida);
  n_cond = 0;
  processa_arquivo(caminho, &saida, dirs_include, 0);
  return saida.dados;
}
