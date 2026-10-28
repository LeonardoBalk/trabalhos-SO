// memoria.h -- memória real (física) do Mancha completo.
//
// mancha.pdf, secção 13 ("Simulador"): "um banco de memória volátil de
// 1Mbyte está diretamente conectado ao processador". Endereços reais têm
// portanto até 20 bits. O processador é big-endian.

#ifndef MEMORIA_H
#define MEMORIA_H

#include <stdint.h>
#include <stddef.h>

#define MEM_TAMANHO (1024 * 1024)
#define MEM_MASCARA (MEM_TAMANHO - 1)

typedef struct mem mem_t;

mem_t *mem_cria(void);
void mem_destroi(mem_t *m);

uint8_t mem_le_byte(mem_t *m, uint32_t endereco);
void mem_escreve_byte(mem_t *m, uint32_t endereco, uint8_t valor);

uint16_t mem_le_palavra(mem_t *m, uint32_t endereco);
void mem_escreve_palavra(mem_t *m, uint32_t endereco, uint16_t valor);

// acesso direto ao vetor de bytes (usado pelo carregador de objeto e pela
// interface de disco, que faz DMA direto na memória)
uint8_t *mem_vetor(mem_t *m);

#endif
