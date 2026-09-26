#include "memoria.h"
#include <stdlib.h>

struct mem {
  uint8_t bytes[MEM_TAMANHO];
};

mem_t *mem_cria(void)
{
  mem_t *m = calloc(1, sizeof(mem_t));
  return m;
}

void mem_destroi(mem_t *m)
{
  free(m);
}

uint8_t mem_le_byte(mem_t *m, uint32_t endereco)
{
  return m->bytes[endereco & MEM_MASCARA];
}

void mem_escreve_byte(mem_t *m, uint32_t endereco, uint8_t valor)
{
  m->bytes[endereco & MEM_MASCARA] = valor;
}

uint16_t mem_le_palavra(mem_t *m, uint32_t endereco)
{
  uint8_t alto = mem_le_byte(m, endereco);
  uint8_t baixo = mem_le_byte(m, endereco + 1);
  return (uint16_t)((alto << 8) | baixo);
}

void mem_escreve_palavra(mem_t *m, uint32_t endereco, uint16_t valor)
{
  mem_escreve_byte(m, endereco, (uint8_t)(valor >> 8));
  mem_escreve_byte(m, endereco + 1, (uint8_t)(valor & 0xFF));
}

uint8_t *mem_vetor(mem_t *m)
{
  return m->bytes;
}
