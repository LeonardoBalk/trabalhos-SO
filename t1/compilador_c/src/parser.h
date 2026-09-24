// parser.h -- ponto de entrada do compilador: consome o vetor de tokens
// (lexer.h) e escreve um programa em linguagem de montagem do Mancha
// completo no FILE* de saída.

#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"
#include <stdio.h>

// "prefixo" diferencia os rótulos internos (_L.../_str_...) gerados por
// invocações separadas do compilador, para que arquivos .asm de módulos
// .c diferentes possam ser montados juntos sem colisão de nomes.
void compilador_compila(token_t *tokens, int n_tokens, FILE *saida, unsigned prefixo);

#endif
