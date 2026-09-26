#include "lexer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdarg.h>

typedef struct {
  const char *buf;
  size_t i;
  const char *arquivo;
  int linha;
} lex_t;

static void erro(lex_t *L, const char *fmt, ...)
{
  fprintf(stderr, "%s:%d: erro: ", L->arquivo ? L->arquivo : "?", L->linha);
  va_list ap; va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  fprintf(stderr, "\n");
  exit(1);
}

static void consome_espacos_e_marcadores(lex_t *L)
{
  for (;;) {
    while (isspace((unsigned char)L->buf[L->i])) L->i++;
    if (strncmp(L->buf + L->i, "#LINHA ", 7) == 0) {
      L->i += 7;
      char *fim;
      long n = strtol(L->buf + L->i, &fim, 10);
      L->i = (size_t)(fim - L->buf);
      while (L->buf[L->i] == ' ') L->i++;
      if (L->buf[L->i] == '"') {
        L->i++;
        size_t ini = L->i;
        while (L->buf[L->i] != '"' && L->buf[L->i] != '\0') L->i++;
        size_t tam = L->i - ini;
        char *nome = malloc(tam + 1);
        memcpy(nome, L->buf + ini, tam);
        nome[tam] = '\0';
        L->arquivo = nome;
        if (L->buf[L->i] == '"') L->i++;
      }
      L->linha = (int)n;
      continue;
    }
    break;
  }
}

typedef struct { const char *palavra; tipo_token_t tipo; } kw_t;
static const kw_t PALAVRAS_CHAVE[] = {
  {"int", TK_KW_INT}, {"char", TK_KW_CHAR}, {"void", TK_KW_VOID},
  {"struct", TK_KW_STRUCT}, {"if", TK_KW_IF}, {"else", TK_KW_ELSE},
  {"while", TK_KW_WHILE}, {"do", TK_KW_DO}, {"for", TK_KW_FOR},
  {"break", TK_KW_BREAK}, {"continue", TK_KW_CONTINUE},
  {"return", TK_KW_RETURN}, {"sizeof", TK_KW_SIZEOF},
};
#define N_PALAVRAS_CHAVE (int)(sizeof(PALAVRAS_CHAVE)/sizeof(PALAVRAS_CHAVE[0]))

static int decodifica_escape(lex_t *L)
{
  // L->buf[L->i] aponta para o caractere logo após a '\'
  char c = L->buf[L->i++];
  switch (c) {
    case 'n': return '\n';
    case 't': return '\t';
    case 'r': return '\r';
    case '0': return '\0';
    case '\\': return '\\';
    case '\'': return '\'';
    case '"': return '"';
    case 'x': {
      int v = 0, n = 0;
      while (isxdigit((unsigned char)L->buf[L->i]) && n < 2) {
        char h = L->buf[L->i++];
        v = v * 16 + (isdigit((unsigned char)h) ? h - '0' : (tolower(h) - 'a' + 10));
        n++;
      }
      return v;
    }
    default:
      erro(L, "sequência de escape desconhecida: \\%c", c);
      return 0;
  }
}

#define MAX_TOKENS 262144
static token_t tabela[MAX_TOKENS];
static int n_tabela = 0;

static void adiciona(token_t t) { if (n_tabela >= MAX_TOKENS) { fprintf(stderr, "mcc: programa grande demais\n"); exit(1); } tabela[n_tabela++] = t; }

token_t *lexer_tokeniza(const char *buffer, int *n_tokens)
{
  n_tabela = 0;
  lex_t L = { .buf = buffer, .i = 0, .arquivo = "?", .linha = 1 };

  for (;;) {
    consome_espacos_e_marcadores(&L);
    char c = L.buf[L.i];
    token_t t; memset(&t, 0, sizeof(t));
    t.arquivo = L.arquivo; t.linha = L.linha;

    if (c == '\0') { t.tipo = TK_EOF; adiciona(t); break; }

    if (isdigit((unsigned char)c)) {
      char *fim;
      long v = strtol(L.buf + L.i, &fim, 0);
      L.i = (size_t)(fim - L.buf);
      t.tipo = TK_INT_LIT; t.valor = v;
      adiciona(t);
      continue;
    }

    if (c == '_' || isalpha((unsigned char)c)) {
      size_t ini = L.i;
      while (L.buf[L.i] == '_' || isalnum((unsigned char)L.buf[L.i])) L.i++;
      size_t n = L.i - ini;
      char nome[64];
      size_t ncopia = n < sizeof(nome) - 1 ? n : sizeof(nome) - 1;
      memcpy(nome, L.buf + ini, ncopia); nome[ncopia] = '\0';
      int achou = -1;
      for (int k = 0; k < N_PALAVRAS_CHAVE; k++)
        if (strcmp(PALAVRAS_CHAVE[k].palavra, nome) == 0) { achou = k; break; }
      if (achou >= 0) t.tipo = PALAVRAS_CHAVE[achou].tipo;
      else { t.tipo = TK_IDENT; snprintf(t.texto, sizeof(t.texto), "%s", nome); }
      adiciona(t);
      continue;
    }

    if (c == '\'') {
      L.i++;
      int v;
      if (L.buf[L.i] == '\\') { L.i++; v = decodifica_escape(&L); }
      else v = (unsigned char)L.buf[L.i++];
      if (L.buf[L.i] != '\'') erro(&L, "literal de caractere mal formado");
      L.i++;
      t.tipo = TK_INT_LIT; t.valor = v;
      adiciona(t);
      continue;
    }

    if (c == '"') {
      L.i++;
      char acc[1024]; int k = 0;
      while (L.buf[L.i] != '"') {
        if (L.buf[L.i] == '\0') erro(&L, "string não terminada");
        int v;
        if (L.buf[L.i] == '\\') { L.i++; v = decodifica_escape(&L); }
        else v = (unsigned char)L.buf[L.i++];
        if (k < (int)sizeof(acc) - 1) acc[k++] = (char)v;
      }
      L.i++;
      acc[k] = '\0';
      t.tipo = TK_STRING_LIT;
      t.str = malloc((size_t)k + 1);
      memcpy(t.str, acc, (size_t)k + 1);
      t.len = k;
      adiciona(t);
      continue;
    }

#define OP2(a,b,tk) if (c==(a) && L.buf[L.i+1]==(b)) { t.tipo=(tk); L.i+=2; adiciona(t); continue; }
#define OP3(a,b,cc,tk) if (c==(a) && L.buf[L.i+1]==(b) && L.buf[L.i+2]==(cc)) { t.tipo=(tk); L.i+=3; adiciona(t); continue; }

    OP3('<','<','=',TK_SHL_ASS);
    OP3('>','>','=',TK_SHR_ASS);
    OP2('-','>',TK_ARROW);
    OP2('+','+',TK_INC);
    OP2('-','-',TK_DEC);
    OP2('&','&',TK_ANDAND);
    OP2('|','|',TK_OROR);
    OP2('=','=',TK_EQ);
    OP2('!','=',TK_NEQ);
    OP2('<','=',TK_LE);
    OP2('>','=',TK_GE);
    OP2('<','<',TK_SHL);
    OP2('>','>',TK_SHR);
    OP2('+','=',TK_PLUS_ASS);
    OP2('-','=',TK_MINUS_ASS);
    OP2('*','=',TK_STAR_ASS);
    OP2('/','=',TK_SLASH_ASS);
    OP2('%','=',TK_PERCENT_ASS);
    OP2('&','=',TK_AMP_ASS);
    OP2('|','=',TK_PIPE_ASS);
    OP2('^','=',TK_CARET_ASS);
#undef OP2
#undef OP3

    tipo_token_t simples = TK_EOF;
    bool achou_simples = true;
    switch (c) {
      case '(': simples = TK_LPAREN; break;
      case ')': simples = TK_RPAREN; break;
      case '{': simples = TK_LBRACE; break;
      case '}': simples = TK_RBRACE; break;
      case '[': simples = TK_LBRACKET; break;
      case ']': simples = TK_RBRACKET; break;
      case ';': simples = TK_SEMI; break;
      case ',': simples = TK_COMMA; break;
      case '.': simples = TK_DOT; break;
      case '+': simples = TK_PLUS; break;
      case '-': simples = TK_MINUS; break;
      case '*': simples = TK_STAR; break;
      case '/': simples = TK_SLASH; break;
      case '%': simples = TK_PERCENT; break;
      case '=': simples = TK_ASSIGN; break;
      case '<': simples = TK_LT; break;
      case '>': simples = TK_GT; break;
      case '!': simples = TK_NOT; break;
      case '&': simples = TK_AMP; break;
      case '|': simples = TK_PIPE; break;
      case '^': simples = TK_CARET; break;
      case '~': simples = TK_TILDE; break;
      case '?': simples = TK_QUESTION; break;
      case ':': simples = TK_COLON; break;
      default: achou_simples = false; break;
    }
    if (achou_simples) { t.tipo = simples; L.i++; adiciona(t); continue; }

    erro(&L, "caractere inesperado: '%c' (0x%02X)", isprint((unsigned char)c) ? c : '?', (unsigned char)c);
  }

  *n_tokens = n_tabela;
  return tabela;
}

const char *token_nome(tipo_token_t t)
{
  switch (t) {
    case TK_EOF: return "fim de arquivo";
    case TK_IDENT: return "identificador";
    case TK_INT_LIT: return "número";
    case TK_STRING_LIT: return "string";
    case TK_KW_INT: return "'int'";
    case TK_KW_CHAR: return "'char'";
    case TK_KW_VOID: return "'void'";
    case TK_KW_STRUCT: return "'struct'";
    case TK_KW_IF: return "'if'";
    case TK_KW_ELSE: return "'else'";
    case TK_KW_WHILE: return "'while'";
    case TK_KW_DO: return "'do'";
    case TK_KW_FOR: return "'for'";
    case TK_KW_BREAK: return "'break'";
    case TK_KW_CONTINUE: return "'continue'";
    case TK_KW_RETURN: return "'return'";
    case TK_KW_SIZEOF: return "'sizeof'";
    case TK_LPAREN: return "'('";
    case TK_RPAREN: return "')'";
    case TK_LBRACE: return "'{'";
    case TK_RBRACE: return "'}'";
    case TK_LBRACKET: return "'['";
    case TK_RBRACKET: return "']'";
    case TK_SEMI: return "';'";
    case TK_COMMA: return "','";
    case TK_DOT: return "'.'";
    case TK_ARROW: return "'->'";
    default: return "operador";
  }
}
