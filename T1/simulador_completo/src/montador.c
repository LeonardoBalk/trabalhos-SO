// montador.c -- montador de dois passos para a linguagem de montagem do
// Mancha completo (mancha.pdf, secções 5, 6, 9 e 10).
//
// Simplificação assumida (igual à do projeto do Mancha Mínimo, mesma
// justificativa): este montador resolve todos os símbolos para
// endereços absolutos já na montagem, atuando como "montador + ligador"
// em um só passo -- não há marcas de relocação binárias nem um ligador
// separado. Vários arquivos .asm dados numa só chamada são tratados como
// um único módulo (permite usar .ext/.pub entre eles mesmo sem ligador).
//
// Os três segmentos (.text/.data/.bss) compartilham um único contador de
// endereço de carga -- eles servem apenas para marcar, no arquivo objeto,
// se cada byte pertence ao segmento de código ou de dados (.bss só
// reserva espaço, sem gerar bytes, já que a memória do simulador começa
// zerada).

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>

#include "instrucao.h"
#include "objeto.h"

#define MAX_LINHAS 16384
#define MAX_SIMBOLOS 4096
#define MAX_PUBLICOS 256

typedef struct { char nome[64]; long valor; } simb_mont_t;
typedef struct { char texto[320]; char *arquivo; int numero; } linha_t;

static linha_t linhas[MAX_LINHAS];
static int n_linhas = 0;

static simb_mont_t simbolos[MAX_SIMBOLOS];
static int n_simbolos = 0;

static char publicos[MAX_PUBLICOS][64];
static int n_publicos = 0;

static long loc = 0;
static char segmento_atual = 'T';
static FILE *saida = NULL;

// ------------------------------------------------------------- utilidades

static void erro(const linha_t *l, const char *fmt, ...)
{
  fprintf(stderr, "%s:%d: erro: ", l->arquivo, l->numero);
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  fprintf(stderr, "\n");
  exit(1);
}

static void trim(char *s)
{
  size_t n = strlen(s);
  while (n > 0 && isspace((unsigned char)s[n - 1])) s[--n] = '\0';
  size_t i = 0;
  while (s[i] != '\0' && isspace((unsigned char)s[i])) i++;
  if (i > 0) memmove(s, s + i, strlen(s + i) + 1);
}

static int divide_virgula(char *s, char *out[], int max)
{
  int n = 0;
  char *tok = strtok(s, ",");
  while (tok != NULL && n < max) {
    trim(tok);
    out[n++] = tok;
    tok = strtok(NULL, ",");
  }
  return n;
}

static void emite_byte(uint16_t endereco, uint8_t valor)
{
  obj_escreve_byte(saida, segmento_atual, endereco, valor);
}

// ------------------------------------------------------------ símbolos

static simb_mont_t *acha_simbolo(const char *nome)
{
  for (int i = 0; i < n_simbolos; i++)
    if (strcmp(simbolos[i].nome, nome) == 0) return &simbolos[i];
  return NULL;
}

static void define_simbolo(const linha_t *l, const char *nome, long valor)
{
  if (acha_simbolo(nome) != NULL) erro(l, "símbolo redefinido: %s", nome);
  if (n_simbolos >= MAX_SIMBOLOS) erro(l, "tabela de símbolos cheia");
  snprintf(simbolos[n_simbolos].nome, sizeof(simbolos[n_simbolos].nome), "%s", nome);
  simbolos[n_simbolos].valor = valor;
  n_simbolos++;
}

// resolve um termo (número, caracter, $, ou símbolo com deslocamento
// opcional +N/-N). "relativo" só afeta símbolos (não números), conforme
// mancha.pdf secção 10: "ao encontrar valores numéricos, o montador não
// os muda... ao encontrar símbolos, o montador subtrai seus valores do
// endereço corrente" -- usado por bra/brac/braq.
static long resolve_expr(const linha_t *l, const char *tok_in, bool relativo, long endereco_relativo)
{
  char tok[128];
  snprintf(tok, sizeof(tok), "%s", tok_in);
  trim(tok);
  if (tok[0] == '\0') erro(l, "expressão vazia");

  if (tok[0] == '\'') {
    size_t n = strlen(tok);
    if (n >= 3 && tok[n - 1] == '\'') return (long)(unsigned char)tok[1];
    erro(l, "caractere mal formado: %s", tok);
  }

  if (strcmp(tok, "$") == 0) return loc;

  char *sinal = NULL;
  for (char *p = tok + 1; *p != '\0'; p++)
    if (*p == '+' || *p == '-') sinal = p;

  char base[128];
  long deslocamento = 0;
  bool tem_deslocamento = false;
  if (sinal != NULL) {
    size_t blen = (size_t)(sinal - tok);
    snprintf(base, sizeof(base), "%.*s", (int)blen, tok);
    deslocamento = strtol(sinal, NULL, 0);
    tem_deslocamento = true;
  } else {
    snprintf(base, sizeof(base), "%s", tok);
  }

  char *fim;
  long numero = strtol(base, &fim, 0);
  if (base[0] != '\0' && *fim == '\0') return numero + (tem_deslocamento ? deslocamento : 0);

  simb_mont_t *s = acha_simbolo(base);
  if (s == NULL) erro(l, "símbolo indefinido: %s", base);
  long valor = s->valor;
  if (relativo) valor -= endereco_relativo;
  return valor + (tem_deslocamento ? deslocamento : 0);
}

// --------------------------------------------------- modos de endereçamento

// determina o modo de endereçamento e, se aplicável, o texto da expressão
// imediata -- puramente sintático, não resolve símbolos (por isso pode
// ser chamado igualmente nas duas passadas)
static void parse_data(const linha_t *l, char *tok, int *mod, int *dreg, char *expr, size_t expr_tam)
{
  *dreg = 0;
  expr[0] = '\0';
  size_t len = strlen(tok);

  if (len >= 2 && tok[0] == '(' && tok[len - 1] == ')') {
    char inner[256];
    snprintf(inner, sizeof(inner), "%.*s", (int)(len - 2), tok + 1);
    trim(inner);
    size_t ilen = strlen(inner);

    if (ilen > 0 && inner[0] == '-') {
      int r = reg_nome_para_codigo(inner + 1);
      if (r < 0) erro(l, "registrador inválido: %s", inner + 1);
      *mod = MOD_PREDEC; *dreg = r;
      return;
    }
    if (ilen > 0 && inner[ilen - 1] == '+') {
      char nome[64];
      snprintf(nome, sizeof(nome), "%.*s", (int)(ilen - 1), inner);
      int r = reg_nome_para_codigo(nome);
      if (r < 0) erro(l, "registrador inválido: %s", nome);
      *mod = MOD_POSINC; *dreg = r;
      return;
    }
    char *mais = strchr(inner, '+');
    if (mais != NULL) {
      char nome[64];
      snprintf(nome, sizeof(nome), "%.*s", (int)(mais - inner), inner);
      trim(nome);
      int r = reg_nome_para_codigo(nome);
      if (r >= 0) {
        *mod = MOD_DESLOC; *dreg = r;
        snprintf(expr, expr_tam, "%s", mais + 1);
        return;
      }
    }
    int r = reg_nome_para_codigo(inner);
    if (r >= 0) { *mod = MOD_IND; *dreg = r; return; }
    *mod = MOD_ABS;
    snprintf(expr, expr_tam, "%s", inner);
    return;
  }

  int r = reg_nome_para_codigo(tok);
  if (r >= 0) { *mod = MOD_REG; *dreg = r; return; }
  *mod = MOD_IMED;
  snprintf(expr, expr_tam, "%s", tok);
}

static bool mod_precisa_imediato(int mod)
{
  return mod == MOD_IMED || mod == MOD_ABS || mod == MOD_DESLOC;
}

// -------------------------------------------------------------- instruções

static void monta_instrucao(const linha_t *l, const instr_info_t *info, char *operandos, bool primeira)
{
  char *partes[4];
  int n = divide_virgula(operandos, partes, 4);

  int mod = -1, dreg = -1, reg = -1, cond = 0;
  char expr[128] = "";
  int tamanho = 2;

  switch (info->forma) {
    case FORMA_REG_DATA:
      if (n != 2) erro(l, "%s espera 2 operandos", info->mnemonico);
      reg = reg_nome_para_codigo(partes[0]);
      if (reg < 0) erro(l, "registrador inválido: %s", partes[0]);
      parse_data(l, partes[1], &mod, &dreg, expr, sizeof(expr));
      if (mod_precisa_imediato(mod)) tamanho += 2;
      break;
    case FORMA_SUP_DATA:
      if (n != 2) erro(l, "%s espera 2 operandos", info->mnemonico);
      reg = sup_nome_para_codigo(partes[0]);
      if (reg < 0) erro(l, "registrador de supervisor inválido: %s", partes[0]);
      parse_data(l, partes[1], &mod, &dreg, expr, sizeof(expr));
      if (mod_precisa_imediato(mod)) tamanho += 2;
      break;
    case FORMA_REG_IM10:
      if (n != 2) erro(l, "%s espera 2 operandos", info->mnemonico);
      reg = reg_nome_para_codigo(partes[0]);
      if (reg < 0) erro(l, "registrador inválido: %s", partes[0]);
      snprintf(expr, sizeof(expr), "%s", partes[1]);
      break;
    case FORMA_DATA_REG7:
    case FORMA_DATA_REG0:
      if (n != 1) erro(l, "%s espera 1 operando", info->mnemonico);
      reg = info->fixo;
      parse_data(l, partes[0], &mod, &dreg, expr, sizeof(expr));
      if (mod_precisa_imediato(mod)) tamanho += 2;
      break;
    case FORMA_COND_IM6:
      if (n != 2) erro(l, "%s espera 2 operandos", info->mnemonico);
      cond = cond_nome_para_codigo(partes[0]);
      if (cond < 0) erro(l, "condição inválida: %s", partes[0]);
      snprintf(expr, sizeof(expr), "%s", partes[1]);
      break;
    case FORMA_COND:
      if (n != 1) erro(l, "%s espera 1 operando", info->mnemonico);
      cond = cond_nome_para_codigo(partes[0]);
      if (cond < 0) erro(l, "condição inválida: %s", partes[0]);
      break;
    case FORMA_IM6:
      if (n != 1) erro(l, "%s espera 1 operando", info->mnemonico);
      snprintf(expr, sizeof(expr), "%s", partes[0]);
      break;
    case FORMA_NENHUMA:
      if (n != 0) erro(l, "%s não espera operandos", info->mnemonico);
      break;
    case FORMA_RET:
      if (n != 0) erro(l, "%s não espera operandos", info->mnemonico);
      reg = 7; mod = MOD_POSINC; dreg = 6;
      break;
    case FORMA_REG_MOD_FIXO:
      if (n != 1) erro(l, "%s espera 1 operando", info->mnemonico);
      reg = reg_nome_para_codigo(partes[0]);
      if (reg < 0) erro(l, "registrador inválido: %s", partes[0]);
      mod = info->fixo; dreg = info->fixo2;
      break;
    case FORMA_SUP_MOD_FIXO:
      if (n != 1) erro(l, "%s espera 1 operando", info->mnemonico);
      reg = sup_nome_para_codigo(partes[0]);
      if (reg < 0) erro(l, "registrador de supervisor inválido: %s", partes[0]);
      mod = info->fixo; dreg = info->fixo2;
      break;
    case FORMA_IM10_REG_FIXO:
      if (n != 1) erro(l, "%s espera 1 operando", info->mnemonico);
      reg = info->fixo;
      snprintf(expr, sizeof(expr), "%s", partes[0]);
      break;
    case FORMA_COND_DATA:
      if (n != 2) erro(l, "%s espera 2 operandos", info->mnemonico);
      cond = cond_nome_para_codigo(partes[0]);
      if (cond < 0) erro(l, "condição inválida: %s", partes[0]);
      parse_data(l, partes[1], &mod, &dreg, expr, sizeof(expr));
      if (mod_precisa_imediato(mod)) tamanho += 2;
      break;
  }

  if (primeira) { loc += tamanho; return; }

  bool relativo = strcasecmp(info->mnemonico, "bra") == 0 ||
                   strcasecmp(info->mnemonico, "brac") == 0 ||
                   strcasecmp(info->mnemonico, "braq") == 0;
  long endereco_prox = loc + tamanho;

  long valor_imediato = 0;
  if (mod_precisa_imediato(mod))
    valor_imediato = resolve_expr(l, expr, relativo && mod == MOD_IMED, endereco_prox);

  int im6_final = 0, im10_final = 0;
  if (info->forma == FORMA_COND_IM6) im6_final = (int)resolve_expr(l, expr, relativo, endereco_prox);
  else if (info->forma == FORMA_COND) im6_final = info->fixo;
  else if (info->forma == FORMA_IM6) im6_final = (int)resolve_expr(l, expr, false, endereco_prox);
  else if (info->forma == FORMA_REG_IM10) im10_final = (int)resolve_expr(l, expr, false, endereco_prox);
  else if (info->forma == FORMA_IM10_REG_FIXO) im10_final = (int)resolve_expr(l, expr, relativo, endereco_prox);

  if ((info->forma == FORMA_COND_IM6) && (im6_final < -32 || im6_final > 31))
    erro(l, "valor fora do intervalo de IM6 (-32..31): %d", im6_final);
  if ((info->forma == FORMA_REG_IM10 || info->forma == FORMA_IM10_REG_FIXO) && (im10_final < -512 || im10_final > 511))
    erro(l, "valor fora do intervalo de IM10 (-512..511): %d", im10_final);

  uint16_t palavra = instrucao_empacota(info->formato, info->codop, info->byte_op,
                                         reg, mod, dreg, cond, im6_final, im10_final);
  emite_byte((uint16_t)loc, (uint8_t)(palavra >> 8));
  emite_byte((uint16_t)(loc + 1), (uint8_t)(palavra & 0xFF));
  if (mod_precisa_imediato(mod)) {
    uint16_t im = (uint16_t)valor_imediato;
    emite_byte((uint16_t)(loc + 2), (uint8_t)(im >> 8));
    emite_byte((uint16_t)(loc + 3), (uint8_t)(im & 0xFF));
  }

  loc += tamanho;
}

// ------------------------------------------------------------ pseudo-ops

static void processa_db(const linha_t *l, char *operandos, bool primeira)
{
  char *p = operandos;
  while (*p != '\0') {
    while (isspace((unsigned char)*p)) p++;
    if (*p == '\0') break;

    if (*p == '"') {
      p++;
      char *inicio = p;
      while (*p != '\0' && *p != '"') p++;
      int tam = (int)(p - inicio);
      if (primeira) {
        loc += tam;
      } else {
        for (int i = 0; i < tam; i++) { emite_byte((uint16_t)loc, (uint8_t)inicio[i]); loc++; }
      }
      if (*p == '"') p++;
    } else {
      char item[64];
      int k = 0;
      while (*p != '\0' && *p != ',') { if (k < 63) item[k++] = *p; p++; }
      item[k] = '\0';
      trim(item);
      if (item[0] != '\0') {
        if (primeira) {
          loc += 1;
        } else {
          long v = resolve_expr(l, item, false, loc);
          emite_byte((uint16_t)loc, (uint8_t)(v & 0xFF));
          loc += 1;
        }
      }
    }
    while (*p == ',' || isspace((unsigned char)*p)) p++;
  }
}

static void processa_pseudo(const linha_t *l, const char *nome, char *resto, bool primeira)
{
  if (strcasecmp(nome, ".equ") == 0) {
    char *igual = strchr(resto, '=');
    if (igual == NULL) erro(l, ".equ espera 'nome = expressão'");
    char simb[64];
    snprintf(simb, sizeof(simb), "%.*s", (int)(igual - resto), resto);
    trim(simb);
    char expr[128];
    snprintf(expr, sizeof(expr), "%s", igual + 1);
    trim(expr);
    long valor = resolve_expr(l, expr, false, loc);
    if (primeira) define_simbolo(l, simb, valor);
    return;
  }
  if (strcasecmp(nome, ".org") == 0) {
    trim(resto);
    loc = resolve_expr(l, resto, false, loc);
    return;
  }
  if (strcasecmp(nome, ".dw") == 0) {
    char *partes[64];
    int n = divide_virgula(resto, partes, 64);
    for (int i = 0; i < n; i++) {
      if (primeira) { loc += 2; continue; }
      long v = resolve_expr(l, partes[i], false, loc);
      emite_byte((uint16_t)loc, (uint8_t)((v >> 8) & 0xFF));
      emite_byte((uint16_t)(loc + 1), (uint8_t)(v & 0xFF));
      loc += 2;
    }
    return;
  }
  if (strcasecmp(nome, ".db") == 0) { processa_db(l, resto, primeira); return; }
  if (strcasecmp(nome, ".ds") == 0) {
    trim(resto);
    long n = resolve_expr(l, resto, false, loc);
    loc += 2 * n;
    return;
  }
  if (strcasecmp(nome, ".ext") == 0) return; // informativo (módulo único combinado)
  if (strcasecmp(nome, ".pub") == 0) {
    if (!primeira) return;
    char *partes[32];
    int n = divide_virgula(resto, partes, 32);
    for (int i = 0; i < n && n_publicos < MAX_PUBLICOS; i++)
      snprintf(publicos[n_publicos++], 64, "%s", partes[i]);
    return;
  }
  if (strcasecmp(nome, ".text") == 0) { segmento_atual = 'T'; return; }
  if (strcasecmp(nome, ".data") == 0) { segmento_atual = 'D'; return; }
  if (strcasecmp(nome, ".bss") == 0) { segmento_atual = 'B'; return; }
  if (strcasecmp(nome, ".dbg") == 0) return; // depuração: não suportado, ignorado

  erro(l, "pseudo-instrução desconhecida: %s", nome);
}

// --------------------------------------------------------------- linhas

static void processa_linha(linha_t *l, bool primeira)
{
  char copia[320];
  snprintf(copia, sizeof(copia), "%s", l->texto);

  bool aspas = false;
  for (char *p = copia; *p != '\0'; p++) {
    if (*p == '"') aspas = !aspas;
    else if (*p == ';' && !aspas) { *p = '\0'; break; }
  }
  trim(copia);
  if (copia[0] == '\0') return;

  char *s = copia;
  char *p = s;
  while (*p != '\0' && !isspace((unsigned char)*p) && *p != ':') p++;
  if (*p == ':') {
    char rotulo[64];
    snprintf(rotulo, sizeof(rotulo), "%.*s", (int)(p - s), s);
    if (primeira) define_simbolo(l, rotulo, loc);
    s = p + 1;
    while (isspace((unsigned char)*s)) s++;
  }
  if (*s == '\0') return;

  char mnem[32];
  int k = 0;
  while (s[k] != '\0' && !isspace((unsigned char)s[k]) && k < 31) { mnem[k] = s[k]; k++; }
  mnem[k] = '\0';
  char *resto = s + k;
  while (isspace((unsigned char)*resto)) resto++;

  if (mnem[0] == '.') { processa_pseudo(l, mnem, resto, primeira); return; }

  const instr_info_t *info = instrucao_procura(mnem);
  if (info == NULL) erro(l, "mnemônico desconhecido: %s", mnem);
  monta_instrucao(l, info, resto, primeira);
}

static void processa_todas(bool primeira)
{
  loc = 0;
  segmento_atual = 'T';
  for (int i = 0; i < n_linhas; i++) processa_linha(&linhas[i], primeira);
}

static void carrega_arquivo(const char *nome)
{
  FILE *f = fopen(nome, "r");
  if (f == NULL) { fprintf(stderr, "não foi possível abrir '%s'\n", nome); exit(1); }
  char buf[320];
  int numero = 0;
  while (fgets(buf, sizeof(buf), f) != NULL) {
    numero++;
    if (n_linhas >= MAX_LINHAS) { fprintf(stderr, "arquivo fonte grande demais\n"); exit(1); }
    snprintf(linhas[n_linhas].texto, sizeof(linhas[n_linhas].texto), "%s", buf);
    linhas[n_linhas].arquivo = strdup(nome);
    linhas[n_linhas].numero = numero;
    n_linhas++;
  }
  fclose(f);
}

int main(int argc, char *argv[])
{
  const char *saida_nome = NULL;
  char *entradas[64];
  int n_entradas = 0;

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) saida_nome = argv[++i];
    else entradas[n_entradas++] = argv[i];
  }
  if (n_entradas == 0) {
    fprintf(stderr, "uso: %s [-o saida.mob] entrada.asm [...]\n", argv[0]);
    return 1;
  }

  static char nome_padrao[300];
  if (saida_nome == NULL) {
    snprintf(nome_padrao, sizeof(nome_padrao), "%s", entradas[0]);
    char *ponto = strrchr(nome_padrao, '.');
    if (ponto != NULL) *ponto = '\0';
    strncat(nome_padrao, ".mob", sizeof(nome_padrao) - strlen(nome_padrao) - 1);
    saida_nome = nome_padrao;
  }

  for (int i = 0; i < n_entradas; i++) carrega_arquivo(entradas[i]);

  processa_todas(true);

  saida = fopen(saida_nome, "w");
  if (saida == NULL) { fprintf(stderr, "não foi possível criar '%s'\n", saida_nome); return 1; }
  obj_escreve_cabecalho(saida);

  processa_todas(false);

  for (int i = 0; i < n_publicos; i++) {
    simb_mont_t *s = acha_simbolo(publicos[i]);
    if (s != NULL) obj_escreve_publico(saida, publicos[i], (uint16_t)s->valor);
  }

  fclose(saida);
  printf("montado: %s\n", saida_nome);
  return 0;
}
