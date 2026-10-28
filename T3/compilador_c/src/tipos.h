// tipos.h -- sistema de tipos do subconjunto de C suportado pelo mcc
// (int, char, void, ponteiros, arrays, structs, ponteiros de função --
// sem float/double, sem union, sem enum, sem typedef, sem qualificadores
// de tamanho/sinal).
//
// Ponteiro de função é representado como um T_PONTEIRO cujo "base" é um
// T_FUNCAO (a assinatura: tipo de retorno + tipos dos parâmetros) --
// deliberadamente reaproveitando a mesma estrutura de ponteiro/array já
// existente, para que a maior parte da máquina de tipos (decaimento,
// tamanho, indexação de array de ponteiros de função, membro de struct)
// funcione sem nenhum código extra. Ver o comentário de
// "parseia_declarador_ex" em parser.c para a sintaxe suportada
// (um único nível: sem ponteiro-de-ponteiro-de-função, sem função
// retornando ponteiro de função).

#ifndef TIPOS_H
#define TIPOS_H

#include <stdbool.h>
#include <stddef.h>

#define TIPO_MAX_PARAMS 16

typedef enum {
  T_VOID,
  T_INT,      // 16 bits, com sinal
  T_CHAR,     // 8 bits, sem sinal
  T_PONTEIRO,
  T_ARRAY,
  T_STRUCT,
  T_FUNCAO,   // só aparece como "base" de um T_PONTEIRO (ponteiro de função);
               // ver comentário acima
} categoria_tipo_t;

typedef struct tipo tipo_t;
typedef struct membro membro_t;
typedef struct estrutura estrutura_t;

struct tipo {
  categoria_tipo_t cat;
  tipo_t *base;        // PONTEIRO: tipo apontado. ARRAY: tipo do elemento.
                          // FUNCAO: tipo de retorno.
  int qtd;              // ARRAY: número de elementos. FUNCAO: número de parâmetros.
  estrutura_t *estrutura; // STRUCT
  tipo_t *params[TIPO_MAX_PARAMS]; // FUNCAO: tipos dos parâmetros, na ordem declarada
};

struct membro {
  char nome[32];
  tipo_t *tipo;
  int offset;           // em bytes, a partir do início da struct
  membro_t *prox;
};

struct estrutura {
  char nome[32];
  membro_t *membros;    // lista ligada, na ordem de declaração
  int tamanho;           // bytes totais (sem preenchimento/padding)
  bool completa;          // true depois que "}" da definição é processado
  estrutura_t *prox;
};

void tipos_inicializa(void);

tipo_t *tipo_void(void);
tipo_t *tipo_int(void);
tipo_t *tipo_char(void);
tipo_t *tipo_ponteiro(tipo_t *base);
tipo_t *tipo_array(tipo_t *base, int qtd);
tipo_t *tipo_de_struct(estrutura_t *e);
tipo_t *tipo_funcao(tipo_t *retorno, tipo_t *const *params, int n_params);

// tabela de structs (por nome de tag)
estrutura_t *estrutura_acha(const char *nome);
estrutura_t *estrutura_declara(const char *nome); // cria (incompleta) se não existir
membro_t *estrutura_acha_membro(estrutura_t *e, const char *nome);
void estrutura_adiciona_membro(estrutura_t *e, const char *nome, tipo_t *tipo);

int tipo_tamanho(const tipo_t *t);            // bytes
bool tipo_eh_ponteiro_ou_array(const tipo_t *t);
tipo_t *tipo_decai(tipo_t *t);                 // array -> ponteiro (decaimento em expressões)
bool tipo_igual(const tipo_t *a, const tipo_t *b);
bool tipo_escalar(const tipo_t *t);            // int/char/ponteiro (cabe num registrador)
const char *tipo_texto(const tipo_t *t);       // para mensagens de erro (buffer estático)

#endif
