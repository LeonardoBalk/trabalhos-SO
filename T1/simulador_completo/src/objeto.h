// objeto.h -- formato de arquivo objeto simplificado usado por este
// projeto (extensão .mob). Ver README.md para a justificativa: o montador
// (src/montador.c) resolve todos os símbolos para endereços absolutos no
// momento da montagem (atuando como "montador + ligador" em um só passo),
// então o arquivo objeto não precisa do mecanismo de relocação binário
// completo descrito na secção 12 do mancha.pdf -- só uma lista de pares
// (endereço absoluto, palavra) para os segmentos de código e dados, mais
// os símbolos públicos (gravados apenas a título de informação/depuração).
//
// Formato texto (granularidade de byte, o que permite representar tanto
// palavras de instrução quanto sequências soltas de bytes geradas por
// .db, sem se preocupar com alinhamento):
//   mobj 1
//   T <endereco-hex> <byte-hex>     -- um byte do segmento de código
//   D <endereco-hex> <byte-hex>     -- um byte do segmento de dados
//   P <nome> <endereco-hex>         -- símbolo público

#ifndef OBJETO_H
#define OBJETO_H

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include "memoria.h"

typedef struct simbolo {
  char nome[64];
  uint16_t endereco;
  struct simbolo *prox;
} simbolo_t;

// escrita (usada pelo montador)
bool obj_escreve_cabecalho(FILE *f);
void obj_escreve_byte(FILE *f, char segmento, uint16_t endereco, uint8_t valor);
void obj_escreve_publico(FILE *f, const char *nome, uint16_t endereco);

// leitura (usada pelo simulador); "simbolos" recebe a lista de símbolos
// públicos encontrados (o chamador deve liberar com obj_libera_simbolos)
bool obj_carrega(const char *arquivo, mem_t *mem, simbolo_t **simbolos, char *erro, size_t erro_tam);
void obj_libera_simbolos(simbolo_t *lista);
uint16_t obj_busca_simbolo(simbolo_t *lista, const char *nome, bool *achou);

#endif
