// parser.c -- o compilador propriamente dito: parser recursivo-descendente
// que constrói pequenas árvores por expressão (para resolver corretamente
// lvalue/rvalue em atribuições e o não-avaliar de sizeof(expr)), e emite
// texto em linguagem de montagem do Mancha completo diretamente enquanto
// percorre declarações e comandos (sem construir uma AST do programa
// inteiro).
//
// Cada função é compilada em duas passadas idênticas (mesma sequência de
// tokens, mesma lógica de parsing): a primeira ("seca") só descobre as
// variáveis locais e o tamanho do quadro de pilha, sem emitir nada; a
// segunda ("real") já conhece os deslocamentos de cada local (na mesma
// ordem de declaração encontrada na primeira) e emite o código de
// verdade. Isso evita ter que calcular o tamanho do quadro antes de saber
// quais variáveis locais existem, sem precisar de uma pilha de escopos
// "flutuante" em tempo de execução do programa gerado (mancha.pdf não tem
// uma instrução de "alocar N bytes dinamicamente"; o quadro inteiro da
// função é alocado de uma vez no prólogo).

#include "parser.h"
#include "tipos.h"
#include "simbolos.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>

// ============================================================== tokens

static token_t *toks;
static int n_toks;
static int pos;

static token_t *TOK(void) { return &toks[pos]; }
static tipo_token_t TT(void) { return toks[pos].tipo; }
static tipo_token_t PEEK(int off) { int i = pos + off; if (i >= n_toks) i = n_toks - 1; return toks[i].tipo; }

static void erro(const char *fmt, ...)
{
  fprintf(stderr, "%s:%d: erro: ", TOK()->arquivo, TOK()->linha);
  va_list ap; va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  fprintf(stderr, "\n");
  exit(1);
}

static void avanca(void) { if (pos < n_toks - 1) pos++; }

static void espera(tipo_token_t t)
{
  if (TT() != t) erro("esperado %s, encontrado %s", token_nome(t), token_nome(TT()));
  avanca();
}

static bool aceita(tipo_token_t t) { if (TT() == t) { avanca(); return true; } return false; }

static char nome_ident_atual(char *out, size_t tam)
{
  if (TT() != TK_IDENT) erro("esperado identificador, encontrado %s", token_nome(TT()));
  snprintf(out, tam, "%s", TOK()->texto);
  avanca();
  return 0;
}

// ============================================================ saída/emit

static bool modo_emite;
static char *buf_texto; static size_t tam_texto; static FILE *f_texto;
static char *buf_dados; static size_t tam_dados; static FILE *f_dados;
static int contador_rotulos = 0;

// prefixo exclusivo desta invocação do compilador (normalmente derivado do
// pid do processo), usado nos rótulos só-internos (_L.../_str_...) para
// que dois arquivos .c compilados separadamente e depois montados juntos
// (o montador do Mancha completo trata vários .asm como um módulo só)
// nunca colidam -- funções e variáveis globais continuam com o mesmo nome
// em todo lugar, de propósito, para permitir chamadas entre arquivos.
static char prefixo_interno[24] = "_i";

static void emite(const char *fmt, ...)
{
  if (!modo_emite) return;
  fputc('\t', f_texto);
  va_list ap; va_start(ap, fmt);
  vfprintf(f_texto, fmt, ap);
  va_end(ap);
  fputc('\n', f_texto);
}

static void define_rotulo(const char *rotulo)
{
  if (!modo_emite) return;
  fprintf(f_texto, "%s:\n", rotulo);
}

static void novo_rotulo(char *buf, size_t tam) { snprintf(buf, tam, "_L%s_%d", prefixo_interno, contador_rotulos++); }

// ============================================================= tipos C

static bool eh_inicio_de_tipo(void)
{
  return TT() == TK_KW_INT || TT() == TK_KW_CHAR || TT() == TK_KW_VOID || TT() == TK_KW_STRUCT;
}

static tipo_t *parseia_tipo_base(void)
{
  if (aceita(TK_KW_INT)) return tipo_int();
  if (aceita(TK_KW_CHAR)) return tipo_char();
  if (aceita(TK_KW_VOID)) return tipo_void();
  if (aceita(TK_KW_STRUCT)) {
    char nome[32]; nome_ident_atual(nome, sizeof(nome));
    estrutura_t *e = estrutura_declara(nome);
    return tipo_de_struct(e);
  }
  erro("tipo esperado, encontrado %s", token_nome(TT()));
  return NULL;
}

// declarador completo: estrelas antes do nome, colchetes depois.
// se "nome_opcional" for true, o identificador pode faltar (protótipos).
static tipo_t *parseia_declarador_ex(tipo_t *base, char *nome_out, size_t nome_tam, bool nome_opcional)
{
  tipo_t *t = base;
  while (aceita(TK_STAR)) t = tipo_ponteiro(t);

  if (TT() == TK_IDENT) { snprintf(nome_out, nome_tam, "%s", TOK()->texto); avanca(); }
  else if (nome_opcional) { nome_out[0] = '\0'; }
  else erro("esperado identificador na declaração");

  int dims[8]; int n_dims = 0;
  while (aceita(TK_LBRACKET)) {
    if (TT() != TK_INT_LIT) erro("tamanho de array deve ser uma constante inteira");
    if (n_dims >= 8) erro("array com dimensões demais");
    dims[n_dims++] = (int)TOK()->valor;
    avanca();
    espera(TK_RBRACKET);
  }
  for (int i = n_dims - 1; i >= 0; i--) t = tipo_array(t, dims[i]);
  return t;
}

static tipo_t *parseia_declarador(tipo_t *base, char *nome_out, size_t nome_tam)
{
  return parseia_declarador_ex(base, nome_out, nome_tam, false);
}

// =========================================================== árvore de expr

typedef enum {
  NO_INT, NO_STR, NO_VAR, NO_UNARIO, NO_BINARIO, NO_LOGICO, NO_ATRIB,
  NO_CHAMADA, NO_INDICE, NO_MEMBRO, NO_DEREF, NO_ENDERECO, NO_TERNARIO,
  NO_INCDEC, NO_CAST,
} no_tipo_t;

typedef struct no {
  no_tipo_t nt;
  tipo_t *tipo;
  long ival;
  char *str_texto; int str_len;
  simbolo_t *simb;
  int op;
  struct no *a, *b, *c;
  struct no *args[SIMB_MAX_PARAMS]; int n_args;
  char membro[32]; bool seta;
  bool prefixo;
} no_t;

static no_t *novo_no(no_tipo_t nt) { no_t *n = calloc(1, sizeof(no_t)); n->nt = nt; return n; }

static bool eh_lvalue(no_t *n)
{
  return n->nt == NO_VAR || n->nt == NO_DEREF || n->nt == NO_INDICE || n->nt == NO_MEMBRO;
}

static no_t *parseia_atribuicao(void);

static no_t *parseia_primaria(void)
{
  if (TT() == TK_INT_LIT) { no_t *n = novo_no(NO_INT); n->ival = TOK()->valor; n->tipo = tipo_int(); avanca(); return n; }
  if (TT() == TK_STRING_LIT) {
    no_t *n = novo_no(NO_STR);
    n->str_texto = malloc((size_t)TOK()->len + 1);
    memcpy(n->str_texto, TOK()->str, (size_t)TOK()->len + 1);
    n->str_len = TOK()->len;
    n->tipo = tipo_ponteiro(tipo_char());
    avanca();
    return n;
  }
  if (TT() == TK_IDENT) {
    char nome[64]; snprintf(nome, sizeof(nome), "%s", TOK()->texto);
    simbolo_t *s = simbolo_acha(nome);
    if (s == NULL) erro("identificador não declarado: '%s'", nome);
    avanca();
    no_t *n = novo_no(NO_VAR); n->simb = s; n->tipo = s->tipo;
    return n;
  }
  if (aceita(TK_LPAREN)) { no_t *n = parseia_atribuicao(); espera(TK_RPAREN); return n; }
  erro("expressão esperada, encontrado %s", token_nome(TT()));
  return NULL;
}

static no_t *parseia_posfixa(void)
{
  no_t *n = parseia_primaria();
  for (;;) {
    if (aceita(TK_LBRACKET)) {
      no_t *idx = parseia_atribuicao();
      espera(TK_RBRACKET);
      tipo_t *decaido = tipo_decai(n->tipo);
      if (decaido->cat != T_PONTEIRO) erro("indexação '[]' requer ponteiro ou array");
      no_t *m = novo_no(NO_INDICE); m->a = n; m->b = idx; m->tipo = decaido->base;
      n = m; continue;
    }
    if (aceita(TK_LPAREN)) {
      if (n->nt != NO_VAR || n->simb->categoria != SIMB_FUNCAO) erro("chamada requer uma função");
      simbolo_t *f = n->simb;
      no_t *m = novo_no(NO_CHAMADA); m->simb = f; m->n_args = 0;
      if (TT() != TK_RPAREN) {
        do {
          if (m->n_args >= SIMB_MAX_PARAMS) erro("chamada com argumentos demais");
          m->args[m->n_args++] = parseia_atribuicao();
        } while (aceita(TK_COMMA));
      }
      espera(TK_RPAREN);
      if (m->n_args != f->n_params) erro("função '%s' espera %d argumento(s), recebeu %d", f->nome, f->n_params, m->n_args);
      m->tipo = f->tipo;
      n = m; continue;
    }
    if (aceita(TK_DOT)) {
      char nome[32]; nome_ident_atual(nome, sizeof(nome));
      if (n->tipo->cat != T_STRUCT) erro("operador '.' requer struct");
      membro_t *mb = estrutura_acha_membro(n->tipo->estrutura, nome);
      if (mb == NULL) erro("struct %s não tem membro '%s'", n->tipo->estrutura->nome, nome);
      no_t *m = novo_no(NO_MEMBRO); m->a = n; snprintf(m->membro, sizeof(m->membro), "%s", nome);
      m->seta = false; m->tipo = mb->tipo;
      n = m; continue;
    }
    if (aceita(TK_ARROW)) {
      char nome[32]; nome_ident_atual(nome, sizeof(nome));
      if (n->tipo->cat != T_PONTEIRO || n->tipo->base->cat != T_STRUCT) erro("operador '->' requer ponteiro para struct");
      membro_t *mb = estrutura_acha_membro(n->tipo->base->estrutura, nome);
      if (mb == NULL) erro("struct %s não tem membro '%s'", n->tipo->base->estrutura->nome, nome);
      no_t *m = novo_no(NO_MEMBRO); m->a = n; snprintf(m->membro, sizeof(m->membro), "%s", nome);
      m->seta = true; m->tipo = mb->tipo;
      n = m; continue;
    }
    if (TT() == TK_INC || TT() == TK_DEC) {
      if (!eh_lvalue(n)) erro("operando de '++'/'--' precisa ser uma variável, *ponteiro, a[i] ou s.campo");
      no_t *m = novo_no(NO_INCDEC); m->op = TT(); avanca(); m->a = n; m->prefixo = false; m->tipo = n->tipo;
      n = m; continue;
    }
    break;
  }
  return n;
}

static no_t *parseia_unaria(void)
{
  if (aceita(TK_PLUS)) return parseia_unaria();
  if (aceita(TK_MINUS)) { no_t *o = parseia_unaria(); no_t *n = novo_no(NO_UNARIO); n->op = TK_MINUS; n->a = o; n->tipo = tipo_int(); return n; }
  if (aceita(TK_NOT)) { no_t *o = parseia_unaria(); no_t *n = novo_no(NO_UNARIO); n->op = TK_NOT; n->a = o; n->tipo = tipo_int(); return n; }
  if (aceita(TK_TILDE)) { no_t *o = parseia_unaria(); no_t *n = novo_no(NO_UNARIO); n->op = TK_TILDE; n->a = o; n->tipo = tipo_int(); return n; }
  if (aceita(TK_STAR)) {
    no_t *o = parseia_unaria();
    if (!tipo_eh_ponteiro_ou_array(o->tipo)) erro("operador '*' (desreferência) requer ponteiro");
    no_t *n = novo_no(NO_DEREF); n->a = o; n->tipo = o->tipo->base;
    return n;
  }
  if (aceita(TK_AMP)) {
    no_t *o = parseia_unaria();
    if (!eh_lvalue(o)) erro("operador '&' requer uma variável, *ponteiro, a[i] ou s.campo");
    no_t *n = novo_no(NO_ENDERECO); n->a = o; n->tipo = tipo_ponteiro(o->tipo);
    return n;
  }
  if (TT() == TK_INC || TT() == TK_DEC) {
    int op = TT(); avanca();
    no_t *o = parseia_unaria();
    if (!eh_lvalue(o)) erro("operando de '++'/'--' precisa ser uma variável, *ponteiro, a[i] ou s.campo");
    no_t *n = novo_no(NO_INCDEC); n->op = op; n->a = o; n->prefixo = true; n->tipo = o->tipo;
    return n;
  }
  if (TT() == TK_KW_SIZEOF) {
    avanca();
    espera(TK_LPAREN);
    long tam;
    if (eh_inicio_de_tipo()) {
      tipo_t *t = parseia_tipo_base();
      while (aceita(TK_STAR)) t = tipo_ponteiro(t);
      tam = tipo_tamanho(t);
    } else {
      no_t *e = parseia_atribuicao();
      tam = tipo_tamanho(e->tipo);
    }
    espera(TK_RPAREN);
    no_t *n = novo_no(NO_INT); n->ival = tam; n->tipo = tipo_int();
    return n;
  }
  if (TT() == TK_LPAREN && (PEEK(1) == TK_KW_INT || PEEK(1) == TK_KW_CHAR || PEEK(1) == TK_KW_VOID || PEEK(1) == TK_KW_STRUCT)) {
    avanca();
    tipo_t *t = parseia_tipo_base();
    while (aceita(TK_STAR)) t = tipo_ponteiro(t);
    espera(TK_RPAREN);
    no_t *o = parseia_unaria();
    no_t *n = novo_no(NO_CAST); n->a = o; n->tipo = t;
    return n;
  }
  return parseia_posfixa();
}

static const char *instr_binaria(int op)
{
  switch (op) {
    case TK_PLUS: return "add";
    case TK_MINUS: return "sub";
    case TK_STAR: return "mul";
    case TK_SLASH: return "div";
    case TK_AMP: return "and";
    case TK_PIPE: return "or";
    case TK_CARET: return "xor";
    case TK_SHL: return "shl";
    case TK_SHR: return "shr";
    default: return NULL;
  }
}

static no_t *parseia_bin_generico(no_t *(*prox)(void), const tipo_token_t *ops, int n_ops)
{
  no_t *n = prox();
  for (;;) {
    bool bate = false;
    for (int i = 0; i < n_ops; i++) if (TT() == ops[i]) { bate = true; break; }
    if (!bate) break;
    int op = TT(); avanca();
    no_t *dir = prox();
    no_t *m = novo_no(NO_BINARIO); m->op = op; m->a = n; m->b = dir;
    bool ptr_esq = tipo_eh_ponteiro_ou_array(n->tipo);
    bool ptr_dir = tipo_eh_ponteiro_ou_array(dir->tipo);
    if ((op == TK_PLUS || op == TK_MINUS) && (ptr_esq || ptr_dir)) {
      if (ptr_esq && ptr_dir && op == TK_MINUS) m->tipo = tipo_int();
      else if (ptr_esq) m->tipo = tipo_decai(n->tipo);
      else m->tipo = tipo_decai(dir->tipo);
    } else {
      m->tipo = tipo_int();
    }
    n = m;
  }
  return n;
}

static no_t *parseia_multiplicativa(void) { tipo_token_t ops[] = { TK_STAR, TK_SLASH, TK_PERCENT }; return parseia_bin_generico(parseia_unaria, ops, 3); }
static no_t *parseia_aditiva(void) { tipo_token_t ops[] = { TK_PLUS, TK_MINUS }; return parseia_bin_generico(parseia_multiplicativa, ops, 2); }
static no_t *parseia_deslocamento(void) { tipo_token_t ops[] = { TK_SHL, TK_SHR }; return parseia_bin_generico(parseia_aditiva, ops, 2); }
static no_t *parseia_relacional(void) { tipo_token_t ops[] = { TK_LT, TK_LE, TK_GT, TK_GE }; return parseia_bin_generico(parseia_deslocamento, ops, 4); }
static no_t *parseia_igualdade(void) { tipo_token_t ops[] = { TK_EQ, TK_NEQ }; return parseia_bin_generico(parseia_relacional, ops, 2); }
static no_t *parseia_bitand(void) { tipo_token_t ops[] = { TK_AMP }; return parseia_bin_generico(parseia_igualdade, ops, 1); }
static no_t *parseia_bitxor(void) { tipo_token_t ops[] = { TK_CARET }; return parseia_bin_generico(parseia_bitand, ops, 1); }
static no_t *parseia_bitor(void) { tipo_token_t ops[] = { TK_PIPE }; return parseia_bin_generico(parseia_bitxor, ops, 1); }

static no_t *parseia_and(void)
{
  no_t *n = parseia_bitor();
  while (aceita(TK_ANDAND)) { no_t *d = parseia_bitor(); no_t *m = novo_no(NO_LOGICO); m->op = TK_ANDAND; m->a = n; m->b = d; m->tipo = tipo_int(); n = m; }
  return n;
}
static no_t *parseia_or(void)
{
  no_t *n = parseia_and();
  while (aceita(TK_OROR)) { no_t *d = parseia_and(); no_t *m = novo_no(NO_LOGICO); m->op = TK_OROR; m->a = n; m->b = d; m->tipo = tipo_int(); n = m; }
  return n;
}

static no_t *parseia_ternario(void)
{
  no_t *cond = parseia_or();
  if (aceita(TK_QUESTION)) {
    no_t *v = parseia_atribuicao();
    espera(TK_COLON);
    no_t *f = parseia_atribuicao();
    no_t *n = novo_no(NO_TERNARIO); n->a = cond; n->b = v; n->c = f; n->tipo = v->tipo;
    return n;
  }
  return cond;
}

static bool eh_op_atribuicao(tipo_token_t t)
{
  switch (t) {
    case TK_ASSIGN: case TK_PLUS_ASS: case TK_MINUS_ASS: case TK_STAR_ASS: case TK_SLASH_ASS:
    case TK_PERCENT_ASS: case TK_AMP_ASS: case TK_PIPE_ASS: case TK_CARET_ASS:
    case TK_SHL_ASS: case TK_SHR_ASS:
      return true;
    default: return false;
  }
}

static no_t *parseia_atribuicao(void)
{
  no_t *esq = parseia_ternario();
  if (eh_op_atribuicao(TT())) {
    if (!eh_lvalue(esq)) erro("lado esquerdo de atribuição não é uma variável, *ponteiro, a[i] ou s.campo");
    int op = TT(); avanca();
    no_t *dir = parseia_atribuicao();
    no_t *n = novo_no(NO_ATRIB); n->op = op; n->a = esq; n->b = dir; n->tipo = esq->tipo;
    return n;
  }
  return esq;
}

// ================================================================ locais

typedef enum { LOC_LOCAL, LOC_GLOBAL, LOC_INDIRECT } loc_kind_t;
typedef struct { loc_kind_t kind; int offset; const char *rotulo; int reg; bool forca_palavra; } loc_t;

// ========================================================= pool de strings

typedef struct { char rotulo[40]; char *texto; int len; } pool_str_t;
#define MAX_POOL 4096
static pool_str_t pool[MAX_POOL];
static int n_pool = 0;

static const char *pool_adiciona(const char *texto, int len)
{
  int idx = n_pool++;
  if (idx >= MAX_POOL) { fprintf(stderr, "mcc: strings demais\n"); exit(1); }
  snprintf(pool[idx].rotulo, sizeof(pool[idx].rotulo), "_str%s_%d", prefixo_interno, idx);
  pool[idx].texto = malloc((size_t)len + 1);
  memcpy(pool[idx].texto, texto, (size_t)len + 1);
  pool[idx].len = len;
  return pool[idx].rotulo;
}

// ==================================================== geração de código

static no_t *gera_valor(no_t *n); // forward
static loc_t gera_endereco(no_t *n);

// IMPORTANTE: no Mancha, "ldb" só substitui o byte BAIXO do registrador de
// destino -- o byte alto fica com o que já estava lá antes (cpu.c,
// busca/grava_operando MOD_*: "(r[reg] & 0xFF00) | (v & 0xFF)"), ao
// contrário do que se poderia supor (não há extensão de zero automática).
// Por isso, depois de todo "ldb", mascaramos com "and r0, 255" para
// garantir um valor de 16 bits corretamente zero-estendido -- necessário
// sempre que o resultado for comparado, usado em aritmética ou passado
// adiante como int.
static void carrega(loc_t loc, tipo_t *tipo)
{
  // parâmetros de função sempre ocupam uma palavra inteira na pilha
  // (ver registra_parametros), mesmo quando o tipo C declarado é "char"
  bool eh_byte = (tipo_tamanho(tipo) == 1) && !loc.forca_palavra;
  const char *instr = eh_byte ? "ldb" : "ld";
  switch (loc.kind) {
    case LOC_LOCAL: emite("%s r0, (bp+%d)", instr, loc.offset); break;
    case LOC_GLOBAL:
      if (loc.offset == 0) emite("%s r0, (%s)", instr, loc.rotulo);
      else emite("%s r0, (%s+%d)", instr, loc.rotulo, loc.offset);
      break;
    case LOC_INDIRECT:
      if (loc.offset == 0) emite("%s r0, (r%d)", instr, loc.reg);
      else emite("%s r0, (r%d+%d)", instr, loc.reg, loc.offset);
      break;
  }
  if (eh_byte) emite("and r0, 255");
}

static void guarda(loc_t loc, tipo_t *tipo)
{
  const char *instr = (tipo_tamanho(tipo) == 1 && !loc.forca_palavra) ? "stb" : "st";
  switch (loc.kind) {
    case LOC_LOCAL: emite("%s r0, (bp+%d)", instr, loc.offset); break;
    case LOC_GLOBAL:
      if (loc.offset == 0) emite("%s r0, (%s)", instr, loc.rotulo);
      else emite("%s r0, (%s+%d)", instr, loc.rotulo, loc.offset);
      break;
    case LOC_INDIRECT:
      if (loc.offset == 0) emite("%s r0, (r%d)", instr, loc.reg);
      else emite("%s r0, (r%d+%d)", instr, loc.reg, loc.offset);
      break;
  }
}

// deixa o ENDEREÇO representado por "loc" em r0 (usado para "&x" e para o
// decaimento de array em ponteiro)
static void materializa_endereco_em_r0(loc_t loc)
{
  switch (loc.kind) {
    case LOC_LOCAL: emite("ld r0, bp"); emite("add r0, %d", loc.offset); break;
    case LOC_GLOBAL:
      if (loc.offset == 0) emite("ld r0, %s", loc.rotulo);
      else emite("ld r0, %s+%d", loc.rotulo, loc.offset);
      break;
    case LOC_INDIRECT:
      if (loc.reg != 0) emite("ld r0, r%d", loc.reg);
      if (loc.offset != 0) emite("add r0, %d", loc.offset);
      break;
  }
}

static loc_t gera_endereco(no_t *n)
{
  loc_t loc = {0};
  switch (n->nt) {
    case NO_VAR:
      if (n->simb->categoria == SIMB_LOCAL) {
        loc.kind = LOC_LOCAL; loc.offset = n->simb->offset; loc.forca_palavra = n->simb->eh_parametro;
        return loc;
      }
      if (n->simb->categoria == SIMB_GLOBAL) { loc.kind = LOC_GLOBAL; loc.rotulo = n->simb->rotulo; loc.offset = 0; return loc; }
      erro("'%s' não é uma variável", n->simb->nome);
      break;
    case NO_DEREF:
      gera_valor(n->a);
      emite("ld r1, r0");
      loc.kind = LOC_INDIRECT; loc.reg = 1; loc.offset = 0;
      return loc;
    case NO_INDICE: {
      gera_valor(n->a); // já trata o decaimento array->ponteiro
      emite("push r0");
      gera_valor(n->b);
      int tam = tipo_tamanho(n->tipo);
      emite("mul r0, %d", tam);
      emite("pop r1");
      emite("add r1, r0");
      loc.kind = LOC_INDIRECT; loc.reg = 1; loc.offset = 0;
      return loc;
    }
    case NO_MEMBRO: {
      membro_t *mb;
      if (n->seta) {
        gera_valor(n->a);
        emite("ld r1, r0");
        mb = estrutura_acha_membro(n->a->tipo->base->estrutura, n->membro);
        loc.kind = LOC_INDIRECT; loc.reg = 1; loc.offset = mb->offset;
        return loc;
      } else {
        loc_t base = gera_endereco(n->a);
        mb = estrutura_acha_membro(n->a->tipo->estrutura, n->membro);
        loc = base;
        loc.offset += mb->offset;
        return loc;
      }
    }
    default:
      erro("expressão não é uma variável, *ponteiro, a[i] ou s.campo");
  }
  loc.kind = LOC_LOCAL; loc.offset = 0;
  return loc;
}

// r0=fonte, r1=destino já calculados; copia "tam" bytes usando r1,r2,r3,r4
static void copia_bytes_r3_r4(int tam)
{
  emite("ld r3, r0");
  emite("ld r4, r1");
  emite("ld r2, 0");
  char lloop[32], lfim[32];
  novo_rotulo(lloop, sizeof(lloop));
  novo_rotulo(lfim, sizeof(lfim));
  define_rotulo(lloop);
  emite("cmp r2, %d", tam);
  emite("jmpc ge, %s", lfim);
  emite("ldb r1, (r3+)");
  emite("stb r1, (r4+)");
  emite("add r2, 1");
  emite("jmp %s", lloop);
  define_rotulo(lfim);
}

static const char *cond_relacional(int op)
{
  switch (op) {
    case TK_LT: return "lt";
    case TK_LE: return "le";
    case TK_GT: return "gt";
    case TK_GE: return "ge";
    case TK_EQ: return "eq";
    case TK_NEQ: return "ne";
    default: return "eq";
  }
}

// gera o valor booleano (0/1) de uma comparação já emitida via "cmp",
// deixando o resultado em r0. IMPORTANTE: no Mancha, "ld"/"ldb"/"pop"
// também alteram as flags N/Z (cpu.c: marca_nz é chamado no caso "ld"),
// então o "jmpc" tem que vir IMEDIATAMENTE depois do "cmp" -- nenhuma
// outra instrução pode ficar no meio, nem para preparar um valor default.
static void materializa_booleano(const char *cond_verdadeiro)
{
  char lv[32], lf[32];
  novo_rotulo(lv, sizeof(lv));
  novo_rotulo(lf, sizeof(lf));
  emite("jmpc %s, %s", cond_verdadeiro, lv);
  emite("ld r0, 0");
  emite("jmp %s", lf);
  define_rotulo(lv);
  emite("ld r0, 1");
  define_rotulo(lf);
}

// r0=atual, r1=rhs -> resultado (atual % rhs) em r0 (usa r2 como escrátil)
static void emite_modulo(void)
{
  emite("ld r2, r0");
  emite("div r0, r1");
  emite("mul r0, r1");
  emite("sub r2, r0");
  emite("ld r0, r2");
}

// pré-condição: r0 = valor atual, reg_rhs = registrador com o lado direito
// (nunca r0) -> resultado em r0. Usado só por atribuições compostas
// (+=, -=, etc.), onde o endereço do lvalue pode já estar guardado em r1.
static void aplica_operador_composto(int op, const char *reg_rhs)
{
  if (op == TK_PERCENT_ASS) {
    emite("ld r3, r0");
    emite("div r0, %s", reg_rhs);
    emite("mul r0, %s", reg_rhs);
    emite("sub r3, r0");
    emite("ld r0, r3");
    return;
  }
  const char *instr = instr_binaria(op == TK_PLUS_ASS ? TK_PLUS : op == TK_MINUS_ASS ? TK_MINUS :
                                     op == TK_STAR_ASS ? TK_STAR : op == TK_SLASH_ASS ? TK_SLASH :
                                     op == TK_AMP_ASS ? TK_AMP : op == TK_PIPE_ASS ? TK_PIPE :
                                     op == TK_CARET_ASS ? TK_CARET : op == TK_SHL_ASS ? TK_SHL :
                                     op == TK_SHR_ASS ? TK_SHR : op);
  emite("%s r0, %s", instr, reg_rhs);
}

static no_t *gera_valor(no_t *n)
{
  switch (n->nt) {
    case NO_INT:
      emite("ld r0, %ld", n->ival);
      break;
    case NO_STR: {
      const char *rot = pool_adiciona(n->str_texto, n->str_len);
      emite("ld r0, %s", rot);
      break;
    }
    case NO_VAR:
      if (n->simb->tipo->cat == T_ARRAY) { loc_t loc = gera_endereco(n); materializa_endereco_em_r0(loc); }
      else { loc_t loc = gera_endereco(n); carrega(loc, n->tipo); }
      break;
    case NO_DEREF:
    case NO_INDICE:
    case NO_MEMBRO: {
      if (n->tipo->cat == T_ARRAY) { loc_t loc = gera_endereco(n); materializa_endereco_em_r0(loc); }
      else if (n->tipo->cat == T_STRUCT) { loc_t loc = gera_endereco(n); materializa_endereco_em_r0(loc); }
      else { loc_t loc = gera_endereco(n); carrega(loc, n->tipo); }
      break;
    }
    case NO_ENDERECO: {
      loc_t loc = gera_endereco(n->a);
      materializa_endereco_em_r0(loc);
      break;
    }
    case NO_CAST:
      gera_valor(n->a);
      break;
    case NO_UNARIO:
      gera_valor(n->a);
      if (n->op == TK_MINUS) { emite("xor r0, -1"); emite("add r0, 1"); }
      else if (n->op == TK_TILDE) { emite("xor r0, -1"); }
      else if (n->op == TK_NOT) { emite("cmp r0, 0"); materializa_booleano("eq"); }
      break;
    case NO_LOGICO: {
      char lfim[32], lcurto[32];
      novo_rotulo(lfim, sizeof(lfim));
      novo_rotulo(lcurto, sizeof(lcurto));
      gera_valor(n->a);
      emite("cmp r0, 0");
      if (n->op == TK_ANDAND) {
        emite("jmpc eq, %s", lcurto); // esquerda falsa: resultado 0
        gera_valor(n->b);
        emite("cmp r0, 0");
        emite("jmpc eq, %s", lcurto);
        emite("ld r0, 1");
        emite("jmp %s", lfim);
        define_rotulo(lcurto);
        emite("ld r0, 0");
      } else {
        emite("jmpc ne, %s", lcurto); // esquerda verdadeira: resultado 1
        gera_valor(n->b);
        emite("cmp r0, 0");
        emite("jmpc ne, %s", lcurto);
        emite("ld r0, 0");
        emite("jmp %s", lfim);
        define_rotulo(lcurto);
        emite("ld r0, 1");
      }
      define_rotulo(lfim);
      break;
    }
    case NO_BINARIO: {
      bool ptr_esq = tipo_eh_ponteiro_ou_array(n->a->tipo);
      bool ptr_dir = tipo_eh_ponteiro_ou_array(n->b->tipo);
      bool eh_relacional = (n->op == TK_LT || n->op == TK_LE || n->op == TK_GT || n->op == TK_GE ||
                             n->op == TK_EQ || n->op == TK_NEQ);
      if (ptr_esq && ptr_dir && n->op == TK_MINUS) {
        int tam = tipo_tamanho(tipo_decai(n->a->tipo)->base);
        gera_valor(n->a); emite("push r0");
        gera_valor(n->b); emite("ld r1, r0");
        emite("pop r0");
        emite("sub r0, r1");
        if (tam > 1) emite("div r0, %d", tam);
        break;
      }
      if ((n->op == TK_PLUS || n->op == TK_MINUS) && (ptr_esq || ptr_dir)) {
        no_t *pno = ptr_esq ? n->a : n->b;
        no_t *ino = ptr_esq ? n->b : n->a;
        int tam = tipo_tamanho(tipo_decai(pno->tipo)->base);
        gera_valor(pno); emite("push r0");
        gera_valor(ino); if (tam > 1) emite("mul r0, %d", tam);
        emite("ld r1, r0");
        emite("pop r0");
        emite("%s r0, r1", n->op == TK_PLUS ? "add" : "sub");
        break;
      }
      gera_valor(n->a);
      emite("push r0");
      gera_valor(n->b);
      emite("ld r1, r0");
      emite("pop r0");
      if (eh_relacional) {
        emite("cmp r0, r1");
        materializa_booleano(cond_relacional(n->op));
      } else if (n->op == TK_PERCENT) {
        emite_modulo();
      } else {
        const char *instr = instr_binaria(n->op);
        emite("%s r0, r1", instr);
      }
      break;
    }
    case NO_TERNARIO: {
      char lfalso[32], lfim[32];
      novo_rotulo(lfalso, sizeof(lfalso));
      novo_rotulo(lfim, sizeof(lfim));
      gera_valor(n->a);
      emite("cmp r0, 0");
      emite("jmpc eq, %s", lfalso);
      gera_valor(n->b);
      emite("jmp %s", lfim);
      define_rotulo(lfalso);
      gera_valor(n->c);
      define_rotulo(lfim);
      break;
    }
    case NO_INCDEC: {
      loc_t loc = gera_endereco(n->a);
      int delta = tipo_eh_ponteiro_ou_array(n->tipo) ? tipo_tamanho(tipo_decai(n->tipo)->base) : 1;
      if (n->op == TK_DEC) delta = -delta;
      carrega(loc, n->tipo);
      if (n->prefixo) {
        emite("add r0, %d", delta);
        guarda(loc, n->tipo);
      } else {
        emite("push r0");
        emite("add r0, %d", delta);
        guarda(loc, n->tipo);
        emite("pop r0");
      }
      break;
    }
    case NO_ATRIB: {
      if (n->a->tipo->cat == T_STRUCT && n->op == TK_ASSIGN) {
        loc_t ldest = gera_endereco(n->a);
        materializa_endereco_em_r0(ldest);
        emite("push r0");                    // pilha: [destino]
        loc_t lsrc = gera_endereco(n->b);
        materializa_endereco_em_r0(lsrc);    // r0 = fonte
        emite("pop r1");                       // r1 = destino
        copia_bytes_r3_r4(tipo_tamanho(n->a->tipo)); // espera r0=fonte, r1=destino
        break;
      }
      loc_t loc = gera_endereco(n->a);
      bool ind = (loc.kind == LOC_INDIRECT);
      if (ind) emite("push r%d", loc.reg);   // protege o endereço através da avaliação do lado direito
      gera_valor(n->b);                        // r0 = RHS
      if (n->op == TK_ASSIGN) {
        if (ind) { emite("pop r1"); loc.reg = 1; }
        guarda(loc, n->tipo);
      } else {
        emite("push r0");                      // salva RHS
        if (ind) {
          emite("pop r2");                      // r2 = RHS
          emite("pop r1");                      // r1 = endereço
          loc.reg = 1;
          carrega(loc, n->tipo);                 // r0 = valor atual (usa r1)
          aplica_operador_composto(n->op, "r2"); // r0 = atual OP rhs
        } else {
          carrega(loc, n->tipo);                 // r0 = valor atual (LOCAL/GLOBAL: seguro, RHS ainda na pilha)
          emite("pop r1");                        // r1 = RHS
          aplica_operador_composto(n->op, "r1");
        }
        guarda(loc, n->tipo);
      }
      break;
    }
    case NO_CHAMADA: {
      for (int i = n->n_args - 1; i >= 0; i--) { gera_valor(n->args[i]); emite("push r0"); }
      emite("call %s", n->simb->rotulo);
      if (n->n_args > 0) emite("add sp, %d", n->n_args * 2);
      break;
    }
  }
  return n;
}

// =============================================================== globais

static void emite_dados(const char *fmt, ...)
{
  fputc('\t', f_dados);
  va_list ap; va_start(ap, fmt);
  vfprintf(f_dados, fmt, ap);
  va_end(ap);
  fputc('\n', f_dados);
}

static void emite_zeros(int n_bytes)
{
  for (int i = 0; i < n_bytes; i++) emite_dados(".db 0");
}

// avalia uma expressão constante simples (inteiro, char, unário -) usada em
// inicializadores de globais -- não aceita variáveis nem chamadas
static long avalia_constante(no_t *n)
{
  switch (n->nt) {
    case NO_INT: return n->ival;
    case NO_UNARIO:
      if (n->op == TK_MINUS) return -avalia_constante(n->a);
      if (n->op == TK_TILDE) return ~avalia_constante(n->a);
      if (n->op == TK_NOT) return !avalia_constante(n->a);
      break;
    default: break;
  }
  erro("inicializador de variável global precisa ser uma constante inteira");
  return 0;
}

static void emite_inicializador_global(tipo_t *tipo);

static void emite_valor_escalar_constante(tipo_t *tipo, no_t *n)
{
  if (tipo->cat == T_PONTEIRO && tipo->base->cat == T_CHAR && n->nt == NO_STR) {
    const char *rot = pool_adiciona(n->str_texto, n->str_len);
    emite_dados(".dw %s", rot);
    return;
  }
  long v = avalia_constante(n);
  if (tipo_tamanho(tipo) == 1) emite_dados(".db %ld", v & 0xFF);
  else emite_dados(".dw %ld", v);
}

static void emite_inicializador_global(tipo_t *tipo);

// declaradores globais: nome já consumido; consome opcionalmente "= inicializador"
// (não consome o ';'/',' final -- isso é responsabilidade do chamador)
static void declara_global_com_inicializador(simbolo_t *g, tipo_t *tipo)
{
  fprintf(f_dados, "%s:\n", g->rotulo);
  if (aceita(TK_ASSIGN)) emite_inicializador_global(tipo);
  else emite_zeros(tipo_tamanho(tipo));
}

static void emite_inicializador_global(tipo_t *tipo)
{
  if (tipo->cat == T_ARRAY || tipo->cat == T_STRUCT) {
    espera(TK_LBRACE);
    if (tipo->cat == T_ARRAY) {
      int i = 0;
      if (TT() != TK_RBRACE) {
        do { if (i >= tipo->qtd) erro("inicializador tem elementos demais"); emite_inicializador_global(tipo->base); i++; } while (aceita(TK_COMMA) && TT() != TK_RBRACE);
      }
      while (i < tipo->qtd) { emite_zeros(tipo_tamanho(tipo->base)); i++; }
    } else {
      membro_t *m = tipo->estrutura->membros;
      if (TT() != TK_RBRACE) {
        do { if (m == NULL) erro("inicializador tem elementos demais"); emite_inicializador_global(m->tipo); m = m->prox; } while (aceita(TK_COMMA) && TT() != TK_RBRACE);
      }
      while (m != NULL) { emite_zeros(tipo_tamanho(m->tipo)); m = m->prox; }
    }
    espera(TK_RBRACE);
  } else {
    no_t *n = parseia_atribuicao();
    emite_valor_escalar_constante(tipo, n);
  }
}

// ============================================================= comandos

#define MAX_LOCAIS_FUNC 2048
static simbolo_t *locais_lista[MAX_LOCAIS_FUNC];
static int n_locais_lista;
static int idx_consumo_local;
static int offset_corrente;

#define MAX_LACOS 128
static char pilha_break[MAX_LACOS][32];
static char pilha_continue[MAX_LACOS][32];
static int n_lacos = 0;

static tipo_t *tipo_retorno_atual;

static void emite_epilogo(void)
{
  emite("ld sp, bp");
  emite("pop bp");
  emite("ret");
}

static void compila_bloco(void); // forward
static void compila_comando(void);

static void compila_declaracao_local(void)
{
  tipo_t *base = parseia_tipo_base();
  do {
    char nome[64];
    tipo_t *tipo = parseia_declarador(base, nome, sizeof(nome));
    if (tipo->cat == T_STRUCT && !tipo->estrutura->completa)
      erro("tipo incompleto: struct %s", tipo->estrutura->nome);

    simbolo_t *s;
    if (!modo_emite) {
      offset_corrente -= tipo_tamanho(tipo);
      s = simbolo_local_cria(nome, tipo, offset_corrente);
      if (n_locais_lista >= MAX_LOCAIS_FUNC) erro("função com variáveis locais demais");
      locais_lista[n_locais_lista++] = s;
    } else {
      s = locais_lista[idx_consumo_local++];
    }
    escopo_declara(s);

    if (aceita(TK_ASSIGN)) {
      if (tipo->cat == T_STRUCT || tipo->cat == T_ARRAY) erro("inicializador de struct/array só é suportado em variáveis globais");
      no_t *e = parseia_atribuicao();
      gera_valor(e);
      loc_t loc = {0}; loc.kind = LOC_LOCAL; loc.offset = s->offset;
      guarda(loc, tipo);
    }
  } while (aceita(TK_COMMA));
  espera(TK_SEMI);
}

static void compila_if(void)
{
  espera(TK_LPAREN);
  no_t *cond = parseia_atribuicao();
  espera(TK_RPAREN);
  gera_valor(cond);
  emite("cmp r0, 0");
  char lelse[32], lfim[32];
  novo_rotulo(lelse, sizeof(lelse));
  emite("jmpc eq, %s", lelse);
  compila_comando();
  if (aceita(TK_KW_ELSE)) {
    novo_rotulo(lfim, sizeof(lfim));
    emite("jmp %s", lfim);
    define_rotulo(lelse);
    compila_comando();
    define_rotulo(lfim);
  } else {
    define_rotulo(lelse);
  }
}

static void empilha_laco(const char *rbreak, const char *rcontinue)
{
  if (n_lacos >= MAX_LACOS) erro("laços aninhados demais");
  snprintf(pilha_break[n_lacos], 32, "%s", rbreak);
  snprintf(pilha_continue[n_lacos], 32, "%s", rcontinue);
  n_lacos++;
}
static void desempilha_laco(void) { n_lacos--; }

static void compila_while(void)
{
  char lini[32], lfim[32];
  novo_rotulo(lini, sizeof(lini));
  novo_rotulo(lfim, sizeof(lfim));
  espera(TK_LPAREN);
  no_t *cond = parseia_atribuicao();
  espera(TK_RPAREN);
  define_rotulo(lini);
  gera_valor(cond);
  emite("cmp r0, 0");
  emite("jmpc eq, %s", lfim);
  empilha_laco(lfim, lini);
  compila_comando();
  desempilha_laco();
  emite("jmp %s", lini);
  define_rotulo(lfim);
}

static void compila_do_while(void)
{
  char lini[32], lcond[32], lfim[32];
  novo_rotulo(lini, sizeof(lini));
  novo_rotulo(lcond, sizeof(lcond));
  novo_rotulo(lfim, sizeof(lfim));
  define_rotulo(lini);
  empilha_laco(lfim, lcond);
  compila_comando();
  desempilha_laco();
  define_rotulo(lcond);
  espera(TK_KW_WHILE);
  espera(TK_LPAREN);
  no_t *cond = parseia_atribuicao();
  espera(TK_RPAREN);
  espera(TK_SEMI);
  gera_valor(cond);
  emite("cmp r0, 0");
  emite("jmpc ne, %s", lini);
  define_rotulo(lfim);
}

static void compila_for(void)
{
  espera(TK_LPAREN);
  int marca = escopo_marca();
  if (TT() == TK_SEMI) avanca();
  else if (eh_inicio_de_tipo()) compila_declaracao_local();
  else { no_t *e = parseia_atribuicao(); gera_valor(e); espera(TK_SEMI); }

  char lini[32], lupd[32], lfim[32];
  novo_rotulo(lini, sizeof(lini));
  novo_rotulo(lupd, sizeof(lupd));
  novo_rotulo(lfim, sizeof(lfim));
  define_rotulo(lini);
  bool tem_cond = (TT() != TK_SEMI);
  no_t *cond = NULL;
  if (tem_cond) cond = parseia_atribuicao();
  espera(TK_SEMI);
  if (tem_cond) { gera_valor(cond); emite("cmp r0, 0"); emite("jmpc eq, %s", lfim); }
  no_t *upd = NULL;
  if (TT() != TK_RPAREN) upd = parseia_atribuicao();
  espera(TK_RPAREN);

  empilha_laco(lfim, lupd);
  compila_comando();
  desempilha_laco();
  define_rotulo(lupd);
  if (upd != NULL) gera_valor(upd);
  emite("jmp %s", lini);
  define_rotulo(lfim);
  escopo_volta(marca);
}

static void compila_comando(void)
{
  if (aceita(TK_SEMI)) return;
  if (TT() == TK_LBRACE) { compila_bloco(); return; }
  if (TT() == TK_KW_IF) { avanca(); compila_if(); return; }
  if (TT() == TK_KW_WHILE) { avanca(); compila_while(); return; }
  if (TT() == TK_KW_DO) { avanca(); compila_do_while(); return; }
  if (TT() == TK_KW_FOR) { avanca(); compila_for(); return; }
  if (TT() == TK_KW_BREAK) {
    avanca(); espera(TK_SEMI);
    if (n_lacos == 0) erro("'break' fora de um laço");
    emite("jmp %s", pilha_break[n_lacos - 1]);
    return;
  }
  if (TT() == TK_KW_CONTINUE) {
    avanca(); espera(TK_SEMI);
    if (n_lacos == 0) erro("'continue' fora de um laço");
    emite("jmp %s", pilha_continue[n_lacos - 1]);
    return;
  }
  if (TT() == TK_KW_RETURN) {
    avanca();
    if (TT() != TK_SEMI) {
      if (tipo_retorno_atual->cat == T_VOID) erro("função 'void' não pode retornar um valor");
      no_t *e = parseia_atribuicao();
      gera_valor(e);
    } else if (tipo_retorno_atual->cat != T_VOID) {
      erro("função com retorno '%s' precisa de um valor em 'return'", tipo_texto(tipo_retorno_atual));
    }
    espera(TK_SEMI);
    emite_epilogo();
    return;
  }
  if (TT() == TK_KW_STRUCT && PEEK(1) == TK_IDENT && PEEK(2) == TK_LBRACE) {
    // definição de struct local (registrada globalmente, ver compila_declaracao_topo)
    avanca();
    char nome[32]; nome_ident_atual(nome, sizeof(nome));
    estrutura_t *e = estrutura_declara(nome);
    if (!e->completa) {
      espera(TK_LBRACE);
      while (TT() != TK_RBRACE) {
        tipo_t *base = parseia_tipo_base();
        do { char mnome[64]; tipo_t *mt = parseia_declarador(base, mnome, sizeof(mnome)); estrutura_adiciona_membro(e, mnome, mt); } while (aceita(TK_COMMA));
        espera(TK_SEMI);
      }
      espera(TK_RBRACE);
      e->completa = true;
    } else {
      // já definida (2ª passada): pula o corpo
      espera(TK_LBRACE);
      int prof = 1;
      while (prof > 0) { if (TT() == TK_LBRACE) prof++; else if (TT() == TK_RBRACE) prof--; avanca(); }
    }
    espera(TK_SEMI);
    return;
  }
  if (eh_inicio_de_tipo()) { compila_declaracao_local(); return; }
  no_t *e = parseia_atribuicao();
  gera_valor(e);
  espera(TK_SEMI);
}

static void compila_bloco(void)
{
  espera(TK_LBRACE);
  int marca = escopo_marca();
  while (TT() != TK_RBRACE) compila_comando();
  espera(TK_RBRACE);
  escopo_volta(marca);
}

// ============================================================== funções

#define MAX_FUNCOES_PEND 512
typedef struct { simbolo_t *f; char nomes_param[SIMB_MAX_PARAMS][32]; } pendente_t;
static pendente_t pendentes[MAX_FUNCOES_PEND];
static int n_pendentes = 0;

static void registra_parametros(simbolo_t *f, char nomes[][32])
{
  int off = 4;
  for (int i = 0; i < f->n_params; i++) {
    if (nomes[i][0] != '\0') {
      simbolo_t *p = simbolo_local_cria(nomes[i], f->params[i], off);
      p->eh_parametro = true;
      escopo_declara(p);
    }
    off += 2;
  }
}

static void compila_corpo_funcao(pendente_t *pend)
{
  simbolo_t *f = pend->f;
  tipo_retorno_atual = f->tipo;
  int idx_abre = f->idx_corpo_abre;

  // --- passada seca: descobre variáveis locais e tamanho do quadro
  modo_emite = false;
  escopo_reinicia();
  n_locais_lista = 0;
  offset_corrente = 0;
  registra_parametros(f, pend->nomes_param);
  pos = idx_abre;
  compila_bloco();
  int tamanho_quadro = -offset_corrente;

  // --- passada real: emite o código de verdade
  modo_emite = true;
  escopo_reinicia();
  idx_consumo_local = 0;
  define_rotulo(f->rotulo);
  emite("push bp");
  emite("ld bp, sp");
  if (tamanho_quadro > 0) emite("sub sp, %d", tamanho_quadro);
  registra_parametros(f, pend->nomes_param);
  pos = idx_abre;
  compila_bloco();
  emite_epilogo();
}

// ======================================================= declarações de topo

static void pula_ate_fechar(tipo_token_t abre, tipo_token_t fecha)
{
  int prof = 1;
  while (prof > 0) {
    if (TT() == abre) prof++;
    else if (TT() == fecha) prof--;
    if (TT() == TK_EOF) erro("chaves desbalanceadas");
    avanca();
  }
}

static void compila_declaracao_topo(void)
{
  if (TT() == TK_KW_STRUCT && PEEK(1) == TK_IDENT && PEEK(2) == TK_LBRACE) {
    avanca();
    char nome[32]; nome_ident_atual(nome, sizeof(nome));
    estrutura_t *e = estrutura_declara(nome);
    if (e->completa) erro("struct '%s' redefinida", nome);
    espera(TK_LBRACE);
    while (TT() != TK_RBRACE) {
      tipo_t *base = parseia_tipo_base();
      do {
        char mnome[64];
        tipo_t *mt = parseia_declarador(base, mnome, sizeof(mnome));
        estrutura_adiciona_membro(e, mnome, mt);
      } while (aceita(TK_COMMA));
      espera(TK_SEMI);
    }
    espera(TK_RBRACE);
    espera(TK_SEMI);
    e->completa = true;
    return;
  }

  tipo_t *base = parseia_tipo_base();

  do {
    char nome[64];
    tipo_t *tipo_decl = parseia_declarador(base, nome, sizeof(nome));

    if (TT() == TK_LPAREN) {
      simbolo_t *f = global_declara_funcao(nome, tipo_decl);
      avanca();
      tipo_t *params[SIMB_MAX_PARAMS];
      char nomes_param[SIMB_MAX_PARAMS][32];
      int n = 0;
      if (TT() == TK_KW_VOID && PEEK(1) == TK_RPAREN) { avanca(); }
      else if (TT() != TK_RPAREN) {
        do {
          if (n >= SIMB_MAX_PARAMS) erro("função com parâmetros demais");
          tipo_t *pbase = parseia_tipo_base();
          nomes_param[n][0] = '\0';
          tipo_t *ptipo = parseia_declarador_ex(pbase, nomes_param[n], sizeof(nomes_param[n]), true);
          params[n] = tipo_eh_ponteiro_ou_array(ptipo) ? tipo_decai(ptipo) : ptipo;
          n++;
        } while (aceita(TK_COMMA));
      }
      espera(TK_RPAREN);
      if (f->declarada && f->n_params != n) erro("declaração de '%s' não bate com o protótipo anterior", nome);
      f->n_params = n;
      for (int i = 0; i < n; i++) f->params[i] = params[i];
      f->declarada = true;

      if (TT() == TK_LBRACE) {
        if (f->definida) erro("função '%s' redefinida", nome);
        f->definida = true;
        f->idx_corpo_abre = pos;
        int inicio = pos;
        avanca();
        pula_ate_fechar(TK_LBRACE, TK_RBRACE);
        f->idx_corpo_fecha = pos - 1;
        if (n_pendentes >= MAX_FUNCOES_PEND) erro("funções demais");
        pendentes[n_pendentes].f = f;
        for (int i = 0; i < n; i++) snprintf(pendentes[n_pendentes].nomes_param[i], 32, "%s", nomes_param[i]);
        n_pendentes++;
        (void)inicio;
      } else {
        espera(TK_SEMI);
      }
      return;
    }

    if (tipo_decl->cat == T_STRUCT && !tipo_decl->estrutura->completa)
      erro("tipo incompleto: struct %s", tipo_decl->estrutura->nome);
    if (global_acha(nome) != NULL) erro("símbolo global '%s' redefinido", nome);
    simbolo_t *g = global_declara_var(nome, tipo_decl);
    declara_global_com_inicializador(g, tipo_decl);
  } while (aceita(TK_COMMA));

  espera(TK_SEMI);
}

// ==================================================================== driver

void compilador_compila(token_t *tokens, int n_tokens, FILE *saida, unsigned prefixo)
{
  toks = tokens; n_toks = n_tokens; pos = 0;
  tipos_inicializa();
  simbolos_inicializa();
  n_pendentes = 0; n_pool = 0; contador_rotulos = 0; n_lacos = 0;
  snprintf(prefixo_interno, sizeof(prefixo_interno), "%x", prefixo);

  f_texto = open_memstream(&buf_texto, &tam_texto);
  f_dados = open_memstream(&buf_dados, &tam_dados);
  modo_emite = true;

  while (TT() != TK_EOF) compila_declaracao_topo();

  for (int i = 0; i < n_pendentes; i++) compila_corpo_funcao(&pendentes[i]);

  fclose(f_texto);
  fclose(f_dados);

  fprintf(saida, "; ===== código gerado por mcc =====\n");
  fprintf(saida, ".text\n");
  fwrite(buf_texto, 1, tam_texto, saida);
  fprintf(saida, "\n.data\n");
  fwrite(buf_dados, 1, tam_dados, saida);
  fprintf(saida, "\n; ----- literais de string -----\n");
  for (int i = 0; i < n_pool; i++) {
    fprintf(saida, "%s:\n\t.db ", pool[i].rotulo);
    for (int k = 0; k <= pool[i].len; k++) fprintf(saida, "%d%s", (unsigned char)pool[i].texto[k], k < pool[i].len ? ", " : "\n");
    if (pool[i].len == 0) fprintf(saida, "0\n");
  }

  free(buf_texto);
  free(buf_dados);
}
