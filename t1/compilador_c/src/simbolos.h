// simbolos.h -- tabela de símbolos: globais/funções (tabela única) e pilha
// de escopos locais (parâmetros e variáveis locais de uma função, com
// visibilidade por bloco).

#ifndef SIMBOLOS_H
#define SIMBOLOS_H

#include "tipos.h"
#include <stdbool.h>

#define SIMB_MAX_PARAMS 16

typedef enum { SIMB_GLOBAL, SIMB_LOCAL, SIMB_FUNCAO } categoria_simbolo_t;

typedef struct simbolo {
  char nome[32];
  categoria_simbolo_t categoria;
  tipo_t *tipo;                        // variável: seu tipo. função: tipo de retorno.
  int offset;                           // LOCAL: deslocamento relativo a bp (bytes)
  char rotulo[40];                      // GLOBAL/FUNCAO: rótulo no assembly gerado
  bool eh_parametro;                    // LOCAL: true para parâmetros -- sempre ocupam
                                          // uma palavra inteira na pilha (ver registra_parametros
                                          // em parser.c), mesmo quando o tipo C é "char"

  tipo_t *params[SIMB_MAX_PARAMS];
  int n_params;
  bool declarada;                       // já viu protótipo/definição
  bool definida;                        // já viu o corpo "{ ... }"
  int idx_corpo_abre, idx_corpo_fecha;  // índices no vetor de tokens

  struct simbolo *prox;
} simbolo_t;

void simbolos_inicializa(void);

// tabela global (variáveis globais e funções compartilham o mesmo namespace,
// como em C)
simbolo_t *global_declara_var(const char *nome, tipo_t *tipo);
simbolo_t *global_declara_funcao(const char *nome, tipo_t *ret);
simbolo_t *global_acha(const char *nome);

// pilha de escopos locais (parâmetros + variáveis locais de uma função)
void escopo_reinicia(void);              // chamado ao começar a compilar uma função
int escopo_marca(void);                   // devolve marcador do topo atual
void escopo_volta(int marcador);          // remove entradas acima do marcador
void escopo_declara(simbolo_t *s);        // empilha para visibilidade

simbolo_t *simbolo_local_cria(const char *nome, tipo_t *tipo, int offset);

// procura: primeiro na pilha de escopos locais (do topo para a base),
// depois na tabela global
simbolo_t *simbolo_acha(const char *nome);

#endif
