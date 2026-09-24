// lexer.h -- tokenizador do subconjunto de C, operando sobre o texto já
// expandido pelo pré-processador (pre.h), que contém marcadores
// "#LINHA <n> \"<arquivo>\"" para rastrear a origem de cada token.

#ifndef LEXER_H
#define LEXER_H

#include <stdbool.h>

typedef enum {
  TK_EOF, TK_IDENT, TK_INT_LIT, TK_STRING_LIT,

  TK_KW_INT, TK_KW_CHAR, TK_KW_VOID, TK_KW_STRUCT, TK_KW_IF, TK_KW_ELSE,
  TK_KW_WHILE, TK_KW_DO, TK_KW_FOR, TK_KW_BREAK, TK_KW_CONTINUE,
  TK_KW_RETURN, TK_KW_SIZEOF,

  TK_LPAREN, TK_RPAREN, TK_LBRACE, TK_RBRACE, TK_LBRACKET, TK_RBRACKET,
  TK_SEMI, TK_COMMA, TK_DOT, TK_ARROW,

  TK_PLUS, TK_MINUS, TK_STAR, TK_SLASH, TK_PERCENT,
  TK_ASSIGN, TK_EQ, TK_NEQ, TK_LT, TK_LE, TK_GT, TK_GE,
  TK_ANDAND, TK_OROR, TK_NOT, TK_AMP, TK_PIPE, TK_CARET, TK_TILDE,
  TK_SHL, TK_SHR, TK_INC, TK_DEC,
  TK_PLUS_ASS, TK_MINUS_ASS, TK_STAR_ASS, TK_SLASH_ASS, TK_PERCENT_ASS,
  TK_AMP_ASS, TK_PIPE_ASS, TK_CARET_ASS, TK_SHL_ASS, TK_SHR_ASS,
  TK_QUESTION, TK_COLON,
} tipo_token_t;

typedef struct {
  tipo_token_t tipo;
  char texto[64];       // TK_IDENT: nome
  long valor;             // TK_INT_LIT: valor
  char *str;               // TK_STRING_LIT: conteúdo já decodificado (escapes resolvidos)
  int len;                  // TK_STRING_LIT: comprimento de str (sem contar '\0')
  const char *arquivo;
  int linha;
} token_t;

// tokeniza todo o buffer (saída do pré-processador); devolve vetor alocado
// (chamador não precisa liberar -- vive até o fim do processo) terminado
// com um token TK_EOF; preenche *n_tokens.
token_t *lexer_tokeniza(const char *buffer_preprocessado, int *n_tokens);

const char *token_nome(tipo_token_t t); // para mensagens de erro

#endif
