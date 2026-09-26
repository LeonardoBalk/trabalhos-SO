// dispositivos.h -- console, disco, relógio e controlador de interrupções
// do Mancha completo (mancha.pdf, secção 13), todos endereçados como um
// espaço de portas de entrada e saída de 64K bytes (tabela 17).
//
// Simplificação assumida (documentada no README): a operação de disco não
// é assíncrona em tempo real -- ao ser disparada ela conta um pequeno
// número de "pulsos" (chamadas a disp_tick) antes de completar e gerar a
// interrupção 9, só para tornar visível na interface que a operação está
// em andamento. O relógio incrementa seu contador a cada instrução
// executada (interpretando "pulso de relógio do processador" como um
// passo de simulação), e ao bater no limite gera a interrupção 10 e volta
// a contar de zero (para servir como temporizador periódico útil).

#ifndef DISPOSITIVOS_H
#define DISPOSITIVOS_H

#include <stdint.h>
#include <stdbool.h>
#include "memoria.h"

#define PORTA_CONSOLE_DADOS   0x0001
#define PORTA_CONSOLE_ESTADO  0x0002
#define PORTA_DISCO_FACE      0x0010
#define PORTA_DISCO_TRILHA    0x0011
#define PORTA_DISCO_SETOR     0x0012
#define PORTA_DISCO_OPERACAO  0x0013
#define PORTA_DISCO_END_ALTO  0x0014
#define PORTA_DISCO_END_BAIXO 0x0015
#define PORTA_RELOGIO_CONT_ALTO   0x0020
#define PORTA_RELOGIO_CONT_BAIXO  0x0021
#define PORTA_RELOGIO_LIM_ALTO    0x0022
#define PORTA_RELOGIO_LIM_BAIXO   0x0023
#define PORTA_CTRL_INTERRUPCOES   0x0030

#define INT_CONSOLE  8
#define INT_DISCO    9
#define INT_RELOGIO  10

#define DISCO_UNIDADES 2
#define DISCO_FACES    2
#define DISCO_TRILHAS  40
#define DISCO_SETORES  18
#define DISCO_TAM_SETOR 512

#define CONSOLE_ENTRADA_MAX 256
#define CONSOLE_SAIDA_MAX 4096

typedef struct dispositivos disp_t;

disp_t *disp_cria(mem_t *mem);
void disp_destroi(disp_t *d);

// carrega (ou cria, se não existir) o arquivo de imagem de disco da unidade
bool disp_monta_disco(disp_t *d, int unidade, const char *arquivo, char *erro, size_t erro_tam);

uint8_t disp_le_byte(disp_t *d, uint16_t porta);
void disp_escreve_byte(disp_t *d, uint16_t porta, uint8_t valor);

// avança um passo de simulação (chamado uma vez por instrução executada)
void disp_tick(disp_t *d);

// interrupções externas pendentes e habilitadas (considerando a máscara do
// controlador de interrupções, porta 0030): retorna o número da de maior
// prioridade (menor número) ou -1 se não houver nenhuma
int disp_interrupcao_pendente(disp_t *d);
void disp_confirma_interrupcao(disp_t *d, int numero);

// console: entrada de teclado simulada pelo operador e saída para a tela
void console_poe_entrada(disp_t *d, char c);
void console_limpa_saida(disp_t *d);
const char *console_saida(disp_t *d, int *tamanho);

// informações para a interface (somente leitura)
void disp_estado_disco(disp_t *d, int unidade, bool *ocupado, bool *erro, int *ticks_restantes);
uint16_t disp_relogio_contador(disp_t *d);
uint16_t disp_relogio_limite(disp_t *d);
uint8_t disp_mascara_interrupcoes(disp_t *d);

#endif
